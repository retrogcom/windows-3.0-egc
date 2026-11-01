; Static drawing adapter/kernel, introduced in phase 2 and shared in phase 3. All scratch belongs to this invocation.
; BP points to the standard Windows frame; DS is driver data, SS is caller.
; 112 local bytes; reserve 320 for locals, nested helpers and IRQ headroom.
; The external Windows/BIOS worst-case stack budget remains an integration gate.
PX_LOCAL equ 112
PX_RESERVE equ 320
S_DESC equ -44
S_KIND equ -46
S_PLANES equ -48
S_STRIDE equ -50
S_SELECTOR equ -52
S_OFFSET equ -54
S_ROW equ -56
S_X equ -58
S_Y equ -60
S_MASK equ -62
S_ROP equ -64
S_PEN equ -66
S_RESULT equ -68
S_ADDR equ -84
S_OLD equ -90
S_HELD equ -92
; Half-open intervals: descriptor [-44,-8), pointers [-84,-68), old [-90,-86).
; Saved DS/SI/DI occupy [-6,0). Keep the fourth pointer below S_RESULT.
.errnz (S_ADDR+16)-S_RESULT
.errnz (S_DESC+36)+8
.errnz (S_OLD+4)-(S_ADDR-2)

public ColorInfo, Pixel, ClonePhysical, CheckReadSpan
public LogicalToPhysical, IndexToPhysical, EnterDrawing, LeaveDrawing
public PreparePixel, PreparePoint, PixelBits, StackRoom

; AX selector, BX offset, CX positive count. AX/BX/CX preserved.
; Readable code is permitted for immutable caller data; expand-down is not.
CheckReadSpan proc near
    push dx
    push si
    or ax,ax
    jz ReadSpanBad
    jcxz ReadSpanBad
    mov si,cx
    dec si
    add si,bx
    jc ReadSpanBad
    cmp ProtectedMode,0
    jne ReadSpanProtected
    cmp ax,0f000h
    jbe ReadSpanGood
    mov dx,0ffffh
    sub dx,ax
    shl dx,1
    shl dx,1
    shl dx,1
    shl dx,1
    or dx,15
    cmp si,dx
    ja ReadSpanBad
    jmp short ReadSpanGood
ReadSpanProtected:
    verr ax
    jnz ReadSpanBad
    lar dx,ax
    jnz ReadSpanBad
    test dx,8000h
    jz ReadSpanBad
    test dx,1000h
    jz ReadSpanBad
    test dx,0800h
    jnz ReadSpanLimit
    test dx,0400h
    jnz ReadSpanBad
ReadSpanLimit:
    lsl dx,ax
    jnz ReadSpanBad
    cmp si,dx
    ja ReadSpanBad
ReadSpanGood:
    pop si
    pop dx
    clc
    ret
ReadSpanBad:
    pop si
    pop dx
    stc
    ret
CheckReadSpan endp

; AX required future space; do not allocate it. Metadata follows DDK CHKSTK.
; Called after the standard six-byte saved-register frame, before local SUB.
StackRoom proc near
    mov dx,sp
    add dx,2                      ; exclude this helper's near return
    sub dx,ax
    jbe StackBad
    cmp dx,ss:[000ah]
    jb StackBad
    cmp dx,ss:[000ch]
    jae StackGood
    mov ss:[000ch],dx
StackGood:
    clc
    ret
StackBad:
    stc
    ret
StackRoom endp

; Internal PDEVICE publication helper; only used after a complete Activate.
; AX destination selector, BX offset -> AX=1/0 and CF. SI/DI preserved.
ClonePhysical proc near
    push si
    push di
    cmp DriverState,ACTIVE
    jne CloneBad
    cmp ScreenGate,1
    jne CloneBad
    mov cx,36
    call CheckSpan
    jc CloneBad
    mov es,ax
    mov di,bx
    mov si,DataOFFSET PhysicalDevice
    mov cx,18
    cld
    rep movsw
    pop di
    pop si
    mov ax,1
    clc
    ret
