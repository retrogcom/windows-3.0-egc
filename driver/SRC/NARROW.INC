; 0.10 single-byte-wide screen columns. Only the source-free, preflighted
; screen fill calls this helper. The saved descriptors are dead after claiming
; this path: use their existing 72 bytes for 8 phases * 4 planes * 2 coefficients.
; No extra public stack reservation or shared mutable coefficient table.
N_COEFF equ X_SDESC
public ScreenNarrowDraw
ScreenNarrowDraw proc near
    cmp word ptr [bp+X_H],8
    jb SNDOriginal
    test word ptr [bp+X_USE],4
    jz SNDConstant
    ; Prove vertical period one from our private brush snapshot. Repeated
    ; non-solid horizontal bits are also valid; ScreenFillPattern applies
    ; the original horizontal rotation and monochrome colour expansion.
    lea si,[bp+T_BRUSH]
    mov cx,4
    cmp word ptr [bp+T_BRUSH+48],4
    jne SNDCheckPlane
    add si,32
    mov cx,1
SNDCheckPlane:
    mov al,ss:[si]
    mov ah,al
    cmp ax,ss:[si]
    jne SNDOriginal
    cmp ax,ss:[si+2]
    jne SNDOriginal
    cmp ax,ss:[si+4]
    jne SNDOriginal
    cmp ax,ss:[si+6]
    jne SNDOriginal
    add si,8
    loop SNDCheckPlane
    mov word ptr [bp+F_PHASE],0
    call ScreenFillPattern
    xor si,si
    xor di,di
SNDConstantPlane:
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov al,byte ptr [bp+B_FIRST]
    and al,byte ptr [bp+B_LAST]
    and dl,al
    not al
    or dh,al
    mov [bp+X_SDESC+16+si],dx
    add si,2
    inc di
    cmp di,4
    jb SNDConstantPlane
SNDConstant:
    jmp ScreenNarrowConstant
SNDOriginal:
    mov ax,[bp+X_H]
    cmp ax,8
    jbe SNDPhaseCount
    mov ax,8
SNDPhaseCount:
    test word ptr [bp+X_USE],4
    jnz SNDKeepPeriod
    mov ax,1
SNDKeepPeriod:
    mov [bp+F_SCAN],ax
    mov word ptr [bp+F_PHASE],0
SNDCompile:
    call ScreenFillPattern
    mov si,[bp+F_PHASE]
    shl si,3
    xor di,di
SNDCompilePlane:
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov al,byte ptr [bp+B_FIRST]
    and al,byte ptr [bp+B_LAST]
    and dl,al
    not al
    or dh,al
    mov [bp+N_COEFF+si],dx
    add si,2
    inc di
    cmp di,4
    jb SNDCompilePlane
    inc word ptr [bp+F_PHASE]
    mov ax,[bp+F_PHASE]
    cmp ax,[bp+F_SCAN]
    jb SNDCompile
    mov ax,[bp+X_DY]
    call ScreenFillOffset
    add ax,[bp+F_XBYTE]
    mov [bp+F_PTR],ax
    mov word ptr [bp+F_PHASE],0
    mov ax,[bp+X_DY]
    mov [bp+F_ENDY],ax
    mov ax,[bp+X_H]
    mov [bp+F_REMAIN],ax
SNDRow:
    mov si,[bp+F_PHASE]
    shl si,3
    xor di,di
SNDPlane:
    mov es,PlaneSelectors[di]
    mov bx,PlaneBases[di]
    add bx,[bp+F_PTR]
    mov dx,[bp+N_COEFF+si]
    mov al,dl
    or dh,dh
    jz SNDStore
    mov ah,es:[bx]
    and ah,dh
    xor al,ah
SNDStore:
    mov es:[bx],al
    add si,2
    add di,2
    cmp di,8
    jb SNDPlane
    mov ax,[bp+F_ENDY]
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne SNDStep
    mov dx,0a050h
SNDStep:
    add [bp+F_PTR],dx
    inc word ptr [bp+F_ENDY]
    inc word ptr [bp+F_PHASE]
    mov ax,[bp+F_PHASE]
    cmp ax,[bp+F_SCAN]
    jb SNDPhaseReady
    mov word ptr [bp+F_PHASE],0
SNDPhaseReady:
    dec word ptr [bp+F_REMAIN]
    jnz SNDRow
    ret
ScreenNarrowDraw endp

; 0.11 source-free/P-free columns: one byte wide, at least eight rows.
; Four physical bank streams per plane, +80 bytes per repeated bank row.
; The complete screen preflight and drawing exclusion already succeeded.
; Dead descriptor space holds four {offset,count} pairs; no frame growth.
NC_ROWS equ X_SDESC
public ScreenNarrowConstant
ScreenNarrowConstant proc near
    test word ptr [bp+X_USE],4
    jnz SNCPrepareStart
    mov dx,[bp+F_ROPCONST]
    mov al,byte ptr [bp+B_FIRST]
    and al,byte ptr [bp+B_LAST]
    and dl,al
    not al
    or dh,al
    mov [bp+F_ROPCONST],dx
