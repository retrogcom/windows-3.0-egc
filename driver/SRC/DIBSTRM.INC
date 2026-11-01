; Synchronous DIB stream in a discardable segment. BP is an owned caller frame.
; Open borrows a valid global Windows allocation starting at its canonical
; allocation selector (an offset within its first 64K is permitted). Like the
; loaded-font contract, arbitrary readable non-global selectors are not valid
; GlobalHandle/GlobalSize arguments. No buffer, selector or lock is retained.
; Huge continuation selectors must map consecutive linear 64K portions, using
; the loader's KERNEL.114 increment rather than bmSegmentIndex or a literal 8.
D_STR equ -480
ST_SEL equ D_STR
ST_OFF equ D_STR+2
ST_POS equ D_STR+4
ST_SIZE equ D_STR+8
ST_BASE equ D_STR+12
ST_FIRSTSEL equ D_STR+16
ST_FIRSTOFF equ D_STR+18
ST_END equ D_STR+20
ST_VALID equ D_STR+22
ST_WRAP equ D_STR+24
ST_WRITE equ D_STR+26
ST_BUDGET equ D_STR+28
ST_HANDLE equ D_STR+32
ST_OWNER equ D_STR+34
ST_ALLOC equ D_STR+36
ST_ROOT equ D_STR+40
public DibStreamOpen,DibStreamSeek,DibStreamRead,DibStreamWrite,DibStreamMap
DNEED macro condition,destination
 local okay
 j&condition okay
 jmp destination
okay:
endm

; AX selector, BX offset, DI read=0/write=1, DX:CX positive byte budget.
; Result CF, bounded owned size in ST_SIZE. Preserves SI/DI/BP/DS.
DibStreamOpen proc far
 push si
 push di
 mov [bp+ST_FIRSTSEL],ax
 mov [bp+ST_FIRSTOFF],bx
 mov [bp+ST_BUDGET],cx
 mov [bp+ST_BUDGET+2],dx
 mov [bp+ST_WRITE],di
 mov word ptr [bp+ST_VALID],0
 mov word ptr [bp+ST_SIZE],0
 mov word ptr [bp+ST_SIZE+2],0
 or dx,cx
 DNEED nz,DSOBad
 mov cx,1
 call DibNormalize
 DNEED nc,DSOBad
 mov [bp+ST_BASE],ax
 mov [bp+ST_BASE+2],dx
 push word ptr [bp+ST_FIRSTSEL]
 call GlobalHandle
 or ax,ax
 DNEED nz,DSOBad
 cmp dx,[bp+ST_FIRSTSEL]
 DNEED e,DSOBad
 mov [bp+ST_HANDLE],ax
 mov [bp+ST_OWNER],dx
 push ax
 call GlobalSize
 mov [bp+ST_ALLOC],ax
 mov [bp+ST_ALLOC+2],dx
 or ax,dx
 DNEED nz,DSOBad
 mov ax,[bp+ST_OWNER]
 xor bx,bx
 mov cx,1
 xor di,di
 call DibNormalize
 DNEED nc,DSOBad
 mov [bp+ST_ROOT],ax
 mov [bp+ST_ROOT+2],dx
 mov cx,[bp+ST_BASE]
 mov bx,[bp+ST_BASE+2]
 sub cx,ax
 sbb bx,dx
 DNEED nc,DSOBad
 mov ax,[bp+ST_ALLOC]
 mov dx,[bp+ST_ALLOC+2]
 sub ax,cx
 sbb dx,bx
 DNEED nc,DSOBad
 mov cx,ax
 or cx,dx
 DNEED nz,DSOBad
 cmp dx,[bp+ST_BUDGET+2]
 jb DSOSize
 ja DSOBudget
 cmp ax,[bp+ST_BUDGET]
 jbe DSOSize
DSOBudget:
 mov ax,[bp+ST_BUDGET]
 mov dx,[bp+ST_BUDGET+2]
DSOSize:
 mov [bp+ST_SIZE],ax
 mov [bp+ST_SIZE+2],dx
 xor ax,ax
 xor dx,dx
 push cs
 call near ptr DibStreamSeek
 pop di
 pop si
 retf
DSOBad:
 mov word ptr [bp+ST_SIZE],0
 mov word ptr [bp+ST_SIZE+2],0
 pop di
 pop si
 stc
 retf
DibStreamOpen endp

; DX:AX absolute stream offset. Seeking exactly to end is allowed; reading
; there fails without a dereference. A rejected seek preserves old position.
DibStreamSeek proc far
 push si
 cmp dx,[bp+ST_SIZE+2]
 DNEED be,DSSBad
 jb DSSRange
 cmp ax,[bp+ST_SIZE]
 DNEED be,DSSBad
 je DSSAtEnd
DSSRange:
 push ax
 push dx
 mov bx,ax
 add bx,[bp+ST_FIRSTOFF]
 adc dx,0
 DNEED nc,DSSMathBad
 mov ax,dx
 mov cx,HugeIncrement
 or cx,cx
 DNEED nz,DSSMathBad
 mul cx
 or dx,dx
 DNEED z,DSSMathBad
 add ax,[bp+ST_FIRSTSEL]
 DNEED nc,DSSMathBad
 mov [bp+ST_SEL],ax
 mov [bp+ST_OFF],bx
 pop dx
 pop ax
