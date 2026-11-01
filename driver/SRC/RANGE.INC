; 0.10: bounded one-envelope proof and prepared row traversal.
; No validation is cached beyond one synchronous BitBlt. Keep full descriptor
; matching, read/write rights, 24-bit physical bounds, and alias scheduling.
public BltRangePreflight, BltStepRow
BltRangePreflight proc near
    mov [bp+X_VIEW],ax
    or ax,ax
    jnz BRPSource
    mov bx,[bp+X_DK]
    lea si,[bp+X_DDESC]
    jmp BRPKind
BRPSource:
    mov bx,[bp+X_SK]
    lea si,[bp+X_SDESC]
BRPKind:
    or bx,bx
    jnz BRPScreen
    cmp word ptr ss:[si+22],0
    jne BRPDecline
    mov word ptr [bp+X_ROW],0
    call BltMapRow              ; complete dimensions/stride/base validation
    jc BRPDecline
    mov ax,[bp+S_ROW]
    mul word ptr [bp+X_H]
    or dx,dx
    jnz BRPDecline              ; 64K exact span retains the original proof
    mov cx,ax
    dec ax
    add ax,[bp+S_OFFSET]
    jc BRPDecline
    mov ax,[bp+S_SELECTOR]
    mov bx,[bp+S_OFFSET]
    mov di,1
    sub di,[bp+X_VIEW]
    call CheckPixelSpan        ; prove all rows before the cached-base lookup
    jc BRPDecline
    call BltNormalizeRow       ; validates physical end, reuses this call's base
    jc BRPDecline
    mov [bp+X_MIN],ax
    mov [bp+X_MIN+2],dx
    dec cx
    add ax,cx
    adc dx,0
    mov [bp+X_MAX],ax
    mov [bp+X_MAX+2],dx
    jmp BRPSave
BRPDecline:
    stc
    ret
BRPScreen:
    push word ptr [bp+X_DX]
    push word ptr [bp+X_DY]
    push word ptr [bp+X_DP]
    cmp word ptr [bp+X_VIEW],0
    je BRPScreenCheck
    mov ax,[bp+X_SX]
    mov [bp+X_DX],ax
    mov ax,[bp+X_SY]
    mov [bp+X_DY],ax
    mov ax,[bp+X_SP]
    mov [bp+X_DP],ax
BRPScreenCheck:
    call ScreenFillPreflight
    pop word ptr [bp+X_DP]
    pop word ptr [bp+X_DY]
    pop word ptr [bp+X_DX]
    jc BRPDecline
    mov ax,[bp+F_MIN]
    mov [bp+X_MIN],ax
    mov word ptr [bp+X_MIN+2],000ah
    mov ax,[bp+F_MAX]
    add ax,8079h
    mov [bp+X_MAX],ax
    mov word ptr [bp+X_MAX+2],000bh
BRPSave:
    mov si,X_DMIN
    mov di,X_DMAX
    mov bx,A_DORDER
    cmp word ptr [bp+X_VIEW],0
    je BRPStore
    mov si,X_SMIN
    mov di,X_SMAX
    mov bx,A_SORDER
BRPStore:
    push si
    mov si,bx
    mov word ptr [bp+si],1
    pop si
    mov ax,[bp+X_MIN]
    mov [bp+si],ax
    mov ax,[bp+X_MIN+2]
    mov [bp+si+2],ax
    mov ax,[bp+X_MAX]
    mov [bp+di],ax
    mov ax,[bp+X_MAX+2]
    mov [bp+di+2],ax
    clc
    ret
BltRangePreflight endp

; The current pointers already have the validated selectors and plane bases.
; AX view. Huge and irregular aliases never enter. The final unused increment
; may wrap the offset but is never dereferenced after the last clipped row.
BltStepRow proc near
    or ax,ax
    jnz BSRSource
    lea si,[bp+X_DPTR]
    mov cx,[bp+X_DP]
    mov ax,[bp+X_DY]
    mov dx,[bp+X_DDESC+6]
    cmp word ptr [bp+X_DK],0
    je BSRMemory
    jmp BSRScreen
BSRSource:
    lea si,[bp+X_SPTR]
    mov cx,[bp+X_SP]
    mov ax,[bp+X_SY]
    mov dx,[bp+X_SDESC+6]
    cmp word ptr [bp+X_SK],0
    jne BSRScreen
BSRMemory:
    mov ax,dx
    mul cx
    mov dx,ax
    cmp word ptr [bp+X_YDIR],1
    je BSRPointers
    neg dx
    jmp BSRPointers
BSRScreen:
    add ax,[bp+X_ROW]
    and ax,3
    cmp word ptr [bp+X_YDIR],1
    jne BSRBack
    mov dx,2000h
    cmp ax,3
    jne BSRPointers
    mov dx,0a050h
    jmp BSRPointers
BSRBack:
    mov dx,0e000h
    or ax,ax
    jnz BSRPointers
    mov dx,5fb0h
BSRPointers:
    add ss:[si],dx
    add si,4
    loop BSRPointers
    ret
BltStepRow endp
