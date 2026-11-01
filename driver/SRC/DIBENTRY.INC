; Windows 3.0 non-palette DIB ABI. No fallback is requested after mutation:
; both entries return an eligible scan count or zero, never FFFFh.
D_CLIPPTR equ -628
D_TRACE equ -632
D_DFMT equ -634
D_TBPP equ -636
D_LOCAL equ 640
D_RESERVE equ 896
public DeviceBitmapBits,DIBScreenBlt
DIBFRAME macro
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
 mov ax,D_RESERVE
 call DibRoom
endm
DIBEXIT macro bytes
 cld
 lea sp,[bp-6]
 pop di
 pop si
 pop ds
 pop bp
 dec bp
 retf bytes
endm
DCALL macro target
 push cs
 call near ptr target
endm
DeviceBitmapBits proc far
 DIBFRAME
 DNEED nc,DBBStack
 sub sp,D_LOCAL
 mov word ptr [bp+D_TRACE],2
 mov word ptr [bp+D_DFMT],0ffffh
 mov word ptr [bp+D_TBPP],0ffffh
 mov ax,[bp+26]
 cmp ax,1
 DNEED be,DBBEarly
 mov [bp+D_ACTION],ax
 mov ax,[bp+24]
 mov [bp+D_START],ax
 mov ax,[bp+22]
 mov [bp+D_COUNT],ax
 mov ax,[bp+18]
 mov [bp+D_BITS],ax
 mov ax,[bp+20]
 mov [bp+D_BITS+2],ax
 mov ax,[bp+14]
 mov [bp+D_INFO],ax
 mov ax,[bp+16]
 mov [bp+D_INFO+2],ax
 mov word ptr [bp+D_ORGX],0
 mov word ptr [bp+D_ORGY],0
 mov word ptr [bp+D_CLIPPTR],0
 mov word ptr [bp+D_CLIPPTR+2],0
 mov ax,[bp+30]
 mov bx,[bp+28]
 call DibExecute
 jmp DBBExit
DBBEarly:
 xor ax,ax
DBBExit:
 DRAWREPORT DrawDib, [bp+D_TRACE], [bp+D_DFMT], [bp+D_TBPP], 13
 jmp DBBReturn
DBBStack:
 xor ax,ax
 DRAWREPORT DrawDib, 1, 0ffffh, 0ffffh, 13
DBBReturn:
 DIBEXIT 26
DeviceBitmapBits endp

DIBScreenBlt proc far
 DIBFRAME
 DNEED nc,DSBStack
 sub sp,D_LOCAL
 mov word ptr [bp+D_TRACE],2
 mov word ptr [bp+D_DFMT],0ffffh
 mov word ptr [bp+D_TBPP],0ffffh
 mov word ptr [bp+D_ACTION],2
 mov ax,[bp+28]
 mov [bp+D_START],ax
 mov ax,[bp+26]
 mov [bp+D_COUNT],ax
 mov ax,[bp+14]
 mov [bp+D_BITS],ax
 mov ax,[bp+16]
 mov [bp+D_BITS+2],ax
 mov ax,[bp+10]
 mov [bp+D_INFO],ax
 mov ax,[bp+12]
 mov [bp+D_INFO+2],ax
 mov ax,[bp+32]
 mov [bp+D_ORGX],ax
 mov ax,[bp+30]
 mov [bp+D_ORGY],ax
 mov ax,[bp+22]
 mov [bp+D_CLIPPTR],ax
 mov ax,[bp+24]
 mov [bp+D_CLIPPTR+2],ax
 mov ax,[bp+36]
 mov bx,[bp+34]
 call DibExecute
 jmp DSBExit
DSBExit:
 DRAWREPORT DrawScreenDib, [bp+D_TRACE], [bp+D_DFMT], [bp+D_TBPP], 16
 jmp DSBReturn
DSBStack:
 xor ax,ax
 DRAWREPORT DrawScreenDib, 1, 0ffffh, 0ffffh, 16
DSBReturn:
 DIBEXIT 32
DIBScreenBlt endp

