; Fixed 286 four-plane AND/XOR cursor. No Windows, DOS or BIOS calls in service.
; SetCursor is synchronous; MoveCursor and CheckCursor may interrupt it or
; each other. Shared service scratch is owned only while CurBusy is set.
; Every critical section uses saved FLAGS: no unconditional STI.
public SetCursor, MoveCursor, CheckCursor, CursorService, CursorRemove
public CursorPaint, CursorTransfer, CursorMasks, CursorOffset, CursorClip
public CursorPrepare

CUR_ENTER macro
    mov ax,ds
    nop
    inc bp
    push bp
    mov bp,sp
    push ds
    pusha
    push es
    mov ax,seg DriverState
    mov ds,ax
    cld
endm
CUR_EXIT macro amount
    pop es
    popa
    pop ds
    pop bp
    dec bp
    retf amount
endm

SetCursor proc far
    CUR_ENTER
    pushf
    cli
    cmp CurBusy,0
    je CSClaim
    popf                         ; synchronous entry cannot preempt a service
    jmp CSDone                   ; defensive refusal of unsupported reentry
CSClaim:
    mov CurBusy,1
    popf
    mov ax,[bp+8]
    or ax,[bp+6]
    jnz CSShape
    mov CurLogical,0
    mov CurPending,1
    jmp CSRelease
CSShape:
    mov ax,[bp+8]
    mov bx,[bp+6]
    mov cx,12
    call CheckReadSpan
    jnc CSHeader
    jmp CSRelease
CSHeader:
    mov di,DataOFFSET CurShape0
    cmp di,CurSlot
    jne CSOther
    mov di,DataOFFSET CurShape1
CSOther:
    mov CurNewSlot,di
    push ds
    push ds
    pop es
    mov ds,ax
    mov si,bx
    mov cx,6
    rep movsw
    pop ds
    mov si,CurNewSlot
    ; Windows 3.0 USER stores planes=1, bits/pixel=1 at offsets 10/11.
    ; Also retain the legacy zero marker accepted by the original fixtures.
    cmp word ptr [si+10],0101h
    je CSMono
    cmp word ptr [si+10],0
    je CSMono
    jmp CSRelease
CSMono:
    mov ax,[si+4]
    dec ax
    cmp ax,31
    jbe CSWidth
    jmp CSRelease
CSWidth:
    mov ax,[si+6]
    dec ax
    cmp ax,31
    jbe CSHeight
    jmp CSRelease
CSHeight:
    mov ax,[si+8]
    cmp ax,2
    je CSStride
    cmp ax,4
    je CSStride
    jmp CSRelease
CSStride:
    mov cl,3
    shl ax,cl
    cmp ax,[si+4]
    jae CSCovers
    jmp CSRelease
CSCovers:
    mov ax,[si+8]
    mul word ptr [si+6]
    push ax                      ; owned mask byte count, <=128
    shl ax,1
    add ax,12
    mov cx,ax
    mov ax,[bp+8]
    mov bx,[bp+6]
    call CheckReadSpan
    pop dx
    jc CSRelease
    mov CurMaskBytes,dx
    push ds
    push ds
    pop es
    mov di,CurNewSlot
    mov ds,ax
    mov si,bx
    rep movsb                    ; caller's complete shape copied before return
    pop ds
    mov ax,CurNewSlot
    mov CurSlot,ax
    mov si,DataOFFSET CurCache
    mov cx,8
CSInvalidateCache:
    mov word ptr [si],0
    add si,332
    loop CSInvalidateCache
    mov CurLogical,1
    mov CurPending,1
CSRelease:
    pushf
    cli
    mov CurBusy,0
    popf
    call CursorService
CSDone:
    CUR_EXIT 4
SetCursor endp

MoveCursor proc far
    CUR_ENTER
    pushf
    cli
    mov ax,[bp+8]
    cmp ax,CurX
    jne CMPositionChanged
    mov ax,[bp+6]
    cmp ax,CurY
    je CMPositionReady
