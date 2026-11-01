; Post019: masked logic uses D & (S | ~M), D ^ (S & M),
; D | (S & M), or D | (~S & M). Preserve the original ordered
; byte/word surface accesses; no new admission, cache or scratch locals.
; 0.14 fused forward shifted copy, AND and XOR rows. Complete rectangle preflight,
; independent plane schedule and even row strides are proved by the caller.
; The head, byte/word interior and tail preserve exactly the old write order.
; The tail reuses the rolling source byte; no read past its coverage is made.
; Pointer deltas account for the one-byte rolling lookahead, including wrap.
; 0.18: keep BL as the carry bits above CH=(1<<phase)-1. Initial
; SHL and byte ROL produce this form; each word masks its next carry.
; Byte ROL with initial AL=0 produces only CH bits, so OR is sufficient.
; Source accesses, aligned destination stores and alias order are unchanged.
P_WORDENTRY equ P_FAST
public BPRShiftPrepare, BPRShiftRows
BPRShiftPrepare proc near
    mov ax,[bp+P_HEAD+2]
    sub ax,[bp+F_SRC]
    mov [bp+P_HDELTA],ax
    xor si,si
BPRSPrepareLoop:
    mov ax,[bp+si+P_RAWD]
    sub ax,[bp+F_RUN]
    mov [bp+si+P_DSTEPS],ax
    mov ax,[bp+si+P_RAWS]
    sub ax,[bp+F_RUN]
    dec ax
    mov [bp+si+P_SSTEPS],ax
    add si,2
    cmp si,8
    jb BPRSPrepareLoop
    ret
BPRShiftPrepare endp
BPRShiftRows proc near
    cld
    mov ax,di
    and ax,1
    mov [bp+P_FRONT],ax
    mov dx,[bp+F_RUN]
    sub dx,ax
    mov ax,dx
    and ax,1
    mov [bp+P_BACK],ax
    shr dx,1
    mov [bp+P_WORDS],dx
    mov ax,si
    inc ax
    add ax,[bp+P_FRONT]
    test ax,1
    jnz BPRSChooseOdd
    cmp word ptr [bp+P_COPY],1
    je BPRSEvenCopy
    cmp word ptr [bp+S_WORD],offset SW12
    je BPRSEvenCopy
    cmp word ptr [bp+S_WORD],offset SW8
    jne BPRSEvenXor
    jmp BPRAndEvenRows
BPRSEvenXor:
    cmp word ptr [bp+S_WORD],offset SW6
    jne @F
    jmp BPRXorEvenRows
@@:
    cmp word ptr [bp+S_WORD],offset SW11
    jne @F
    jmp BPRNotOrEvenRows
@@:
    cmp word ptr [bp+S_WORD],offset SW14
    jne @F
    jmp BPROrEvenRows
@@:
    jmp BPRXorEvenRows
BPRSEvenCopy:
    ; Post020 E2: five or more words loop to the last four and fall into
    ; the unrolled bodies; three and four keep their 0.20 entries; two or
    ; fewer keep the plain loop. Still chosen once per plane.
    mov ax,[bp+P_WORDS]
    mov word ptr [bp+P_WORDENTRY],offset BPRSEvenThreeWords
    cmp ax,3
    je BPRSEvenEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSEvenFourWords
    cmp ax,4
    je BPRSEvenEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSEvenGenericWords
    ja BPRSEvenEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSEvenFewWords
BPRSEvenEntryReady:
    jmp BPRShiftEvenRows
BPRSChooseOdd:
    cmp word ptr [bp+P_COPY],1
    je BPRSOddCopy
    cmp word ptr [bp+S_WORD],offset SW12
    je BPRSOddCopy
    cmp word ptr [bp+S_WORD],offset SW8
    jne BPRSOddXor
    jmp BPRAndOddRows
BPRSOddXor:
    cmp word ptr [bp+S_WORD],offset SW6
    jne @F
    jmp BPRXorOddRows
@@:
    cmp word ptr [bp+S_WORD],offset SW11
    jne @F
    jmp BPRNotOrOddRows
@@:
    cmp word ptr [bp+S_WORD],offset SW14
    jne @F
    jmp BPROrOddRows
