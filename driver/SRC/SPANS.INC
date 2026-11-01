; Phase 4 packed scanlines, using the phase 3 Output invocation frame.
; Immutable per-row pattern/ROP masks; at most one destination byte at a time.
B_PAT equ -220                   ; four plane pattern bytes
B_COVER equ -222                 ; row coverage byte (independent of colour)
B_AND equ -226                   ; four ROP AND bytes, coverage already applied
B_XOR equ -230                   ; four ROP XOR bytes, coverage already applied
B_FIRST equ -232                 ; first byte edge mask
B_LAST equ -234                  ; last byte edge mask
B_BYTES equ -236                 ; bytes in clipped interval
B_RAW equ -238                   ; temporary aligned monochrome pattern
public CompileSpanRow, PackedSpan, SpanEdgeByte

; Convert a row of the selected object to device-aligned plane bytes.
; Then compile f(P,D) to (D AND A) XOR X. No destination access here.
CompileSpanRow proc near
    mov byte ptr [bp+B_COVER],0ffh
    cmp word ptr [bp+T_USEBR],0
    je @F
    jmp SpanBrushRow
@@:
    mov dl,byte ptr [bp+T_PEN+3]
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp SpanPenIndex
@@:
    mov cl,4
    shr dl,cl
    and dl,1
SpanPenIndex:
    xor di,di
SpanPenPlane:
    mov cx,di
    mov al,dl
    shr al,cl
    and al,1
    neg al
    mov byte ptr [bp+B_PAT+di],al
    inc di
    cmp di,[bp+S_PLANES]
    jae @F
    jmp SpanPenPlane
@@:
    jmp SpanCoefficients
SpanBrushRow:
    mov ax,[bp+T_Y]
    sub al,byte ptr [bp+T_BRUSH+51]
    and ax,7
    mov si,ax
    mov cl,byte ptr [bp+T_BRUSH+50]
    and cl,7
    cmp word ptr [bp+T_BRUSH+48],2
    je @F
    jmp SpanBrushPixels
@@:
    cmp word ptr [bp+T_DM+2],1
    je @F
    jmp SpanBrushPixels
@@:
    mov al,byte ptr [bp+T_BRUSH+40+si]
    ror al,cl
    mov byte ptr [bp+B_COVER],al
SpanBrushPixels:
    cmp word ptr [bp+T_BRUSH+48],4
    jne @F
    jmp SpanMonoPattern
@@:
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp SpanColourPattern
@@:
    mov al,byte ptr [bp+T_BRUSH+32+si]
    ror al,cl
    mov byte ptr [bp+B_PAT],al
    jmp SpanCoefficients
SpanColourPattern:
    xor di,di
SpanColourRow:
    mov al,byte ptr [bp+T_BRUSH+si]
    ror al,cl
    mov byte ptr [bp+B_PAT+di],al
    add si,8
    inc di
    cmp di,4
    jae @F
    jmp SpanColourRow
@@:
    jmp SpanCoefficients
SpanMonoPattern:
    mov al,byte ptr [bp+T_BRUSH+32+si]
    ror al,cl
    mov byte ptr [bp+B_RAW],al
    mov dl,byte ptr [bp+T_DM+11]  ; raw zero -> current foreground
    mov dh,byte ptr [bp+T_DM+7]   ; raw one -> current background
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp SpanMonoColours
@@:
    mov cl,4
    shr dl,cl
    shr dh,cl
SpanMonoColours:
    xor di,di
SpanMonoPlane:
    mov cx,di
    mov al,dl
    mov ah,dh
    shr al,cl
    shr ah,cl
    and al,1
    and ah,1
    neg al
    neg ah
    mov bl,byte ptr [bp+B_RAW]
    and ah,bl
    not bl
    and al,bl
    or al,ah
    mov byte ptr [bp+B_PAT+di],al
    inc di
    cmp di,[bp+S_PLANES]
    jae @F
    jmp SpanMonoPlane
@@:
SpanCoefficients:
    mov bx,[bp+S_ROP]
    dec bx
    xor di,di
SpanCoefficientPlane:
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    not dh
    xor ax,ax
    test bx,1                    ; f(P=0,D=0)
    jnz @F
    jmp SpanD0P1
@@:
    or al,dh
SpanD0P1:
    test bx,4                    ; f(P=1,D=0)
    jnz @F
    jmp SpanD1P0
@@:
    or al,dl
SpanD1P0:
    test bx,2                    ; f(P=0,D=1)
    jnz @F
    jmp SpanD1P1
@@:
    or ah,dh
SpanD1P1:
    test bx,8                    ; f(P=1,D=1)
    jnz @F
    jmp SpanCoverage
@@:
    or ah,dl
