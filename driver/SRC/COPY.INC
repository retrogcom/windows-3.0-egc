; Phase 8 SRCCOPY row fast path. Caller has preflighted all rows and selected
; X_MODE=0 (disjoint storage or a proved equal physical layout), equal plane
; counts. Matching phases use MOVSB; shifted phases use a rolling window.
; No conversion or general
; mono alias pass enters here. Logical row direction is already established.
;
; In that envelope, corresponding planes are independent: disjoint storage
; has no source dependencies, and equal layouts preserve plane identity and
; exclude aliases between different rows/planes in the union. Whole-byte runs
; can therefore copy one plane at a time in the selected horizontal direction.
; Partial edges retain BltTile's all-plane source staging and masked writes.
public BltCopyRow, BltCopyRun

BltCopyRow proc near
    mov word ptr [bp+B_PAT],0
    mov byte ptr [bp+B_PAT+2],0
    mov ax,[bp+X_W]
    mov [bp+X_LEFT],ax
    mov word ptr [bp+X_POS],0
    cmp word ptr [bp+X_XDIR],1
    je @F
    jmp CRReverse
@@:
    mov ax,[bp+X_DX]
    and ax,7
    jnz @F
    jmp CRForwardRun
@@:
    mov cx,8
    sub cx,ax
    cmp cx,[bp+X_LEFT]
    ja @F
    jmp CRForwardEdge
@@:
    mov cx,[bp+X_LEFT]
CRForwardEdge:
    mov [bp+X_COUNT],cx
    call BltTile
    mov ax,[bp+X_COUNT]
    add [bp+X_POS],ax
    sub [bp+X_LEFT],ax
CRForwardRun:
    mov ax,[bp+X_LEFT]
    mov cl,3
    shr ax,cl
    mov [bp+F_RUN],ax
    or ax,ax
    jnz @F
    jmp CRForwardTail
@@:
    call BltCopyRun
    mov ax,[bp+F_RUN]
    mov cl,3
    shl ax,cl
    add [bp+X_POS],ax
    sub [bp+X_LEFT],ax
CRForwardTail:
    mov ax,[bp+X_LEFT]
    or ax,ax
    jnz @F
    jmp CRDone
@@:
    mov [bp+X_COUNT],ax
    call BltTile
    jmp CRDone
CRReverse:
    mov ax,[bp+X_DX]
    add ax,[bp+X_W]
    and ax,7
    jnz @F
    jmp CRReverseRun
@@:
    cmp ax,[bp+X_LEFT]
    ja @F
    jmp CRReverseEdge
@@:
    mov ax,[bp+X_LEFT]
CRReverseEdge:
    mov [bp+X_COUNT],ax
    sub [bp+X_LEFT],ax
    mov ax,[bp+X_LEFT]
    mov [bp+X_POS],ax
    call BltTile
CRReverseRun:
    mov ax,[bp+X_LEFT]
    mov cl,3
    shr ax,cl
    mov [bp+F_RUN],ax
    shl ax,cl
    sub [bp+X_LEFT],ax
    mov ax,[bp+X_LEFT]
    mov [bp+X_POS],ax
    cmp word ptr [bp+F_RUN],0
    jne @F
    jmp CRReverseHead
@@:
    call BltCopyRun
CRReverseHead:
    mov ax,[bp+X_LEFT]
    or ax,ax
    jnz @F
    jmp CRDone
@@:
    mov [bp+X_COUNT],ax
    mov word ptr [bp+X_POS],0
    call BltTile
CRDone:
    cld
    ret
BltCopyRow endp

; F_RUN full bytes beginning at logical relative X_POS. Save driver DS around
; actual MOVSB source reads. DF is cleared before returning to any caller.
BltCopyRun proc near
    cmp word ptr [bp+X_ROP],0cch
    jne BCRSource
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    je BCROriginal
BCRSource:
    jmp BltSourceRun
