; Checked borrowed-memory identity for BitBlt alias classification.
; Internal helper; eventual entry reserves stack and owns mapping lifetime.
public BlitNormalize

; AX selector/segment, BX offset, CX byte count, DI 0=read / 1=write.
; -> DX:AX linear first byte, CF error. BX/CX/SI/DI/BP/DS/ES preserved.
; Require an ordinary LDT data selector before KERNEL.186; that Windows 3
; helper has no selector validation and masks away TI before its LDT lookup.
; Real mode uses paragraph arithmetic. No allocation, yield, raw INT31 or I/O.
BlitNormalize proc near
    push si
    cmp di,1
    jbe @F
    jmp NormalizeBad
@@:
    or di,di
    jnz @F
    jmp NormalizeRead
@@:
    call CheckSpan
    jmp NormalizeAccess
NormalizeRead:
    call CheckReadSpan
NormalizeAccess:
    jnc @F
    jmp NormalizeBad
@@:
    cmp ProtectedMode,0
    jne @F
    jmp NormalizeReal
@@:
    test ax,4
    jnz @F
    jmp NormalizeBad
@@:
    lar dx,ax
    jz @F
    jmp NormalizeBad
@@:
    test dx,0800h               ; readable code is not a bitmap data mapping
    jz @F
    jmp NormalizeBad
@@:
    push bx
    push cx
    push si
    push di
    push bp
    push ds
    push es
    cCall GetSelectorBase,<ax>
    pop es
    pop ds
    pop bp
    pop di
    pop si
    pop cx
    pop bx
    cmp dx,00ffh                ; target is the 286's 24-bit physical space
    jbe @F
    jmp NormalizeBad
@@:
    jmp NormalizeOffset
NormalizeReal:
    xor dx,dx
    shl ax,1
    rcl dx,1
    shl ax,1
    rcl dx,1
    shl ax,1
    rcl dx,1
    shl ax,1
    rcl dx,1
NormalizeOffset:
    add ax,bx
    adc dx,0
    mov si,cx
    dec si
    add ax,si
    adc dx,0
    cmp dx,00ffh
    jbe @F
    jmp NormalizeBad
@@:
    sub ax,si
    sbb dx,0
    pop si
    clc
    ret
NormalizeBad:
    xor ax,ax
    xor dx,dx
    pop si
    stc
    ret
BlitNormalize endp