CloneBad:
    pop di
    pop si
    xor ax,ax
    stc
    ret
ClonePhysical endp

 ; Interrupt-atomic ownership. Legacy entry excludes the whole screen.
; Rectangle entry takes clipped half-open AX/BX/CX/DX = L/T/R/B. Compare
; with complete saved BYTES, not just cursor pixels: restoring an edge byte
; must never erase a neighbouring pixel changed while the pointer is visible.
public EnterDrawingRect, CursorRectUnion
EnterDrawing proc near
    push si
    xor si,si
    jmp DrawingClaim
EnterDrawing endp
EnterDrawingRect proc near
    push si
    mov si,1
DrawingClaim:
    pushf
    cli
    cmp DriverState,ACTIVE
    jne DrawingBusy
    cmp ScreenGate,1
    jne DrawingBusy
    cmp BridgeBusy,0
    jne DrawingBusy
    cmp CurBusy,0
    jne DrawingBusy
    cmp DrawDepth,0ffffh
    je DrawingBusy
    inc DrawDepth
    cmp CurDrawn,0
    je DrawingClaimed
    or si,si
    jz DrawingRemove
    cmp ax,cx
    jae DrawingClaimed
    cmp bx,dx
    jae DrawingClaimed
    cmp dx,CurSaveY
    jbe DrawingClaimed
    mov si,CurSaveY
    add si,CurSaveH
    cmp bx,si
    jae DrawingClaimed
    mov si,CurSaveX
    shl si,3
    cmp cx,si
    jbe DrawingClaimed
    mov si,CurSaveX
    add si,CurSaveW
    shl si,3
    cmp ax,si
    jae DrawingClaimed
DrawingRemove:
    ; Nested full/overlapping calls must also exclude a cursor that an outer
    ; disjoint rectangle intentionally left visible.
    mov CurBusy,1
    popf
    call CursorRemove
    mov CurPending,1
    mov CurBusy,0
    pop si
    clc
    ret
DrawingClaimed:
    popf
    pop si
    clc
    ret
DrawingBusy:
    popf
    pop si
    stc
    ret
EnterDrawingRect endp

; Union nonempty clipped AX/BX/CX/DX into SS:SI {L,T,R,B}.
; All registers preserved. Initialise an empty screen box to {640,400,0,0}.
CursorRectUnion proc near
    cmp ax,cx
    jae CRUnionDone
    cmp bx,dx
    jae CRUnionDone
    cmp ax,ss:[si]
    jae CRUnionTop
    mov ss:[si],ax
CRUnionTop:
    cmp bx,ss:[si+2]
    jae CRUnionRight
    mov ss:[si+2],bx
CRUnionRight:
    cmp cx,ss:[si+4]
    jbe CRUnionBottom
    mov ss:[si+4],cx
CRUnionBottom:
    cmp dx,ss:[si+6]
    jbe CRUnionDone
    mov ss:[si+6],dx
CRUnionDone:
    ret
CursorRectUnion endp

LeaveDrawing proc near
    pushf
    cli
    cmp DrawDepth,0
    je DrawingUnderflow
    dec DrawDepth
    jnz DrawingStillHeld
    popf
    ; USER's CheckCursor services the pending repaint after drawing. Keeping
    ; the pointer excluded across a burst avoids restore/repaint per element.
    ; MoveCursor/SetCursor remain bounded services; nesting still blocks them.
    clc
    ret
DrawingStillHeld:
    popf
    clc
    ret
DrawingUnderflow:
    mov ScreenGate,0
    mov DriverState,FAULTED
    mov LastError,ERR_BUSY
    mov CurDrawn,0
    mov CurPending,1
    popf
    stc
    ret
LeaveDrawing endp

