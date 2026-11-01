; Phase 7 fixed-space mapping and dependency traversal. 286 instructions only.
; All local state is in BlitRectangle's checked invocation frame.
; Inverse scans segment groups, not every preceding pixel/row. It never reads
; bitmap bits. Ordinary data selectors and complete rows were preflighted.
public AliasPosition, AliasMap, AliasInverse, AliasUnique, AliasGeneral
public AliasDraw, AliasStep

; A_INDEX unsigned 32-bit operation index, strictly below X_W*X_H.
; DIV is safe: quotient is a clipped signed-word row, at most 32766.
AliasPosition proc near
    mov ax,[bp+A_INDEX]
    mov dx,[bp+A_INDEX+2]
    div word ptr [bp+X_W]
    mov [bp+X_ROW],ax
    mov [bp+X_POS],dx
    mov word ptr [bp+X_COUNT],1
    ret
AliasPosition endp

; AX view 0=D/1=S, A_INDEX -> DX:AX physical bit identity. CF invalid.
; Retains the validated row pointers in X_DPTR/X_SPTR, never fetches pixels.
AliasMap proc near
    mov [bp+A_MVIEW],ax
    call AliasPosition
    mov ax,[bp+A_MVIEW]
    call BltMapRow
    jnc @F
    jmp AMReturn
@@:
    mov bx,[bp+X_DX]
    cmp word ptr [bp+A_MVIEW],0
    jne @F
    jmp AMColumn
@@:
    mov bx,[bp+X_SX]
AMColumn:
    add bx,[bp+X_POS]
    mov si,bx
    and si,7
    mov cl,3
    shr bx,cl
    mov ax,[bp+X_START]
    mov dx,[bp+X_START+2]
    add ax,bx
    adc dx,0
    shl ax,1
    rcl dx,1
    shl ax,1
    rcl dx,1
    shl ax,1
    rcl dx,1
    add ax,si
    adc dx,0
    clc
AMReturn:
    ret
AliasMap endp

; After BltMapRow: CX = remaining clipped rows in this physical segment.
; All divisions/selector calculations are covered by the row preflight.
AliasGroup proc near
    mov cx,[bp+X_H]
    sub cx,[bp+X_ROW]
    cmp word ptr [bp+S_DESC+22],0
    jne @F
    jmp AGReturn
@@:
    mov ax,[bp+S_Y]
    xor dx,dx
    div word ptr [bp+S_DESC+24]
    mov ax,[bp+S_DESC+24]
    sub ax,dx
    cmp cx,ax
    ja @F
    jmp AGReturn
@@:
    mov cx,ax
AGReturn:
    ret
AliasGroup endp

; BX view, DX:AX physical bit identity -> CX status:
; 0 absent, 1 unique (DX:AX = 32-bit operation index), 2 repeated bit,
; 3 invalid mapping. Inputs/caller indices are not reused as iterator storage.
; Clipped x/y, padding, segment fill, partial first/last groups are excluded.
AliasInverse proc near
    mov [bp+A_IVIEW],bx
    mov bx,ax
    and bx,7
    mov [bp+A_QBIT],bx
    shr dx,1
    rcr ax,1
    shr dx,1
    rcr ax,1
    shr dx,1
    rcr ax,1
    mov [bp+A_QBYTE],ax
    mov [bp+A_QBYTE+2],dx
    mov word ptr [bp+A_SCAN],0
    mov word ptr [bp+A_FOUND],0
AILoop:
    mov ax,[bp+A_SCAN]
    mov [bp+X_ROW],ax
    mov ax,[bp+A_IVIEW]
    call BltMapRow
    jnc @F
    jmp AIInvalid
@@:
    call AliasGroup
    mov [bp+A_GROUP],cx
    mov ax,[bp+A_QBYTE]
    mov dx,[bp+A_QBYTE+2]
    sub ax,[bp+X_START]
    sbb dx,[bp+X_START+2]
    jnc @F
    jmp AINext
@@:
    or dx,dx
    jnz AINext                   ; one segment is at most 64 KiB
    div word ptr [bp+S_STRIDE]
    cmp ax,[bp+A_GROUP]
    jb @F
    jmp AINext
@@:
    mov si,ax
    cmp dx,4096                 ; larger remainders are row padding
    jb @F
    jmp AINext