@@:
    jmp BPRXorOddRows
BPRSOddCopy:
    ; Post020 E2: five or more words loop to the last four and fall into
    ; the unrolled bodies; three and four keep their 0.20 entries; two or
    ; fewer keep the plain loop. Still chosen once per plane.
    mov ax,[bp+P_WORDS]
    mov word ptr [bp+P_WORDENTRY],offset BPRSOddThreeWords
    cmp ax,3
    je BPRSOddEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSOddFourWords
    cmp ax,4
    je BPRSOddEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSOddGenericWords
    ja BPRSOddEntryReady
    mov word ptr [bp+P_WORDENTRY],offset BPRSOddFewWords
BPRSOddEntryReady:
    jmp BPRShiftOddRows
BPRShiftRows endp
BPRShiftEvenRows proc near
BPRSEvenRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRSEvenInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRSEvenHeadLoaded
    mov al,[bx+si+1]
BPRSEvenHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BPRSEvenInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRSEvenWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    mov es:[di],al
    mov bl,ah
    inc si
    inc di
BPRSEvenWordCount:
    jmp word ptr [bp+P_WORDENTRY]
BPRSEvenFewWords:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRSEvenFewDone
BPRSEvenFew:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    dec dx
    jnz BPRSEvenFew
BPRSEvenFewDone:
    jmp BPRSEvenByteTail
BPRSEvenGenericWords:
    mov dx,[bp+P_WORDS]
    sub dx,4
BPRSEvenWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    dec dx
    jnz BPRSEvenWords
BPRSEvenFourWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
BPRSEvenThreeWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
BPRSEvenByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRSEvenTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    mov es:[di],al
    mov bl,ah
    inc si
    inc di
BPRSEvenTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRSEvenNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRSEvenTailLoaded
    mov ah,[si]
BPRSEvenTailLoaded:
    rol ax,cl
    or al,bl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di],al
BPRSEvenNext:
    dec word ptr [bp+P_LEFT]
    jz BPRSEvenDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRSEvenRow
BPRSEvenDone:
    cld
    ret
BPRShiftEvenRows endp
BPRShiftOddRows proc near
BPRSOddRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRSOddInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRSOddHeadLoaded
    mov al,[bx+si+1]
BPRSOddHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BPRSOddInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRSOddWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    mov es:[di],al
    mov bl,ah
    inc si
    inc di
BPRSOddWordCount:
    jmp word ptr [bp+P_WORDENTRY]
BPRSOddFewWords:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRSOddFewDone
BPRSOddFew:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    dec dx
    jnz BPRSOddFew
BPRSOddFewDone:
    jmp BPRSOddByteTail
BPRSOddGenericWords:
    mov dx,[bp+P_WORDS]
    sub dx,4
BPRSOddWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    dec dx
    jnz BPRSOddWords
BPRSOddFourWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
BPRSOddThreeWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    stosw
BPRSOddByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRSOddTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    mov es:[di],al
    mov bl,ah
    inc si
    inc di
BPRSOddTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRSOddNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRSOddTailLoaded
    mov ah,[si]
BPRSOddTailLoaded:
    rol ax,cl
    or al,bl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di],al
BPRSOddNext:
    dec word ptr [bp+P_LEFT]
    jz BPRSOddDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRSOddRow
BPRSOddDone:
    cld
    ret
BPRShiftOddRows endp

BPRAndEvenRows proc near
BPRAndEvenRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRAndEvenInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRAndEvenHeadLoaded
    mov al,[bx+si+1]
BPRAndEvenHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    not al
    and al,byte ptr [bp+P_HEAD+6]
    not al
    and es:[di-1],al
BPRAndEvenInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRAndEvenWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    and es:[di],al
    mov bl,ah
    inc si
    inc di
BPRAndEvenWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRAndEvenByteTail
BPRAndEvenWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    and ax,es:[di]
    stosw
    dec dx
    jnz BPRAndEvenWords
BPRAndEvenByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRAndEvenTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    and es:[di],al
    mov bl,ah
    inc si
    inc di
BPRAndEvenTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRAndEvenNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRAndEvenTailLoaded
    mov ah,[si]
