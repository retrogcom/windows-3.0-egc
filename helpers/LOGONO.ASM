; EGC native 640x400 colour Windows 3.0 logo component, 286 real mode.
; DOS/BIOS only. Private appended byte-RLE, not Microsoft BMP RLE4.
.286
PAYLOAD_BYTES equ 13124
code segment para public 'CODE'
assume cs:code,ds:code
org 0
db 'LOGO'
jmp near ptr DrawLogo
jmp near ptr RestoreLogo
Armed db 0
DeviceHandle dw 0ffffh
RestoreLogo proc far
    pushf
    pusha
    push ds
    push es
    push cs
    pop ds
    call CloseDevice
    cmp Armed,0
    je RestoreDone
    mov ax,3
    int 10h
    mov Armed,0
RestoreDone:
    pop es
    pop ds
    popa
    popf
    retf
RestoreLogo endp
CloseDevice proc near
    mov bx,DeviceHandle
    cmp bx,0ffffh
    je DeviceClosed
    mov ah,3eh
    int 21h
    jc CloseReturn
    mov DeviceHandle,0ffffh
DeviceClosed:
    clc
CloseReturn:
    ret
CloseDevice endp
KeepEnd label byte
DrawLogo proc far
    pushf
    pusha
    push ds
    push es
    push cs
    pop ds
    cld
    mov Armed,0
    mov ax,0f00h
    call Video
    cmp ax,5003h
    je @F
    jmp NoDraw
@@:
    cmp bh,0
    je @F
    jmp NoDraw
@@:
    mov dx,offset DeviceName
    mov ax,3d00h
    int 21h
    jnc @F
    jmp NoDraw
@@:
    mov DeviceHandle,ax
    mov bx,ax
    mov ax,4400h
    int 21h
    jnc @F
    jmp CloseBad
@@:
    test dl,80h
    jnz @F
    jmp CloseBad
@@:
    call CloseDevice
    jnc @F
    jmp NoDraw
@@:
    cmp word ptr LogoData,4745h
    je @F
    jmp NoDraw
@@:
    cmp word ptr LogoData+2,3143h
    je @F
    jmp NoDraw
@@:
    mov Armed,1
    xor bx,bx
    mov ax,0042h
    call Video
    mov ax,0f00h
    call Video
    cmp ax,5042h
    je @F
    jmp Failed
@@:
    cmp bh,0
    je @F
    jmp Failed
@@:
    push cs
    pop es
    mov si,offset Palette
    xor bx,bx
    mov ax,0b05h
    call Video
    mov si,offset LogoData+4
    mov Plane,0
NextPlane:
    mov bx,Plane
    mov es,Segments[bx]
    xor di,di
    mov dx,8000h
Packet:
    cmp si,offset LogoData+PAYLOAD_BYTES
    jb @F
    jmp Failed
@@:
    lodsb
    xor ah,ah
    mov cx,ax
    and cx,7fh
    inc cx
    cmp cx,dx
    jbe @F
    jmp Failed
@@:
    sub dx,cx
    test al,80h
    jz Literal
    cmp si,offset LogoData+PAYLOAD_BYTES
    jb @F
    jmp Failed
@@:
    lodsb
    rep stosb
    jmp PacketDone
Literal:
    mov ax,si
    add ax,cx
    jnc @F
    jmp Failed
@@:
    cmp ax,offset LogoData+PAYLOAD_BYTES
    jbe @F
    jmp Failed
@@:
    rep movsb
PacketDone:
    or dx,dx
    jnz Packet
    add Plane,2
    cmp Plane,8
    jb NextPlane
    cmp si,offset LogoData+PAYLOAD_BYTES
    je @F
    jmp Failed
@@:
    jmp NoDraw
CloseBad:
    call CloseDevice
    jmp NoDraw
Failed:
    mov ax,3
    call Video
    mov Armed,0
NoDraw:
    pop es
    pop ds
    popa
    popf
    mov ax,offset KeepEnd
    retf
DrawLogo endp
Video proc near
    push ds
    push es
    push si
    push di
    push bp
    int 10h
    pop bp
    pop di
    pop si
    pop es
    pop ds
    ret
Video endp
DeviceName db 'DEBDRIVE',0
Plane dw 0
Segments dw 0a000h,0a800h,0b000h,0b800h
Palette db 4
rept 16
db 0,4,2,6,1,5,3,8,7,12,10,14,9,13,11,15
endm
LogoData label byte
code ends
end