@@:
    shl dx,1
    shl dx,1
    shl dx,1
    add dx,[bp+A_QBIT]
    mov bx,[bp+X_DX]
    cmp word ptr [bp+A_IVIEW],0
    jne @F
    jmp AIColumn
@@:
    mov bx,[bp+X_SX]
AIColumn:
    sub dx,bx
    jnc @F
    jmp AINext
@@:
    cmp dx,[bp+X_W]
    jb @F
    jmp AINext
@@:
    cmp word ptr [bp+A_FOUND],0
    je @F
    jmp AIDuplicate
@@:
    mov bx,dx
    mov ax,si
    add ax,[bp+A_SCAN]
    mul word ptr [bp+X_W]
    add ax,bx
    adc dx,0
    mov [bp+A_RET],ax
    mov [bp+A_RET+2],dx
    mov word ptr [bp+A_FOUND],1
AINext:
    mov ax,[bp+A_GROUP]
    add [bp+A_SCAN],ax
    mov ax,[bp+A_SCAN]
    cmp ax,[bp+X_H]
    jae @F
    jmp AILoop
@@:
    mov cx,[bp+A_FOUND]
    mov ax,[bp+A_RET]
    mov dx,[bp+A_RET+2]
    ret
AIDuplicate:
    mov cx,2
    ret
AIInvalid:
    mov cx,3
    ret
AliasInverse endp

; A_UVIEW, BX first relative row -> complete segment-group envelope and next.
AliasEnvelope proc near
    mov [bp+X_ROW],bx
    mov ax,[bp+A_UVIEW]
    call BltMapRow
    jnc @F
    jmp AEReturn
@@:
    call AliasGroup
    add cx,[bp+X_ROW]
    mov [bp+A_ENEXT],cx
    mov ax,[bp+X_START]
    mov [bp+A_EFIRST],ax
    mov ax,[bp+X_START+2]
    mov [bp+A_EFIRST+2],ax
    dec cx
    mov [bp+X_ROW],cx
    mov ax,[bp+A_UVIEW]
    call BltMapRow
    jnc @F
    jmp AEReturn
@@:
    mov ax,[bp+X_END]
    mov [bp+A_ELAST],ax
    mov ax,[bp+X_END+2]
    mov [bp+A_ELAST+2],ax
    clc
AEReturn:
    ret
AliasEnvelope endp

; AX view -> CF clear injective, CF set AX=0 invalid/FFFF unproved/repeated.
; Increasing full rows are already proved by BltPreflight. For other views,
; compare segment-group envelopes. Overlapping envelopes are NOT sufficient
; reason to reject mono: exact inverse(map(i)) checks follow for those cases.
AliasUnique proc near
    mov [bp+A_UVIEW],ax
    mov bx,A_DORDER
    mov si,X_DK
    or ax,ax
    jnz @F
    jmp AUOrder
@@:
    mov bx,A_SORDER
    mov si,X_SK
AUOrder:
    cmp word ptr [bp+si],1
    jne @F
    jmp AUOK
@@:
    mov si,bx
    cmp word ptr [bp+si],1
    jne @F
    jmp AUOK
@@:
    mov word ptr [bp+A_OUTNEXT],0
AUOuter:
    mov bx,[bp+A_OUTNEXT]
    mov [bp+A_OUTROW],bx
    call AliasEnvelope
    jnc @F
    jmp AUInvalid
@@:
    mov ax,[bp+A_ENEXT]
    mov [bp+A_OUTNEXT],ax
    mov ax,[bp+A_EFIRST]
    mov [bp+A_OFIRST],ax
    mov ax,[bp+A_EFIRST+2]
    mov [bp+A_OFIRST+2],ax
    mov ax,[bp+A_ELAST]
    mov [bp+A_OLAST],ax
    mov ax,[bp+A_ELAST+2]
    mov [bp+A_OLAST+2],ax
    mov word ptr [bp+A_INROW],0
AUInner:
    mov bx,[bp+A_INROW]
    cmp bx,[bp+A_OUTROW]
    jb @F
    jmp AUAdvance
@@:
    call AliasEnvelope
    jnc @F
    jmp AUInvalid
@@:
    ; Earlier group's end < outer start OR outer end < earlier start.
    mov ax,[bp+A_ELAST+2]
    cmp ax,[bp+A_OFIRST+2]
    jae @F
    jmp AUNextInner
@@:
    jbe @F
    jmp AUOtherOrder
