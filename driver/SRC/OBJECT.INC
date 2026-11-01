; Phase 3 physical pens/brushes; caller-owned, no allocation or shared scratch.
O_LOCAL equ 256
O_RESERVE equ 416
O_BUF equ -168                   ; 56 byte staged result
O_LOG equ -180                   ; 12 byte logical input snapshot
O_SIZE equ -182
O_FG equ -186
O_BG equ -190
O_GREY equ -192
O_PLANE equ -194
O_ROW equ -196
O_LEVEL equ -198
O_LOIDX equ -200
O_HIIDX equ -202
O_TILE equ -204
O_BIT equ -206
O_COLUMN equ -208
public RealizeObject, DitherTile

; AL component, CX nonzero permits special greys, SS:DI eight result bytes.
DitherTile proc near
    xor ah,ah
    inc ax
    shr ax,1
    shr ax,1
    jcxz DitherRaw
    cmp ax,16
    je DitherGrey
    cmp ax,32
    je DitherGrey
    cmp ax,48
    jne DitherRaw
DitherGrey:
    shr ax,1
    shr ax,1
    shr ax,1
    sub ax,2
    mov bx,ax
    mov ax,word ptr GreyRows[bx]
    mov cx,4
DitherGreyCopy:
    mov ss:[di],ax
    add di,2
    loop DitherGreyCopy
    ret
DitherRaw:
    shl ax,1
    shl ax,1
    shl ax,1
    mov si,ax
    mov cx,8
DitherCopy:
    mov al,DitherRows[si]
    mov ss:[di],al
    inc si
    inc di
    loop DitherCopy
    ret
DitherTile endp

RealizeObject proc far
    mov ax,ds
    nop
    inc bp
    push bp
    mov bp,sp
    push ds
    push si
    push di
    mov ax,seg DriverState
    mov ds,ax
    mov ax,O_RESERVE
    call StackRoom
    jnc @F
    jmp ObjectBad
@@:
    sub sp,O_LOCAL
    mov ax,[bp+18]
    or ax,ax
    jns @F
    jmp ObjectOK                 ; no resources to free, including font deletion
@@:
    mov cx,6
    cmp ax,1
    je ObjectSize
    mov cx,56
    cmp ax,2
    je ObjectSize
    jmp ObjectBad
ObjectSize:
    mov [bp+O_SIZE],cx
    mov ax,[bp+10]
    or ax,[bp+12]
    jnz ObjectActual
    mov ax,cx                    ; inquiry must not dereference input/device
    jmp ObjectReturn
ObjectActual:
    mov ax,[bp+22]
    mov bx,[bp+20]
    call SnapshotSurface
    jnc @F
    jmp ObjectBad
@@:
    call ClassifyFormat
    jnc @F
    jmp ObjectBad
@@:
    mov ax,[bp+12]
    mov bx,[bp+10]
    mov cx,[bp+O_SIZE]
    call CheckSpan
    jnc @F
    jmp ObjectBad
@@:
    mov ax,[bp+16]
    mov bx,[bp+14]
    mov cx,10
    cmp word ptr [bp+18],1
    je ObjectRead
    mov cx,12
ObjectRead:
    call CheckReadSpan
    jnc @F
    jmp ObjectBad
@@:
    ; Both spans proved; keep a complete local snapshot before staging output.
    push ds
    mov ds,ax
    mov si,bx
    push ss
    pop es
    lea di,[bp+O_LOG]
    shr cx,1
    cld
    rep movsw
    pop ds
    lea di,[bp+O_BUF]
    mov cx,[bp+O_SIZE]
    shr cx,1
    xor ax,ax
    rep stosw
    cmp word ptr [bp+18],1
    jne BrushStart
    mov cx,[bp+O_LOG]
    cmp cx,5
    jne PenColour
    mov word ptr [bp+O_BUF+4],5
    jmp ObjectCommit
PenColour:
    cmp word ptr [bp+O_LOG+2],1
    ja PenSolid
    cmp cx,5
    jbe PenStyleReady
