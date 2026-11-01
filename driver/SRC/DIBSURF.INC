; Geometry in DIB coordinates; private surface adapter retains actual stride,
; plane count and huge-row layout. All source/destination byte spans are
; preflighted before a caller enters the conversion or output passes.
D_BITBASE equ -564
D_BITSIZE equ -568
D_ACTSIZE equ -572
D_YBASE equ -576
D_SCAN equ -578
D_CURSTART equ -584
D_CUREND equ -588
D_OTHERSTART equ -592
D_OTHEREND equ -596
public DibGeometry,DibPreflightSurface,DibGetIndex,DibPutIndex,DibNoOverlap
DibGeometry proc far
 push si
 push di
 mov word ptr [bp+D_RESULT],0
 cmp word ptr [bp+S_DESC+2],0
 DNEED g,DGBad
 cmp word ptr [bp+S_DESC+4],0
 DNEED g,DGBad
 mov ax,[bp+D_HEIGHT]
 cmp [bp+D_START],ax
 DNEED b,DGEmpty
 sub ax,[bp+D_START]
 cmp ax,[bp+D_COUNT]
 jbe DGCount
 mov ax,[bp+D_COUNT]
DGCount:
 mov [bp+D_ROWS],ax
 or ax,ax
 DNEED nz,DGEmpty
 ; Clip rectangle uses signed half-open destination coordinates.
 mov ax,[bp+D_CLIP]
 or ax,ax
 jge DGLeft
 xor ax,ax
DGLeft:
 mov [bp+D_CLIP],ax
 mov ax,[bp+D_CLIP+2]
 or ax,ax
 jge DGTop
 xor ax,ax
DGTop:
 mov [bp+D_CLIP+2],ax
 mov ax,[bp+D_CLIP+4]
 cmp ax,[bp+S_DESC+2]
 jle DGRight
 mov ax,[bp+S_DESC+2]
DGRight:
 mov [bp+D_CLIP+4],ax
 cmp ax,[bp+D_CLIP]
 DNEED g,DGEmpty
 mov ax,[bp+D_CLIP+6]
 cmp ax,[bp+S_DESC+4]
 jle DGBottom
 mov ax,[bp+S_DESC+4]
DGBottom:
 mov [bp+D_CLIP+6],ax
 cmp ax,[bp+D_CLIP+2]
 DNEED g,DGEmpty
 mov ax,[bp+D_CLIP]
 call DGXBound
 mov [bp+D_LEFT],ax
 mov ax,[bp+D_CLIP+4]
 call DGXBound
 mov [bp+D_RIGHT],ax
 cmp ax,[bp+D_LEFT]
 DNEED a,DGEmpty
 ; Y = originY + height - start - 1 - payloadRow. Keep the sum wide.
 mov ax,[bp+D_ORGY]
 cwd
 add ax,[bp+D_HEIGHT]
 adc dx,0
 sub ax,[bp+D_START]
 sbb dx,0
 mov [bp+D_YBASE],ax
 mov [bp+D_YBASE+2],dx
 sub ax,[bp+D_CLIP+6]
 sbb dx,0
 call DGYBound
 mov [bp+D_TOP],ax
 mov ax,[bp+D_YBASE]
 mov dx,[bp+D_YBASE+2]
 sub ax,[bp+D_CLIP+2]
 sbb dx,0
 call DGYBound
 mov [bp+D_BOTTOM],ax
 sub ax,[bp+D_TOP]
 DNEED a,DGEmpty
 mov [bp+D_RESULT],ax
DGEmpty:
 pop di
 pop si
 clc
 retf
DGBad:
 pop di
 pop si
 stc
 retf
DibGeometry endp
DGXBound proc near
 cwd
 mov bx,[bp+D_ORGX]
 mov cx,bx
 sar cx,15
 sub ax,bx
 sbb dx,cx
 mov cx,[bp+D_WIDTH]
 jmp DGBound
DGXBound endp
DGYBound proc near
 mov cx,[bp+D_ROWS]
DGBound:
 or dx,dx
 js DGBoundZero
 jnz DGBoundMax
 cmp ax,cx
 jbe DGBoundDone
DGBoundMax:
 mov ax,cx
DGBoundDone:
 ret
DGBoundZero:
 xor ax,ax
 ret
DGYBound endp

; Current DIB x/payload-row -> destination point, CF means outside the clipped
; geometric band (a normal skipped pixel, not malformed input).
DibLocate proc near
 mov ax,[bp+D_COL]
 cmp ax,[bp+D_LEFT]
 jb DLOut
 cmp ax,[bp+D_RIGHT]
 jae DLOut
 mov dx,[bp+D_ROW]
 cmp dx,[bp+D_TOP]
 jb DLOut
 cmp dx,[bp+D_BOTTOM]
 jae DLOut
 add ax,[bp+D_ORGX]
 mov [bp+S_X],ax
 mov ax,[bp+D_YBASE]
 dec ax
 sub ax,dx
 mov [bp+S_Y],ax
 clc
 ret
DLOut:
 stc
 ret
DibLocate endp

; Given DX:AX start and positive CX byte length, require separation from the
; original BITMAPINFO and the owned DIB stream range. Rejection is conservative
; for fabricated alias views; normal GDI bitmap storage is separately owned.
DibNoOverlap proc far
 push si
 push di
 mov [bp+D_CURSTART],ax
 mov [bp+D_CURSTART+2],dx
 dec cx
 add ax,cx
 adc dx,0
 mov [bp+D_CUREND],ax
 mov [bp+D_CUREND+2],dx
 mov ax,[bp+D_INFOBASE]
 mov dx,[bp+D_INFOBASE+2]
 mov bx,[bp+D_INFOLEN]
 xor cx,cx
 call DRangeSeparate
 jc DNOBad
 mov ax,[bp+D_BITBASE]
 mov dx,[bp+D_BITBASE+2]
 mov bx,[bp+D_BITSIZE]
 mov cx,[bp+D_BITSIZE+2]
 call DRangeSeparate
 pop di
 pop si
 retf