@@:
    mov ax,[bp+A_ELAST]
    cmp ax,[bp+A_OFIRST]
    jae @F
    jmp AUNextInner
@@:
AUOtherOrder:
    mov ax,[bp+A_OLAST+2]
    cmp ax,[bp+A_EFIRST+2]
    jae @F
    jmp AUNextInner
@@:
    jbe @F
    jmp AUExact
@@:
    mov ax,[bp+A_OLAST]
    cmp ax,[bp+A_EFIRST]
    jb @F
    jmp AUExact
@@:
AUNextInner:
    mov ax,[bp+A_ENEXT]
    mov [bp+A_INROW],ax
    jmp AUInner
AUAdvance:
    mov ax,[bp+A_OUTNEXT]
    cmp ax,[bp+X_H]
    jae @F
    jmp AUOuter
@@:
AUOK:
    clc
    ret
AUExact:
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp AURepeated              ; colour views require disjoint envelopes
@@:
    mov ax,[bp+X_W]
    mul word ptr [bp+X_H]
    mov [bp+A_UN],ax
    mov [bp+A_UN+2],dx
    mov word ptr [bp+A_UINDEX],0
    mov word ptr [bp+A_UINDEX+2],0
AUExactLoop:
    mov ax,[bp+A_UINDEX]
    mov [bp+A_INDEX],ax
    mov ax,[bp+A_UINDEX+2]
    mov [bp+A_INDEX+2],ax
    mov ax,[bp+A_UVIEW]
    call AliasMap
    jnc @F
    jmp AUInvalid
@@:
    mov bx,[bp+A_UVIEW]
    call AliasInverse
    cmp cx,3
    jne @F
    jmp AUInvalid
@@:
    cmp cx,1
    je @F
    jmp AURepeated
@@:
    cmp ax,[bp+A_UINDEX]
    je @F
    jmp AURepeated
@@:
    cmp dx,[bp+A_UINDEX+2]
    je @F
    jmp AURepeated
@@:
    add word ptr [bp+A_UINDEX],1
    adc word ptr [bp+A_UINDEX+2],0
    mov ax,[bp+A_UINDEX+2]
    cmp ax,[bp+A_UN+2]
    jae @F
    jmp AUExactLoop
@@:
    mov ax,[bp+A_UINDEX]
    cmp ax,[bp+A_UN]
    jae @F
    jmp AUExactLoop
@@:
    jmp AUOK
AURepeated:
    mov ax,0ffffh
    stc
    ret
AUInvalid:
    xor ax,ax
    stc
    ret
AliasUnique endp

; Same physical row layout, regardless of selector numbers, bitmap offsets,
; segment increments/partitions or GDI bookkeeping fields. Stride and plane
; count must match. Both descriptors must map the union's logical rows.
; Prove destination-union injectivity, then compare actual first-plane row
; origins on both views; equal stride/planes gives equal complete row layout.
; No pixel access. Original clipping and order flags are restored on all exits.
AliasSameHuge proc near
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    je @F
    jmp ASHNo
@@:
    mov ax,[bp+X_DDESC+6]
    cmp ax,[bp+X_SDESC+6]
    je @F
    jmp ASHNo
@@:
    push word ptr [bp+X_DX]
    push word ptr [bp+X_DY]
    push word ptr [bp+X_SX]
    push word ptr [bp+X_SY]
    push word ptr [bp+X_W]
    push word ptr [bp+X_H]
    push word ptr [bp+A_DORDER]
    push word ptr [bp+A_SORDER]
    mov ax,[bp+X_DX]
    mov bx,[bp+X_SX]
    cmp ax,bx
    ja @F
    jmp ASHX
@@:
    xchg ax,bx
ASHX:
    mov [bp+X_DX],ax
    mov [bp+X_SX],ax
    sub bx,ax
    add [bp+X_W],bx
    mov ax,[bp+X_DY]
    mov bx,[bp+X_SY]
    cmp ax,bx
    ja @F
    jmp ASHY
@@:
    xchg ax,bx
ASHY:
    mov [bp+X_DY],ax
    mov [bp+X_SY],ax
    sub bx,ax
    add [bp+X_H],bx
    xor ax,ax
    call BltPreflight
    jnc @F
    jmp ASHRestore
@@:
    mov word ptr [bp+X_ROW],0
ASHRow:
    xor ax,ax
    call BltMapRow
    jnc @F
    jmp ASHRestore