; Plain logical RGB in AX/DL -> nearest fixed-palette RGB + DH metadata.
; Full 32-bit squared distance (maximum 195075); stable lower-index tie.
; Mono classification uses ORIGINAL input RGB sum, independently of nearest.
; No shared scratch, allocations, device access, division or 386 instructions.
LogicalToPhysical proc near
    push si
    push cx
    mov si,DataOFFSET CanonicalColours
    mov cx,16
QuantExact:
    cmp ax,[si]
    jne QuantExactNext
    cmp dl,[si+2]
    je QuantExactFound
QuantExactNext:
    add si,4
    loop QuantExact
    pop cx
    pop si
    jmp QuantGeneral
QuantExactFound:
    mov dx,[si+2]             ; canonical RGB and its exact mono/index flags
    pop cx
    pop si
    ret
QuantGeneral:
    ; Exact palette decomposition: nearest point in the 0/128 cube, the
    ; independent grey 192, and the 0/255 cube. Compare squared distances
    ; in palette-index order; equal scores retain the lower index.
    push bp
    mov bp,sp
    push bx
    push cx
    push si
    push di
    sub sp,16
    xor bx,bx
    mov bl,al
    mov [bp-10],bx
    mov bl,ah
    mov [bp-12],bx
    mov bl,dl
    mov [bp-14],bx
    add bx,[bp-10]
    add bx,[bp-12]
    mov word ptr [bp-16],0
    cmp bx,382
    jb Q16MonoReady
    mov word ptr [bp-16],10h
Q16MonoReady:
    xor bx,bx
    xor di,di
    mov ax,[bp-10]
    cmp ax,64
    jbe Q16Dark0
    sub ax,128
    or di,1
Q16Dark0:
    imul ax
    add bx,ax
    mov ax,[bp-12]
    cmp ax,64
    jbe Q16Dark1
    sub ax,128
    or di,2
Q16Dark1:
    imul ax
    add bx,ax
    mov ax,[bp-14]
    cmp ax,64
    jbe Q16Dark2
    sub ax,128
    or di,4
Q16Dark2:
    imul ax
    add bx,ax
    mov [bp-18],bx
    mov [bp-22],di
    xor bx,bx
    xor cx,cx
    mov ax,[bp-10]
    sub ax,192
    imul ax
    add bx,ax
    adc cx,dx
    mov ax,[bp-12]
    sub ax,192
    imul ax
    add bx,ax
    adc cx,dx
    mov ax,[bp-14]
    sub ax,192
    imul ax
    add bx,ax
    adc cx,dx
    or cx,cx
    jnz Q16Bright
    cmp bx,[bp-18]
    jae Q16Bright
    mov [bp-18],bx
    mov word ptr [bp-22],8
Q16Bright:
    xor bx,bx
    xor di,di
    mov ax,[bp-10]
    cmp ax,128
    jb Q16Light0
    sub ax,255
    or di,1
Q16Light0:
    imul ax
    add bx,ax
    mov ax,[bp-12]
    cmp ax,128
    jb Q16Light1
    sub ax,255
    or di,2
Q16Light1:
    imul ax
    add bx,ax
    mov ax,[bp-14]
    cmp ax,128
    jb Q16Light2
    sub ax,255
    or di,4
Q16Light2:
    imul ax
    add bx,ax
    cmp bx,[bp-18]
    jae Q16Result
    or di,di
    jz Q16Store
    or di,8
Q16Store:
    mov [bp-22],di
Q16Result:
    mov ax,[bp-22]
    call IndexToPhysical
    and dh,2fh
    or dh,byte ptr [bp-16]
    lea sp,[bp-8]
    pop di
    pop si
    pop cx
    pop bx
    pop bp
    ret
LogicalToPhysical endp

; AL index 0..15 -> canonical physical DWORD, including canonical mono flag.
IndexToPhysical proc near
    push bx
    xor bx,bx
    mov bl,al
    and bl,15
    shl bx,1
    shl bx,1
    mov ax,word ptr CanonicalColours[bx]
    mov dx,word ptr CanonicalColours[bx+2]
    pop bx
    ret