BCROriginal:
    push ds
    mov ax,[bp+X_POS]
    add ax,[bp+X_SX]
    mov dx,ax
    and dx,7
    mov cl,dl
    mov dh,1
    shl dh,cl
    dec dh                  ; 0.13: high byte = (1 shl phase) - 1
    mov [bp+X_SPHASE],dx
    mov cl,3
    shr ax,cl
    mov [bp+F_SRC],ax
    mov ax,[bp+X_POS]
    add ax,[bp+X_DX]
    shr ax,cl
    mov [bp+F_DST],ax
    xor bx,bx
CRPlane:
    mov si,bx
    mov dx,[bp+X_SPTR+si+2]
    mov ax,[bp+X_DPTR+si+2]
    mov di,[bp+X_DPTR+si]
    mov si,[bp+X_SPTR+si]
    add si,[bp+F_SRC]
    add di,[bp+F_DST]
    mov ds,dx
    mov es,ax
    mov cx,[bp+F_RUN]
    cmp word ptr [bp+X_SPHASE],0
    je CRAlignedPlane
    call BltShiftPlane
    jmp CRNextPlane
CRAlignedPlane:
    call BltCopyAlignedPlane
    jmp CRNextPlane
CRNextPlane:
    add bx,4
    mov ax,bx
    shr ax,1
    shr ax,1
    cmp ax,[bp+X_DP]
    jae @F
    jmp CRPlane
@@:
    cld
    pop ds
    ret
BltCopyRun endp

; Full destination bytes with source phase 1..7, same proved layout envelope.
; DS:SI source left byte, ES:DI destination, CX byte count. Preserve BX for
; the outer plane iterator. Read each participating source byte exactly once.
; Rolling windows are loaded before the corresponding overlapping write.
public BltShiftPlane
BltShiftPlane proc near
    cmp byte ptr [bp+X_SPHASE],1
    jne CSDispatchSeven
    cmp word ptr [bp+X_XDIR],1
    je CSDispatchGeneral
    jmp BltShiftOnePhase
CSDispatchSeven:
    cmp byte ptr [bp+X_SPHASE],7
    jne CSDispatchGeneral
    cmp word ptr [bp+X_XDIR],1
    je CSDispatchGeneral
    jmp BltShiftSevenPhase
CSDispatchGeneral:
    cld                     ; Windows may enter with DF set
    push bx
    push bp
    mov dx,cx
    mov cx,[bp+X_SPHASE]
    cmp word ptr [bp+X_XDIR],1
    je @F
    jmp CSBackward
@@:
    ; 0.13: one ROL per word. BL holds the previous source byte already
    ; shifted left by CL; CH holds the low-bit mask (1 shl CL) - 1.
    mov bp,dx
    mov bl,[si]
    inc si
    shl bl,cl               ; BL = prev shl CL; CH = low-bit mask from X_SPHASE
    test di,1
    jz CSForwardCount
    call CSForwardOne
    dec bp
    jnz CSForwardCount
    jmp CSDoneShift
CSForwardCount:
    mov dh,0
    shr bp,1
    adc dh,0
    or bp,bp
    jz CSForwardTail
    test si,1
    jnz CSForwardBytes
CSForwardWords:
    lodsw
    xchg al,ah
    rol ax,cl
    mov dl,al
    xor dl,bl
    and dl,ch
    xor dl,bl
    mov bl,al
    mov al,dl
    stosw
    dec bp
    jnz CSForwardWords
    jmp CSForwardTail
CSForwardBytes:
    mov ah,[si]
    mov al,[si+1]
    add si,2
    rol ax,cl
    mov dl,al
    xor dl,bl
    and dl,ch
    xor dl,bl
    mov bl,al
    mov al,dl
    stosw
    dec bp
    jnz CSForwardBytes
CSForwardTail:
    or dh,dh
    jnz @F
    jmp CSDoneShift
@@:
    call CSForwardOne
    jmp CSDoneShift
CSForwardOne:
    mov ah,[si]
    xor al,al
    rol ax,cl
    xor al,bl
    and al,ch
    xor al,bl
    mov es:[di],al
    mov bl,ah
    inc si
    inc di
    ret