@@:
    mov ax,1
    call BltMapRow
    jnc @F
    jmp ASHRestore
@@:
    mov ax,[bp+X_DNORM]
    cmp ax,[bp+X_SNORM]
    je @F
    jmp ASHMismatch
@@:
    mov ax,[bp+X_DNORM+2]
    cmp ax,[bp+X_SNORM+2]
    je @F
    jmp ASHMismatch
@@:
    inc word ptr [bp+X_ROW]
    mov ax,[bp+X_ROW]
    cmp ax,[bp+X_H]
    jae @F
    jmp ASHRow
@@:
    clc
    jmp ASHRestore
ASHMismatch:
    stc
ASHRestore:
    pop word ptr [bp+A_SORDER]
    pop word ptr [bp+A_DORDER]
    pop word ptr [bp+X_H]
    pop word ptr [bp+X_W]
    pop word ptr [bp+X_SY]
    pop word ptr [bp+X_SX]
    pop word ptr [bp+X_DY]
    pop word ptr [bp+X_DX]
    ret
ASHNo:
    stc
    ret
AliasSameHuge endp

; A_INDEX -> A_NEXT and CX=0 absent/1 present/2 repeated/3 invalid.
AliasNext proc near
    mov ax,1
    call AliasMap
    jnc @F
    jmp ANInvalid
@@:
    xor bx,bx
    call AliasInverse
    mov [bp+A_NEXT],ax
    mov [bp+A_NEXT+2],dx
    ret
ANInvalid:
    mov cx,3
    ret
AliasNext endp

AliasFromCandidate proc near
    mov ax,[bp+A_CAND]
    mov [bp+A_INDEX],ax
    mov ax,[bp+A_CAND+2]
    mov [bp+A_INDEX+2],ax
    mov word ptr [bp+A_STEPS],0
    mov word ptr [bp+A_STEPS+2],0
    ret
AliasFromCandidate endp

; An invariant bound of N operations, not a discretionary workload timeout.
AliasStep proc near
    add word ptr [bp+A_STEPS],1
    adc word ptr [bp+A_STEPS+2],0
    mov ax,[bp+A_STEPS+2]
    cmp ax,[bp+A_N+2]
    jae @F
    jmp ASTOK
@@:
    jbe @F
    jmp ASTBad
@@:
    mov ax,[bp+A_STEPS]
    cmp ax,[bp+A_N]
    jbe @F
    jmp ASTBad
@@:
ASTOK:
    clc
    ret
ASTBad:
    stc
    ret
AliasStep endp

; D pointer/position from AliasMap(0). AX raw destination bit 0/1.
AliasReadDest proc near
    mov ax,[bp+X_DX]
    add ax,[bp+X_POS]
    mov cx,ax
    and cx,7
    mov dl,80h
    shr dl,cl
    mov cl,3
    shr ax,cl
    mov di,[bp+X_DPTR]
    add di,ax
    mov ax,[bp+X_DPTR+2]
    mov es,ax
    xor ax,ax
    test es:[di],dl
    jnz @F
    jmp ARDReturn
@@:
    inc ax
ARDReturn:
    ret
AliasReadDest endp

; Exactly one mono operation; all pointer preparation precedes its bit fetch.
; The cycle's last source uses the saved leader D. Neighbours use CURRENT D.
AliasDraw proc near
    call AliasPosition
    xor ax,ax
    call BltMapRow
    jnc @F
    jmp ADReturn
@@:
    mov ax,1
    call BltMapRow
    jnc @F
    jmp ADReturn
@@:
    call BltPattern
    xor dx,dx
    cmp word ptr [bp+A_OVERRIDE],0
    je @F
    jmp ADSubstitute
@@:
    mov ax,[bp+X_SX]
    add ax,[bp+X_POS]
    mov cx,ax
    and cx,7
    mov dl,80h
    shr dl,cl
    mov cl,3
    shr ax,cl
    mov si,[bp+X_SPTR]
    add si,ax
    mov ax,[bp+X_SPTR+2]
    mov es,ax
    test es:[si],dl
    mov dl,0
    jnz @F
    jmp ADSource
@@:
    dec dl
    jmp ADSource
ADSubstitute:
    mov dx,[bp+A_SAVED]
    neg dl