IndexToPhysical endp

; AX:BX caller descriptor. A BITMAP remains 32 bytes; only private PDEVICE
; grows to 36. Read type safely first, then validate the complete selected span.
; This allows a 32-byte BITMAP ending exactly at a selector limit.
SnapshotSurface proc near
    mov cx,2
    call CheckReadSpan
    jnc @F
    jmp SnapshotBad
@@:
    mov es,ax
; Internal entry: caller has proved the two-byte tag and loaded ES=AX.
; Keep the original full extent proof and snapshot for every memory surface.
SnapshotTyped label near
    mov cx,32
    cmp word ptr es:[bx],4745h
    jne SnapshotLength
    mov cx,36
SnapshotLength:
    call CheckReadSpan
    jnc @F
    jmp SnapshotBad
@@:
    mov si,bx
    lea di,[bp+S_DESC]
    mov word ptr [bp+S_DESC+32],0
    mov word ptr [bp+S_DESC+34],0
    ; 0.15: copy the complete proved span with forward string operations.
    ; Restore driver DS and caller-descriptor ES; keep SI/DI advanced, CX=0.
    push ds
    push es
    push es
    pop ds
    push ss
    pop es
    shr cx,1
    cld
    rep movsw
    pop es
    pop ds
    clc
    ret
SnapshotBad:
    stc
    ret
SnapshotSurface endp

; Validate the declared format independently of dimensions/storage. ColorInfo
; needs a format, not a bitmap allocation or an active screen.
ClassifyFormat proc near
    mov word ptr [bp+S_KIND],0
    mov ax,[bp+S_DESC]
    or ax,ax
    jnz @F
    jmp MemoryFormat
@@:
    cmp ax,4745h
    je @F
    jmp FormatBad
@@:
    ; All private fields, including owner/generation/plane pointers, must match.
    mov si,DataOFFSET PhysicalDevice
    lea di,[bp+S_DESC]
    ; Compare all 36 live identity bytes, including owner and generation.
    push es
    push ss
    pop es
    mov cx,18
    cld
    repe cmpsw
    pop es
    jne FormatBad
    mov word ptr [bp+S_KIND],1
MemoryFormat:
    mov ax,[bp+S_DESC+8]
    cmp ax,0101h
    je FormatOK
    cmp ax,0104h
    je @F
    jmp FormatBad
@@:
FormatOK:
    xor ah,ah
    mov [bp+S_PLANES],ax
    clc
    ret
FormatBad:
    stc
    ret
ClassifyFormat endp

; ColorInfo consumes only the format. Validate the same complete input
; extent and screen identity without a redundant stack snapshot. Borrowed
; caller metadata must remain stable for this synchronous call, as elsewhere.
; AX:BX descriptor -> S_PLANES, CF. Driver DS remains unchanged.
public ColourQueryFormat
ColourQueryFormat proc near
    mov cx,2
    call CheckReadSpan
    jc CQFormatBad
    mov es,ax
    mov cx,32
    cmp word ptr es:[bx],4745h
    jne CQFormatLength
    mov cx,36
CQFormatLength:
    call CheckReadSpan
    jc CQFormatBad
    mov ax,es:[bx]
    or ax,ax
    jz CQMemoryFormat
    cmp ax,4745h
    jne CQFormatBad
    mov si,DataOFFSET PhysicalDevice
    mov di,bx
    mov cx,18
    cld
    repe cmpsw
    jne CQFormatBad
CQMemoryFormat:
    mov ax,es:[bx+8]
    cmp ax,0101h
    je CQFormatGood
    cmp ax,0104h
    jne CQFormatBad
CQFormatGood:
    xor ah,ah
    mov [bp+S_PLANES],ax
    clc
    ret
CQFormatBad:
    stc
    ret
ColourQueryFormat endp

