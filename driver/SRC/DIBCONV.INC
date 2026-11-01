; Streaming BI_RGB converters and RLE Get encoder. Scalar correctness path;
; all loops retain source phase through clipping and require stable borrowed
; input across complete validation/counting and the subsequent output pass.
D_FIRST equ -598
D_SECOND equ -600
D_RUN equ -602
D_PACK equ -608
D_PHASE equ -610
D_LINEEND equ -614
D_TOTAL equ -618
D_QUERY equ -620
D_RUNX equ -624
public DibSetRGB,DibSetRLE,DibGetRGB,DibGetRLE
DWRITE macro
 call DibFastWrite
endm
DGETINDEX macro
 call DibGetPackedIndex
endm
DPUTINDEX macro
 push cs
 call near ptr DibPutIndex
endm
DREWIND macro
 xor ax,ax
 xor dx,dx
 push cs
 call near ptr DibStreamSeek
endm

DibReadColour proc near
 cmp word ptr [bp+D_BPP],24
 je DRC24
 cmp word ptr [bp+D_BPP],8
 je DRC8
 cmp word ptr [bp+D_PHASE],0
 jne DRCBuffered
 DREAD
 DNEED nc,DRCBad
 mov [bp+D_PACK],ax
DRCBuffered:
 mov ax,[bp+D_PACK]
 cmp word ptr [bp+D_BPP],4
 jne DRC1
 cmp word ptr [bp+D_PHASE],0
 jne DRC4Low
 shr ax,4
DRC4Low:
 and ax,15
 xor word ptr [bp+D_PHASE],1
 jmp DRCIndex
DRC1:
 mov cx,7
 sub cx,[bp+D_PHASE]
 shr ax,cl
 and ax,1
 inc word ptr [bp+D_PHASE]
 and word ptr [bp+D_PHASE],7
 jmp DRCIndex
DRC8:
 DREAD
 DNEED nc,DRCBad
DRCIndex:
 cmp ax,[bp+D_USED]
 DNEED b,DRCBad
 mov si,ax
 mov al,[bp+si+D_CACHE]
 xor ah,ah
 clc
 ret
DRC24:
 DREAD
 DNEED nc,DRCBad
 mov byte ptr [bp+D_RGB],al
 DREAD
 DNEED nc,DRCBad
 mov byte ptr [bp+D_RGB+1],al
 DREAD
 DNEED nc,DRCBad
 mov ah,byte ptr [bp+D_RGB+1]
 mov dl,byte ptr [bp+D_RGB]
 call DibColor
 mov al,dh
 xor ah,ah
 clc
 ret
DRCBad:
 stc
 ret
DibReadColour endp

DibSetRGB proc far
 push si
 push di
 DREWIND
 DNEED nc,DSRGBBad
 mov word ptr [bp+D_ROW],0
 mov word ptr [bp+D_LINEEND],0
 mov word ptr [bp+D_LINEEND+2],0
 mov word ptr [bp+D_PACK],0          ; per-call 24-bit colour cache empty
DSRGBRow:
 mov ax,[bp+D_STRIDE]
 mov dx,[bp+D_STRIDE+2]
 add [bp+D_LINEEND],ax
 adc [bp+D_LINEEND+2],dx
 mov word ptr [bp+D_COL],0
 mov word ptr [bp+D_PHASE],0
 call DibRowFast                     ; packed row or scalar
 jnc DSRGBRowDone
 or ax,ax
 DNEED z,DSRGBBad
DSRGBPixel:
 call DibReadColour
 DNEED nc,DSRGBBad
 cmp word ptr [bp+D_PASS],0
 je DSRGBNext
 DPUTINDEX
 DNEED nc,DSRGBBad
DSRGBNext:
 inc word ptr [bp+D_COL]
 mov ax,[bp+D_COL]
 cmp ax,[bp+D_WIDTH]
 jb DSRGBPixel
DSRGBRowDone:
 mov ax,[bp+D_LINEEND]
 mov dx,[bp+D_LINEEND+2]
 push cs
 call near ptr DibStreamSeek
 DNEED nc,DSRGBBad
 inc word ptr [bp+D_ROW]
 mov ax,[bp+D_ROW]
 cmp ax,[bp+D_ROWS]
 jb DSRGBRow
 pop di
 pop si
 clc
 retf