; AX:BX surface descriptor; all state lives in this checked invocation frame.
DibExecute proc near
 mov word ptr [bp+S_HELD],0
 mov word ptr [bp+D_QUERY],0
 mov word ptr [bp+D_BITSIZE],0
 mov word ptr [bp+D_BITSIZE+2],0
 mov word ptr [bp+D_RESULT],0
 mov word ptr [bp+D_TRACE],3
 call DibSnapshot
 DNEED nc,DEBad
 call DibClassify
 DNEED nc,DEBad
 mov ax,[bp+S_DESC+8]
 mov [bp+D_DFMT],ax
 cmp word ptr [bp+D_ACTION],2
 jne DEDevice
 cmp word ptr [bp+S_KIND],1
 DNEED e,DEBad
DEDevice:
 mov word ptr [bp+S_ROP],13
 cmp word ptr [bp+D_ACTION],1
 jne DEClip
 mov word ptr [bp+S_ROP],0
DEClip:
 mov word ptr [bp+D_TRACE],4
 mov word ptr [bp+D_CLIP],0
 mov word ptr [bp+D_CLIP+2],0
 mov ax,[bp+S_DESC+2]
 mov [bp+D_CLIP+4],ax
 mov ax,[bp+S_DESC+4]
 mov [bp+D_CLIP+6],ax
 mov ax,[bp+D_CLIPPTR+2]
 or ax,[bp+D_CLIPPTR]
 jz DEHeader
 mov ax,[bp+D_CLIPPTR+2]
 mov bx,[bp+D_CLIPPTR]
 mov cx,8
 xor dx,dx
 xor di,di
 call DibSpan
 DNEED nc,DEBad
 mov es,ax
 mov si,bx
 lea di,[bp+D_CLIP]
 mov cx,4
DECopyClip:
 mov ax,es:[si]
 mov ss:[di],ax
 add si,2
 add di,2
 loop DECopyClip
DEHeader:
 mov word ptr [bp+D_TRACE],5
 DCALL DibHeaderOpen
 DNEED nc,DEBad
 mov ax,[bp+D_HEADER+14]
 mov [bp+D_TBPP],ax
 mov word ptr [bp+D_TRACE],6
 DCALL DibGeometry
 DNEED nc,DEBad
 cmp word ptr [bp+D_RESULT],0
 DNEED ne,DEEmpty
 mov ax,[bp+D_STRIDE]
 mul word ptr [bp+D_ROWS]
 mov [bp+D_TOTAL],ax
 mov [bp+D_TOTAL+2],dx
 mov ax,[bp+D_STRIDE+2]
 mul word ptr [bp+D_ROWS]
 or dx,dx
 DNEED z,DEBad
 add [bp+D_TOTAL+2],ax
 DNEED nc,DEBad
 mov ax,[bp+D_BITS+2]
 or ax,[bp+D_BITS]
 jnz DEOpenBits
 cmp word ptr [bp+D_ACTION],1
 DNEED e,DEBad
 mov word ptr [bp+D_QUERY],1
 jmp DEPreflight
DEOpenBits:
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 jne DEInputBudget
 inc di
 mov cx,0ffffh
 mov dx,cx
 jmp DEOpen
DEInputBudget:
 cmp word ptr [bp+D_COMP],0
 jne DERLEBudget
 mov cx,[bp+D_TOTAL]
 mov dx,[bp+D_TOTAL+2]
 jmp DEOpen
DERLEBudget:
 mov cx,[bp+D_HEADER+20]
 mov dx,[bp+D_HEADER+22]
DEOpen:
 mov word ptr [bp+D_TRACE],7
 mov ax,[bp+D_BITS+2]
 mov bx,[bp+D_BITS]
 DCALL DibStreamOpen
 DNEED nc,DEBad
 mov ax,[bp+ST_BASE]
 mov dx,[bp+ST_BASE+2]
 mov [bp+D_BITBASE],ax
 mov [bp+D_BITBASE+2],dx
 mov ax,[bp+ST_SIZE]
 mov dx,[bp+ST_SIZE+2]
 mov [bp+D_BITSIZE],ax
 mov [bp+D_BITSIZE+2],dx
 cmp word ptr [bp+D_ACTION],1
 jne DEValidateRGB
 call DEInfoSeparate
 DNEED nc,DEBad
