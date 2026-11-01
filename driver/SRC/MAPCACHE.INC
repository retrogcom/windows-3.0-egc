; Phase 8 bounded, invocation-local selector-base cache, eight entries.
; Only BltMapRow calls this after PreparePoint validated the COMPLETE row with
; CheckSpan/CheckReadSpan. A miss uses the full BlitNormalize contract, including
; LDT data/rights/base checks. Borrowed selectors remain stable within this
; synchronous call. Cache count is reset at every BlitRectangle entry.
; A full cache simply stops admitting new selectors; correctness is unchanged.
; No heap allocation, global cache, mapping mutation or source-pixel fetch.
public BltNormalizeRow

; AX selector, BX offset, CX full row bytes, DI read/write. Same return and
; preserved-register contract as BlitNormalize. Entry validation is a caller
; precondition even on a cache hit. Retain the 24-bit end-address check.
BltNormalizeRow proc near
    push si
    push di
    mov [bp+C_KEY],ax
    mov dx,[bp+C_COUNT]
    mov si,C_CACHE
CNFind:
    or dx,dx
    jnz @F
    jmp CNMiss
@@:
    cmp ax,[bp+si]
    jne @F
    jmp CNHit
@@:
    add si,6
    dec dx
    jmp CNFind
CNMiss:
    call BlitNormalize
    jnc @F
    jmp CNReturn
@@:
    cmp word ptr [bp+C_COUNT],8
    jb @F
    jmp CNReturn
@@:
    push ax
    push dx
    sub ax,bx
    sbb dx,0
    mov [bp+si+2],ax
    mov [bp+si+4],dx
    mov ax,[bp+C_KEY]
    mov [bp+si],ax
    inc word ptr [bp+C_COUNT]
    pop dx
    pop ax
    clc
    jmp CNReturn
CNHit:
    mov ax,[bp+si+2]
    mov dx,[bp+si+4]
    add ax,bx
    adc dx,0
    mov si,cx
    dec si
    add ax,si
    adc dx,0
    cmp dx,00ffh
    jbe @F
    jmp CNBad
@@:
    sub ax,si
    sbb dx,0
    clc
    jmp CNReturn
CNBad:
    xor ax,ax
    xor dx,dx
    stc
CNReturn:
    pop di
    pop si
    ret
BltNormalizeRow endp