DSSAtEnd:
 mov [bp+ST_POS],ax
 mov [bp+ST_POS+2],dx
 mov word ptr [bp+ST_VALID],0
 mov word ptr [bp+ST_WRAP],0
 pop si
 clc
 retf
DSSMathBad:
 pop dx
 pop ax
DSSBad:
 pop si
 stc
 retf
DibStreamSeek endp

; Establish an accessible chunk. Allocation, budget, ordinary selector rights,
; actual selector limit and linear continuity all constrain the cached span.
; This routine never insists on a next selector after the final consumed byte.
DibStreamMap proc near
 mov ax,[bp+ST_POS]
 mov dx,[bp+ST_POS+2]
 cmp dx,[bp+ST_SIZE+2]
 DNEED be,DSMBad
 jb DSMRemaining
 cmp ax,[bp+ST_SIZE]
 DNEED b,DSMBad
DSMRemaining:
 cmp word ptr [bp+ST_WRAP],0
 je DSMReady
 mov ax,[bp+ST_SEL]
 add ax,HugeIncrement
 DNEED nc,DSMBad
 mov [bp+ST_SEL],ax
 mov word ptr [bp+ST_WRAP],0
 mov word ptr [bp+ST_VALID],0
DSMReady:
 cmp word ptr [bp+ST_VALID],0
 je DSMNew
 mov ax,[bp+ST_OFF]
 cmp ax,[bp+ST_END]
 DNEED be,DSMBad
 clc
 ret
DSMNew:
 mov ax,[bp+ST_SEL]
 mov bx,[bp+ST_OFF]
 mov cx,1
 mov di,[bp+ST_WRITE]
 call DibNormalize
 DNEED nc,DSMBad
 sub ax,[bp+ST_BASE]
 sbb dx,[bp+ST_BASE+2]
 DNEED nc,DSMBad
 cmp ax,[bp+ST_POS]
 DNEED e,DSMBad
 cmp dx,[bp+ST_POS+2]
 DNEED e,DSMBad
 mov ax,[bp+ST_SEL]
 cmp ProtectedMode,0
 je DSMReal
 lsl ax,ax
 DNEED z,DSMBad
 jmp DSMLimit
DSMReal:
 cmp ax,0f000h
 jbe DSMFull
 mov dx,0ffffh
 sub dx,ax
 shl dx,4
 or dx,15
 mov ax,dx
 jmp DSMLimit
DSMFull:
 mov ax,0ffffh
DSMLimit:
 mov bx,ax
 mov ax,[bp+ST_SIZE]
 mov dx,[bp+ST_SIZE+2]
 sub ax,[bp+ST_POS]
 sbb dx,[bp+ST_POS+2]
 or dx,dx
 jnz DSMEnd
 dec ax
 add ax,[bp+ST_OFF]
 jc DSMEnd
 cmp ax,bx
 ja DSMEnd
 mov bx,ax
DSMEnd:
 mov [bp+ST_END],bx
 mov cx,bx
 sub cx,[bp+ST_OFF]
 xor dx,dx
 add cx,1
 adc dx,0                    ; 65536 is represented as 0001:0000
 mov ax,[bp+ST_SEL]
 mov bx,[bp+ST_OFF]
 mov di,[bp+ST_WRITE]
 call DibSpan
 DNEED nc,DSMBad
 mov word ptr [bp+ST_VALID],1
 clc
 ret
DSMBad:
 stc
 ret
DibStreamMap endp

DibStreamAdvance proc near
 add word ptr [bp+ST_POS],1
 adc word ptr [bp+ST_POS+2],0
 inc word ptr [bp+ST_OFF]
 jnz DSANoWrap
 mov word ptr [bp+ST_WRAP],1
DSANoWrap:
 ret
DibStreamAdvance endp

DibStreamRead proc far
 push di
 cmp word ptr [bp+ST_WRITE],0
 DNEED e,DSRBad
 call DibStreamMap
 DNEED nc,DSRBad
 mov ax,[bp+ST_SEL]
 mov es,ax
 mov bx,[bp+ST_OFF]
 xor ax,ax
 mov al,es:[bx]
 call DibStreamAdvance
 pop di
 clc
 retf
DSRBad:
 pop di
 stc
 retf
DibStreamRead endp

; AL byte. The stored byte is the only output; no advance occurs on failure.
DibStreamWrite proc far
 push di
 push ax
 cmp word ptr [bp+ST_WRITE],1
 jne DSWBad
 call DibStreamMap
 jc DSWBad
 mov ax,[bp+ST_SEL]
 mov es,ax
 mov bx,[bp+ST_OFF]
 pop ax
 mov es:[bx],al
 call DibStreamAdvance
 pop di
 clc
 retf
DSWBad:
 pop ax
 pop di
 stc
 retf
DibStreamWrite endp
