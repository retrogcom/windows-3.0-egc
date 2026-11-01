; 0.10: source/destination-only full-byte runs, including mono expansion.
; X_MODE=0 proves disjoint storage or equivalent layout/plane identity.
; Edges are still staged by BltTile before any plane write. No speculative
; source reads: shifted windows consume exactly run+1 participating bytes.
S_C0 equ F_SCAN
S_C1 equ F_ENDY
S_M0 equ F_ROPCONST
S_M1 equ F_ROPMASK
S_WORD equ F_READY
S_SHIFT equ F_PHASE
public BltSourceRun
BltSourceRun proc near
    mov ax,[bp+X_POS]
    add ax,[bp+X_SX]
    mov dx,ax
    and dx,7
    mov [bp+X_SPHASE],dx
    shr ax,3
    mov [bp+F_SRC],ax
    mov ax,[bp+X_POS]
    add ax,[bp+X_DX]
    shr ax,3
    mov [bp+F_DST],ax
    mov word ptr [bp+F_PLANE],0
BSRPlane:
    call BltSourceCoefficients
    mov bx,[bp+F_PLANE]
    mov si,bx
    mov di,[bp+X_DPTR+si]
    mov es,[bp+X_DPTR+si+2]
    cmp word ptr [bp+X_SP],1
    jne BSRSourcePlane
    xor si,si
BSRSourcePlane:
    mov dx,[bp+X_SPTR+si+2]
    mov si,[bp+X_SPTR+si]
    add si,[bp+F_SRC]
    add di,[bp+F_DST]
    mov bx,[bp+F_RUN]
    push ds
    mov ds,dx
    call BltSourcePlane
BSRPlaneDone:
    pop ds
    add word ptr [bp+F_PLANE],4
    mov ax,[bp+F_PLANE]
    shr ax,2
    cmp ax,[bp+X_DP]
    jae BSRDone
    jmp BSRPlane
BSRDone:
    cld
    ret
BltSourceRun endp

; AL raw source, ES:DI full destination byte. BX/CX/SI/DI preserved.
BSRWriteByte proc near
    mov dl,al
    and al,byte ptr [bp+S_C1]
    xor al,byte ptr [bp+S_C0]
    and dl,byte ptr [bp+S_M1]
    xor dl,byte ptr [bp+S_M0]
    or dl,dl
    jz BSRByteStore
    and dl,es:[di]
    xor al,dl
BSRByteStore:
    mov es:[di],al
    ret
BSRWriteByte endp

public BltSourceCoefficients, BltSourcePlane
BltSourceCoefficients proc near
    mov bx,[bp+X_ROP]
    test word ptr [bp+X_USE],4
    jz BSRPatternSelected
    mov cx,[bp+F_PLANE]
    shr cx,2
    mov ax,[bp+P_PATTERN]
    shr ax,cl
    test ax,1
    jz BSRPatternSelected
    shr bx,4
BSRPatternSelected:
    and bx,15
    shl bx,2
    mov ax,FillRopPairs[bx]
    mov dx,FillRopPairs[bx+2]
    mov cx,[bp+X_SP]
    cmp cx,[bp+X_DP]
    je BSRCoefs
    ; Raw zero is foreground; raw one is background. Transform the truth
    ; coefficients once per plane, rather than recolouring each source byte.
    mov cx,[bp+F_PLANE]
    shr cx,2
    mov bl,byte ptr [bp+T_DM+11]
    shr bl,cl
    and bl,1
    neg bl
    mov bh,bl
    and bx,dx
    xor ax,bx
    mov bl,byte ptr [bp+T_DM+11]
    xor bl,byte ptr [bp+T_DM+7]
    shr bl,cl
    and bl,1
    neg bl
    mov bh,bl
    and dx,bx
