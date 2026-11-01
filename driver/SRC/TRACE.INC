; AX is the stage. Preserve all registers and FLAGS, including caller CF/IF/DF.
; Fixed startup data owns the line buffer. Logging failure never changes the
; display result. A failed close is retained and retried before another open.
public TraceRecord
TraceRecord proc near
    pushf
    pusha
    push es
    cmp TraceEnabled,0
    jne TraceEnabledNow
    jmp TraceExit
TraceEnabledNow:
    pushf
    pop dx
    test dx,0200h               ; never call DOS with interrupts disabled
    jnz TraceInterruptsOn
    jmp TraceExit
TraceInterruptsOn:
    pushf
    cli
    cmp TraceBusy,0
    je TraceClaim
    jmp TraceAlreadyBusy
TraceClaim:
    mov TraceBusy,1
    popf
    mov TraceStage,ax
    mov ax,256
    call StackRoom
    jnc TraceStackOK
    jmp TraceLowStack
TraceStackOK:
    cld
    cmp TraceHandle,0ffffh
    je TraceFormat
    call TraceClose
    jnc TraceFormat
    jmp TraceUnlock
TraceFormat:
    cmp TraceStage,0073h
    jne TraceNotPerf
    mov si,DataOFFSET PerfStart
    mov bp,DataOFFSET PerfFields
    mov cx,(PerfEnd-PerfStart)/2
    jmp TraceDrawFieldLoop
TraceNotPerf:
    cmp TraceStage,0070h
    jb TraceStartupFormat
    cmp TraceStage,0072h
    ja TraceStartupFormat
    mov ax,TraceStage
    sub ax,006fh
    mov di,DataOFFSET DrawOpText
    call TraceHex
    mov ax,TraceStage
    sub ax,0070h
    mov dx,DrawRecordSize
    mul dx
    mov si,DataOFFSET DrawBlt
    add si,ax
    mov bp,DataOFFSET DrawTraceFields
    mov cx,28
TraceDrawFieldLoop:
    lodsw
    mov di,ds:[bp]
    add bp,2
    push cx
    call TraceHex
    pop cx
    loop TraceDrawFieldLoop
    cmp TraceStage,0073h
    jne TraceDrawLine
    mov TraceWriteOffset,DataOFFSET PerfLine
    mov TraceWriteLength,PerfLineEnd-PerfLine
    jmp TraceOpenFile
TraceDrawLine:
    mov TraceWriteOffset,DataOFFSET DrawTraceLine
    mov TraceWriteLength,DrawTraceEnd-DrawTraceLine
    jmp TraceOpenFile
TraceStartupFormat:
    mov TraceWriteOffset,DataOFFSET TraceLine
    mov TraceWriteLength,TraceLineEnd-TraceLine
    mov si,DataOFFSET TraceFields
    mov cx,11
TraceFieldLoop:
    lodsw
    mov bx,ax
    mov dx,[bx]
    lodsw
    mov di,ax
    mov ax,dx
    push cx
    call TraceHex
    pop cx
    loop TraceFieldLoop
TraceOpenFile:
    mov dx,DataOFFSET TraceName
    mov ax,3d01h
    int 21h
    jnc TraceOpened
    cmp ax,2                    ; only absent file permits creation
    jne TraceIoFailed
    mov dx,DataOFFSET TraceName
    xor cx,cx
    mov ah,3ch
    int 21h
    jc TraceIoFailed
TraceOpened:
    mov TraceHandle,ax
    mov bx,ax
    xor cx,cx
    xor dx,dx
    mov ax,4202h
    int 21h
    jc TraceWriteFailed
    mov bx,TraceHandle
    mov dx,TraceWriteOffset
    mov cx,TraceWriteLength
    mov ah,40h
    int 21h
    jc TraceWriteFailed
    cmp ax,TraceWriteLength
    je TraceCloseNow
    mov ax,0ffffh               ; short write, e.g. disk full
TraceWriteFailed:
    mov TraceIoError,ax
TraceCloseNow:
    call TraceClose
    jmp short TraceUnlock
TraceIoFailed:
    mov TraceIoError,ax
    jmp short TraceUnlock
TraceLowStack:
    mov TraceIoError,0fffeh
TraceUnlock:
    mov TraceBusy,0
    jmp short TraceExit
TraceAlreadyBusy:
    popf
TraceExit:
    pop es
    popa
    popf
    ret
TraceRecord endp

TraceClose proc near
    mov bx,TraceHandle
    mov ah,3eh
    int 21h
    jc TraceCloseFailed
    mov TraceHandle,0ffffh
    clc
    ret
TraceCloseFailed:
    mov TraceIoError,ax
    stc
    ret
TraceClose endp

TraceHex proc near
    mov bx,ax
    mov cx,4
TraceHexDigit:
    rol bx,4
    mov al,bl
    and al,0fh
    cmp al,9
    jbe TraceHexDecimal
    add al,7
TraceHexDecimal:
    add al,'0'
    mov [di],al
    inc di
    loop TraceHexDigit
    ret
TraceHex endp
