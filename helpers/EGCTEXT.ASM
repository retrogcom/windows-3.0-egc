title EGC native mode-3 text switching companion
.286
_TEXT segment para public 'CODE'
assume cs:_TEXT,ds:_TEXT
org 0
public StdFuncTable,ExtFuncTable,InquireGrab,InquireSave,SaveScreen,RestoreScreen,InitScreen
public Busy,BaselineValid,QualifiedPages,QualifiedShape,Header,Live,GuardLow,StackBottom,StackTop,GuardHigh,ModuleEnd
public SaveWork,RestoreWork,InitWork,CRCBytes,CheckLayout,PrivateEnter,PrivateExit,CheckBuffer
NEED macro condition,destination
 local okay
 j&condition okay
 jmp destination
okay:
endm
StdFuncTable label word
 db 0e9h
 dw InquireGrab-$-2
 db 0e9h
 dw Unsupported-$-2
 db 0e9h
 dw Unsupported-$-2
 db 0e9h
 dw Unsupported-$-2
 db 0e9h
 dw InquireSave-$-2
 db 0e9h
 dw SaveScreen-$-2
 db 0e9h
 dw RestoreScreen-$-2
 db 0e9h
 dw InitScreen-$-2
ExtFuncTable dw Noop,Noop,Unsupported,Unsupported,BlockUnsupported,Unsupported
 dw GetVersion,Noop,Noop,Noop,GetInfo,Unsupported

; Dispatch by tail jump; every target retains the original far return frame.
InquireGrab proc near
 cmp ax,0fff4h
 jb IGSize
 push bx
 mov bx,ax
 sub bx,0fff4h
 shl bx,1
 mov bx,cs:ExtFuncTable[bx]
 push bp
 mov bp,sp
 xchg bx,ss:[bp+2]
 pop bp
 ret                        ; selected routine returns far to original caller
IGSize:
 cmp ax,1
 je IGEmpty
 cmp ax,2
 jne Unsupported
IGEmpty:
 xor ax,ax                  ; no clipboard storage or block support
 xor dx,dx
 retf
InquireGrab endp
Unsupported:
 stc
 retf
BlockUnsupported:
 mov ax,00ffh
 stc
 retf
GetVersion:
 mov ax,0300h
 retf
Noop:
 retf
InquireSave proc near
 cmp ax,1
 je ISSingle
 cmp ax,2
 jne Unsupported
 mov ax,8040h
 jmp ISDone
ISSingle:
 mov ax,1040h
ISDone:
 xor dx,dx
 retf
InquireSave endp

; Only PUSHF touches the caller stack before switching. DS=CS is the ABI;
; CS overrides protect the claim and saved stack even from defensive reentry.
PRIVATE macro number
 local accepted
 pushf
 cli
 cmp cs:Busy,0
 je accepted
 popf
 stc
 retf
accepted:
 mov cs:Busy,1
 mov cs:Operation,number
 mov cs:CallerSS,ss
 mov cs:CallerSP,sp
 mov cs:CallerAX,ax
 mov cs:CallerBX,bx
 jmp PrivateEnter
endm
SaveScreen:
 PRIVATE 1
RestoreScreen:
 PRIVATE 2
InitScreen:
 PRIVATE 3
GetInfo:
 PRIVATE 4
PrivateEnter:
 mov ax,cs
 mov ss,ax
 mov sp,offset StackTop
 mov ax,cs:CallerAX
 pusha
 push ds
 push es
 mov ax,cs:CallerSS
 mov es,ax
 mov bx,cs:CallerSP
 push word ptr es:[bx]
 popf                       ; restore caller IF for ordinary work, then CLD
 push cs
 pop ds
 cld
 mov ax,CallerAX
 ; Recover ES from the private saved-register image without a caller pointer.
 mov bp,sp
 mov bx,ss:[bp]
 mov es,bx
 mov BufferSeg,bx
 mov BufferOff,di
 mov Capacity,ax
 cmp GuardLow,0a55ah
 NEED e,PrivateBad
 cmp GuardHigh,05aa5h
 NEED e,PrivateBad
 cmp Operation,1
 jne PENotSave
 call SaveWork
 jmp PrivateExit
PENotSave:
 cmp Operation,2
 jne PENotRestore
 call RestoreWork
 jmp PrivateExit
