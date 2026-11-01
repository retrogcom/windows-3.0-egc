; Normalized 40-byte DIB header and up to 256 physical-index/mono values.
; Public entry code will reserve the complete shared BP frame before calling.
D_HEADER equ -168
D_CACHE equ -424
D_INFO equ -484                 ; ordinary far BITMAPINFO: offset, selector
D_INFOLEN equ -486
D_BITS equ -490
D_HEIGHT equ -492
D_STRIDE equ -496               ; DWORD
D_IMAGE equ -500                ; full BI_RGB image DWORD
D_ACTION equ -502               ; 0 Set, 1 Get, 2 direct-screen Set
D_START equ -504
D_COUNT equ -506
D_ORGX equ -508
D_ORGY equ -510
D_CLIP equ -518                 ; left, top, right, bottom
D_LEFT equ -520
D_RIGHT equ -522
D_TOP equ -524
D_BOTTOM equ -526
D_ROW equ -528
D_COL equ -530
D_PASS equ -532
D_RESULT equ -534
D_PIXEL equ -536
D_TEMP equ -538
D_RGB equ -542
D_INFOBASE equ -546             ; DWORD linear address, retained across stream Open
D_OUTSIZE equ -550              ; counted RLE payload or BI_RGB full image size
D_COMP equ -552
public DibHeaderOpen,DibHeaderPublish
DibHeaderOpen proc far
 push si
 push di
 ; BITMAPINFO is an ordinary far structure, including a Windows stack
 ; address. It is not required to be a global heap allocation. Validate
 ; its complete header/table span; only the huge bitmap bits use StreamOpen.
 mov ax,[bp+D_INFO+2]
 mov bx,[bp+D_INFO]
 mov cx,40
 xor dx,dx
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 jne DHCheck
 inc di
DHCheck:
 call DibSpan
 DNEED nc,DHBad
 mov es,ax
 mov si,bx
 lea di,[bp+D_HEADER]
 mov cx,20
DHSnapshot:
 mov ax,es:[si]
 mov ss:[di],ax
 add si,2
 add di,2
 loop DHSnapshot
 cmp word ptr [bp+D_HEADER],40
 DNEED e,DHBad
 cmp word ptr [bp+D_HEADER+2],0
 DNEED e,DHBad
 cmp word ptr [bp+D_HEADER+6],0
 DNEED e,DHBad
 cmp word ptr [bp+D_HEADER+10],0
 DNEED e,DHBad
 mov ax,[bp+D_HEADER+4]
 or ax,ax
 DNEED g,DHBad
 mov [bp+D_WIDTH],ax
 mov ax,[bp+D_HEADER+8]
 or ax,ax
 DNEED g,DHBad
 mov [bp+D_HEIGHT],ax
 cmp word ptr [bp+D_HEADER+12],1
 DNEED e,DHBad
 cmp word ptr [bp+D_HEADER+18],0
 DNEED e,DHBad
 mov ax,[bp+D_HEADER+16]
 mov [bp+D_COMP],ax
 mov ax,[bp+D_HEADER+14]
 mov [bp+D_BPP],ax
 cmp ax,1
 je DHOne
 cmp ax,4
 je DHFour
 cmp ax,8
 je DHEight
 cmp ax,24
 DNEED e,DHBad
 xor bx,bx
 cmp word ptr [bp+D_COMP],0
 DNEED e,DHBad
 jmp DHUsed
DHOne:
 mov bx,2
 cmp word ptr [bp+D_COMP],0
 DNEED e,DHBad
 jmp DHUsed
DHFour:
 mov bx,16
 cmp word ptr [bp+D_COMP],0
 je DHUsed
 cmp word ptr [bp+D_COMP],2
 DNEED e,DHBad
 jmp DHUsed
DHEight:
 mov bx,256
 cmp word ptr [bp+D_COMP],1
 DNEED be,DHBad
DHUsed:
 cmp word ptr [bp+D_ACTION],1
 je DHGetUsed
 or bx,bx
 jz DHGetUsed
 cmp word ptr [bp+D_HEADER+34],0
 DNEED e,DHBad
 mov ax,[bp+D_HEADER+32]
 or ax,ax
 jz DHGetUsed
 cmp ax,bx
 DNEED be,DHBad
 mov bx,ax
DHGetUsed:
 mov [bp+D_USED],bx
 shl bx,1
 shl bx,1
 add bx,40
 mov [bp+D_INFOLEN],bx
DHTableSpan:
 mov cx,bx
 mov bx,[bp+D_INFO]
 mov ax,[bp+D_INFO+2]
 xor dx,dx
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 jne DHTableAccess
 inc di
DHTableAccess:
 call DibSpan
 DNEED nc,DHBad
 ; Keep a normalized metadata span for Get overlap checks, without
 ; interpreting this selector/segment as a GlobalHandle/GlobalSize owner.
 mov ax,[bp+D_INFO+2]
 mov bx,[bp+D_INFO]
 mov cx,[bp+D_INFOLEN]
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 jne DHNormalize
 inc di