BPRAndEvenTailLoaded:
    rol ax,cl
    or al,bl
    not al
    and al,byte ptr [bp+P_TAIL+6]
    not al
    and es:[di],al
BPRAndEvenNext:
    dec word ptr [bp+P_LEFT]
    jz BPRAndEvenDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRAndEvenRow
BPRAndEvenDone:
    cld
    ret
BPRAndEvenRows endp

BPRXorEvenRows proc near
BPRXorEvenRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRXorEvenInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRXorEvenHeadLoaded
    mov al,[bx+si+1]
BPRXorEvenHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    and al,byte ptr [bp+P_HEAD+6]
    xor es:[di-1],al
BPRXorEvenInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRXorEvenWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    xor es:[di],al
    mov bl,ah
    inc si
    inc di
BPRXorEvenWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRXorEvenByteTail
BPRXorEvenWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    xor ax,es:[di]
    stosw
    dec dx
    jnz BPRXorEvenWords
BPRXorEvenByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRXorEvenTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    xor es:[di],al
    mov bl,ah
    inc si
    inc di
BPRXorEvenTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRXorEvenNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRXorEvenTailLoaded
    mov ah,[si]
BPRXorEvenTailLoaded:
    rol ax,cl
    or al,bl
    and al,byte ptr [bp+P_TAIL+6]
    xor es:[di],al
BPRXorEvenNext:
    dec word ptr [bp+P_LEFT]
    jz BPRXorEvenDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRXorEvenRow
BPRXorEvenDone:
    cld
    ret
BPRXorEvenRows endp

BPRAndOddRows proc near
BPRAndOddRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRAndOddInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRAndOddHeadLoaded
    mov al,[bx+si+1]
BPRAndOddHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    not al
    and al,byte ptr [bp+P_HEAD+6]
    not al
    and es:[di-1],al
BPRAndOddInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRAndOddWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    and es:[di],al
    mov bl,ah
    inc si
    inc di
BPRAndOddWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRAndOddByteTail
BPRAndOddWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    and ax,es:[di]
    stosw
    dec dx
    jnz BPRAndOddWords
BPRAndOddByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRAndOddTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    and es:[di],al
    mov bl,ah
    inc si
    inc di
BPRAndOddTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRAndOddNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRAndOddTailLoaded
    mov ah,[si]
BPRAndOddTailLoaded:
    rol ax,cl
    or al,bl
    not al
    and al,byte ptr [bp+P_TAIL+6]
    not al
    and es:[di],al
BPRAndOddNext:
    dec word ptr [bp+P_LEFT]
    jz BPRAndOddDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRAndOddRow
BPRAndOddDone:
    cld
    ret
BPRAndOddRows endp

BPRXorOddRows proc near
BPRXorOddRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRXorOddInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRXorOddHeadLoaded
    mov al,[bx+si+1]
BPRXorOddHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    and al,byte ptr [bp+P_HEAD+6]
    xor es:[di-1],al
BPRXorOddInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRXorOddWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    xor es:[di],al
    mov bl,ah
    inc si
    inc di
BPRXorOddWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRXorOddByteTail
BPRXorOddWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    xor ax,es:[di]
    stosw
    dec dx
    jnz BPRXorOddWords
BPRXorOddByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRXorOddTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    xor es:[di],al
    mov bl,ah
    inc si
    inc di
BPRXorOddTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRXorOddNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRXorOddTailLoaded
    mov ah,[si]
BPRXorOddTailLoaded:
    rol ax,cl
    or al,bl
    and al,byte ptr [bp+P_TAIL+6]
    xor es:[di],al
BPRXorOddNext:
    dec word ptr [bp+P_LEFT]
    jz BPRXorOddDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRXorOddRow
BPRXorOddDone:
    cld
    ret
BPRXorOddRows endp

; 0.16 reduced per-plane D OR S and D OR NOT S fused rows.
BPRNotOrEvenRows proc near
BPRNotOrEvenRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRNotOrEvenInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRNotOrEvenHeadLoaded
    mov al,[bx+si+1]
BPRNotOrEvenHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    not al
    and al,byte ptr [bp+P_HEAD+6]
    or es:[di-1],al
BPRNotOrEvenInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRNotOrEvenWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    not al
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPRNotOrEvenWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRNotOrEvenByteTail
BPRNotOrEvenWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    not ax
    or es:[di],ax
    add di,2
    dec dx
    jnz BPRNotOrEvenWords
BPRNotOrEvenByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRNotOrEvenTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    not al
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPRNotOrEvenTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRNotOrEvenNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRNotOrEvenTailLoaded
    mov ah,[si]
BPRNotOrEvenTailLoaded:
    rol ax,cl
    or al,bl
    not al
    and al,byte ptr [bp+P_TAIL+6]
    or es:[di],al
BPRNotOrEvenNext:
    dec word ptr [bp+P_LEFT]
    jz BPRNotOrEvenDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRNotOrEvenRow
BPRNotOrEvenDone:
    cld
    ret
BPRNotOrEvenRows endp

BPROrEvenRows proc near
BPROrEvenRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPROrEvenInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPROrEvenHeadLoaded
    mov al,[bx+si+1]
BPROrEvenHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    and al,byte ptr [bp+P_HEAD+6]
    or es:[di-1],al
BPROrEvenInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPROrEvenWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPROrEvenWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPROrEvenByteTail
BPROrEvenWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    or es:[di],ax
    add di,2
    dec dx
    jnz BPROrEvenWords
BPROrEvenByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPROrEvenTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPROrEvenTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPROrEvenNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPROrEvenTailLoaded
    mov ah,[si]
BPROrEvenTailLoaded:
    rol ax,cl
    or al,bl
    and al,byte ptr [bp+P_TAIL+6]
    or es:[di],al
BPROrEvenNext:
    dec word ptr [bp+P_LEFT]
    jz BPROrEvenDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPROrEvenRow
BPROrEvenDone:
    cld
    ret
BPROrEvenRows endp

BPRNotOrOddRows proc near
BPRNotOrOddRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRNotOrOddInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPRNotOrOddHeadLoaded
    mov al,[bx+si+1]
BPRNotOrOddHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    not al
    and al,byte ptr [bp+P_HEAD+6]
    or es:[di-1],al
BPRNotOrOddInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPRNotOrOddWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    not al
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPRNotOrOddWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPRNotOrOddByteTail
BPRNotOrOddWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    not ax
    or es:[di],ax
    add di,2
    dec dx
    jnz BPRNotOrOddWords
BPRNotOrOddByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPRNotOrOddTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    not al
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPRNotOrOddTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRNotOrOddNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPRNotOrOddTailLoaded
    mov ah,[si]
BPRNotOrOddTailLoaded:
    rol ax,cl
    or al,bl
    not al
    and al,byte ptr [bp+P_TAIL+6]
    or es:[di],al
BPRNotOrOddNext:
    dec word ptr [bp+P_LEFT]
    jz BPRNotOrOddDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRNotOrOddRow
BPRNotOrOddDone:
    cld
    ret
BPRNotOrOddRows endp

BPROrOddRows proc near
BPROrOddRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPROrOddInterior
    mov bx,[bp+P_HDELTA]
    mov ah,[bx+si]
    xor al,al
    cmp byte ptr [bp+P_HEAD+8],0
    je BPROrOddHeadLoaded
    mov al,[bx+si+1]
BPROrOddHeadLoaded:
    mov cl,byte ptr [bp+P_HEAD+4]
    shl ax,cl
    mov al,ah
    mov cl,byte ptr [bp+P_HEAD+5]
    shr al,cl
    and al,byte ptr [bp+P_HEAD+6]
    or es:[di-1],al
BPROrOddInterior:
    mov cx,[bp+X_SPHASE]
    mov bl,[si]
    inc si
    shl bl,cl
    cmp word ptr [bp+P_FRONT],0
    je BPROrOddWordCount
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPROrOddWordCount:
    mov dx,[bp+P_WORDS]
    or dx,dx
    jz BPROrOddByteTail
BPROrOddWords:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov bh,al
    and al,ch
    xor bh,al
    or al,bl
    mov bl,bh
    or es:[di],ax
    add di,2
    dec dx
    jnz BPROrOddWords