PENotRestore:
 cmp Operation,3
 jne PEInfo
 call InitWork
 jmp PrivateExit
PEInfo:
 call InfoWork
 jmp PrivateExit
PrivateBad:
 stc
PrivateExit:
 pushf
 pop ax
 and ax,1
 mov ResultCF,ax
 cmp GuardLow,0a55ah
 jne PEGuardBad
 cmp GuardHigh,05aa5h
 je PEUnwind
PEGuardBad:
 mov ResultCF,1
PEUnwind:
 pop es
 pop ds
 popa
 cli
 ; No borrowed record pointer survives a call.
 mov cs:BufferSeg,0
 mov cs:BufferOff,0
 mov cs:Capacity,0
 mov ax,cs:CallerSS
 mov bx,cs:CallerSP
 mov ss,ax
 mov sp,bx
 mov cs:CallerSS,0
 mov cs:CallerSP,0
 and word ptr ss:[bx],0fffeh
 cmp cs:ResultCF,0
 je PEClear
 or word ptr ss:[bx],1
PEClear:
 mov ax,cs:CallerAX
 cmp cs:Operation,4
 jne PERestoreBX
 xor ax,ax                  ; GetInfo always reports no block operations
PERestoreBX:
 mov bx,cs:CallerBX
 mov cs:Busy,0
 popf                       ; entry flags, with only result CF replaced
 retf

; AX bytes at ES:DI; reject wrap, low memory, video and module overlap.
; Caller supplies a valid owned conventional-memory allocation/capacity.
CheckBuffer proc near
 or ax,ax
 NEED nz,CBFail
 mov bx,di
 mov cx,ax
 dec cx
 add cx,bx
 NEED nc,CBFail
 mov dx,es
 mov ax,dx
 shr bx,4
 shr cx,4
 add bx,dx
 NEED nc,CBFail
 add cx,ax
 NEED nc,CBFail
 cmp bx,50h
 NEED ae,CBFail
 cmp cx,0a000h
 NEED b,CBFail
 mov ax,cs
 cmp cx,ax
 jb CBStack
 mov dx,offset ModuleEnd
 add dx,15
 shr dx,4
 add ax,dx
 cmp bx,ax
 NEED ae,CBFail
CBStack:
 ; The record must not overwrite the caller FLAGS/far-return frame.
 mov ax,CallerSP
 mov dx,ax
 add dx,5
 NEED nc,CBFail
 shr ax,4
 shr dx,4
 add ax,CallerSS
 NEED nc,CBFail
 add dx,CallerSS
 NEED nc,CBFail
 cmp cx,ax
 jb CBOkay
 cmp bx,dx
 NEED a,CBFail
CBOkay:
 clc
 ret
CBFail:
 stc
 ret
CheckBuffer endp

ReadLive proc near
 push ds
 push cs
 pop es
 xor ax,ax
 mov ds,ax
 mov si,449h
 mov di,offset Live
 mov cx,15
 rep movsw
 pop ds
 ret
ReadLive endp

; DS:SI is the exact 30-byte BIOS video block. Only qualified active pages
; and (by default) the native initial cursor shape may be captured.
CheckLayout proc near
 cmp byte ptr [si],3
 NEED e,CLBad
 cmp word ptr [si+1],80
 NEED e,CLBad
 cmp word ptr [si+3],1000h
 NEED e,CLBad
 cmp word ptr [si+26],03d4h
 NEED e,CLBad
 xor ax,ax
 mov al,[si+25]
 cmp ax,8
 NEED b,CLBad
 mov cx,ax
 mov bx,1
 shl bx,cl
 test QualifiedPages,bx
 NEED nz,CLBad
 shl ax,12
 cmp ax,[si+5]
 NEED e,CLBad
 mov bx,si
 add bx,7
 mov cx,8
CLCursor:
 cmp byte ptr [bx],80
 NEED b,CLBad
 cmp byte ptr [bx+1],25
 NEED b,CLBad
 add bx,2
 loop CLCursor
 cmp BaselineValid,0
 je CLOkay
 mov ax,[si+28]
 cmp ax,BaselineControls
 NEED e,CLBad
 cmp QualifiedShape,0
 jne CLOkay
 mov ax,[si+23]
 cmp ax,BaselineShape
 NEED e,CLBad
CLOkay:
 clc
 ret
CLBad:
 stc
 ret