CSBackward:
    mov bp,dx
    add si,dx
    add di,dx
    dec si
    dec di
    xor bh,bh
    mov bl,[si+1]
    rol bx,cl
    mov bl,bh                  ; low carry from the next source byte
    test di,1
    jnz CSBackwardCount
    call CSBackwardOne
    dec bp
    jz CSDoneShift
CSBackwardCount:
    mov dh,0
    shr bp,1
    adc dh,0
    or bp,bp
    jz CSBackwardTail
    test si,1
    jz CSBackwardBytes
CSBackwardWords:
    mov ax,[si-1]
    xchg al,ah
    rol ax,cl
    mov dl,al
    xor al,bl
    and al,ch
    xor al,dl
    mov bl,dl
    xchg al,ah
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CSBackwardWords
    jmp CSBackwardTail
CSBackwardBytes:
    mov ah,[si-1]
    mov al,[si]
    rol ax,cl
    mov dl,al
    xor al,bl
    and al,ch
    xor al,dl
    mov bl,dl
    xchg al,ah
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CSBackwardBytes
CSBackwardTail:
    or dh,dh
    jz CSDoneShift
    call CSBackwardOne
CSDoneShift:
    cld
    pop bp
    pop bx
    ret
CSBackwardOne:
    mov ah,[si]
    xor al,al
    rol ax,cl
    xchg al,ah
    mov dl,al
    xor al,bl
    and al,ch
    xor al,dl
    mov bl,ah
    mov es:[di],al
    dec si
    dec di
    ret
BltShiftPlane endp

; Replay only after complete preflight + alias scheduling, for X_MODE=0
; equal-format SRCCOPY. No normalized address is consumed by that renderer.
; Stable snapshotted ordinary bitmaps and fixed screen mappings need no
; second descriptor copy/rights check per row. Huge bitmaps retain BltMapRow.
; AX view (0 destination, 1 source); updates only that view's row pointers.
public BltCopyMapRow
BltCopyMapRow proc near
    or ax,ax
    jnz BCMSource
    lea si,[bp+X_DDESC]
    lea di,[bp+X_DPTR]
    mov bx,[bp+X_DY]
    mov cx,[bp+X_DP]
    cmp word ptr [bp+X_DK],0
    jne BCMScreen
    jmp BCMMemory
BCMSource:
    lea si,[bp+X_SDESC]
    lea di,[bp+X_SPTR]
    mov bx,[bp+X_SY]
    mov cx,[bp+X_SP]
    cmp word ptr [bp+X_SK],0
    jne BCMScreen
BCMMemory:
    cmp word ptr ss:[si+22],0
    je BCMOrdinary
    jmp BltMapRow
BCMOrdinary:
    mov ax,bx
    add ax,[bp+X_ROW]
    mul cx
    mul word ptr ss:[si+6]
    add ax,ss:[si+10]
    mov bx,ss:[si+12]
BCMMemoryPlane:
    mov ss:[di],ax
    mov ss:[di+2],bx
    add ax,ss:[si+6]
    add di,4
    loop BCMMemoryPlane
    clc
    ret
BCMScreen:
    mov ax,bx
    add ax,[bp+X_ROW]
    call ScreenFillOffset
    xor si,si
BCMScreenPlane:
    mov bx,PlaneBases[si]
    add bx,ax
    mov ss:[di],bx
    mov bx,PlaneSelectors[si]
    mov ss:[di+2],bx
    add di,4
    add si,2
    cmp si,8
    jb BCMScreenPlane
    clc
    ret
BltCopyMapRow endp

public BltCopyAlignedPlane
BltCopyAlignedPlane proc near
    cmp word ptr [bp+X_XDIR],1
    je @F
    jmp CRBackward
@@:
    cld
    mov ax,si
    xor ax,di
    test ax,1
    jnz CRForwardBytes
    test di,1
    jz CRForwardWords
    movsb
    dec cx
CRForwardWords:
    shr cx,1
    rep movsw
    jnc BCPDone
    movsb
    jmp BCPDone
CRForwardBytes:
    rep movsb
    jmp BCPDone
CRBackward:
    add si,cx
    add di,cx
    dec si
    dec di
    std
    mov ax,si
    xor ax,di
    test ax,1
    jnz CRBackwardBytes
    test di,1
    jnz CRBackwardWords
    movsb
    dec cx