PenSolid:
    xor cx,cx
PenStyleReady:
    mov [bp+O_BUF+4],cx
    mov ax,[bp+O_LOG+6]
    mov dx,[bp+O_LOG+8]
    or dh,dh
    jz @F
    jmp ObjectBad
@@:
    call LogicalToPhysical
    mov [bp+O_BUF],ax
    mov [bp+O_BUF+2],dx
    jmp ObjectCommit
BrushStart:
    mov ax,[bp+6]
    and al,7
    mov byte ptr [bp+O_BUF+50],al
    mov ax,[bp+8]
    and al,7
    mov byte ptr [bp+O_BUF+51],al
    mov ax,[bp+O_LOG]
    cmp ax,1
    jne BrushVisible
    mov word ptr [bp+O_BUF+48],1
    jmp ObjectCommit
BrushVisible:
    mov word ptr [bp+O_BUF+40],0ffffh
    mov word ptr [bp+O_BUF+42],0ffffh
    mov word ptr [bp+O_BUF+44],0ffffh
    mov word ptr [bp+O_BUF+46],0ffffh
    cmp ax,3
    jne @F
    jmp BrushPattern
@@:
    mov ax,[bp+O_LOG+2]
    mov dx,[bp+O_LOG+4]
    or dh,dh
    jz @F
    jmp ObjectBad
@@:
    cmp word ptr [bp+O_LOG],2
    jne BrushSolid
    jmp BrushHatch
BrushSolid:
    ; Exact canonical colours are solid. Other greys interpolate adjacent
    ; entries 0/7/8/15. Other RGB values dither in the dark 0/128 cube when
    ; max<=128, otherwise the bright 0/255 cube; its zero corner is index 0.
    ; Every intermediate pixel is a real palette index, never an RGB channel.
    call LogicalToPhysical
    cmp ax,[bp+O_LOG+2]
    jne SolidApproximate
    cmp dl,byte ptr [bp+O_LOG+4]
    jne SolidApproximate
    mov al,dh
    and ax,15
    ; Canonical colour: no per-row dither mixing is needed for colour planes.
    mov bx,ax
    lea di,[bp+O_BUF]
    mov dx,4
SolidExactPlane:
    shr bx,1
    sbb ax,ax
    mov ss:[di],ax
    mov ss:[di+2],ax
    mov ss:[di+4],ax
    mov ss:[di+6],ax
    add di,8
    dec dx
    jnz SolidExactPlane
    jmp SolidMono
SolidApproximate:
    mov ax,[bp+O_LOG+2]
    cmp al,ah
    je @F
    jmp SolidCube
@@:
    cmp al,byte ptr [bp+O_LOG+4]
    je @F
    jmp SolidCube
@@:
    xor ah,ah
    mov word ptr [bp+O_LOIDX],0
    mov word ptr [bp+O_HIIDX],7
    mov bx,128
    cmp ax,128
    jb SolidGreyQuantize
    mov word ptr [bp+O_LOIDX],7
    mov word ptr [bp+O_HIIDX],8
    mov bx,64
    cmp ax,192
    jae SolidLightGrey
    sub ax,128
    jmp SolidGreyQuantize
SolidLightGrey:
    mov word ptr [bp+O_LOIDX],8
    mov word ptr [bp+O_HIIDX],15
    mov bx,63
    sub ax,192
SolidGreyQuantize:
    call BrushLevel             ; AX=rounded coverage 0..64
SolidIndexTile:
    shl ax,3
    mov [bp+O_TILE],ax
    mov word ptr [bp+O_PLANE],0
    lea di,[bp+O_BUF]
SolidIndexPlane:
    mov cx,[bp+O_PLANE]
    mov ax,[bp+O_LOIDX]
    mov dx,[bp+O_HIIDX]
    shr ax,cl
    shr dx,cl
    and al,1
    and dl,1
    neg al
    neg dl
    mov ah,al                   ; low pixel byte
    mov dh,dl                   ; high pixel byte
    mov si,[bp+O_TILE]
    mov cx,8