CheckLayout endp

; BIOS calls use the private stack. Preserve segment/general registers around
; each call because a particular BIOS's scratch registers are not our state.
VideoBIOS proc near
 pusha
 push ds
 push es
 int 10h
 pop es
 pop ds
 popa
 cld
 ret
VideoBIOS endp
InitWork proc near
 mov BaselineValid,0
 mov ax,3
 call VideoBIOS
 call ReadLive
 mov si,offset Live
 call CheckLayout
 jc IWBad
 mov ax,word ptr Live+28
 mov BaselineControls,ax
 mov ax,word ptr Live+23
 mov BaselineShape,ax
 mov BaselineValid,1
 clc
 ret
IWBad:
 stc
 ret
InitWork endp

SaveWork proc near
 cmp BufferOff,0
 NEED e,SWBad
 cmp BaselineValid,1
 NEED e,SWBad
 mov ax,Capacity
 cmp ax,1040h
 NEED ae,SWBad
 mov ax,1040h
 cmp Capacity,8040h
 jb SWSize
 mov ax,8040h
SWSize:
 mov TotalBytes,ax
 mov es,BufferSeg
 xor di,di
 call CheckBuffer
 NEED nc,SWBad
 call ReadLive
 mov si,offset Live
 call CheckLayout
 NEED nc,SWBad
 push cs
 pop es
 mov di,offset Header
 xor ax,ax
 mov cx,32
 rep stosw
 mov word ptr Header,4745h
 mov word ptr Header+2,3154h
 mov word ptr Header+4,1
 mov ax,TotalBytes
 mov word ptr Header+6,ax
 sub ax,40h
 mov word ptr Header+8,ax
 mov word ptr Header+12,1
 cmp ax,8000h
 jne SWPage
 mov word ptr Header+14,1
 jmp SWMeta
SWPage:
 mov ax,word ptr Live+5
 mov word ptr Header+10,ax
SWMeta:
 mov si,offset Live
 mov di,offset Header+20h
 mov cx,15
 rep movsw
 ; Preflight complete: no BIOS, DOS, allocator or failure branch follows.
 mov es,BufferSeg
 xor di,di
 xor ax,ax
 stosw
 stosw                      ; invalidate the previous signature
 push ds
 mov ax,0b800h
 mov ds,ax
 mov si,word ptr cs:Header+10
 mov cx,word ptr cs:Header+8
 shr cx,1
 mov di,40h
 rep movsw
 pop ds
 mov dx,0ffffh
 mov si,offset Header
 mov cx,40h
 call CRCBytes
 push ds
 mov cx,word ptr Header+8
 mov ds,BufferSeg
 mov si,40h
 call CRCBytes
 pop ds
 mov word ptr Header+10h,dx
 mov es,BufferSeg
 mov di,4
 mov si,offset Header+4
 mov cx,30
 rep movsw
 mov ax,word ptr Header+2
 mov es:[2],ax
 mov ax,word ptr Header
 mov es:[0],ax               ; publish complete record last
 clc
 ret
SWBad:
 stc
 ret
SaveWork endp

; CRC-16/CCITT-FALSE DS:SI/CX bytes, initial/current DX; no reflection.
CRCBytes proc near
 jcxz CRCDone
CRCByte:
 lodsb
 xor dh,al
 mov bl,8
CRCBit:
 shl dx,1
 jnc CRCNext
 xor dx,1021h
CRCNext:
 dec bl
 jnz CRCBit
 loop CRCByte
CRCDone:
 ret
CRCBytes endp

RestoreWork proc near
 cmp BufferOff,0
 NEED e,RWBad
 cmp BaselineValid,1
 NEED e,RWBad
 cmp Capacity,40h
 NEED ae,RWBad
 mov ax,40h
 mov es,BufferSeg
 xor di,di
 call CheckBuffer
 NEED nc,RWBad
 push ds
 mov ds,BufferSeg
 push cs
 pop es
 xor si,si
 mov di,offset Header
 mov cx,32
 rep movsw
 pop ds
 cmp word ptr Header,4745h
 NEED e,RWBad
 cmp word ptr Header+2,3154h
 NEED e,RWBad
 cmp word ptr Header+4,1
 NEED e,RWBad
 cmp word ptr Header+12,1
 NEED e,RWBad
 cmp word ptr Header+14,0
 je RWPage
 cmp word ptr Header+14,1
 NEED e,RWBad
 cmp word ptr Header+8,8000h
 NEED e,RWBad
 cmp word ptr Header+10,0
 NEED e,RWBad
 jmp RWSize