DSRGBBad:
 pop di
 pop si
 stc
 retf
DibSetRGB endp

; 0.14: packed BI_RGB rows; complete validation precedes output.
; Based on the isolated Claude experiment, with bounded scalar restart.
; Each output row maps its destination span once (PreparePoint for ordinary
; memory bitmaps, PrepareScreenSpan for the screen), seeks and maps its source
; bytes once, and packs up to eight pixel indices into one byte per plane.
; Rows whose source bytes cross the mapped chunk, huge destinations, Get and
; RLE keep the existing scalar row. The validation pass checks every palette
; index of every requested row with the same row mapping, before any output.
; Frame words: Get-only RLE scratch is reused; -638/-640 were unused.
DF_LEFTX equ D_FIRST            ; pixels remaining in the row
DF_GROUP equ D_SECOND           ; pixels in the current group
DF_START equ D_RUN              ; start bit of the current group
DF_BYTES equ D_RUNX             ; source bytes this row needs
DF_COL equ D_TEMP               ; DWORD byte offset of the first column
DF_INDEX equ -638               ; destination byte index within the span
DF_MASK equ -640                ; current group's coverage mask (low byte)

DFACC4 macro                    ; cache byte in AL -> planes DL,DH,BL,BH
 shr al,1
 adc dl,dl
 shr al,1
 adc dh,dh
 shr al,1
 adc bl,bl
 shr al,1
 adc bh,bh
endm
DFACC1 macro                    ; mono flag (bit 4) -> DL
 shr al,5
 adc dl,dl
endm
DFFETCH8 macro                  ; AH is zero throughout the loops
 lodsb
 mov di,ax
 mov al,[bp+di+D_CACHE]
endm
DFFETCH4 macro
 local low,go
 cmp word ptr [bp+D_PHASE],0
 jne low
 lodsb
 mov byte ptr [bp+D_PACK],al
 shr al,4
 mov word ptr [bp+D_PHASE],1
 jmp go
low:
 mov al,byte ptr [bp+D_PACK]
 and al,15
 mov word ptr [bp+D_PHASE],0
go:
 mov di,ax
 mov al,[bp+di+D_CACHE]
endm
DFFETCH1 macro
 local bit
 cmp word ptr [bp+D_PHASE],0
 jne bit
 lodsb
 mov byte ptr [bp+D_PACK],al
 mov word ptr [bp+D_PHASE],8
bit:
 dec word ptr [bp+D_PHASE]
 shl byte ptr [bp+D_PACK],1
 mov ax,0
 adc ax,0
 mov di,ax
 mov al,[bp+di+D_CACHE]
endm
DFFETCH24 macro                 ; last-colour cache in D_RGB (B,G,R,flags)
 local miss0,miss1,miss2,hit
 lodsb
 cmp al,byte ptr [bp+D_RGB]
 jne miss0
 lodsb
 cmp al,byte ptr [bp+D_RGB+1]
 jne miss1
 lodsb
 cmp al,byte ptr [bp+D_RGB+2]
 jne miss2
 cmp word ptr [bp+D_PACK],0
 je miss2
 mov al,byte ptr [bp+D_RGB+3]
 jmp hit
miss0:
 lodsb
miss1:
 lodsb
miss2:
 call DFColour24
hit:
endm
DFLOOP macro name,fetch,acc
name:
 fetch
 acc
 loop name
 jmp DRFStore
endm

; DS:SI just past one pixel's B,G,R. Quantize through the driver's table,
; cache the result, return AL = flags with AH = 0. BX/CX/DX/SI/DI preserved.
DFColour24 proc near
 push bx
 push cx
 push dx
 push ds
 mov al,[si-1]
 mov ah,[si-2]
 mov dl,[si-3]
 mov byte ptr [bp+D_RGB+2],al
 mov byte ptr [bp+D_RGB+1],ah
 mov byte ptr [bp+D_RGB],dl
 mov bx,seg DriverState
 mov ds,bx
 call DibColor
 pop ds
 mov byte ptr [bp+D_RGB+3],dh
 mov word ptr [bp+D_PACK],1
 mov al,dh
 xor ah,ah
 pop dx
 pop cx
 pop bx
 ret