BPROrOddByteTail:
    cmp word ptr [bp+P_BACK],0
    je BPROrOddTail
    mov ah,[si]
    xor al,al
    rol ax,cl
    or al,bl
    or es:[di],al
    mov bl,ah
    inc si
    inc di
BPROrOddTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPROrOddNext
    xor ax,ax
    cmp byte ptr [bp+P_TAIL+8],0
    je BPROrOddTailLoaded
    mov ah,[si]
BPROrOddTailLoaded:
    rol ax,cl
    or al,bl
    and al,byte ptr [bp+P_TAIL+6]
    or es:[di],al
BPROrOddNext:
    dec word ptr [bp+P_LEFT]
    jz BPROrOddDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPROrOddRow
BPROrOddDone:
    cld
    ret
BPROrOddRows endp

public BPRCaptionRows
BPRCaptionRows proc near
    cmp word ptr [bp+F_RUN],1
    je BCR1Row
    cmp word ptr [bp+F_RUN],2
    je BCR2Row
    jmp BCR3Row
BCR1Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BCR1Interior
    mov al,[si-1]
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BCR1Interior:
    mov al,[si]
    mov es:[di],al
    cmp byte ptr [bp+P_TAIL+7],0
    je BCR1Next
    mov al,[si+1]
    mov ah,es:[di+1]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+1],al
BCR1Next:
    dec word ptr [bp+P_LEFT]
    jz BCR1Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BCR1Row
BCR1Done:
    ret
BCR2Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BCR2Interior
    mov al,[si-1]
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BCR2Interior:
    mov al,[si]
    mov es:[di],al
    mov al,[si+1]
    mov es:[di+1],al
    cmp byte ptr [bp+P_TAIL+7],0
    je BCR2Next
    mov al,[si+2]
    mov ah,es:[di+2]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+2],al
BCR2Next:
    dec word ptr [bp+P_LEFT]
    jz BCR2Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BCR2Row
BCR2Done:
    ret
BCR3Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BCR3Interior
    mov al,[si-1]
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BCR3Interior:
    mov al,[si]
    mov es:[di],al
    mov al,[si+1]
    mov es:[di+1],al
    mov al,[si+2]
    mov es:[di+2],al
    cmp byte ptr [bp+P_TAIL+7],0
    je BCR3Next
    mov al,[si+3]
    mov ah,es:[di+3]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+3],al
BCR3Next:
    dec word ptr [bp+P_LEFT]
    jz BCR3Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BCR3Row
BCR3Done:
    ret
BPRCaptionRows endp

; 0.17 same-phase backward SRCCOPY, independent planes, preflight complete.
; Right-to-left tail/interior/head order preserves horizontal overlap. Even
; strides retain parity; mixed source/destination parity uses byte copies.
; Raw steps plus the interior run exactly undo the backward pointer movement.
public BPRBackwardRows
BPRBackwardRows proc near
    mov cx,[bp+F_RUN]
    dec cx
    add si,cx
    add di,cx
    mov ax,si
    xor ax,di
    test ax,1
    jz @F
    jmp BBRBytesStart
@@:
    mov ax,di
    not ax
    and ax,1
    mov [bp+P_FRONT],ax
    mov cx,[bp+F_RUN]
    sub cx,ax
    mov ax,cx
    and ax,1
    mov [bp+P_BACK],ax
    shr cx,1
    mov [bp+P_WORDS],cx
    std
BBRWordRow:
    cmp byte ptr [bp+P_TAIL+7],0
    je BBRWordInterior
    mov al,[si+1]
    mov ah,es:[di+1]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+1],al
BBRWordInterior:
    cmp word ptr [bp+P_FRONT],0
    je BBRWordCount
    movsb
BBRWordCount:
    dec si
    dec di
    mov cx,[bp+P_WORDS]
    rep movsw
    inc si
    inc di
    cmp word ptr [bp+P_BACK],0
    je BBRWordHead
    movsb
BBRWordHead:
    cmp byte ptr [bp+P_HEAD+7],0
    je BBRWordNext
    mov al,[si]
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di],al
BBRWordNext:
    dec word ptr [bp+P_LEFT]
    jz BBRDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BBRWordRow
BBRBytesStart:
    std