CMPositionChanged:
    mov ax,[bp+8]
    mov CurX,ax
    mov ax,[bp+6]
    mov CurY,ax
    mov CurPending,1
CMPositionReady:
    popf
    call CursorService
    CUR_EXIT 4
MoveCursor endp

CheckCursor proc far
    CUR_ENTER
    call CursorService
    CUR_EXIT 0
CheckCursor endp

; Preserve all registers. A request arriving during this single update is
; left pending; never chase an unbounded sequence of mouse interrupts.
CursorService proc near
    pusha
    push es
    pushf
    cli
    cmp CurBusy,0
    jne CRBlocked
    cmp CurPending,0
    je CRBlocked
    cmp DriverState,ACTIVE
    jne CRBlocked
    cmp ScreenGate,1
    jne CRBlocked
    cmp BridgeBusy,0
    jne CRBlocked
    cmp DrawDepth,0
    jne CRBlocked
    mov CurBusy,1
    mov CurPending,0
    mov ax,CurX
    mov CurWorkX,ax
    mov ax,CurY
    mov CurWorkY,ax
    popf
    cmp CurLogical,0
    je CRHide
    call CursorClip
    jc CRHide
    ; Calculate the next masks before erasing the old pointer. Save geometry
    ; remains untouched until CursorRemove has restored the old background.
    call CursorPrepare
    call CursorRemove
    call CursorPaint
    jmp CRRelease
CRHide:
    call CursorRemove
CRRelease:
    pushf
    cli
    mov CurBusy,0
CRBlocked:
    popf
    pop es
    popa
    ret
CursorService endp

; CurBusy ownership required. Mode changes explicitly invalidate CurDrawn;
; the epoch check additionally prevents restoring an old reconstruction.
CursorRemove proc near
    pusha
    push es
    cmp CurDrawn,0
    je CRemoveDone
    mov CurDrawn,0
    mov ax,Generation
    cmp ax,CurEpoch
    jne CRemoveDone
    mov CurOp,0
    call CursorTransfer
CRemoveDone:
    pop es
    popa
    ret
CursorRemove endp

; Signed 32-bit hotspot subtraction, narrowed only after rejecting values
; which cannot intersect the fixed 640x400 surface with a <=32x32 shape.
CursorClip proc near
    mov si,CurSlot
    mov ax,CurWorkX
    cwd
    mov bx,[si]
    mov cx,bx
    sar cx,15
    sub ax,bx
    sbb dx,cx
    or dx,dx
    jz CCXPositive
    cmp dx,0ffffh
    je CCFar0
    jmp CCEmpty
CCFar0:
    cmp ax,0ffe0h
    jae CCFar1
    jmp CCEmpty
CCFar1:
    mov CurOriginX,ax
    add ax,[si+4]
    jg CCFar2
    jmp CCEmpty
CCFar2:
    mov CurRight,ax
    mov CurLeft,0
    jmp CCY
CCXPositive:
    cmp ax,640
    jb CCFar3
    jmp CCEmpty
CCFar3:
    mov CurOriginX,ax
    mov CurLeft,ax
    add ax,[si+4]
    cmp ax,640
    jbe CCXRight
    mov ax,640
CCXRight:
    mov CurRight,ax
CCY:
    mov ax,CurWorkY
    cwd
    mov bx,[si+2]
    mov cx,bx
    sar cx,15
    sub ax,bx
    sbb dx,cx
    or dx,dx
    jz CCYPositive
    cmp dx,0ffffh
    je CCFar4
    jmp CCEmpty
CCFar4:
    cmp ax,0ffe0h
    jae CCFar5
    jmp CCEmpty
CCFar5:
    mov CurOriginY,ax
    add ax,[si+6]
    jg CCFar6
    jmp CCEmpty
CCFar6:
    mov CurBottom,ax
    mov CurTop,0
    clc
    ret
CCYPositive:
    cmp ax,400
    jb CCFar7
    jmp CCEmpty
CCFar7:
    mov CurOriginY,ax
    mov CurTop,ax
    add ax,[si+6]
    cmp ax,400
    jbe CCYBottom
    mov ax,400
