; Remaining synchronous Windows helper/callback entries.
public Control, EnumDFonts, EnumObj, ScanLR, DeviceMode, DeviceBitmap
public FastBorder, SetAttribute, CreateBitmap, Inquire
H_INDEX equ -22
H_RESULT equ -24
H_SAVEDX equ -96
H_TARGET equ -98

H_ENTER macro
    mov ax,ds
    nop
    inc bp
    push bp
    mov bp,sp
    push ds
    push si
    push di
    mov ax,seg DriverState
    mov ds,ax
    cld
endm
H_EXIT macro amount
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf amount
endm

Control proc far
    H_ENTER
    mov ax,[bp+14]
    cmp ax,8
    je HCInput
    cmp ax,5
    je HCInput
    cmp ax,4
    je HCInput
    xor ax,ax
    jmp HCDone
HCInput:
    mov ax,[bp+12]
    mov bx,[bp+10]
    mov cx,2
    call CheckReadSpan
    jc HCError
    mov es,ax
    mov si,es:[bx]              ; snapshot before potentially overlapping output
    cmp word ptr [bp+14],8
    jne HCColour
    mov ax,1
    cmp si,8
    je HCDone
    cmp si,5
    je HCDone
    xor ax,ax
    jmp HCDone
HCColour:
    cmp si,16
    jae HCError
    mov ax,[bp+8]
    mov bx,[bp+6]
    mov cx,4
    call CheckSpan
    jc HCError
    mov es,ax
    mov ax,si
    call IndexToPhysical
    xor dh,dh
    mov es:[bx],ax
    mov es:[bx+2],dx
    mov ax,1
    jmp HCDone
HCError:
    mov ax,0ffffh
HCDone:
    H_EXIT 14
Control endp

EnumObj proc far
    H_ENTER
    mov ax,[bp+14]
    cmp ax,1
    je HEStart
    cmp ax,2
    je HEStart
    mov ax,1
    jmp HEDone
HEStart:
    mov ax,192
    call StackRoom
    jnc HEStack
    jmp HEError
HEStack:
    sub sp,32
    mov ax,[bp+12]
    mov bx,[bp+10]
    or ax,ax
    jnz HECallback
    jmp HEError
HECallback:
    cmp ProtectedMode,0
    je HERealCallback
    lar dx,ax
    jz HECodeAccess
    jmp HEError
HECodeAccess:
    and dx,9800h
    cmp dx,9800h
    je HECodeLimit
    jmp HEError
HECodeLimit:
    lsl dx,ax
    jz HEOffset
    jmp HEError
HEOffset:
    cmp bx,dx
    jbe HEReady
    jmp HEError
HERealCallback:
    mov cx,1
    call CheckReadSpan
    jnc HEReady
    jmp HEError
HEReady:
    mov word ptr [bp+H_INDEX],0
    mov word ptr [bp+H_RESULT],1
HELoop:
    push ss
    pop es
    lea di,[bp-20]
    xor ax,ax
    mov cx,6
    rep stosw                  ; all LOGPEN/LOGBRUSH bytes initialized
    mov si,[bp+H_INDEX]
    mov al,EnumColours[si]
    call IndexToPhysical
    xor dh,dh
    lea di,[bp-18]             ; brush RGB at +2
    cmp word ptr [bp+14],2
    je HERecord
    add di,4                  ; pen RGB at +6
HERecord:
    mov ss:[di],ax
    mov ss:[di+2],dx
    push ds
    push ss
    lea ax,[bp-20]
    push ax
    push word ptr [bp+8]
    push word ptr [bp+6]
    call dword ptr [bp+10]
    pop ds
    cld
    mov [bp+H_RESULT],ax
    or ax,ax
    jz HEDone
    inc word ptr [bp+H_INDEX]
    cmp word ptr [bp+H_INDEX],16
    jb HELoop
    mov ax,[bp+H_RESULT]
    jmp HEDone
HEError:
    xor ax,ax
HEDone:
    H_EXIT 14
EnumObj endp

ScanLR proc far
    H_ENTER
    mov ax,PX_RESERVE
    call StackRoom
    jnc HSStack
    jmp HSEarly
HSStack:
    sub sp,PX_LOCAL
    mov word ptr [bp+S_HELD],0
    mov word ptr [bp+S_ROP],0
    mov bx,[bp+16]
    mov ax,[bp+18]
    call SnapshotSurface
    jnc HSSurface
    jmp HSError
HSSurface:
    call ClassifyFormat
    jnc HSFormat
    jmp HSError
HSFormat:
    mov ax,[bp+14]
    mov [bp+S_X],ax
    mov [bp+H_SAVEDX],ax
    mov ax,[bp+12]
    mov [bp+S_Y],ax
    call PreparePoint           ; starting point must be valid, not clamped
    jnc HSFirst
    jmp HSError