; Endpoint shortcut belongs to public colour queries only. Shared brush,
; text and DIB quantization retains its original cost and exact algorithm.
public ColourQueryRGB
ColourQueryRGB proc near
    ; Desk experiment: exact endpoints used by USER; no descriptor bypass.
    or ax,ax
    jz CQTryBlack
    cmp ax,0ffffh
    jne CQScan
    cmp dl,0ffh
    jne CQScan
    mov dh,3fh                   ; white, mono-white and pure flags
    ret
CQTryBlack:
    or dl,dl
    jnz CQScan
    mov dh,20h                  ; black and pure flag
    ret
CQScan:
    jmp LogicalToPhysical
ColourQueryRGB endp

; Standard Windows ABI: ColorInfo(device, colour, optional physical output).
; No defined GDI retry on error: 8000:0000 is diagnostic only.
ColorInfo proc far
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
    mov ax,PX_RESERVE
    call StackRoom
    jnc @F
    jmp ColourError
@@:
    sub sp,PX_LOCAL
    mov bx,[bp+14]
    mov ax,[bp+16]
    call ColourQueryFormat
    jnc @F
    jmp ColourError
@@:
    mov ax,[bp+8]
    mov bx,[bp+6]
    or ax,bx
    jz ColourReverse
    mov ax,[bp+8]
    mov cx,4
    call CheckSpan
    jnc @F
    jmp ColourError
@@:
    mov es,ax
    mov di,bx
    mov ax,[bp+10]
    mov dx,[bp+12]
    or dh,dh                     ; plain RGB only; no palette flags advertised
    jz @F
    jmp ColourError
@@:
    call ColourQueryRGB
    mov es:[di],ax
    mov es:[di+2],dx
    jmp short ColourReturnRGB
ColourReverse:
    mov ax,[bp+10]
    mov dx,[bp+12]
ColourReturnRGB:
    cmp word ptr [bp+S_PLANES],1
    jne ColourRGB
    test dh,10h
    jz ColourBlack
    mov ax,0ffffh
    mov dx,00ffh
    jmp short ColourDone
ColourBlack:
    xor ax,ax
    xor dx,dx
    jmp short ColourDone
ColourRGB:
    xor dh,dh
    jmp short ColourDone
ColourError:
    xor ax,ax
    mov dx,8000h
ColourDone:
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf 12
ColorInfo endp

; Snapshot + preflight. All needed addresses are checked before the first bit
; is touched. The descriptor, bitmap bits and draw mode are always borrowed.
PreparePixel proc near
    mov ax,[bp+16]
    mov [bp+S_X],ax
    mov ax,[bp+14]
    mov [bp+S_Y],ax
    mov bx,[bp+18]
    mov ax,[bp+20]
    call SnapshotSurface
    jnc @F
    jmp PrepareBad
@@:
    call ClassifyFormat
    jnc @F
    jmp PrepareBad
@@:
PreparePoint label near
    cmp word ptr [bp+S_KIND],0
    je PrepareMemoryPoint
    mov cx,1
    jmp PrepareScreenSpan
PrepareMemoryPoint:
    mov ax,[bp+S_DESC+2]
    or ax,ax
    jg @F
    jmp PrepareBad
@@:
    mov dx,[bp+S_X]
    cmp dx,ax
    jb @F
    jmp PrepareBad
@@:
    mov [bp+S_X],dx
    mov ax,[bp+S_DESC+4]
    or ax,ax
    jg @F
    jmp PrepareBad
@@:
    mov dx,[bp+S_Y]
    cmp dx,ax
    jb @F
    jmp PrepareBad
@@:
    mov [bp+S_Y],dx
    mov cx,[bp+S_X]
    and cx,7
    mov ax,80h
    shr ax,cl
    mov [bp+S_MASK],ax
PrepareMemory:
    mov ax,[bp+S_DESC+6]
    or ax,ax
    jnz @F
    jmp PrepareBad