CCYBottom:
    mov CurBottom,ax
    clc
    ret
CCEmpty:
    stc
    ret
CursorClip endp

CursorPrepare proc near
    mov ax,CurLeft
    mov cl,3
    shr ax,cl
    mov CurNextX,ax
    mov bx,CurRight
    dec bx
    shr bx,cl
    inc bx
    sub bx,ax
    mov CurNextW,bx
    mov ax,CurTop
    mov CurNextY,ax
    mov CurRow,ax
    mov bx,CurBottom
    sub bx,ax
    mov CurNextH,bx
    mov CurRowsLeft,bx
    call CursorCachedMasks
    jc CPBuild
    ret
CPBuild:
    ; Relative source phase and clip coverage do not change between rows.
    mov ax,CurNextX
    shl ax,3
    mov bx,ax
    sub ax,CurOriginX
    mov cx,ax
    and cx,7
    mov CurMaskShift,cx
    sar ax,3
    mov CurMaskSource,ax
    xor di,di
CPGeometry:
    mov al,0ffh
    mov dx,CurLeft
    sub dx,bx
    jle CPGeomRight
    mov cx,dx
    shr al,cl
CPGeomRight:
    mov dx,CurRight
    sub dx,bx
    cmp dx,8
    jae CPGeomSave
    mov cx,8
    sub cx,dx
    mov ah,0ffh
    shl ah,cl
    and al,ah
CPGeomSave:
    mov CurMaskCover[di],al
    add bx,8
    inc di
    cmp di,CurNextW
    jb CPGeometry
    mov word ptr CurAndPad,0ffffh
    mov word ptr CurAndPad+2,0ffffh
    mov word ptr CurAndPad+4,0ffffh
    mov word ptr CurXorPad,0
    mov word ptr CurXorPad+2,0
    mov word ptr CurXorPad+4,0
    mov CurIndex,0
CPRow:
    mov ax,CurNextX
    mov CurByte,ax
    mov ax,CurNextW
    mov CurBytesLeft,ax
    mov si,CurSlot
    mov ax,CurRow
    sub ax,CurOriginY
    mul word ptr [si+8]
    add ax,si
    add ax,12
    mov CurAndRow,ax
    add ax,CurMaskBytes
    mov CurXorRow,ax
    call CursorLoadPaddedRow
CPByte:
    call CursorMasks
    mov bx,CurIndex
    mov al,CurAnd
    mov CurPreparedAnd[bx],al
    mov al,CurXor
    mov CurPreparedXor[bx],al
    inc CurIndex
    inc CurByte
    dec CurBytesLeft
    jnz CPByte
    inc CurRow
    dec CurRowsLeft
    jnz CPRow
    ; Publish only under CurBusy, after the complete entry has been copied.
    push ds
    pop es
    mov di,CurCacheOffset
    add di,12
    mov si,DataOFFSET CurPreparedAnd
    call CursorCopyMasks
    mov si,CurCacheOffset
    mov word ptr [si],1
    ret
CursorPrepare endp

; CF clear on a complete cache hit, set after preparing the new entry key.
; Logical shape ownership and CurBusy protect both keys and mask data.
CursorCachedMasks proc near
    mov ax,CurOriginX
    and ax,7
    mov bx,332
    mul bx
    add ax,DataOFFSET CurCache
    mov CurCacheOffset,ax
    mov si,ax
    mov ax,CurNextX
    shl ax,3
    sub ax,CurOriginX
    mov bx,CurNextY
    sub bx,CurOriginY
    mov cx,CurRight
    sub cx,CurOriginX
    cmp word ptr [si],1
    jne CCMiss
    cmp [si+2],ax
    jne CCMiss
    cmp [si+4],bx
    jne CCMiss
    cmp [si+10],cx
    jne CCMiss
    mov dx,CurNextW
    cmp [si+6],dx
    jne CCMiss
    mov dx,CurNextH
    cmp [si+8],dx
    jne CCMiss
    add si,12
    push ds
    pop es
    mov di,DataOFFSET CurPreparedAnd
    call CursorCopyMasks
    clc
    ret