ADSource:
    mov ax,[bp+X_DX]
    add ax,[bp+X_POS]
    mov cx,ax
    and cx,7
    mov bl,80h
    shr bl,cl
    mov cl,3
    shr ax,cl
    mov di,[bp+X_DPTR]
    add di,ax
    mov ax,[bp+X_DPTR+2]
    mov es,ax
    mov cl,bl
    mov bl,byte ptr [bp+B_PAT]
    mov ax,[bp+X_ROP]
    call BlitWriteByte
    clc
ADReturn:
    ret
AliasDraw endp

; General injective mono graph, no visited array, recursion, or bitmap backup.
; Each chain starts where D is absent from S. Only the smallest operation in
; each cycle writes it. Save one leader destination bit to close that cycle.
AliasGeneral proc near
    mov ax,[bp+X_W]
    mul word ptr [bp+X_H]
    mov [bp+A_N],ax
    mov [bp+A_N+2],dx
    mov word ptr [bp+A_CAND],0
    mov word ptr [bp+A_CAND+2],0
AGCandidate:
    call AliasFromCandidate
    xor ax,ax
    call AliasMap
    jnc @F
    jmp AGFail
@@:
    mov bx,1
    call AliasInverse
    cmp cx,1
    jbe @F
    jmp AGFail
@@:
    mov word ptr [bp+A_CYCLE],0
    or cx,cx
    jnz @F
    jmp AGExecuteStart
@@:
    call AliasFromCandidate
AGElect:
    call AliasStep
    jnc @F
    jmp AGFail
@@:
    call AliasNext
    cmp cx,1
    jbe @F
    jmp AGFail
@@:
    or cx,cx
    jnz @F
    jmp AGAdvance
@@:
    mov dx,[bp+A_NEXT+2]
    cmp dx,[bp+A_CAND+2]
    jae @F
    jmp AGAdvance
@@:
    jbe @F
    jmp AGElectNext
@@:
    mov ax,[bp+A_NEXT]
    cmp ax,[bp+A_CAND]
    jae @F
    jmp AGAdvance
@@:
    jne @F
    jmp AGCycle
@@:
AGElectNext:
    mov ax,[bp+A_NEXT]
    mov [bp+A_INDEX],ax
    mov ax,[bp+A_NEXT+2]
    mov [bp+A_INDEX+2],ax
    jmp AGElect
AGCycle:
    mov word ptr [bp+A_CYCLE],1
    call AliasFromCandidate
    xor ax,ax
    call AliasMap
    jnc @F
    jmp AGFail
@@:
    call AliasReadDest
    mov [bp+A_SAVED],ax
AGExecuteStart:
    call AliasFromCandidate
AGExecute:
    call AliasStep
    jnc @F
    jmp AGFail
@@:
    call AliasNext
    cmp cx,1
    jbe @F
    jmp AGFail
@@:
    mov [bp+A_STATUS],cx
    mov word ptr [bp+A_OVERRIDE],0
    cmp word ptr [bp+A_CYCLE],0
    jne @F
    jmp AGDraw
@@:
    or cx,cx
    jnz @F
    jmp AGFail
@@:
    mov ax,[bp+A_NEXT]
    cmp ax,[bp+A_CAND]
    je @F
    jmp AGDraw
@@:
    mov ax,[bp+A_NEXT+2]
    cmp ax,[bp+A_CAND+2]
    je @F
    jmp AGDraw
@@:
    mov word ptr [bp+A_OVERRIDE],1
AGDraw:
    call AliasDraw
    jnc @F
    jmp AGFail
@@:
    cmp word ptr [bp+A_OVERRIDE],1
    jne @F
    jmp AGAdvance
@@:
    cmp word ptr [bp+A_STATUS],0
    jne @F
    jmp AGAdvance
@@:
    mov ax,[bp+A_NEXT]
    mov [bp+A_INDEX],ax
    mov ax,[bp+A_NEXT+2]
    mov [bp+A_INDEX+2],ax
    jmp AGExecute
AGAdvance:
    add word ptr [bp+A_CAND],1
    adc word ptr [bp+A_CAND+2],0
    mov ax,[bp+A_CAND+2]
    cmp ax,[bp+A_N+2]
    jae @F
    jmp AGCandidate
@@:
    mov ax,[bp+A_CAND]
    cmp ax,[bp+A_N]
    jae @F
    jmp AGCandidate
@@:
    clc
    ret
AGFail:
    xor ax,ax
    stc
    ret
AliasGeneral endp