@@:
    test ax,1
    jz @F
    jmp PrepareBad
@@:
    mov [bp+S_STRIDE],ax
    mov bx,[bp+S_DESC+2]
    add bx,7
    mov cl,3
    shr bx,cl
    cmp ax,bx
    jae @F
    jmp PrepareBad
@@:
    mul word ptr [bp+S_PLANES]
    or dx,dx
    jz @F
    jmp PrepareBad
@@:
    mov [bp+S_ROW],ax           ; complete interleaved row including padding
    mov bx,[bp+S_DESC+10]
    test bx,1
    jz @F
    jmp PrepareBad
@@:
    mov [bp+S_OFFSET],bx
    mov ax,[bp+S_DESC+12]
    mov [bp+S_SELECTOR],ax
    mov ax,[bp+S_Y]
    cmp word ptr [bp+S_DESC+22],0
    je MemoryRow
    mov cx,[bp+S_DESC+24]
    or cx,cx
    jnz @F
    jmp PrepareBad
@@:
    xor dx,dx
    div cx                      ; AX segment index, DX row within segment
    push dx
    mul word ptr [bp+S_DESC+22]
    or dx,dx
    pop dx
    jz @F
    jmp PrepareBad
@@:
    add [bp+S_SELECTOR],ax
    jnc @F
    jmp PrepareBad
@@:
    mov ax,dx
MemoryRow:
    mul word ptr [bp+S_ROW]
    or dx,dx
    jz @F
    jmp PrepareBad
@@:
    add [bp+S_OFFSET],ax
    jnc @F
    jmp PrepareBad
@@:
    mov ax,[bp+S_SELECTOR]
    mov bx,[bp+S_OFFSET]
    mov cx,[bp+S_ROW]
    call CheckPixelSpan
    jnc @F
    jmp PrepareBad
@@:
    mov bx,[bp+S_X]
    mov cl,3
    shr bx,cl
    add bx,[bp+S_OFFSET]
    xor si,si
    mov cx,[bp+S_PLANES]
MemoryPointer:
    mov [bp+S_ADDR+si],bx
    mov ax,[bp+S_SELECTOR]
    mov [bp+S_ADDR+si+2],ax
    add bx,[bp+S_STRIDE]
    add si,4
    loop MemoryPointer
    clc
    ret
PrepareBad:
    stc
    ret
PreparePixel endp

CheckPixelSpan proc near
    cmp word ptr [bp+S_ROP],0
    je ReadPixelSpan
    jmp CheckSpan
ReadPixelSpan:
    jmp CheckReadSpan
CheckPixelSpan endp

; Pixel(device, x, y, physical_colour, drawmode-or-NULL).
; NULL draw mode reads physical RGB/metadata, never logical RGB.
Pixel proc far
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
    mov ax,PX_RESERVE
    call StackRoom
    jnc @F
    jmp PixelEarlyError
@@:
    sub sp,PX_LOCAL
    mov word ptr [bp+S_HELD],0
    mov word ptr [bp+S_ROP],0
    mov ax,[bp+8]
    mov bx,[bp+6]
    or ax,bx
    jz PixelPreparedMode
    mov ax,[bp+8]
    mov cx,2                    ; only Rop2 is used; background is ignored
    call CheckReadSpan
    jnc @F
    jmp PixelError
@@:
    mov es,ax
    mov ax,es:[bx]
    cmp ax,1
    jae @F
    jmp PixelError
@@:
    cmp ax,16
    jbe @F
    jmp PixelError
@@:
    mov [bp+S_ROP],ax
PixelPreparedMode:
    call PreparePixel
    jnc @F
    jmp PixelError
@@:
    mov al,byte ptr [bp+13]      ; physical index and mono flag from metadata
    cmp word ptr [bp+S_PLANES],1
    jne PixelPenColour
    mov cl,4
    shr al,cl
    and al,1
    jmp short PixelPenReady
PixelPenColour:
    and al,15