BSRCoefs:
    mov bl,ah
    mov ah,al
    mov [bp+S_C0],ax
    mov al,bl
    mov ah,bl
    mov [bp+S_M0],ax
    mov al,dl
    mov ah,dl
    mov [bp+S_C1],ax
    mov al,dh
    mov ah,dh
    mov [bp+S_M1],ax
    mov al,byte ptr [bp+S_C0]
    and al,1
    mov bl,byte ptr [bp+S_C0]
    xor bl,byte ptr [bp+S_M0]
    and bl,2
    or al,bl
    mov bl,byte ptr [bp+S_C0]
    xor bl,byte ptr [bp+S_C1]
    mov bh,bl
    and bl,4
    or al,bl
    xor bh,byte ptr [bp+S_M0]
    xor bh,byte ptr [bp+S_M1]
    and bh,8
    or al,bh
    xor ah,ah
    shl ax,2
    mov bx,ax
    mov ax,SourceKernelTable[bx]
    mov [bp+S_WORD],ax
    mov ax,SourceKernelTable[bx+2]
    mov [bp+S_SHIFT],ax
    ret
BltSourceCoefficients endp

BltSourcePlane proc near
    cld                     ; Windows may enter with DF set
    cmp word ptr [bp+S_WORD],offset SW10
    jne @F
    ret
@@:
    cmp word ptr [bp+X_XDIR],1
    jne BSPGeneral
    cmp word ptr [bp+X_SPHASE],0
    je BSPTryWords
    ; The forward rolling window consumes exactly count+1 source bytes.
    ; Saved BP holds the register count only while the selected kernel runs.
    mov cx,[bp+X_SPHASE]
    mov dx,[bp+S_SHIFT]
    push bp
    mov bp,bx
    mov bl,[si]
    inc si
    call dx
    pop bp
    cld
    ret
BSPTryWords:
    cmp bx,2
    jb BSPGeneral
    mov ax,si
    xor ax,di
    test ax,1
    jz @F
    jmp BSPMixed
@@:
    test di,1
    jz BSPWordsReady
    lodsb
    call BSRWriteByte
    inc di
    dec bx
    cmp bx,2
    jb BSPWordTail
BSPWordsReady:
    mov cx,bx
    shr cx,1
    call word ptr [bp+S_WORD]
    test bx,1
    jz BSPFastDone
BSPWordTail:
    lodsb
    call BSRWriteByte
BSPFastDone:
    cld
    ret
BSPGeneral:
    cmp word ptr [bp+X_XDIR],1
    je BSRForward
    add si,bx
    add di,bx
    dec si
    dec di
    jmp BSRBackward
BSRForward:
    cmp word ptr [bp+X_SPHASE],0
    jne BSRForwardShift
BSRForwardAligned:
    cmp bx,2
    jb BSRForwardByte
    mov ax,si
    xor ax,di
    test ax,1
    jnz BSRForwardByte
    test di,1
    jnz BSRForwardByte
    mov ax,[si]
    mov dx,ax
    and ax,[bp+S_C1]
    xor ax,[bp+S_C0]
    and dx,[bp+S_M1]
    xor dx,[bp+S_M0]
    or dx,dx
    jz BSRForwardWordStore
    and dx,es:[di]
    xor ax,dx
BSRForwardWordStore:
    mov es:[di],ax
    add si,2
    add di,2
    sub bx,2
    jnz BSRForwardAligned
    jmp BSPDone
BSRForwardByte:
    mov al,[si]
    call BSRWriteByte
    add si,1
    add di,1
    dec bx
    jnz BSRForwardAligned
    jmp BSPDone
BSRForwardShift:
    mov cx,[bp+X_SPHASE]
    mov dl,[si]
BSRForwardShiftByte:
    mov ah,dl
    mov al,[si+1]
    mov dl,al
    shl ax,cl
    mov al,ah
    push dx
    call BSRWriteByte
    pop dx
    add si,1
    add di,1
    dec bx
    jnz BSRForwardShiftByte
    jmp BSPDone
BSRBackward:
    cmp word ptr [bp+X_SPHASE],0
    jne BSRBackwardShift
BSRBackwardAligned:
    cmp bx,2
    jb BSRBackwardByte
    mov ax,si
    xor ax,di
    test ax,1
    jnz BSRBackwardByte
    test di,1
    jz BSRBackwardByte
    mov ax,[si-1]
    mov dx,ax
    and ax,[bp+S_C1]
    xor ax,[bp+S_C0]
    and dx,[bp+S_M1]
    xor dx,[bp+S_M0]
    or dx,dx
    jz BSRBackwardWordStore
    and dx,es:[di-1]
    xor ax,dx
BSRBackwardWordStore:
    mov es:[di-1],ax
    sub si,2
    sub di,2
    sub bx,2
    jnz BSRBackwardAligned
    jmp BSPDone