BBRByteRow:
    cmp byte ptr [bp+P_TAIL+7],0
    je BBRByteInterior
    mov al,[si+1]
    mov ah,es:[di+1]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+1],al
BBRByteInterior:
    mov cx,[bp+F_RUN]
    rep movsb
    cmp byte ptr [bp+P_HEAD+7],0
    je BBRByteNext
    mov al,[si]
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di],al
BBRByteNext:
    dec word ptr [bp+P_LEFT]
    jz BBRDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BBRByteRow
BBRDone:
    cld
    ret
BPRBackwardRows endp

; 0.17 shifted SRCCOPY with 1..3 complete interior bytes. Select the width
; once per plane, retain the exact last input byte for the partial tail,
; and reuse raw bank steps. No alignment assumption or widened access.
public BPRSmallShiftRows
BPRSmallShiftRows proc near
    cmp word ptr [bp+F_RUN],1
    jne @F
    jmp BSS1Row
@@:
    cmp word ptr [bp+F_RUN],2
    jne @F
    jmp BSS2Row
@@:
    jmp BSS3Row
BSS1Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BSS1Interior
    mov bx,[bp+P_HDELTA]
    mov cl,byte ptr [bp+P_FRONT]
    cmp byte ptr [bp+P_HEAD+8],0
    jne BSS1HeadTwo
    mov al,[si+bx]
    shr al,cl
    jmp BSS1HeadValue
BSS1HeadTwo:
    mov ah,[si+bx]
    mov al,[si+bx+1]
    shl ax,cl
    mov al,ah
BSS1HeadValue:
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BSS1Interior:
    mov cl,byte ptr [bp+X_SPHASE]
    mov ah,[si]
    mov al,[si+1]
    mov bl,al
    rol ax,cl
    mov es:[di+0],ah
    cmp byte ptr [bp+P_TAIL+7],0
    je BSS1Next
    mov ah,bl
    xor al,al
    cmp byte ptr [bp+P_TAIL+8],0
    je BSS1TailLoaded
    mov al,[si+2]
BSS1TailLoaded:
    rol ax,cl
    mov al,ah
    mov ah,es:[di+1]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+1],al
BSS1Next:
    dec word ptr [bp+P_LEFT]
    jz BSS1Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BSS1Row
BSS1Done:
    ret
BSS2Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BSS2Interior
    mov bx,[bp+P_HDELTA]
    mov cl,byte ptr [bp+P_FRONT]
    cmp byte ptr [bp+P_HEAD+8],0
    jne BSS2HeadTwo
    mov al,[si+bx]
    shr al,cl
    jmp BSS2HeadValue
BSS2HeadTwo:
    mov ah,[si+bx]
    mov al,[si+bx+1]
    shl ax,cl
    mov al,ah
BSS2HeadValue:
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BSS2Interior:
    mov cl,byte ptr [bp+X_SPHASE]
    mov ah,[si]
    mov al,[si+1]
    mov bl,al
    rol ax,cl
    mov es:[di+0],ah
    mov ah,bl
    mov al,[si+2]
    mov bl,al
    rol ax,cl
    mov es:[di+1],ah
    cmp byte ptr [bp+P_TAIL+7],0
    je BSS2Next
    mov ah,bl
    xor al,al
    cmp byte ptr [bp+P_TAIL+8],0
    je BSS2TailLoaded
    mov al,[si+3]
BSS2TailLoaded:
    rol ax,cl
    mov al,ah
    mov ah,es:[di+2]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+2],al
BSS2Next:
    dec word ptr [bp+P_LEFT]
    jz BSS2Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BSS2Row
BSS2Done:
    ret
BSS3Row:
    cmp byte ptr [bp+P_HEAD+7],0
    je BSS3Interior
    mov bx,[bp+P_HDELTA]
    mov cl,byte ptr [bp+P_FRONT]
    cmp byte ptr [bp+P_HEAD+8],0
    jne BSS3HeadTwo
    mov al,[si+bx]
    shr al,cl
    jmp BSS3HeadValue
BSS3HeadTwo:
    mov ah,[si+bx]
    mov al,[si+bx+1]
    shl ax,cl
    mov al,ah