SolidIndexRow:
    mov al,DitherRows[si]
    mov dl,al
    and al,dh
    not dl
    and dl,ah
    or al,dl
    mov ss:[di],al
    inc di
    inc si
    loop SolidIndexRow
    inc word ptr [bp+O_PLANE]
    cmp word ptr [bp+O_PLANE],4
    jb SolidIndexPlane
    jmp SolidMono
SolidCube:
    mov word ptr [bp+O_LEVEL],128
    cmp al,128
    ja SolidBright
    cmp ah,128
    ja SolidBright
    cmp byte ptr [bp+O_LOG+4],128
    jbe SolidCubeReady
SolidBright:
    mov word ptr [bp+O_LEVEL],255
SolidCubeReady:
    mov word ptr [bp+O_PLANE],0
    lea di,[bp+O_BUF]
SolidCubePlane:
    mov si,[bp+O_PLANE]
    xor ax,ax
    mov al,byte ptr [bp+O_LOG+2+si]
    mov bx,[bp+O_LEVEL]
    call BrushLevel
    shl ax,3
    mov si,ax
    mov cx,8
SolidCubeRow:
    mov al,DitherRows[si]
    mov ss:[di],al
    inc si
    inc di
    loop SolidCubeRow
    inc word ptr [bp+O_PLANE]
    cmp word ptr [bp+O_PLANE],3
    jb SolidCubePlane
    cmp word ptr [bp+O_LEVEL],255
    jne SolidMono
    xor si,si
SolidIntensity:
    mov al,byte ptr [bp+O_BUF+si]
    or al,byte ptr [bp+O_BUF+8+si]
    or al,byte ptr [bp+O_BUF+16+si]
    mov byte ptr [bp+O_BUF+24+si],al
    inc si
    cmp si,8
    jb SolidIntensity
SolidMono:
    xor ax,ax
    xor bx,bx
    mov al,byte ptr [bp+O_LOG+2]
    mov bl,byte ptr [bp+O_LOG+3]
    add ax,bx
    mov bl,byte ptr [bp+O_LOG+4]
    add ax,bx
    xor dx,dx
    mov bx,3
    div bx
    lea di,[bp+O_BUF+32]
    mov cx,1
    call DitherTile
    jmp ObjectCommit
BrushHatch:
    mov bx,[bp+O_LOG+6]
    cmp bx,5
    jbe @F
    jmp ObjectBad
@@:
    call LogicalToPhysical
    mov [bp+O_FG],ax
    mov [bp+O_FG+2],dx
    mov ax,[bp+O_LOG+8]
    mov dx,[bp+O_LOG+10]
    mov [bp+O_BG],ax
    mov [bp+O_BG+2],dx
    mov word ptr [bp+O_BUF+48],2
    shl bx,1
    shl bx,1
    shl bx,1
    xor si,si
HatchRow:
    mov al,HatchRows[bx+si]
    mov byte ptr [bp+O_BUF+40+si],al
    inc si
    cmp si,8
    jb HatchRow
    ; Plane 4 is the independent cached mono bit (metadata bit 4).
    mov word ptr [bp+O_PLANE],0
    lea di,[bp+O_BUF]
HatchPlane:
    mov cx,[bp+O_PLANE]
    mov al,byte ptr [bp+O_FG+3]
    mov ah,byte ptr [bp+O_BG+3]
    shr al,cl
    shr ah,cl
    and al,1
    and ah,1
    neg al
    neg ah
    mov dl,al
    mov dh,ah
    xor si,si
HatchPixelRow:
    mov al,byte ptr [bp+O_BUF+40+si]
    mov ah,al
    and al,dl
    not ah
    and ah,dh
    or al,ah
    mov ss:[di],al
    inc di
    inc si
    cmp si,8
    jb HatchPixelRow
    inc word ptr [bp+O_PLANE]
    cmp word ptr [bp+O_PLANE],5
    jb HatchPlane
    jmp ObjectCommit