BSRBackwardByte:
    mov al,[si]
    call BSRWriteByte
    sub si,1
    sub di,1
    dec bx
    jnz BSRBackwardAligned
    jmp BSPDone
BSRBackwardShift:
    mov cx,[bp+X_SPHASE]
    mov dl,[si+1]
BSRBackwardShiftByte:
    mov ah,[si]
    mov al,dl
    mov dl,ah
    shl ax,cl
    mov al,ah
    push dx
    call BSRWriteByte
    pop dx
    sub si,1
    sub di,1
    dec bx
    jnz BSRBackwardShiftByte
    jmp BSPDone
BSPDone:
    ret
BSPMixed:
    ; 0.13: source and destination parity differ. Build destination
    ; words from two source bytes; the generic coefficients serve every
    ; truth function. Exactly BX source bytes are read.
    test di,1
    jz BSPMixedWords
    lodsb
    call BSRWriteByte
    inc di
    dec bx
    cmp bx,2
    jb BSPMixedTail
BSPMixedWords:
    mov cx,bx
    shr cx,1
BSPMixedLoop:
    mov al,[si]
    mov ah,[si+1]
    add si,2
    mov dx,ax
    and ax,[bp+S_C1]
    xor ax,[bp+S_C0]
    and dx,[bp+S_M1]
    xor dx,[bp+S_M0]
    or dx,dx
    jz BSPMixedStore
    and dx,es:[di]
    xor ax,dx
BSPMixedStore:
    mov es:[di],ax
    add di,2
    loop BSPMixedLoop
BSPMixedTail:
    test bx,1
    jz BSPMixedDone
    lodsb
    call BSRWriteByte
BSPMixedDone:
    cld
    ret
BltSourcePlane endp

; 0.12 static full-word and shifted-byte truth kernels.
; Caller proves forward overlap order and exact source/destination extents.
SW0 proc near
SW0Loop:
    xor ax,ax
    stosw
    dec cx
    jnz SW0Loop
    ret
SW0 endp
SB0 proc near
SB0Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    xor al,al
    stosb
    dec bp
    jnz SB0Loop
    ret
SB0 endp
SW1 proc near
SW1Loop:
    lodsw
    or ax,es:[di]
    not ax
    stosw
    dec cx
    jnz SW1Loop
    ret
SW1 endp
SB1 proc near
SB1Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    or al,es:[di]
    not al
    stosb
    dec bp
    jnz SB1Loop
    ret
SB1 endp
SW2 proc near
SW2Loop:
    lodsw
    not ax
    and ax,es:[di]
    stosw
    dec cx
    jnz SW2Loop
    ret
SW2 endp
SB2 proc near
SB2Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    not al
    and al,es:[di]
    stosb
    dec bp
    jnz SB2Loop
    ret
SB2 endp
SW3 proc near
SW3Loop:
    lodsw
    not ax
    stosw
    dec cx
    jnz SW3Loop
    ret
SW3 endp
SB3 proc near
SB3Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    not al
    stosb
    dec bp
    jnz SB3Loop
    ret
SB3 endp
SW4 proc near
SW4Loop:
    lodsw
    mov dx,es:[di]
    not dx
    and ax,dx
    stosw
    dec cx
    jnz SW4Loop
    ret
SW4 endp
SB4 proc near
SB4Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    mov dl,es:[di]
    not dl
    and al,dl
    stosb
    dec bp
    jnz SB4Loop
    ret
SB4 endp
SW5 proc near
SW5Loop:
    mov ax,es:[di]
    not ax
    stosw
    dec cx
    jnz SW5Loop
    ret
SW5 endp
SB5 proc near
SB5Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    mov al,es:[di]
    not al
    stosb
    dec bp
    jnz SB5Loop
    ret
SB5 endp
SW6 proc near
SW6Loop:
    lodsw
    xor ax,es:[di]
    stosw
    dec cx
    jnz SW6Loop
    ret
SW6 endp
SB6 proc near
SB6Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    xor al,es:[di]
    stosb
    dec bp
    jnz SB6Loop
    ret
SB6 endp
SW7 proc near
SW7Loop:
    lodsw
    and ax,es:[di]
    not ax
    stosw
    dec cx
    jnz SW7Loop
    ret
