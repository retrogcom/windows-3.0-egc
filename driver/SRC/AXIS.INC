; Solid axis-aligned polyline segments after the existing major-axis clip.
; T_LO/T_HI preserve the original excluded endpoint, including reverse lines.
; Output still preflights the ENTIRE polyline before entering its write pass.
; No new locals, allocation, I/O, or ownership rules.
public AxisLine
AxisLine proc near
    cmp word ptr [bp+T_STEEP],0
    jne ALVertical
    mov ax,[bp+T_Y0]
    cmp ax,[bp+T_CLIP+2]
    jl ALHorizontalEmpty
    cmp ax,[bp+T_CLIP+6]
    jge ALHorizontalEmpty
    mov [bp+T_Y],ax
    mov ax,[bp+T_X0]
    add ax,[bp+T_LO]
    mov [bp+T_X],ax
    mov ax,[bp+T_X0]
    add ax,[bp+T_HI]
    inc ax
    mov [bp+T_END],ax
    call CompileSpanRow
    call PackedSpan
    ret
ALHorizontalEmpty:
    clc
    ret
ALVertical:
    mov ax,[bp+T_X0]
    cmp ax,[bp+T_CLIP]
    jge @F
    jmp ALDone
@@:
    cmp ax,[bp+T_CLIP+4]
    jl @F
    jmp ALDone
@@:
    mov [bp+T_X],ax
    mov ax,[bp+T_LO]
    cmp word ptr [bp+T_SIGN],0
    jg ALY
    neg ax
ALY:
    add ax,[bp+T_Y0]
    mov [bp+T_Y],ax
    cmp word ptr [bp+T_PASS],0
    jne ALCompile
    cmp word ptr [bp+T_LEFT],8
    jbe ALCompile
    cmp word ptr [bp+S_KIND],0
    jne ALFastProof
    cmp word ptr [bp+S_DESC+22],0
    jne ALCompile
ALFastProof:
    ; In each of four screen banks addresses are monotone. First and last
    ; four rows prove each complete column interval. Ordinary linear memory
    ; is monotone too; huge row/selector transitions retain the checked loop.
    push word ptr [bp+T_Y]
    call AxisProofFour
    jc ALProofDone
    mov ax,[bp+T_LEFT]
    sub ax,4
    imul word ptr [bp+T_SIGN]
    pop bx
    push bx
    add ax,bx
    mov [bp+T_Y],ax
    call AxisProofFour
ALProofDone:
    pop word ptr [bp+T_Y]
    ret
ALCompile:
    call CompileSpanRow
    cmp word ptr [bp+T_PASS],0
    je ALRow
    cmp word ptr [bp+S_KIND],0
    je ALRow
    cmp word ptr [bp+T_LEFT],8
    jb ALRow
    cmp word ptr [bp+S_ROP],11
    jne @F
    jmp ALDone
@@:
    call AxisScreenColumn
    ret
ALRow:
    mov ax,[bp+T_X]
    mov [bp+S_X],ax
    mov ax,[bp+T_Y]
    mov [bp+S_Y],ax
    call OutputMapPoint
    jc ALBad
    cmp word ptr [bp+T_PASS],0
    je ALNext
    cmp word ptr [bp+S_ROP],11
    je ALNext
    xor si,si
    xor di,di
ALPlane:
    mov es,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov dh,byte ptr [bp+B_AND+di]
    mov dl,byte ptr [bp+B_XOR+di]
    mov al,byte ptr [bp+S_MASK]
    call SpanEdgeByte
    add si,4
    inc di
    cmp di,[bp+S_PLANES]
    jb ALPlane
ALNext:
    mov ax,[bp+T_SIGN]
    add [bp+T_Y],ax
    dec word ptr [bp+T_LEFT]
    jnz ALRow
ALDone:
    clc
    ret
ALBad:
    stc
    ret
AxisLine endp

AxisProofFour proc near
    mov word ptr [bp+T_MAPX],4
APFRow:
    mov ax,[bp+T_X]
    mov [bp+S_X],ax
    mov ax,[bp+T_Y]
    mov [bp+S_Y],ax
    call PreparePoint
    jc APFDone
    mov ax,[bp+T_SIGN]
    add [bp+T_Y],ax
    dec word ptr [bp+T_MAPX]
    jnz APFRow
    clc
APFDone:
    ret
AxisProofFour endp

; The full polyline has already passed its original proof walk and owns the
; drawing rectangle. Every fourth screen row is adjacent at +/-80 bytes in
; one bank. Map each of the first four rows once, then draw one plane column.
; No descriptor, selector, clipping, endpoint or ownership checks are removed.
; T_END, B_BYTES and B_RAW are dead span scratch for this vertical segment.
AxisScreenColumn proc near
    mov word ptr [bp+T_END],4
    mov ax,80
    cmp word ptr [bp+T_SIGN],0
    jg ASCStep
    neg ax
ASCStep:
    mov [bp+B_RAW],ax
ASCBank:
    mov ax,[bp+T_LEFT]
    add ax,3
    shr ax,1
    shr ax,1
    mov [bp+B_BYTES],ax
    mov ax,[bp+T_X]
    mov [bp+S_X],ax
    mov ax,[bp+T_Y]
    mov [bp+S_Y],ax
    call OutputMapPoint
    jc ASCFailed
    xor si,si
    xor di,di
ASCPlane:
    mov es,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov al,byte ptr [bp+S_MASK]
    mov dl,byte ptr [bp+B_XOR+di]
    and dl,al
    not al
    or al,byte ptr [bp+B_AND+di]
    mov dh,al
    mov cx,[bp+B_BYTES]
    push si
    mov si,[bp+B_RAW]
ASCByte:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    add bx,si
    loop ASCByte
    pop si
    add si,4
    inc di
    cmp di,4
    jb ASCPlane
    mov ax,[bp+T_SIGN]
    add [bp+T_Y],ax
    dec word ptr [bp+T_LEFT]
    dec word ptr [bp+T_END]
    jnz ASCBank
    ; S_ADDR remains the mapped first pixel of the last bank. T_MAPX/Y agree
    ; with it; the next polyline segment may safely reuse OutputMapPoint.
    clc
    ret
ASCFailed:
    stc
    ret
AxisScreenColumn endp
