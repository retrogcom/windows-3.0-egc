; Shared screen-span mapping. S_X/S_Y are clipped pixel coordinates;
; CX is a positive byte count starting at floor(S_X/8), within this row.
; Caller already snapshotted/classified the device and holds stable mappings.
; Gates, coordinates, every selector's rights/limit and the complete requested
; span are checked on EVERY call. No persistent cache or framebuffer access.
; Returns S_MASK and S_ADDR for all physical planes, CF on invalid mapping.
public PrepareScreenSpan
PrepareScreenSpan proc near
    cmp ScreenGate,1
    jne SSpanBad
    cmp DriverState,ACTIVE
    jne SSpanBad
    cmp word ptr [bp+S_PLANES],4
    jne SSpanBad
    cmp word ptr [bp+S_X],640
    jae SSpanBad
    cmp word ptr [bp+S_Y],400
    jae SSpanBad
    jcxz SSpanBad
    mov bx,[bp+S_X]
    shr bx,1
    shr bx,1
    shr bx,1
    mov ax,80
    sub ax,bx
    cmp cx,ax
    ja SSpanBad
    jmp SSpanValid
SSpanBad:
    stc
    ret
SSpanValid:
    push cx
    mov cx,[bp+S_X]
    and cx,7
    mov ax,80h
    shr ax,cl
    mov [bp+S_MASK],ax
    mov dx,[bp+S_Y]
    mov ax,dx
    and ax,3
    mov cl,13
    shl ax,cl
    mov di,ax
    mov ax,dx
    shr ax,1
    shr ax,1
    mov cx,80
    mul cx
    add di,ax
    add di,bx
    mov dx,di                   ; common offset, calculated once for all planes
    pop cx                      ; requested span, preserved by CheckPixelSpan
    xor si,si
    xor di,di
SSpanPlane:
    mov bx,dx
    add bx,PlaneBases[di]
    mov ax,PlaneSelectors[di]
    call CheckPixelSpan
    jc SSpanInvalidPlane
    mov [bp+S_ADDR+si],bx
    mov [bp+S_ADDR+si+2],ax
    add si,4
    add di,2
    cmp di,8
    jb SSpanPlane
    clc
    ret
SSpanInvalidPlane:
    stc
    ret
PrepareScreenSpan endp