DFColour24 endp

; AL packed byte, DI = S_ADDR table index. Store at the group's destination
; byte, preserving pixels outside the coverage mask. DI advances one entry.
DRFPlane proc near
 push bx
 mov es,[bp+S_ADDR+di+2]
 mov bx,[bp+S_ADDR+di]
 add bx,[bp+DF_INDEX]
 mov ah,byte ptr [bp+DF_MASK]
 cmp ah,0ffh
 je DRFPlaneFull
 not ah
 and ah,es:[bx]
 or al,ah
DRFPlaneFull:
 mov es:[bx],al
 pop bx
 add di,4
 ret
DRFPlane endp

; Seek the stream to this row's start (D_LINEEND already includes the row).
DRFRowStart proc near
 mov ax,[bp+D_LINEEND]
 mov dx,[bp+D_LINEEND+2]
 sub ax,[bp+D_STRIDE]
 sbb dx,[bp+D_STRIDE+2]
 push cs
 call near ptr DibStreamSeek
 ret
DRFRowStart endp

; CF=0: the row is complete (or needs nothing). CF=1 and AX=0: run the
; existing scalar row from the row start. CF=1 and AX=1: invalid input.
DibRowFast proc near
 cld                            ; Windows may enter with DF set; this path uses LODSB
 cmp word ptr [bp+D_COMP],0
 DNEED e,DRFScalar
 cmp word ptr [bp+D_ACTION],1
 DNEED ne,DRFScalar
 cmp word ptr [bp+D_PASS],0
 DNEED ne,DRFValidate
 mov ax,[bp+D_ROW]
 cmp ax,[bp+D_TOP]
 DNEED ae,DRFSkip
 cmp ax,[bp+D_BOTTOM]
 DNEED b,DRFSkip
 cmp word ptr [bp+S_KIND],0
 DNEED e,DRFLocate
 cmp word ptr [bp+S_DESC+22],0
 DNEED e,DRFScalar
DRFLocate:
 mov ax,[bp+D_LEFT]
 mov [bp+D_COL],ax
 call DibLocate
 DNEED nc,DRFScalar
 mov ax,[bp+D_RIGHT]
 sub ax,[bp+D_LEFT]
 mov [bp+DF_LEFTX],ax
 cmp word ptr [bp+S_KIND],0
 DNEED e,DRFScreen
 call DibPoint
 DNEED nc,DRFScalar
 jmp DRFSource
DRFScreen:
 mov cx,[bp+S_X]
 add cx,ax
 dec cx
 shr cx,3
 mov ax,[bp+S_X]
 shr ax,3
 sub cx,ax
 inc cx
 call DibScreenSpan
 DNEED nc,DRFScalar
DRFSource:
 ; first needed source byte and the byte count of the clipped columns
 mov ax,[bp+D_LEFT]
 mov cx,[bp+D_RIGHT]
 cmp word ptr [bp+D_BPP],24
 DNEED e,DRFNot24
 mov bx,3
 mul bx
 mov [bp+DF_COL],ax
 mov [bp+DF_COL+2],dx
 mov ax,[bp+DF_LEFTX]
 mul bx
 or dx,dx
 DNEED z,DRFScalar
 mov [bp+DF_BYTES],ax
 jmp DRFSeek
DRFNot24:
 cmp word ptr [bp+D_BPP],8
 DNEED e,DRFNot8
 mov [bp+DF_COL],ax
 mov word ptr [bp+DF_COL+2],0
 mov ax,[bp+DF_LEFTX]
 mov [bp+DF_BYTES],ax
 jmp DRFSeek
DRFNot8:
 cmp word ptr [bp+D_BPP],4
 DNEED e,DRFOne
 shr ax,1
 mov [bp+DF_COL],ax
 mov word ptr [bp+DF_COL+2],0
 inc cx
 shr cx,1
 sub cx,ax
 mov [bp+DF_BYTES],cx
 jmp DRFSeek
DRFOne:
 shr ax,3
 mov [bp+DF_COL],ax
 mov word ptr [bp+DF_COL+2],0
 add cx,7
 shr cx,3
 sub cx,ax
 mov [bp+DF_BYTES],cx