SW7 endp
SB7 proc near
SB7Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    and al,es:[di]
    not al
    stosb
    dec bp
    jnz SB7Loop
    ret
SB7 endp
SW8 proc near
SW8Loop:
    lodsw
    and ax,es:[di]
    stosw
    dec cx
    jnz SW8Loop
    ret
SW8 endp
SB8 proc near
SB8Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    and al,es:[di]
    stosb
    dec bp
    jnz SB8Loop
    ret
SB8 endp
SW9 proc near
SW9Loop:
    lodsw
    xor ax,es:[di]
    not ax
    stosw
    dec cx
    jnz SW9Loop
    ret
SW9 endp
SB9 proc near
SB9Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    xor al,es:[di]
    not al
    stosb
    dec bp
    jnz SB9Loop
    ret
SB9 endp
SW10 proc near
SW10Loop:
    ret
SW10 endp
SB10 proc near
SB10Loop:
    ret
SB10 endp
SW11 proc near
SW11Loop:
    lodsw
    not ax
    or ax,es:[di]
    stosw
    dec cx
    jnz SW11Loop
    ret
SW11 endp
SB11 proc near
SB11Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    not al
    or al,es:[di]
    stosb
    dec bp
    jnz SB11Loop
    ret
SB11 endp
SW12 proc near
SW12Loop:
    lodsw
    stosw
    dec cx
    jnz SW12Loop
    ret
SW12 endp
SB12 proc near
SB12Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    stosb
    dec bp
    jnz SB12Loop
    ret
SB12 endp
SW13 proc near
SW13Loop:
    lodsw
    mov dx,es:[di]
    not dx
    or ax,dx
    stosw
    dec cx
    jnz SW13Loop
    ret
SW13 endp
SB13 proc near
SB13Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    mov dl,es:[di]
    not dl
    or al,dl
    stosb
    dec bp
    jnz SB13Loop
    ret
SB13 endp
SW14 proc near
SW14Loop:
    lodsw
    or ax,es:[di]
    stosw
    dec cx
    jnz SW14Loop
    ret
SW14 endp
SB14 proc near
SB14Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    or al,es:[di]
    stosb
    dec bp
    jnz SB14Loop
    ret
SB14 endp
SW15 proc near
SW15Loop:
    mov ax,0ffffh
    stosw
    dec cx
    jnz SW15Loop
    ret
SW15 endp
SB15 proc near
SB15Loop:
    mov ah,bl
    lodsb
    mov bl,al
    shl ax,cl
    mov al,ah
    mov al,0ffh
    stosb
    dec bp
    jnz SB15Loop
    ret
SB15 endp

; CF clear: P_PATTERN is the effective four-bit constant brush value.
; All 8 rows are checked, not merely the 4-row repetition test used by fills.
; Mono brushes are expanded with the already snapshotted drawing colours.
public BltConstantPattern
BltConstantPattern proc near
    cmp word ptr [bp+T_BRUSH+48],4
    je BCPMono
    xor si,si
    xor dx,dx
    mov bx,1
BCPPlane:
    mov al,byte ptr [bp+T_BRUSH+si]
    or al,al
    jz BCPValue
    cmp al,0ffh
    jne BCPBad
    or dx,bx
BCPValue:
    mov ah,al
    cmp ax,[bp+T_BRUSH+si+0]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+si+2]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+si+4]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+si+6]
    jne BCPBad
    add si,8
    shl bx,1
    cmp si,32
    jb BCPPlane
    mov [bp+P_PATTERN],dx
    clc
    ret
BCPMono:
    mov al,byte ptr [bp+T_BRUSH+32]
    or al,al
    jz BCPMonoValue
    cmp al,0ffh
    jne BCPBad
BCPMonoValue:
    mov ah,al
    cmp ax,[bp+T_BRUSH+32]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+34]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+36]
    jne BCPBad
    cmp ax,[bp+T_BRUSH+38]
    jne BCPBad
    or ax,ax
    mov al,byte ptr [bp+T_DM+11]
    jz BCPMonoReady
    mov al,byte ptr [bp+T_DM+7]
BCPMonoReady:
    and ax,15
    mov [bp+P_PATTERN],ax
    clc
    ret
BCPBad:
    stc
    ret
BltConstantPattern endp