HSFirst:
    mov word ptr [bp+S_X],0
    call PreparePoint
    jnc HSLast
    jmp HSError
HSLast:
    mov ax,[bp+S_DESC+2]
    dec ax
    mov [bp+S_X],ax
    call PreparePoint           ; all planes of complete row accessible
    jnc HSAdmission
    jmp HSError
HSAdmission:
    cmp word ptr [bp+S_KIND],0
    je HSOwned
    xor ax,ax
    mov bx,[bp+S_Y]
    mov cx,640
    mov dx,bx
    inc dx
    call EnterDrawingRect
    jnc HSClaimed
    jmp HSError
HSClaimed:
    mov word ptr [bp+S_HELD],1
HSOwned:
    ; The complete row was proved above. Map the first byte once, then advance
    ; only within that same logical row; bank/selector identity cannot change.
    mov ax,[bp+H_SAVEDX]
    mov [bp+S_X],ax
    call PreparePoint
    jnc @F
    jmp HSError
@@:
    mov al,byte ptr [bp+11]
    shr al,4
    and ax,1
    mov [bp+H_TARGET],ax
    cmp word ptr [bp+S_PLANES],1
    je HSByte
    ; Colour comparisons use exact canonical RGB, not the caller's index tag.
    mov ax,[bp+8]
    mov dx,[bp+10]
    call LogicalToPhysical
    cmp ax,[bp+8]
    jne HSNoColour
    cmp dl,byte ptr [bp+10]
    jne HSNoColour
    mov al,dh
    and ax,15
    mov [bp+H_TARGET],ax
    jmp HSByte
HSNoColour:
    mov word ptr [bp+H_TARGET],0ffffh
HSByte:
    xor dx,dx
    mov cx,[bp+H_TARGET]
    cmp cx,0ffffh
    je HSByteMatch
    mov dl,0ffh
    xor si,si
    mov di,[bp+S_PLANES]
HSPlane:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov al,es:[bx]
    shr cx,1
    jc HSPlaneSet
    not al
HSPlaneSet:
    and dl,al
    add si,4
    dec di
    jnz HSPlane
HSByteMatch:
    test word ptr [bp+6],1
    jnz HSSeekBit
    not dl
HSSeekBit:
    test dl,byte ptr [bp+S_MASK]
    jnz HSFound
    test word ptr [bp+6],2
    jnz HSLeft
    inc word ptr [bp+S_X]
    mov ax,[bp+S_X]
    cmp ax,[bp+S_DESC+2]
    jae HSAbsent
    shr byte ptr [bp+S_MASK],1
    jnz HSSeekBit
    mov byte ptr [bp+S_MASK],80h
    mov bx,1
    jmp HSNextByte
HSLeft:
    dec word ptr [bp+S_X]
    js HSAbsent
    shl byte ptr [bp+S_MASK],1
    jnz HSSeekBit
    mov byte ptr [bp+S_MASK],1
    mov bx,-1
HSNextByte:
    xor si,si
    mov cx,[bp+S_PLANES]
HSAdvancePlane:
    add [bp+S_ADDR+si],bx
    add si,4
    loop HSAdvancePlane
    jmp HSByte
HSAbsent:
    mov ax,0ffffh
    jmp HSRelease
HSFound:
    mov ax,[bp+S_X]
    jmp HSRelease
HSError:
    mov ax,8000h
HSRelease:
    cmp word ptr [bp+S_HELD],0
    je HSDone
    call LeaveDrawing
    jmp HSDone
HSEarly:
    mov ax,8000h
HSDone:
    H_EXIT 14
ScanLR endp

Inquire proc far
    H_ENTER
    mov bx,[bp+6]
    mov ax,[bp+8]
    mov cx,4
    call CheckSpan
    jc HIError
    mov es,ax
    mov word ptr es:[bx],1
    mov word ptr es:[bx+2],2
    mov ax,4
    jmp HIDone
HIError:
    xor ax,ax
HIDone:
    H_EXIT 4
Inquire endp

EnumDFonts proc far
    cld
    mov ax,1
    retf 16
EnumDFonts endp
DeviceMode proc far
    cld
    mov ax,0ffffh
    retf 12
DeviceMode endp
DeviceBitmap proc far
    cld
    xor ax,ax
    retf 14
DeviceBitmap endp
include C:\SRC\BORDER.INC
SetAttribute proc far
    cld
    xor ax,ax
    retf 12
SetAttribute endp
CreateBitmap proc far
    cld
    xor ax,ax
    retf
CreateBitmap endp

