; Phase 5 internal BitBlt core. No public BitBlt entry/rectangle scheduler yet.
; Pure 286 register/byte helpers. Caller owns surface lifetime, exclusion and
; validated destination span; multi-plane source staging is a caller obligation.
public BlitDecode, BlitReduce, BlitWriteByte, BlitClipAxis
public BlitAlign, BlitMonoToColour, BlitColourToMono

; DX:AX Windows ROP -> AX truth index, BX use bits (D=1,S=2,P=4), CF error.
; The historical low word is unused; a nonzero top byte is invalid.
; Destroys CX/DX. Preserves SI/DI/BP/DS/ES.
BlitDecode proc near
    or dh,dh
    jnz BlitDecodeBad
    ; One immutable dependency byte for every eight-bit Boolean truth table.
    ; No borrowed-memory cache: the full public high-byte rejection remains.
    mov ax,dx
    mov bx,ax
    mov bl,RopUseTable[bx]
    xor bh,bh
    clc
    ret
BlitDecodeBad:
    xor ax,ax
    xor bx,bx
    stc
    ret
BlitDecode endp

; CL four-bit table, DL source byte -> AL=f(S,0), AH=f(S,1).
; Internal value expansion; BX/CX/DL/SI/DI/BP/DS/ES preserved, DH destroyed.
BlitTwoValues proc near
    mov dh,dl
    not dh
    xor ax,ax
    test cl,1
    jnz @F
    jmp BlitTwoD0S1
@@:
    or al,dh
BlitTwoD0S1:
    test cl,4
    jnz @F
    jmp BlitTwoD1S0
@@:
    or al,dl
BlitTwoD1S0:
    test cl,2
    jnz @F
    jmp BlitTwoD1S1
@@:
    or ah,dh
BlitTwoD1S1:
    test cl,8
    jnz @F
    jmp BlitTwoDone
@@:
    or ah,dl
BlitTwoDone:
    ret
BlitTwoValues endp

; AL ROP3 truth index, BL pattern byte, DL source byte ->
; DH AND coefficient, DL XOR coefficient, result=(D AND DH) XOR DL.
; AX destroyed; all registers other than AX/DX/flags preserved.
BlitReduce proc near
    ; For each fixed P, FillRopPairs gives source-dependent XOR/AND
    ; coefficients directly. Blend those two coefficient pairs by P.
    ; All 256 truth tables remain supported; no surface access or admission.
    push bx
    push cx
    push si
    mov cl,al                    ; retain the complete ROP truth byte
    mov ch,bl                    ; retain P while BX indexes the table
    mov bx,ax
    and bx,0fh
    shl bx,2
    mov al,dl
    mov ah,dl
    and ax,FillRopPairs[bx+2]
    xor ax,FillRopPairs[bx]
    mov si,ax                    ; coefficients for P=0
    mov bl,cl                    ; BH is still zero, index was <=60
    shr bl,4
    shl bx,2
    mov al,dl
    mov ah,dl
    and ax,FillRopPairs[bx+2]
    xor ax,FillRopPairs[bx]       ; coefficients for P=1
    mov dl,ch
    mov dh,ch
    and ax,dx
    not dx
    and si,dx
    or ax,si
    mov dx,ax                    ; DL=XOR, DH=AND, already in final form
    pop si
    pop cx
    pop bx
    ret
BlitReduce endp

; AL truth index, BL P, DL S, CL write mask, ES:DI validated destination.
; Touches at most one byte; masked neighbours use the CURRENT destination.
; A zero mask and destination identity do not access the destination at all.
; Source/P are supplied by value; this helper never fetches those operands.
; AX/DX/flags destroyed, other registers preserved. No status/value return.
BlitWriteByte proc near
    or cl,cl
    jnz @F
    jmp BlitByteDone
@@:
    cmp al,0aah
    jne @F
    jmp BlitByteDone
@@:
    call BlitReduce
    mov al,cl
    not al
    or dh,al
    and dl,cl
    mov al,dl
    or dh,dh
    jnz @F
    jmp BlitByteStore
@@:
    mov ah,es:[di]
    and ah,dh
    xor al,ah
BlitByteStore:
    mov es:[di],al
BlitByteDone:
    ret
BlitWriteByte endp

; One coupled-clipping axis. AX signed destination origin, BX signed source,
; CX signed extent, DX destination limit, SI source limit, DI 0=no S/1=use S.
; -> AX/BX adjusted origins, CX clipped extent, CF error. Empty CX=0 is valid.
; Unused BX is preserved; unused source bounds are not examined. Zero extent
; examines no bounds. DX/SI/DI/BP/DS/ES preserved; no pointer arithmetic here.
BlitClipAxis proc near
    push dx
    push bp
    or cx,cx
    jns @F
    jmp BlitClipBad
@@:
    jnz @F
    jmp BlitClipOK
@@:
    cmp di,1
    jbe @F
    jmp BlitClipBad
@@:
    or dx,dx
    jg @F
    jmp BlitClipBad
@@:
    or di,di
    jnz @F
    jmp BlitClipDestination
@@:
    or si,si
    jg @F
    jmp BlitClipBad
@@:
BlitClipDestination:
    or ax,ax
    js @F
    jmp BlitClipRight
@@:
    mov bp,ax
    neg bp                       ; -32768 becomes unsigned 32768, safely
    cmp bp,cx
    jb @F
    jmp BlitClipEmpty