BSS3HeadValue:
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BSS3Interior:
    mov cl,byte ptr [bp+X_SPHASE]
    mov ah,[si]
    mov al,[si+1]
    mov bl,al
    rol ax,cl
    mov es:[di+0],ah
    mov ah,bl
    mov al,[si+2]
    mov bl,al
    rol ax,cl
    mov es:[di+1],ah
    mov ah,bl
    mov al,[si+3]
    mov bl,al
    rol ax,cl
    mov es:[di+2],ah
    cmp byte ptr [bp+P_TAIL+7],0
    je BSS3Next
    mov ah,bl
    xor al,al
    cmp byte ptr [bp+P_TAIL+8],0
    je BSS3TailLoaded
    mov al,[si+4]
BSS3TailLoaded:
    rol ax,cl
    mov al,ah
    mov ah,es:[di+3]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di+3],al
BSS3Next:
    dec word ptr [bp+P_LEFT]
    jz BSS3Done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_RAWD]
    add si,ss:[bx+P_RAWS]
    jmp BSS3Row
BSS3Done:
    ret
BPRSmallShiftRows endp

; Post-0.18 experiment: fused forward same-bit-phase AND/XOR. Entry has
; positive interior bytes, even row strides and all alias/access proofs.
; Same parity permits LODSW; mixed parity constructs words from two bytes.
; Only aligned destination words are read or written. Partial edges read
; exactly their one participating source byte, with no speculative lookahead.
public BPRLogicRows
BPRLogicRows proc near
    cld
    mov ax,di
    and ax,1
    mov [bp+P_FRONT],ax
    mov dx,[bp+F_RUN]
    sub dx,ax
    mov ax,dx
    and ax,1
    mov [bp+P_BACK],ax
    shr dx,1
    mov [bp+P_WORDS],dx
    mov ax,si
    xor ax,di
    test ax,1
    jnz BPLMixed
    cmp word ptr [bp+S_WORD],offset SW8
    jne BPLChooseXor
    jmp BPLAndEven
BPLChooseXor:
    jmp BPLXorEven
BPLMixed:
    cmp word ptr [bp+S_WORD],offset SW8
    jne BPLChooseMixedXor
    jmp BPLAndMixed
BPLChooseMixedXor:
    jmp BPLXorMixed
BPRLogicRows endp

; The four small bodies keep operation/parity branches outside row loops.
; F_RUN bytes are consumed; the existing prepared steps undo that movement.
BPLBODY macro opname, mixed
    local row, interior, words, wordloop, back, tail, nextrow, done
row:
    cmp byte ptr [bp+P_HEAD+7],0
    je interior
    mov al,[si-1]
    ifidn <opname>,<and>
        mov ah,byte ptr [bp+P_HEAD+6]
        not ah
        or al,ah
    else
        and al,byte ptr [bp+P_HEAD+6]
    endif
    opname es:[di-1],al
interior:
    cmp word ptr [bp+P_FRONT],0
    je words
    lodsb
    opname es:[di],al
    inc di
words:
    mov cx,[bp+P_WORDS]
    jcxz back
wordloop:
    if mixed
        mov al,[si]
        mov ah,[si+1]
        add si,2
    else
        lodsw
    endif
    opname es:[di],ax
    add di,2
    loop wordloop
back:
    cmp word ptr [bp+P_BACK],0
    je tail
    lodsb
    opname es:[di],al
    inc di
tail:
    cmp byte ptr [bp+P_TAIL+7],0
    je nextrow
    mov al,[si]
    ifidn <opname>,<and>
        mov ah,byte ptr [bp+P_TAIL+6]
        not ah
        or al,ah
    else
        and al,byte ptr [bp+P_TAIL+6]
    endif
    opname es:[di],al
nextrow:
    dec word ptr [bp+P_LEFT]
    jz done
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp row
done:
    ret
endm
public BPLAndEven, BPLXorEven, BPLAndMixed, BPLXorMixed
BPLAndEven proc near
    BPLBODY and,0
BPLAndEven endp
BPLXorEven proc near
    BPLBODY xor,0
BPLXorEven endp
BPLAndMixed proc near
    BPLBODY and,1
BPLAndMixed endp
BPLXorMixed proc near
    BPLBODY xor,1
BPLXorMixed endp