DHNormalize:
 call DibNormalize
 DNEED nc,DHBad
 mov [bp+D_INFOBASE],ax
 mov [bp+D_INFOBASE+2],dx
 ; Only compressed input uses biSizeImage as a read budget.
 cmp word ptr [bp+D_ACTION],1
 je DHArithmetic
 cmp word ptr [bp+D_COMP],0
 je DHArithmetic
 mov ax,[bp+D_HEADER+20]
 or ax,[bp+D_HEADER+22]
 DNEED nz,DHBad
DHArithmetic:
 mov ax,[bp+D_WIDTH]
 mul word ptr [bp+D_BPP]
 add ax,31
 adc dx,0
 mov cx,5
DHAlign:
 shr dx,1
 rcr ax,1
 loop DHAlign
 shl ax,1
 rcl dx,1
 shl ax,1
 rcl dx,1
 mov [bp+D_STRIDE],ax
 mov [bp+D_STRIDE+2],dx
 mul word ptr [bp+D_HEIGHT]
 mov [bp+D_IMAGE],ax
 mov [bp+D_IMAGE+2],dx
 mov ax,[bp+D_STRIDE+2]
 mul word ptr [bp+D_HEIGHT]
 or dx,dx
 DNEED z,DHBad
 add [bp+D_IMAGE+2],ax
 DNEED nc,DHBad
 cmp word ptr [bp+D_ACTION],1
 je DHOkay
 mov cx,[bp+D_USED]
 jcxz DHOkay
 mov es,[bp+D_INFO+2]
 mov si,[bp+D_INFO]
 add si,40
 lea di,[bp+D_CACHE]
DHColour:
 mov dl,es:[si]
 mov ah,es:[si+1]
 mov al,es:[si+2]
 call DibColor
 mov ss:[di],dh
 inc di
 add si,4
 loop DHColour
DHOkay:
 pop di
 pop si
 clc
 retf
DHBad:
 pop di
 pop si
 stc
 retf
DibHeaderOpen endp

; Output is called only after all bits/allocation/overlap/preflight succeeds.
; Get's biSizeImage is an output field, never an allocation-capacity argument.
DibHeaderPublish proc far
 push si
 push di
 mov es,[bp+D_INFO+2]
 mov si,[bp+D_INFO]
 mov ax,[bp+D_OUTSIZE]
 mov dx,[bp+D_OUTSIZE+2]
 mov es:[si+20],ax
 mov es:[si+22],dx
 mov word ptr es:[si+32],0
 mov word ptr es:[si+34],0
 mov word ptr es:[si+36],0
 mov word ptr es:[si+38],0
 mov cx,[bp+D_USED]
 jcxz DHPDone
 add si,40
 xor di,di
DHPColour:
 xor ax,ax
 cmp word ptr [bp+D_BPP],1
 je DHPMono
 cmp word ptr [bp+S_PLANES],1
 je DHPMono
 cmp di,16
 jae DHPIndex
 mov ax,di
 jmp DHPIndex
DHPMono:
 cmp di,1
 jne DHPIndex
 mov ax,15
DHPIndex:
 push cx
 call DibIndex
 pop cx
 mov es:[si],dl
 mov es:[si+1],ah
 mov es:[si+2],al
 mov byte ptr es:[si+3],0
 add si,4
 inc di
 loop DHPColour
DHPDone:
 pop di
 pop si
 retf
DibHeaderPublish endp

D_VALIDATE equ -556
public DibValidateStream
; DX:AX required prefix length. Validate every actual selector/chunk before
; output, without reading/writing any payload bytes; rewind to zero on success.
DibValidateStream proc far
 push di
 mov [bp+D_VALIDATE],ax
 mov [bp+D_VALIDATE+2],dx
 cmp dx,[bp+ST_SIZE+2]
 DNEED be,DVSBad
 jb DVSStart
 cmp ax,[bp+ST_SIZE]
 DNEED be,DVSBad
DVSStart:
 xor ax,ax
 xor dx,dx
 push cs
 call near ptr DibStreamSeek
 DNEED nc,DVSBad
DVSLoop:
 mov ax,[bp+ST_POS]
 mov dx,[bp+ST_POS+2]
 cmp dx,[bp+D_VALIDATE+2]
 jb DVSChunk
 cmp ax,[bp+D_VALIDATE]
 je DVSOkay
DVSChunk:
 call DibStreamMap
 DNEED nc,DVSBad
 mov ax,[bp+ST_END]
 sub ax,[bp+ST_OFF]
 xor dx,dx
 add ax,1
 adc dx,0
 add ax,[bp+ST_POS]
 adc dx,[bp+ST_POS+2]
 cmp dx,[bp+D_VALIDATE+2]
 jb DVSSeek
 ja DVSLast
 cmp ax,[bp+D_VALIDATE]
 jbe DVSSeek
DVSLast:
 mov ax,[bp+D_VALIDATE]
 mov dx,[bp+D_VALIDATE+2]
DVSSeek:
 push cs
 call near ptr DibStreamSeek
 DNEED nc,DVSBad
 jmp DVSLoop
DVSOkay:
 xor ax,ax
 xor dx,dx
 push cs
 call near ptr DibStreamSeek
 pop di
 retf
DVSBad:
 pop di
 stc
 retf
DibValidateStream endp