@@:
    sub cx,bp
    xor ax,ax
    or di,di
    jnz @F
    jmp BlitClipRight
@@:
    add bx,bp
    jno @F
    jmp BlitClipEmpty              ; source only moves up; above signed limit
@@:
BlitClipRight:
    cmp ax,dx
    jl @F
    jmp BlitClipEmpty
@@:
    sub dx,ax
    cmp cx,dx
    ja @F
    jmp BlitClipSource
@@:
    mov cx,dx
BlitClipSource:
    or di,di
    jnz @F
    jmp BlitClipOK
@@:
    or bx,bx
    js @F
    jmp BlitClipSourceRight
@@:
    mov bp,bx
    neg bp
    cmp bp,cx
    jb @F
    jmp BlitClipEmpty
@@:
    sub cx,bp
    add ax,bp                    ; already bounded by destination clipping
    xor bx,bx
BlitClipSourceRight:
    cmp bx,si
    jl @F
    jmp BlitClipEmpty
@@:
    mov dx,si
    sub dx,bx
    cmp cx,dx
    ja @F
    jmp BlitClipOK
@@:
    mov cx,dx
BlitClipOK:
    pop bp
    pop dx
    clc
    ret
BlitClipEmpty:
    xor cx,cx
    jmp BlitClipOK
BlitClipBad:
    xor cx,cx
    pop bp
    pop dx
    stc
    ret
BlitClipAxis endp

; AX is a source window (AH first byte, AL following byte). DX source phase,
; BX destination phase, CX participating pixels (1..8, dest phase+CX <= 8).
; -> AL aligned source bits, AH destination mask, CF error. Other registers
; preserved. Fetch a following byte ONLY if source phase+CX > 8; otherwise
; caller supplies zero there. This helper performs no memory reads.
BlitAlign proc near
    push cx
    push dx
    push si
    cmp dx,7
    jbe @F
    jmp BlitAlignBad
@@:
    cmp bx,7
    jbe @F
    jmp BlitAlignBad
@@:
    or cx,cx
    jnz @F
    jmp BlitAlignBad
@@:
    cmp cx,8
    jbe @F
    jmp BlitAlignBad
@@:
    mov si,cx
    add cx,bx
    cmp cx,8
    jbe @F
    jmp BlitAlignBad
@@:
    mov cl,dl
    shl ax,cl
    mov al,ah
    mov cx,bx
    shr al,cl
    mov ah,0ffh
    shr ah,cl
    mov cx,8
    sub cx,bx
    sub cx,si
    mov dl,0ffh
    shl dl,cl
    and ah,dl
    and al,ah
    pop si
    pop dx
    pop cx
    clc
    ret
BlitAlignBad:
    xor ax,ax
    pop si
    pop dx
    pop cx
    stc
    ret
BlitAlign endp

; AL raw mono, BL/BH foreground/background drawing indices 0..15 ->
; AL/AH/DL/DH plane bytes 0/1/2/3, CF error. BX/CX/SI/DI/BP/DS/ES preserved.
BlitMonoToColour proc near
    test bx,0f0f0h
    jz @F
    jmp BlitMonoBad
@@:
    push bx
    push cx
    push si
    xor ah,ah
    mov si,ax
    call BlitMonoPlane
    push ax
    shr bl,1
    shr bh,1
    mov ax,si
    call BlitMonoPlane
    push ax
    shr bl,1
    shr bh,1
    mov ax,si
    call BlitMonoPlane
    push ax
    shr bl,1
    shr bh,1
    mov ax,si
    call BlitMonoPlane
    mov dh,al
    pop ax
    mov dl,al
    pop ax
    mov ah,al
    pop cx
    mov al,cl
    pop si
    pop cx
    pop bx
    clc
    ret
BlitMonoBad:
    xor ax,ax
    xor dx,dx
    stc
    ret
BlitMonoToColour endp

; AL raw mono, BL/BH low bits are fg/bg. AL result; DX destroyed.
BlitMonoPlane proc near
    mov dl,al
    not dl
    mov dh,bl
    and dh,1
    neg dh
    and dh,dl
    mov dl,al
    mov al,bh
    and al,1
    neg al
    and al,dl
    or al,dh
    ret
BlitMonoPlane endp

; AL/AH/DL/DH plane bytes, BL background index 0..15 -> AX mono byte, CF.
; Exactly background-matching colour pixels become 1; not cached intensity.
; DX destroyed; BX/CX/SI/DI/BP/DS/ES preserved.
BlitColourToMono proc near
    cmp bl,15
    jbe @F
    jmp BlitColourBad
@@:
    push cx
    mov ch,al
    mov cl,ah
    test bl,1
    jz @F
    jmp BlitColourGreen
@@:
    not ch
BlitColourGreen:
    test bl,2
    jz @F
    jmp BlitColourBlue
@@:
    not cl
BlitColourBlue:
    test bl,4
    jz @F
    jmp BlitColourMatch
@@:
    not dl
BlitColourMatch:
    test bl,8
    jnz BlitColourHigh
    not dh
BlitColourHigh:
    and ch,dh
    and ch,cl
    and ch,dl
    mov al,ch
    xor ah,ah
    pop cx
    clc
    ret
BlitColourBad:
    xor ax,ax
    stc
    ret
BlitColourToMono endp