BrushPattern:
    mov ax,[bp+O_LOG+4]
    mov bx,[bp+O_LOG+2]
    call SnapshotSurface
    jnc @F
    jmp ObjectBad
@@:
    call ClassifyFormat
    jnc @F
    jmp ObjectBad
@@:
    cmp word ptr [bp+S_KIND],0
    je @F
    jmp ObjectBad
@@:
    cmp word ptr [bp+S_DESC+2],8
    je @F
    jmp PatternBad
@@:
    cmp word ptr [bp+S_DESC+4],8
    je @F
    jmp PatternBad
@@:
    mov word ptr [bp+O_BUF+48],3
    cmp word ptr [bp+S_PLANES],4
    je PatternRows
    mov word ptr [bp+O_BUF+48],4
PatternRows:
    mov word ptr [bp+S_ROP],0
    mov word ptr [bp+S_X],0
    mov word ptr [bp+O_ROW],0
PatternRow:
    mov ax,[bp+O_ROW]
    mov [bp+S_Y],ax
    call PreparePoint
    jnc @F
    jmp PatternBad
@@:
    mov bx,[bp+S_ADDR]
    mov es,[bp+S_ADDR+2]
    mov al,es:[bx]
    mov di,[bp+O_ROW]
    cmp word ptr [bp+S_PLANES],1
    jne PatternColour
    mov byte ptr [bp+O_BUF+32+di],al
    jmp short PatternNext
PatternColour:
    mov byte ptr [bp+O_BUF+di],al
    mov bx,[bp+S_ADDR+4]
    mov es,[bp+S_ADDR+6]
    mov al,es:[bx]
    mov byte ptr [bp+O_BUF+8+di],al
    mov bx,[bp+S_ADDR+8]
    mov es,[bp+S_ADDR+10]
    mov al,es:[bx]
    mov byte ptr [bp+O_BUF+16+di],al
    mov bx,[bp+S_ADDR+12]
    mov es,[bp+S_ADDR+14]
    mov al,es:[bx]
    mov byte ptr [bp+O_BUF+24+di],al
    ; Canonical mono classification depends on the complete index. It is
    ; intentionally separate from BitBlt's background-colour match rule.
    mov word ptr [bp+O_BIT],80h
PatternMonoPixel:
    xor ax,ax
    xor si,si
    mov dl,1
    mov dh,byte ptr [bp+O_BIT]
PatternIndexPlane:
    push di
    add di,si
    test byte ptr [bp+O_BUF+di],dh
    pop di
    jz PatternIndexNext
    or al,dl
PatternIndexNext:
    shl dl,1
    add si,8
    cmp si,32
    jb PatternIndexPlane
    call IndexToPhysical
    test dh,10h
    jz PatternMonoNext
    mov al,byte ptr [bp+O_BIT]
    or byte ptr [bp+O_BUF+32+di],al
PatternMonoNext:
    shr word ptr [bp+O_BIT],1
    jnz PatternMonoPixel
PatternNext:
    inc word ptr [bp+O_ROW]
    cmp word ptr [bp+O_ROW],8
    jae @F
    jmp PatternRow
@@:
    jmp short ObjectCommit
PatternBad:
    jmp short ObjectBad
ObjectCommit:
    mov es,[bp+12]
    mov di,[bp+10]
    lea si,[bp+O_BUF]
    mov cx,[bp+O_SIZE]
    ; Both physical object sizes are even; DS is restored by the public exit.
    push ss
    pop ds
    shr cx,1
    cld
    rep movsw
ObjectOK:
    mov ax,1
    jmp short ObjectReturn
ObjectBad:
    xor ax,ax
ObjectReturn:
    xor dx,dx
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf 18
RealizeObject endp

; AX numerator component/delta <= denominator BX (63,64,128,255).
; Rounded 64-position coverage. Clobbers CX/DX; preserves BX and DI.
BrushLevel proc near
    shl ax,6
    mov cx,bx
    shr cx,1
    add ax,cx
    xor dx,dx
    div bx
    ret
BrushLevel endp