PixelPenReady:
    mov byte ptr [bp+S_PEN],al
    cmp word ptr [bp+S_KIND],0
    je PixelHasAccess
    mov ax,[bp+S_X]
    mov bx,[bp+S_Y]
    mov cx,ax
    mov dx,bx
    inc cx
    inc dx
    call EnterDrawingRect
    jnc @F
    jmp PixelError
@@:
    mov word ptr [bp+S_HELD],1
PixelHasAccess:
    call PixelBits
    jmp short PixelRelease
PixelError:
    xor ax,ax
    mov dx,8000h
PixelRelease:
    cmp word ptr [bp+S_HELD],0
    je PixelReturn
    call LeaveDrawing           ; preserves AX/DX result
    jmp short PixelReturn
PixelEarlyError:
    xor ax,ax
    mov dx,8000h
PixelReturn:
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf 16
Pixel endp

; Shared checked one-pixel kernel. Caller owns screen exclusion and addresses.
PixelBits proc near
    ; A solid copy does not need to reconstruct the old four-bit colour or
    ; evaluate a truth table per plane. Addresses and ownership are already
    ; proven by the caller; retain the same one read/write per plane.
    cmp word ptr [bp+S_ROP],13
    jne PixelGeneralROP
    xor si,si
    mov cx,[bp+S_PLANES]
    mov dl,byte ptr [bp+S_MASK]
    mov dh,dl
    not dh
    mov ah,byte ptr [bp+S_PEN]
PixelCopyPlane:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov al,es:[bx]
    and al,dh
    shr ah,1
    jnc PixelCopyZero
    or al,dl
PixelCopyZero:
    mov es:[bx],al
    add si,4
    loop PixelCopyPlane
    mov ax,1
    xor dx,dx
    ret
PixelGeneralROP:
    ; R2_NOP is valid and needs neither destination reads nor writes.
    cmp word ptr [bp+S_ROP],11
    jne @F
    jmp PixelSetOK
@@:
    xor si,si
    xor di,di
    mov cx,[bp+S_PLANES]
    mov byte ptr [bp+S_RESULT],0
PixelReadByte:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov al,es:[bx]
    mov byte ptr [bp+S_OLD+di],al
    test al,byte ptr [bp+S_MASK]
    jz PixelBitZero
    push cx
    mov cx,di
    mov al,1
    shl al,cl
    or byte ptr [bp+S_RESULT],al
    pop cx
PixelBitZero:
    add si,4
    inc di
    loop PixelReadByte
    cmp word ptr [bp+S_ROP],0
    jne PixelWrite
    mov al,byte ptr [bp+S_RESULT]
    cmp word ptr [bp+S_PLANES],1
    jne PixelGetColour
    or al,al
    jz PixelGetColour
    mov al,15
PixelGetColour:
    call IndexToPhysical
    jmp PixelBitsDone
PixelWrite:
    xor si,si
    xor di,di
PixelWritePlane:
    ; Each ROP2 is a four-bit truth table, indexed by (P<<1)|D.
    mov cx,di
    mov al,byte ptr [bp+S_PEN]
    shr al,cl
    and al,1
    shl al,1
    mov ah,byte ptr [bp+S_RESULT]
    shr ah,cl
    and ah,1
    or al,ah
    mov cl,al
    mov ax,[bp+S_ROP]
    dec ax
    shr ax,cl
    and al,1
    mov dl,byte ptr [bp+S_MASK]
    mov ah,byte ptr [bp+S_OLD+di]
    mov dh,dl
    not dh
    and ah,dh
    or al,al
    jz PixelWriteZero
    or ah,dl
PixelWriteZero:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov es:[bx],ah
    add si,4
    inc di
    cmp di,[bp+S_PLANES]
    jb PixelWritePlane
PixelSetOK:
    mov ax,1
    xor dx,dx
PixelBitsDone:
    ret
PixelBits endp