CCMiss:
    mov word ptr [si],0
    mov [si+2],ax
    mov [si+4],bx
    mov [si+10],cx
    mov ax,CurNextW
    mov [si+6],ax
    mov ax,CurNextH
    mov [si+8],ax
    stc
    ret
CursorCachedMasks endp

CursorPaint proc near
    mov ax,CurNextX
    mov CurSaveX,ax
    mov ax,CurNextY
    mov CurSaveY,ax
    mov ax,CurNextW
    mov CurSaveW,ax
    mov ax,CurNextH
    mov CurSaveH,ax
    mov ax,Generation
    mov CurEpoch,ax
    mov CurOp,1
    call CursorTransfer
    mov CurDrawn,1
    ret
CursorPaint endp

; Row, plane, byte order permits contiguous restoration of each plane row.
; Whole edge bytes are saved: 32 rows * 4 planes * 5 bytes = 640 maximum.
; Prepared masks are row, byte order and are shared by the four planes.
CursorTransfer proc near
    push bp
    cld
    mov CurIndex,0
    mov CurMaskRow,0
    mov ax,CurSaveY
    mov CurRow,ax
    mov ax,CurSaveH
    mov CurRowsLeft,ax
    mov ax,CurSaveX
    mov CurByte,ax
    mov ax,CurRow
    and ax,3
    mov cl,13
    shl ax,cl
    mov di,ax
    mov ax,CurRow
    shr ax,1
    shr ax,1
    mov cx,80
    mul cx
    add ax,di
    add ax,CurByte
    mov CurRowOffset,ax
    and ax,1
    mov CurLead,ax
    mov cx,CurSaveW
    sub cx,ax
    mov ax,cx
    and ax,1
    mov CurTail,ax
    shr cx,1
    mov CurWords,cx
    ; Geometry, byte parity and operation are stable for this transfer.
    ; Borrow BP only while CurBusy owns the scratch; preserve the entry frame.
    mov bp,OFFSET CTRestore
    cmp CurOp,0
    je CTSelected
    mov ax,CurSaveW
    mov bp,OFFSET CTDraw1
    cmp ax,1
    je CTSelected
    cmp CurLead,0
    jne CTSelectOdd
    mov bp,OFFSET CTDraw2Even
    cmp ax,2
    je CTSelected
    mov bp,OFFSET CTDraw3Even
    cmp ax,3
    je CTSelected
    mov bp,OFFSET CTDraw4Even
    cmp ax,4
    je CTSelected
    mov bp,OFFSET CTDraw5Even
    jmp CTSelected
CTSelectOdd:
    mov bp,OFFSET CTDraw2Odd
    cmp ax,2
    je CTSelected
    mov bp,OFFSET CTDraw3Odd
    cmp ax,3
    je CTSelected
    mov bp,OFFSET CTDraw4Odd
    cmp ax,4
    je CTSelected
    mov bp,OFFSET CTDraw5Odd
CTSelected:
    xor bx,bx                    ; save offset, consumed monotonically
CTRow:
    xor dx,dx                    ; plane selector index
    mov CurPlane,dx
public CursorPlaneTransfer
CursorPlaneTransfer:
CTPlane:
    ; Each plane base is even, so the selected parity remains valid.
    mov si,dx
    mov es,PlaneSelectors[si]
    mov di,PlaneBases[si]
    add di,CurRowOffset
    jmp bp
CTRestore:
    lea si,CurSave[bx]
    mov cx,CurSaveW
    test di,1
    jz CTRestoreWords
    movsb
    dec cx
CTRestoreWords:
    shr cx,1
    rep movsw
    jnc CTNext
    movsb
    jmp CTNext
CTNext:
    add bx,CurSaveW
    add dx,2
    mov CurPlane,dx
    cmp dx,8
    jae @F
    jmp CTPlane
@@:
    mov ax,CurSaveW
    add CurMaskRow,ax
    mov ax,CurRow
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne CTStep
    mov dx,0a050h
CTStep:
    add CurRowOffset,dx
    inc CurRow
    dec CurRowsLeft
    jz CTDone
    jmp CTRow