DRFSeek:
 mov ax,[bp+D_LINEEND]
 mov dx,[bp+D_LINEEND+2]
 sub ax,[bp+D_STRIDE]
 sbb dx,[bp+D_STRIDE+2]
 add ax,[bp+DF_COL]
 adc dx,[bp+DF_COL+2]
 push cs
 call near ptr DibStreamSeek
 DNEED nc,DRFScalar
 call DibStreamMap
 DNEED nc,DRFScalar
 mov ax,[bp+ST_OFF]
 add ax,[bp+DF_BYTES]
 DNEED nc,DRFScalar
 dec ax
 cmp ax,[bp+ST_END]
 DNEED be,DRFScalar
 ; the row is eligible: read it directly from the mapped chunk
 push ds
 mov ds,[bp+ST_SEL]
 mov si,[bp+ST_OFF]
 cmp word ptr [bp+D_BPP],4
 DNEED e,DRFNotOdd4
 test word ptr [bp+D_LEFT],1
 DNEED nz,DRFPhased
 lodsb
 mov byte ptr [bp+D_PACK],al
 mov word ptr [bp+D_PHASE],1
 jmp DRFPhased
DRFNotOdd4:
 cmp word ptr [bp+D_BPP],1
 DNEED e,DRFPhased
 mov cx,[bp+D_LEFT]
 and cx,7
 DNEED nz,DRFPhased
 lodsb
 shl al,cl
 mov byte ptr [bp+D_PACK],al
 mov ax,8
 sub ax,cx
 mov [bp+D_PHASE],ax
DRFPhased:
 mov word ptr [bp+DF_INDEX],0
 mov ax,[bp+S_X]
 and ax,7
 mov [bp+DF_START],ax
 mov cx,8
 sub cx,ax
DRFGroup:
 cmp cx,[bp+DF_LEFTX]
 DNEED a,DRFGroupSize
 mov cx,[bp+DF_LEFTX]
DRFGroupSize:
 mov [bp+DF_GROUP],cx
 sub [bp+DF_LEFTX],cx
 xor dx,dx
 xor bx,bx
 xor ah,ah
 cmp word ptr [bp+S_PLANES],1
 DNEED ne,DRFMono
 cmp word ptr [bp+D_BPP],8
 DNEED ne,DRFL8C
 cmp word ptr [bp+D_BPP],4
 DNEED ne,DRFL4C
 cmp word ptr [bp+D_BPP],1
 DNEED ne,DRFL1C
 jmp DRFL24C
DRFMono:
 cmp word ptr [bp+D_BPP],8
 DNEED ne,DRFL8M
 cmp word ptr [bp+D_BPP],4
 DNEED ne,DRFL4M
 cmp word ptr [bp+D_BPP],1
 DNEED ne,DRFL1M
 jmp DRFL24M
 DFLOOP DRFL8C,<DFFETCH8>,<DFACC4>
 DFLOOP DRFL4C,<DFFETCH4>,<DFACC4>
 DFLOOP DRFL1C,<DFFETCH1>,<DFACC4>
 DFLOOP DRFL24C,<DFFETCH24>,<DFACC4>
 DFLOOP DRFL8M,<DFFETCH8>,<DFACC1>
 DFLOOP DRFL4M,<DFFETCH4>,<DFACC1>
 DFLOOP DRFL1M,<DFFETCH1>,<DFACC1>
 DFLOOP DRFL24M,<DFFETCH24>,<DFACC1>
DRFStore:
 ; shift = 8 - start - k; mask = (FFh shr (8 - k)) shl shift
 mov cx,8
 sub cx,[bp+DF_GROUP]
 mov al,0ffh
 shr al,cl
 mov cx,8
 sub cx,[bp+DF_START]
 sub cx,[bp+DF_GROUP]
 shl al,cl
 mov byte ptr [bp+DF_MASK],al
 jcxz DRFShifted
 shl dl,cl
 shl dh,cl
 shl bl,cl
 shl bh,cl
DRFShifted:
 mov word ptr [bp+DF_START],0
 xor di,di
 mov al,dl
 call DRFPlane
 cmp word ptr [bp+S_PLANES],1
 DNEED ne,DRFStored
 mov al,dh
 call DRFPlane
 mov al,bl
 call DRFPlane
 mov al,bh
 call DRFPlane