CRBackwardWords:
    dec si
    dec di
    shr cx,1
    rep movsw
    jnc BCPDone
    inc si
    inc di
    movsb
    jmp BCPDone
CRBackwardBytes:
    rep movsb
BCPDone:
    cld
    ret
BltCopyAlignedPlane endp

; Fixed backward one-bit shift. Forward calls use the general/fused loop.
public BltShiftOnePhase
BltShiftOnePhase proc near
    cld                     ; Windows may enter with DF set
    push bx
    push bp
    mov dx,cx
    xor cx,cx
    ; 0.14: only backward calls reach this specialization.
CS1Backward:
    mov bp,dx
    add si,dx
    add di,dx
    dec si
    dec di
    mov bl,[si+1]
    test di,1
    jnz CS1BackwardCount
    call CS1BackwardOne
    dec bp
    jz CS1DoneShift
CS1BackwardCount:
    mov ch,0
    shr bp,1
    adc ch,0
    or bp,bp
    jz CS1BackwardTail
    test si,1
    jz CS1BackwardBytes
CS1BackwardWords:
    mov dx,[si-1]
    mov ah,dh
    mov al,bl
    shl ax,1
    mov bh,ah
    mov ah,dl
    mov al,dh
    mov bl,dl
    shl ax,1
    mov al,ah
    mov ah,bh
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CS1BackwardWords
    jmp CS1BackwardTail
CS1BackwardBytes:
    mov dl,[si-1]
    mov dh,[si]
    mov ah,dh
    mov al,bl
    shl ax,1
    mov bh,ah
    mov ah,dl
    mov al,dh
    mov bl,dl
    shl ax,1
    mov al,ah
    mov ah,bh
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CS1BackwardBytes
CS1BackwardTail:
    or ch,ch
    jz CS1DoneShift
    call CS1BackwardOne
CS1DoneShift:
    cld
    pop bp
    pop bx
    ret
CS1BackwardOne:
    mov ah,[si]
    mov al,bl
    mov bl,ah
    shl ax,1
    mov es:[di],ah
    dec si
    dec di
    ret
BltShiftOnePhase endp

; Fixed backward one-bit shift. Forward calls use the general/fused loop.
public BltShiftSevenPhase
BltShiftSevenPhase proc near
    cld                     ; Windows may enter with DF set
    push bx
    push bp
    mov dx,cx
    xor cx,cx
    ; 0.14: only backward calls reach this specialization.
CS7Backward:
    mov bp,dx
    add si,dx
    add di,dx
    dec si
    dec di
    mov bl,[si+1]
    test di,1
    jnz CS7BackwardCount
    call CS7BackwardOne
    dec bp
    jz CS7DoneShift
CS7BackwardCount:
    mov ch,0
    shr bp,1
    adc ch,0
    or bp,bp
    jz CS7BackwardTail
    test si,1
    jz CS7BackwardBytes
CS7BackwardWords:
    mov dx,[si-1]
    mov ah,dh
    mov al,bl
    shr ax,1
    mov bh,al
    mov ah,dl
    mov al,dh
    mov bl,dl
    shr ax,1
    mov ah,bh
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CS7BackwardWords
    jmp CS7BackwardTail
CS7BackwardBytes:
    mov dl,[si-1]
    mov dh,[si]
    mov ah,dh
    mov al,bl
    shr ax,1
    mov bh,al
    mov ah,dl
    mov al,dh
    mov bl,dl
    shr ax,1
    mov ah,bh
    mov es:[di-1],ax
    sub si,2
    sub di,2
    dec bp
    jnz CS7BackwardBytes
CS7BackwardTail:
    or ch,ch
    jz CS7DoneShift
    call CS7BackwardOne
CS7DoneShift:
    cld
    pop bp
    pop bx
    ret
CS7BackwardOne:
    mov ah,[si]
    mov al,bl
    mov bl,ah
    shr ax,1
    mov es:[di],al
    dec si
    dec di
    ret
BltShiftSevenPhase endp