CTDone:
    mov CurIndex,bx
    pop bp
    ret

; Fully unrolled paint rows. Never round the 1..5 byte footprint up.
; AX/SI are scratch; BX (save base), DX (plane), BP (selected entry)
; and DI (video base) stay fixed. Each kernel joins the common advance.
CTDraw1:
    mov si,CurMaskRow
    mov al,es:[di]
    mov CurSave[bx],al
    and al,CurPreparedAnd[si]
    xor al,CurPreparedXor[si]
    mov es:[di],al
    jmp CTNext
CTDraw2Even:
    mov si,CurMaskRow
    mov ax,es:[di]
    mov word ptr CurSave[bx],ax
    and ax,word ptr CurPreparedAnd[si]
    xor ax,word ptr CurPreparedXor[si]
    mov es:[di],ax
    jmp CTNext
CTDraw2Odd:
    mov si,CurMaskRow
    mov al,es:[di]
    mov CurSave[bx],al
    and al,CurPreparedAnd[si]
    xor al,CurPreparedXor[si]
    mov es:[di],al
    mov al,es:[di+1]
    mov CurSave[bx+1],al
    and al,CurPreparedAnd[si+1]
    xor al,CurPreparedXor[si+1]
    mov es:[di+1],al
    jmp CTNext
CTDraw3Even:
    mov si,CurMaskRow
    mov ax,es:[di]
    mov word ptr CurSave[bx],ax
    and ax,word ptr CurPreparedAnd[si]
    xor ax,word ptr CurPreparedXor[si]
    mov es:[di],ax
    mov al,es:[di+2]
    mov CurSave[bx+2],al
    and al,CurPreparedAnd[si+2]
    xor al,CurPreparedXor[si+2]
    mov es:[di+2],al
    jmp CTNext
CTDraw3Odd:
    mov si,CurMaskRow
    mov al,es:[di]
    mov CurSave[bx],al
    and al,CurPreparedAnd[si]
    xor al,CurPreparedXor[si]
    mov es:[di],al
    mov ax,es:[di+1]
    mov word ptr CurSave[bx+1],ax
    and ax,word ptr CurPreparedAnd[si+1]
    xor ax,word ptr CurPreparedXor[si+1]
    mov es:[di+1],ax
    jmp CTNext
CTDraw4Even:
    mov si,CurMaskRow
    mov ax,es:[di]
    mov word ptr CurSave[bx],ax
    and ax,word ptr CurPreparedAnd[si]
    xor ax,word ptr CurPreparedXor[si]
    mov es:[di],ax
    mov ax,es:[di+2]
    mov word ptr CurSave[bx+2],ax
    and ax,word ptr CurPreparedAnd[si+2]
    xor ax,word ptr CurPreparedXor[si+2]
    mov es:[di+2],ax
    jmp CTNext
CTDraw4Odd:
    mov si,CurMaskRow
    mov al,es:[di]
    mov CurSave[bx],al
    and al,CurPreparedAnd[si]
    xor al,CurPreparedXor[si]
    mov es:[di],al
    mov ax,es:[di+1]
    mov word ptr CurSave[bx+1],ax
    and ax,word ptr CurPreparedAnd[si+1]
    xor ax,word ptr CurPreparedXor[si+1]
    mov es:[di+1],ax
    mov al,es:[di+3]
    mov CurSave[bx+3],al
    and al,CurPreparedAnd[si+3]
    xor al,CurPreparedXor[si+3]
    mov es:[di+3],al
    jmp CTNext
CTDraw5Even:
    mov si,CurMaskRow
    mov ax,es:[di]
    mov word ptr CurSave[bx],ax
    and ax,word ptr CurPreparedAnd[si]
    xor ax,word ptr CurPreparedXor[si]
    mov es:[di],ax
    mov ax,es:[di+2]
    mov word ptr CurSave[bx+2],ax
    and ax,word ptr CurPreparedAnd[si+2]
    xor ax,word ptr CurPreparedXor[si+2]
    mov es:[di+2],ax
    mov al,es:[di+4]
    mov CurSave[bx+4],al
    and al,CurPreparedAnd[si+4]
    xor al,CurPreparedXor[si+4]
    mov es:[di+4],al
    jmp CTNext