DEValidateRGB:
 mov word ptr [bp+D_TRACE],8
 cmp word ptr [bp+D_COMP],0
 jne DEPreflight
 mov ax,[bp+D_TOTAL]
 mov dx,[bp+D_TOTAL+2]
 DCALL DibValidateStream
 DNEED nc,DEBad
DEPreflight:
 mov word ptr [bp+D_TRACE],9
 DCALL DibPreflightSurface
 DNEED nc,DEBad
 cmp word ptr [bp+D_ACTION],1
 je DEClaim
 mov word ptr [bp+D_TRACE],10
 mov word ptr [bp+D_PASS],0
 call DESetPass
 DNEED nc,DEBad
DEClaim:
 mov word ptr [bp+D_TRACE],11
 cmp word ptr [bp+S_KIND],0
 je DEClaimed
 call DibEnter
 DNEED nc,DEBad
 mov word ptr [bp+S_HELD],1
DEClaimed:
 mov word ptr [bp+D_TRACE],12
 cmp word ptr [bp+D_ACTION],1
 je DEGet
 mov word ptr [bp+D_PASS],1
 call DESetPass
 DNEED nc,DEBad
 jmp DESuccess
DEGet:
 cmp word ptr [bp+D_COMP],0
 jne DEGetRLE
 mov ax,[bp+D_IMAGE]
 mov dx,[bp+D_IMAGE+2]
 mov [bp+D_OUTSIZE],ax
 mov [bp+D_OUTSIZE+2],dx
 cmp word ptr [bp+D_QUERY],0
 jne DEPublish
 DCALL DibGetRGB
 DNEED nc,DEBad
 jmp DEPublish
DEGetRLE:
 mov word ptr [bp+D_PASS],0
 DCALL DibGetRLE
 DNEED nc,DEBad
 cmp word ptr [bp+D_QUERY],0
 jne DEPublish
 mov ax,[bp+D_OUTSIZE]
 mov dx,[bp+D_OUTSIZE+2]
 mov [bp+D_ACTSIZE],ax
 mov [bp+D_ACTSIZE+2],dx
 DCALL DibValidateStream
 DNEED nc,DEBad
 mov word ptr [bp+D_PASS],1
 DCALL DibGetRLE
 DNEED nc,DEBad
 mov ax,[bp+D_OUTSIZE]
 cmp ax,[bp+D_ACTSIZE]
 DNEED e,DEBad
 mov ax,[bp+D_OUTSIZE+2]
 cmp ax,[bp+D_ACTSIZE+2]
 DNEED e,DEBad
DEPublish:
 mov word ptr [bp+D_TRACE],12
 DCALL DibHeaderPublish
DEEmpty:
DESuccess:
 mov word ptr [bp+D_TRACE],0
 mov ax,[bp+D_RESULT]
 jmp DEReturn
DEBad:
 xor ax,ax
DEReturn:
 cmp word ptr [bp+S_HELD],0
 je DERet
 call DibLeave
DERet:
 ret
DibExecute endp

DESetPass proc near
 cmp word ptr [bp+D_COMP],0
 jne DESetRLE
 DCALL DibSetRGB
 ret
DESetRLE:
 DCALL DibSetRLE
 ret
DESetPass endp

; Reject output bits/header overlap before output. Input bits and the input
; header can share one allocation; the owned header/cache is already copied.
DEInfoSeparate proc near
 mov ax,[bp+D_INFOBASE]
 mov dx,[bp+D_INFOBASE+2]
 mov [bp+D_CURSTART],ax
 mov [bp+D_CURSTART+2],dx
 mov bx,[bp+D_INFOLEN]
 dec bx
 add ax,bx
 adc dx,0
 mov [bp+D_CUREND],ax
 mov [bp+D_CUREND+2],dx
 mov ax,[bp+D_BITBASE]
 mov dx,[bp+D_BITBASE+2]
 mov bx,[bp+D_BITSIZE]
 mov cx,[bp+D_BITSIZE+2]
 call DRangeSeparate
 ret
DEInfoSeparate endp