SpanCoverage:
    xor ah,al                    ; A = f(P,0) XOR f(P,1), X = f(P,0)
    not ah
    and ah,byte ptr [bp+B_COVER]
    not ah                       ; preserve every uncovered bit
    and al,byte ptr [bp+B_COVER]
    mov byte ptr [bp+B_AND+di],ah
    mov byte ptr [bp+B_XOR+di],al
    inc di
    cmp di,[bp+S_PLANES]
    jae @F
    jmp SpanCoefficientPlane
@@:
    ret
CompileSpanRow endp

; Clipped [T_X,T_END) at T_Y. All pointers checked before the first write.
; The outer Output completes pass zero for every interval before pass one.
PackedSpan proc near
    mov ax,[bp+T_X]
    mov [bp+S_X],ax
    mov bx,ax
    and ax,7
    mov cx,ax
    mov al,0ffh
    shr al,cl
    mov byte ptr [bp+B_FIRST],al
    mov ax,[bp+T_END]
    dec ax
    mov dx,ax
    and ax,7
    mov cx,7
    sub cx,ax
    mov al,0ffh
    shl al,cl
    mov byte ptr [bp+B_LAST],al
    mov cl,3
    shr bx,cl
    shr dx,cl
    sub dx,bx
    inc dx
    mov [bp+B_BYTES],dx
    mov al,byte ptr [bp+B_COVER]
    or al,al
    jnz @F
    jmp PackedDone
@@:
    cmp dx,1
    je @F
    jmp PackedMultipleCoverage
@@:
    and al,byte ptr [bp+B_FIRST]
    and al,byte ptr [bp+B_LAST]
    jnz @F
    jmp PackedDone
@@:
    jmp PackedPrepare
PackedMultipleCoverage:
    cmp dx,2
    jbe @F
    jmp PackedPrepare
@@:
    mov al,byte ptr [bp+B_FIRST]
    or al,byte ptr [bp+B_LAST]
    test al,byte ptr [bp+B_COVER]
    jnz @F
    jmp PackedDone
@@:
PackedPrepare:
    mov ax,[bp+T_Y]
    mov [bp+S_Y],ax
    cmp word ptr [bp+S_KIND],0
    je PackedMemoryPrepare
    mov cx,[bp+B_BYTES]
    call PrepareScreenSpan
    jnc PackedValidated
    jmp PackedBad
PackedMemoryPrepare:
    call PreparePoint
    jnc @F
    jmp PackedBad
@@:
    xor si,si
    mov di,[bp+S_PLANES]
PackedValidatePlane:
    mov ax,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov cx,[bp+B_BYTES]
    call CheckSpan
    jnc @F
    jmp PackedBad
@@:
    add si,4
    dec di
    jz @F
    jmp PackedValidatePlane
@@:
PackedValidated:
    cmp word ptr [bp+T_PASS],0
    jne @F
    jmp PackedDone
@@:
    cmp word ptr [bp+S_ROP],11
    jne @F
    jmp PackedDone
@@:
    xor si,si
    xor di,di
PackedPlane:
    mov es,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov dh,byte ptr [bp+B_AND+di]
    mov dl,byte ptr [bp+B_XOR+di]
    mov cx,[bp+B_BYTES]
    mov al,byte ptr [bp+B_FIRST]
    cmp cx,1
    je @F
    jmp PackedManyBytes
@@:
    and al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    jmp PackedNextPlane
PackedManyBytes:
    call SpanEdgeByte
    inc bx
    sub cx,2
    jcxz PackedLastByte
    or dh,dh
    jnz @F
    jmp PackedConstant
@@:
PackedInnerByte:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    loop PackedInnerByte
    jmp PackedLastByte
PackedConstant:
    ; A=0 implies full coverage and destination-independent output.
    ; Aligned interior bytes therefore need no destination read.
    mov al,dl
    xchg bx,di
    cld
    rep stosb
    xchg bx,di
PackedLastByte:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
PackedNextPlane:
    add si,4
    inc di
    cmp di,[bp+S_PLANES]
    jae @F
    jmp PackedPlane
@@:
PackedDone:
    clc
    ret
PackedBad:
    stc
    ret
PackedSpan endp

; ES:BX byte, DH AND coefficient, DL XOR coefficient, AL edge mask.
; Preserve BX/CX/DX/SI/DI. Do not touch a byte with no covered pixel.
SpanEdgeByte proc near
    test al,byte ptr [bp+B_COVER]
    jnz @F
    jmp SpanEdgeDone
@@:
    mov ah,al
    not ah
    or ah,dh
    and al,dl
    or ah,ah
    jz SpanEdgeConstant
    and ah,es:[bx]
    xor al,ah
SpanEdgeConstant:
    mov es:[bx],al
SpanEdgeDone:
    ret
SpanEdgeByte endp