DRFStored:
 inc word ptr [bp+DF_INDEX]
 mov cx,8
 cmp word ptr [bp+DF_LEFTX],0
 DNEED e,DRFGroup
 pop ds
 clc
 ret
DRFSkip:
 clc
 ret
DRFScalar:
 ; Restart the complete row, including clipped columns, with fresh phase.
 ; Scalar RGB24 overwrites part of D_RGB: invalidate its packed-row cache.
 mov word ptr [bp+D_COL],0
 mov word ptr [bp+D_PHASE],0
 mov word ptr [bp+D_PACK],0
 call DRFRowStart
 jc DRFSeekBad
 xor ax,ax
 stc
 ret
DRFSeekBad:
 mov ax,1
 stc
 ret
DRFInvalid:
 pop ds
 mov ax,1
 stc
 ret

; Validation pass: every index of columns 0..W-1 of this row must be below
; D_USED. 24-bit needs no index check; 1-bit only when fewer than two entries.
DRFValidate:
 cmp word ptr [bp+D_BPP],24
 DNEED ne,DRFSkip
 cmp word ptr [bp+D_BPP],1
 DNEED e,DRFValidateRow
 cmp word ptr [bp+D_USED],2
 DNEED b,DRFSkip
DRFValidateRow:
 call DRFRowStart
 DNEED nc,DRFScalar
 call DibStreamMap
 DNEED nc,DRFScalar
 mov ax,[bp+D_WIDTH]
 mul word ptr [bp+D_BPP]
 add ax,7
 adc dx,0
 mov cl,3
DRFValidateDiv:
 shr dx,1
 rcr ax,1
 dec cl
 DNEED z,DRFValidateDiv
 or dx,dx
 DNEED z,DRFScalar
 mov [bp+DF_BYTES],ax
 add ax,[bp+ST_OFF]
 DNEED nc,DRFScalar
 dec ax
 cmp ax,[bp+ST_END]
 DNEED be,DRFScalar
 push ds
 mov ds,[bp+ST_SEL]
 mov si,[bp+ST_OFF]
 cmp word ptr [bp+D_BPP],8
 DNEED e,DRFValidate4
 mov cx,[bp+D_WIDTH]
 xor ah,ah
DRFV8:
 lodsb
 cmp ax,[bp+D_USED]
 DNEED b,DRFInvalid
 loop DRFV8
 jmp DRFValidated
DRFValidate4:
 cmp word ptr [bp+D_BPP],4
 DNEED e,DRFValidate1
 mov cx,[bp+D_WIDTH]
 shr cx,1
 jcxz DRFV4Tail
DRFV4:
 lodsb
 mov ah,al
 shr al,4
 cmp al,byte ptr [bp+D_USED]
 DNEED b,DRFInvalid
 mov al,ah
 and al,15
 cmp al,byte ptr [bp+D_USED]
 DNEED b,DRFInvalid
 loop DRFV4
DRFV4Tail:
 test word ptr [bp+D_WIDTH],1
 DNEED nz,DRFValidated
 lodsb
 shr al,4
 cmp al,byte ptr [bp+D_USED]
 DNEED b,DRFInvalid
 jmp DRFValidated
DRFValidate1:
 ; a single palette entry: every used bit must be zero
 mov cx,[bp+D_WIDTH]
 shr cx,3
 jcxz DRFV1Tail
DRFV1:
 lodsb
 or al,al
 DNEED z,DRFInvalid
 loop DRFV1
DRFV1Tail:
 mov cx,[bp+D_WIDTH]
 and cx,7
 DNEED nz,DRFValidated
 lodsb
 mov ah,0ffh
 neg cl
 add cl,8
 shl ah,cl
 and al,ah
 DNEED z,DRFInvalid
DRFValidated:
 pop ds
 clc
 ret
DibRowFast endp

DibSetRLE proc far
 push si
 push di
 mov word ptr [bp+DG_VALID],0
 DREWIND
 DNEED nc,DSRLEBad
 push cs
 call near ptr DibRleReset
 DNEED nc,DSRLEBad