SNCPrepareStart:
    ; 0.18: map the first row once. Subsequent banks advance by 2000h;
    ; bank three wraps with a net A050h delta (+80 into bank zero).
    ; H >= 8 and full preflight guarantee all four streams are nonempty.
    mov ax,[bp+X_DY]
    call ScreenFillOffset
    add ax,[bp+F_XBYTE]
    mov dx,ax
    mov cx,[bp+X_H]
    add cx,3
    mov bx,[bp+X_DY]
    and bx,3
    xor si,si
SNCPrepare:
    mov [bp+NC_ROWS+si],dx
    mov ax,cx
    shr ax,2
    mov [bp+NC_ROWS+si+2],ax
    add si,4
    cmp si,16
    je SNCPrepared
    dec cx
    add dx,2000h
    inc bx
    cmp bx,4
    jb SNCPrepare
    xor bx,bx
    add dx,8050h
    jmp SNCPrepare
SNCPrepared:
    mov word ptr [bp+F_PHASE],4
    mov dx,[bp+F_ROPCONST]
    xor bx,bx
SNCPlane:
    test word ptr [bp+X_USE],4
    jz SNCPlaneReady
    mov si,bx
    mov dx,[bp+X_SDESC+16+si]
SNCPlaneReady:
    ; Select once per plane, shared by all four banks. F_RUN is dead in
    ; this source-free column path; per-call scratch preserves reentrancy.
    cmp word ptr [bp+X_H],32
    jb SNCShortSelect
    mov ax,offset SNCConstantFour
    or dh,dh
    jz SNCSelected
    mov ax,offset SNCAndRowFour
    or dl,dl
    jz SNCSelected
    mov ax,offset SNCXorRowFour
    cmp dh,0ffh
    je SNCSelected
    mov al,dh
    not al
    cmp al,dl
    mov ax,offset SNCOrRowFour
    je SNCSelected
    mov ax,offset SNCGenericRow
    jmp SNCSelected
SNCShortSelect:
    mov ax,offset SNCConstant
    or dh,dh
    jz SNCSelected
    mov ax,offset SNCAndRow
    or dl,dl
    jz SNCSelected
    mov ax,offset SNCXorRow
    cmp dh,0ffh
    je SNCSelected
    mov al,dh
    not al
    cmp al,dl
    mov ax,offset SNCOrRow
    je SNCSelected
    mov ax,offset SNCGenericRow
SNCSelected:
    mov [bp+F_RUN],ax
    mov es,PlaneSelectors[bx]
    xor si,si
SNCBank:
    mov di,[bp+NC_ROWS+si]
    add di,PlaneBases[bx]
    mov cx,[bp+NC_ROWS+si+2]
    jmp word ptr [bp+F_RUN]
SNCConstant:
    mov es:[di],dl
    add di,80
    loop SNCConstant
    jmp SNCNext
SNCGenericRow:
    mov al,es:[di]
    and al,dh
    xor al,dl
    mov es:[di],al
    add di,80
    loop SNCGenericRow
    jmp SNCNext
SNCAndRow:
    and es:[di],dh
    add di,80
    loop SNCAndRow
    jmp SNCNext
SNCXorRow:
    xor es:[di],dl
    add di,80
    loop SNCXorRow
    jmp SNCNext
SNCOrRow:
    or es:[di],dl
    add di,80
    loop SNCOrRow
SNCNext:
    add si,4
    cmp si,16
    jae SNCBankDone
    jmp SNCBank
SNCBankDone:
    add bx,2
    cmp bx,8
    jae SNCFinished
    jmp SNCPlane
SNCFinished:
    ret

; 0.17 long columns: select once per plane, then four exact bank rows.
SNCConstantFour:
    mov ax,cx
    and ax,3
    shr cx,2
SNCConstantFourLoop:
    mov es:[di],dl
    mov es:[di+80],dl
    mov es:[di+160],dl
    mov es:[di+240],dl
    add di,320
    loop SNCConstantFourLoop
    mov cx,ax
    jcxz SNCConstantFourDone
    jmp SNCConstant
SNCConstantFourDone:
    jmp SNCNext
SNCAndRowFour:
    mov ax,cx
    and ax,3
    shr cx,2
SNCAndRowFourLoop:
    and es:[di],dh
    and es:[di+80],dh
    and es:[di+160],dh
    and es:[di+240],dh
    add di,320
    loop SNCAndRowFourLoop
    mov cx,ax
    jcxz SNCAndRowFourDone
    jmp SNCAndRow
SNCAndRowFourDone:
    jmp SNCNext
SNCXorRowFour:
    mov ax,cx
    and ax,3
    shr cx,2
SNCXorRowFourLoop:
    xor es:[di],dl
    xor es:[di+80],dl
    xor es:[di+160],dl
    xor es:[di+240],dl
    add di,320
    loop SNCXorRowFourLoop
    mov cx,ax
    jcxz SNCXorRowFourDone
    jmp SNCXorRow
SNCXorRowFourDone:
    jmp SNCNext
SNCOrRowFour:
    mov ax,cx
    and ax,3
    shr cx,2
SNCOrRowFourLoop:
    or es:[di],dl
    or es:[di+80],dl
    or es:[di+160],dl
    or es:[di+240],dl
    add di,320
    loop SNCOrRowFourLoop
    mov cx,ax
    jcxz SNCOrRowFourDone
    jmp SNCOrRow
SNCOrRowFourDone:
    jmp SNCNext
ScreenNarrowConstant endp
