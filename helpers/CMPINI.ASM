; DOS 2+ exact binary comparison for the owner-image driver switch.
; CMPINI file1 file2 : ERRORLEVEL 0 equal, 1 different, 2 argument/I/O error.
; DOS termination closes any handles, including a partially opened pair.
cpu 8086
bits 16
org 100h
start:
    push cs
    pop ds
    push cs
    pop es
    cld
    xor bx,bx
    mov bl,[80h]
    mov byte [bx+81h],0
    mov si,81h
    call skip_spaces
    cmp byte [si],0
    jne short guard_1
    jmp near error
guard_1:
    mov dx,si
    call end_token
    mov [next_arg],si
    mov ax,3d00h
    int 21h
    jnc short guard_2
    jmp near error
guard_2:
    mov [first_handle],ax
    mov si,[next_arg]
    call skip_spaces
    cmp byte [si],0
    jne short guard_3
    jmp near error
guard_3:
    mov dx,si
    call end_token
    call skip_spaces
    cmp byte [si],0
    je short guard_4
    jmp near error
guard_4:
    mov ax,3d00h
    int 21h
    jnc short guard_5
    jmp near error
guard_5:
    mov [second_handle],ax
compare:
    mov bx,[first_handle]
    mov dx,first_buffer
    mov cx,256
    mov ah,3fh
    int 21h
    jnc short guard_6
    jmp near error
guard_6:
    mov [read_count],ax
    mov bx,[second_handle]
    mov dx,second_buffer
    mov cx,256
    mov ah,3fh
    int 21h
    jnc short guard_7
    jmp near error
guard_7:
    cmp ax,[read_count]
    jne different
    or ax,ax
    jz equal
    mov cx,ax
    mov si,first_buffer
    mov di,second_buffer
    cld
    repe cmpsb
    jne different
    jmp compare
skip_spaces:
    cmp byte [si],20h
    je .skip
    cmp byte [si],9
    jne .done
.skip:
    inc si
    jmp skip_spaces
.done:
    ret
end_token:
    cmp byte [si],0
    je .done
    cmp byte [si],20h
    je .end
    cmp byte [si],9
    je .end
    inc si
    jmp end_token
.end:
    mov byte [si],0
    inc si
.done:
    ret
equal:
    mov ax,4c00h
    int 21h
different:
    mov ax,4c01h
    int 21h
error:
    mov ax,4c02h
    int 21h
next_arg dw 0
first_handle dw 0
second_handle dw 0
read_count dw 0
first_buffer times 256 db 0
second_buffer times 256 db 0