DSRLEPixel:
 push cs
 call near ptr DibRleNext
 DNEED nc,DSRLEBad
 cmp ax,0ffffh
 je DSRLEDone
 cmp word ptr [bp+D_PASS],0
 je DSRLEPixel
 mov [bp+D_COL],bx
 mov [bp+D_ROW],dx
 mov si,ax
 mov al,[bp+si+D_CACHE]
 xor ah,ah
 call DibRleAccumulate
 DNEED nc,DSRLEBad
 jmp DSRLEPixel
DSRLEDone:
 call DibRleFlush
 mov ax,[bp+ST_POS]
 mov dx,[bp+ST_POS+2]
 mov [bp+D_ACTSIZE],ax
 mov [bp+D_ACTSIZE+2],dx
 pop di
 pop si
 clc
 retf
DSRLEBad:
 pop di
 pop si
 stc
 retf
DibSetRLE endp

; Write one already-converted output index with zero-initialized tail bits.
DibWriteColour proc near
 cmp word ptr [bp+D_BPP],24
 je DWC24
 cmp word ptr [bp+D_BPP],8
 je DWCByte
 cmp word ptr [bp+D_BPP],4
 jne DWC1
 cmp word ptr [bp+D_PHASE],0
 jne DWC4Low
 shl ax,4
 mov [bp+D_PACK],ax
 mov word ptr [bp+D_PHASE],1
 clc
 ret
DWC4Low:
 or ax,[bp+D_PACK]
 mov word ptr [bp+D_PHASE],0
 jmp DWCByte
DWC1:
 or ax,ax
 jz DWC1Zero
 mov ax,80h
 mov cx,[bp+D_PHASE]
 shr ax,cl
 or [bp+D_PACK],ax
DWC1Zero:
 inc word ptr [bp+D_PHASE]
 cmp word ptr [bp+D_PHASE],8
 jne DWCDone
 mov ax,[bp+D_PACK]
 mov word ptr [bp+D_PHASE],0
 mov word ptr [bp+D_PACK],0
DWCByte:
 DWRITE
 ret
DWC24:
 ; A monochrome Get index is 0/1; 24-bit output has no palette to expand it.
 cmp word ptr [bp+S_PLANES],1
 jne DWC24RGB
 or ax,ax
 jz DWC24RGB
 mov ax,15
DWC24RGB:
 call DibIndex
 mov [bp+D_RGB],ax
 mov byte ptr [bp+D_RGB+2],dl
 mov al,dl
 DWRITE
 DNEED nc,DWCBad
 mov al,byte ptr [bp+D_RGB+1]
 DWRITE
 DNEED nc,DWCBad
 mov al,byte ptr [bp+D_RGB]
 DWRITE
 ret
DWCDone:
 clc
 ret
DWCBad:
 stc
 ret
DibWriteColour endp

DibGetRGB proc far
 push si
 push di
 mov word ptr [bp+DG_VALID],0
 DREWIND
 DNEED nc,DGRGBBad
 mov word ptr [bp+D_ROW],0
 mov word ptr [bp+D_LINEEND],0
 mov word ptr [bp+D_LINEEND+2],0
DGRGBRow:
 mov ax,[bp+D_STRIDE]
 mov dx,[bp+D_STRIDE+2]
 add [bp+D_LINEEND],ax
 adc [bp+D_LINEEND+2],dx
 mov word ptr [bp+D_COL],0
 mov word ptr [bp+D_PHASE],0
 mov word ptr [bp+D_PACK],0
DGRGBPixel:
 DGETINDEX
 DNEED nc,DGRGBBad
 call DibWriteColour
 DNEED nc,DGRGBBad
 inc word ptr [bp+D_COL]
 mov ax,[bp+D_COL]
 cmp ax,[bp+D_WIDTH]
 jb DGRGBPixel
 cmp word ptr [bp+D_PHASE],0
 je DGRGBPad
 mov ax,[bp+D_PACK]
 DWRITE
 DNEED nc,DGRGBBad
DGRGBPad:
 mov ax,[bp+ST_POS+2]
 cmp ax,[bp+D_LINEEND+2]
 jb DGRGBZero
 ja DGRGBBad
 mov ax,[bp+ST_POS]
 cmp ax,[bp+D_LINEEND]
 je DGRGBNext
 ja DGRGBBad