CTDraw5Odd:
    mov si,CurMaskRow
    mov al,es:[di]
    mov CurSave[bx],al
    and al,CurPreparedAnd[si]
    xor al,CurPreparedXor[si]
    mov es:[di],al
    mov ax,es:[di+1]
    mov word ptr CurSave[bx+1],ax
    and ax,word ptr CurPreparedAnd[si+1]
    xor ax,word ptr CurPreparedXor[si+1]
    mov es:[di+1],ax
    mov ax,es:[di+3]
    mov word ptr CurSave[bx+3],ax
    and ax,word ptr CurPreparedAnd[si+3]
    xor ax,word ptr CurPreparedXor[si+3]
    mov es:[di+3],ax
    jmp CTNext
CursorTransfer endp

CursorOffset proc near
    mov di,CurRowOffset
    mov bx,CurPlane
    mov ax,PlaneSelectors[bx]
    mov es,ax
    add di,PlaneBases[bx]
    ret
CursorOffset endp

; Two-byte AND/XOR windows, shifted together. Out-of-row bytes are A=1,X=0.
; Clip to the visible pixel interval so arbitrary mask padding is ignored.
; CurAndRow/CurXorRow and row offset are invocation scratch owned by CurBusy.
CursorMasks proc near
    mov di,CurByte
    sub di,CurNextX
    mov bx,CurMaskSource
    add bx,di
    ; Source window is -1..stride, padded with transparent A=1/X=0.
    mov ax,word ptr CurAndPad[bx+1]
    mov dx,word ptr CurXorPad[bx+1]
    xchg al,ah
    xchg dl,dh
    mov cx,CurMaskShift
    shl ax,cl
    shl dx,cl
    mov CurAnd,ah
    mov CurXor,dh
    mov al,CurMaskCover[di]
    and CurXor,al
    not al
    or CurAnd,al
    ret
CursorMasks endp

public CursorLoadPaddedRow
CursorLoadPaddedRow proc near
    mov si,CurAndRow
    mov ax,[si]
    mov word ptr CurAndPad+1,ax
    mov si,CurXorRow
    mov ax,[si]
    mov word ptr CurXorPad+1,ax
    mov si,CurSlot
    cmp word ptr [si+8],4
    jne CLPRDone
    mov si,CurAndRow
    mov ax,[si+2]
    mov word ptr CurAndPad+3,ax
    mov si,CurXorRow
    mov ax,[si+2]
    mov word ptr CurXorPad+3,ax
CLPRDone:
    ret
CursorLoadPaddedRow endp

; BX signed byte index. Return AL=AND and AH=XOR, preserving BX/CX/DX/DI.
; Only the current owned shape row is read, never the next row or mask.
public CursorMaskPair
CursorMaskPair proc near
    mov ax,00ffh
    or bx,bx
    js CMPairDone
    mov si,CurSlot
    cmp bx,[si+8]
    jae CMPairDone
    mov si,CurAndRow
    mov al,[si+bx]
    mov si,CurXorRow
    mov ah,[si+bx]
CMPairDone:
    ret
CursorMaskPair endp

; Copy only populated rows/columns of each 160-byte mask array. Tiny
; cursors no longer move the unused tail on every cached phase hit/miss.
; CurBusy owns source and destination; SI/DI name their AND array starts.
public CursorCopyMasks
CursorCopyMasks proc near
    mov ax,CurNextW
    mul CurNextH
    cmp ax,160
    je CCMFull
    mov bx,ax
    mov dx,2
CCMPart:
    mov cx,bx
    shr cx,1
    rep movsw
    jnc CCMPartNext
    movsb
CCMPartNext:
    add si,160
    sub si,bx
    add di,160
    sub di,bx
    dec dx
    jnz CCMPart
    ret
CCMFull:
    mov cx,160
    rep movsw
    ret
CursorCopyMasks endp