RWPage:
 cmp word ptr Header+8,1000h
 NEED e,RWBad
 mov ax,word ptr Header+25h
 cmp ax,word ptr Header+10
 NEED e,RWBad
RWSize:
 mov ax,word ptr Header+8
 add ax,40h
 cmp ax,word ptr Header+6
 NEED e,RWBad
 cmp ax,Capacity
 NEED be,RWBad
 mov es,BufferSeg
 xor di,di
 call CheckBuffer
 NEED nc,RWBad
 mov si,offset Header+12h
 mov cx,7
RWReserved:
 lodsw
 or ax,ax
 NEED z,RWBad
 loop RWReserved
 cmp word ptr Header+3eh,0
 NEED e,RWBad
 mov si,offset Header+20h
 call CheckLayout
 NEED nc,RWBad
 mov ax,word ptr Header+10h
 mov ExpectedCRC,ax
 mov word ptr Header+10h,0
 mov si,offset Header
 mov cx,40h
 mov dx,0ffffh
 call CRCBytes
 push ds
 mov cx,word ptr Header+8
 mov ds,BufferSeg
 mov si,40h
 call CRCBytes
 pop ds
 cmp dx,ExpectedCRC
 NEED e,RWBad
 ; All input validation precedes the first display mutation.
 mov ax,3
 call VideoBIOS
 call ReadLive
 mov si,offset Live
 call CheckLayout
 NEED nc,RWBad
 push ds
 mov si,40h
 mov di,word ptr cs:Header+10
 mov cx,word ptr cs:Header+8
 shr cx,1
 mov ax,0b800h
 mov es,ax
 mov ds,BufferSeg
 rep movsw
 pop ds
 xor bx,bx
 mov si,offset Header+27h
RWCursor:
 mov dx,[si]
 mov ah,2
 call VideoBIOS
 add si,2
 inc bh
 cmp bh,8
 jb RWCursor
 mov cx,word ptr Header+37h
 mov ah,1
 call VideoBIOS
 mov al,byte ptr Header+39h
 mov ah,5
 call VideoBIOS
 call ReadLive
 ; Compare actual BIOS results; never fabricate BIOS data to hide failure.
 mov si,offset Header+20h
 mov di,offset Live
 mov cx,30
 repe cmpsb
 NEED e,RWBad
 clc
 ret
RWBad:
 stc
 ret
RestoreWork endp

InfoWork proc near
 mov ax,48
 mov es,BufferSeg
 mov di,BufferOff
 call CheckBuffer
 jc IFBad
 call ReadLive
 mov es,BufferSeg
 mov di,BufferOff
 xor ax,ax
 mov cx,24
 rep stosw
 mov di,BufferOff
 mov byte ptr es:[di],5      ; DI_OLIVETTI
 cmp byte ptr Live,3
 jne IFUnknown
 mov si,offset Live
 call CheckLayout
 jc IFUnknown
 mov byte ptr es:[di+6],80
 mov byte ptr es:[di+7],25
 mov byte ptr es:[di+8],8
 mov byte ptr es:[di+9],8
 ; Physical raster dimensions remain zero until measured/qualified.
 clc
 ret
IFUnknown:
 mov byte ptr es:[di+1],1    ; ST_GRPH/unsupported context
 clc
 ret
IFBad:
 stc
 ret
InfoWork endp

even
Busy dw 0
Operation dw 0
CallerSS dw 0
CallerSP dw 0
CallerAX dw 0
CallerBX dw 0
ResultCF dw 0
BufferSeg dw 0
BufferOff dw 0
Capacity dw 0
TotalBytes dw 0
ExpectedCRC dw 0
BaselineValid dw 0
BaselineControls dw 0
BaselineShape dw 0
QualifiedPages dw 1          ; only active page 0 until M28 qualification
QualifiedShape dw 0          ; only InitScreen's cursor shape until qualification
Header db 64 dup (0)
Live db 30 dup (0)
GuardLow dw 0a55ah
StackBottom db 2048 dup (0cch)
StackTop label byte
GuardHigh dw 05aa5h
ModuleEnd label byte
_TEXT ends
end