DGRGBZero:
 xor ax,ax
 DWRITE
 DNEED nc,DGRGBBad
 jmp DGRGBPad
DGRGBNext:
 inc word ptr [bp+D_ROW]
 mov ax,[bp+D_ROW]
 cmp ax,[bp+D_ROWS]
 DNEED ae,DGRGBRow
 pop di
 pop si
 clc
 retf
DGRGBBad:
 pop di
 pop si
 stc
 retf
DibGetRGB endp

; Every byte is counted in both passes; only pass 1 writes payload bytes.
DibEmitByte proc near
 add word ptr [bp+D_OUTSIZE],1
 adc word ptr [bp+D_OUTSIZE+2],0
 jc DEBBad
 cmp word ptr [bp+D_PASS],0
 je DEBDone
 DWRITE
 ret
DEBDone:
 clc
 ret
DEBBad:
 stc
 ret
DibEmitByte endp

DibGetRLE proc far
 push si
 push di
 mov word ptr [bp+DG_VALID],0
 mov word ptr [bp+D_OUTSIZE],0
 mov word ptr [bp+D_OUTSIZE+2],0
 cmp word ptr [bp+D_PASS],0
 je DGRLEStart
 DREWIND
 DNEED nc,DGRLEBad
DGRLEStart:
 mov word ptr [bp+D_ROW],0
DGRLERow:
 mov word ptr [bp+D_COL],0
DGRLEPacket:
 mov ax,[bp+D_COL]
 mov [bp+D_RUNX],ax
 DGETINDEX
 DNEED nc,DGRLEBad
 mov [bp+D_FIRST],ax
 mov [bp+D_SECOND],ax
 cmp word ptr [bp+D_BPP],4
 jne DGRLERunStart
 mov ax,[bp+D_COL]
 inc ax
 cmp ax,[bp+D_WIDTH]
 jae DGRLERunStart
 mov [bp+D_COL],ax
 DGETINDEX
 DNEED nc,DGRLEBad
 mov [bp+D_SECOND],ax
 dec word ptr [bp+D_COL]
DGRLERunStart:
 mov word ptr [bp+D_RUN],1
DGRLELook:
 cmp word ptr [bp+D_RUN],255
 je DGRLEEmit
 mov ax,[bp+D_RUNX]
 add ax,[bp+D_RUN]
 cmp ax,[bp+D_WIDTH]
 jae DGRLEEmit
 mov [bp+D_COL],ax
 DGETINDEX
 DNEED nc,DGRLEBad
 mov bx,[bp+D_RUNX]
 mov [bp+D_COL],bx
 mov bx,[bp+D_FIRST]
 cmp word ptr [bp+D_BPP],8
 je DGRLECompare
 test word ptr [bp+D_RUN],1
 jz DGRLECompare
 mov bx,[bp+D_SECOND]
DGRLECompare:
 cmp ax,bx
 jne DGRLEEmit
 inc word ptr [bp+D_RUN]
 jmp DGRLELook
DGRLEEmit:
 mov ax,[bp+D_RUN]
 call DibEmitByte
 DNEED nc,DGRLEBad
 mov ax,[bp+D_FIRST]
 cmp word ptr [bp+D_BPP],8
 je DGRLEValue
 shl ax,4
 or ax,[bp+D_SECOND]
DGRLEValue:
 call DibEmitByte
 DNEED nc,DGRLEBad
 mov ax,[bp+D_RUNX]
 add ax,[bp+D_RUN]
 mov [bp+D_COL],ax
 cmp ax,[bp+D_WIDTH]
 DNEED ae,DGRLEPacket
 xor ax,ax
 call DibEmitByte
 DNEED nc,DGRLEBad
 xor ax,ax
 call DibEmitByte
 DNEED nc,DGRLEBad
 inc word ptr [bp+D_ROW]
 mov ax,[bp+D_ROW]
 cmp ax,[bp+D_ROWS]
 DNEED ae,DGRLERow
 xor ax,ax
 call DibEmitByte
 DNEED nc,DGRLEBad
 mov ax,1
 call DibEmitByte
 DNEED nc,DGRLEBad
 pop di
 pop si
 clc
 retf
DGRLEBad:
 pop di
 pop si
 stc
 retf
DibGetRLE endp