DNOBad:
 pop di
 pop si
 stc
 retf
DibNoOverlap endp

; Compare current range with DX:AX start and CX:BX length. Zero is absent.
DRangeSeparate proc near
 mov si,bx
 or si,cx
 jz DRSOkay
 mov [bp+D_OTHERSTART],ax
 mov [bp+D_OTHERSTART+2],dx
 add ax,bx
 adc dx,cx
 jc DRSBad
 sub ax,1
 sbb dx,0
 mov [bp+D_OTHEREND],ax
 mov [bp+D_OTHEREND+2],dx
 mov dx,[bp+D_CUREND+2]
 cmp dx,[bp+D_OTHERSTART+2]
 jb DRSOkay
 ja DRSSecond
 mov ax,[bp+D_CUREND]
 cmp ax,[bp+D_OTHERSTART]
 jb DRSOkay
DRSSecond:
 mov dx,[bp+D_OTHEREND+2]
 cmp dx,[bp+D_CURSTART+2]
 jb DRSOkay
 ja DRSBad
 mov ax,[bp+D_OTHEREND]
 cmp ax,[bp+D_CURSTART]
 jb DRSOkay
DRSBad:
 stc
 ret
DRSOkay:
 clc
 ret
DRangeSeparate endp

DibPreflightSurface proc far
 push si
 push di
 cmp word ptr [bp+D_RESULT],0
 DNEED ne,DPSOkay
 mov ax,[bp+D_TOP]
 mov [bp+D_SCAN],ax
DPSRow:
 mov ax,[bp+D_SCAN]
 mov [bp+D_ROW],ax
 mov ax,[bp+D_LEFT]
 mov [bp+D_COL],ax
 call DibLocate
 DNEED nc,DPSBad
 call DibPoint
 DNEED nc,DPSBad
 cmp word ptr [bp+S_KIND],0
 jne DPSScreen
 mov ax,[bp+S_SELECTOR]
 mov bx,[bp+S_OFFSET]
 mov cx,[bp+S_ROW]
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 je DPSMemory
 inc di
DPSMemory:
 call DibNormalize
 DNEED nc,DPSBad
 push cs
 call near ptr DibNoOverlap
 DNEED nc,DPSBad
 jmp DPSNext
DPSScreen:
 xor si,si
DPSPlane:
 mov ax,[bp+D_RIGHT]
 add ax,[bp+D_ORGX]
 dec ax
 shr ax,3
 mov cx,[bp+S_X]
 shr cx,3
 sub ax,cx
 inc ax
 mov cx,ax
 mov ax,[bp+S_ADDR+si+2]
 mov bx,[bp+S_ADDR+si]
 xor dx,dx
 xor di,di
 cmp word ptr [bp+D_ACTION],1
 je DPSSpan
 inc di
DPSSpan:
 call DibSpan
 DNEED nc,DPSBad
 mov ax,bx
 mov dx,000ah
 cmp si,8
 jb DPSPhysical
 inc dx
 cmp si,12
 jb DPSPhysical
 add ax,8000h
 adc dx,0
DPSPhysical:
 push cs
 call near ptr DibNoOverlap
 DNEED nc,DPSBad
 add si,4
 cmp si,16
 jb DPSPlane
DPSNext:
 inc word ptr [bp+D_SCAN]
 mov ax,[bp+D_SCAN]
 cmp ax,[bp+D_BOTTOM]
 DNEED ae,DPSRow
DPSOkay:
 pop di
 pop si
 clc
 retf
DPSBad:
 pop di
 pop si
 stc
 retf
DibPreflightSurface endp

; Return the deterministic output DIB index at D_COL/D_ROW. Unavailable columns
; or rows are black without a surface access. Caller owns cursor exclusion.
DibGetIndex proc far
 push si
 push di
 call DibLocate
 jc DGIOutside
 call DibPoint
 jc DGIBad
 call DibPixel
 mov al,dh
 cmp word ptr [bp+D_BPP],1
 je DGIMono
 cmp word ptr [bp+S_PLANES],1
 je DGIMono
 and ax,15
 jmp DGIDone
DGIMono:
 shr al,4
 and ax,1
 jmp DGIDone
DGIOutside:
 xor ax,ax
DGIDone:
 pop di
 pop si
 clc
 retf
DGIBad:
 pop di
 pop si
 stc
 retf
DibGetIndex endp

; AL cached RGB/mono metadata; direct copy, no DRAWMODE recolouring/ROP.
DibPutIndex proc far
 push si
 push di
 mov [bp+D_PIXEL],ax
 call DibLocate
 jc DPIOutside
 call DibPoint
 jc DPIBad
 mov ax,[bp+D_PIXEL]
 cmp word ptr [bp+S_PLANES],1
 jne DPIColour
 shr al,4
 and al,1
 jmp DPIPen
DPIColour:
 and al,15
DPIPen:
 mov byte ptr [bp+S_PEN],al
 call DibPixel
DPIOutside:
 pop di
 pop si
 clc
 retf
DPIBad:
 pop di
 pop si
 stc
 retf
DibPutIndex endp
