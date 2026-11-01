; Phase 4 Output. Two complete walks: checked addresses, then writes.
; Inputs are borrowed stable GDI data; surface/drawmode/object/clip are snapshots.
; Packed scanlines and axis lines; other slopes use the checked point kernel.
T_LOCAL equ 304
T_RESERVE equ 464
T_BRUSH equ -152                 ; 56 bytes, ending immediately below BP-96
T_DM equ -164                    ; first 12 bytes of DRAWMODE
T_PEN equ -170                   ; physical colour + style
T_CLIP equ -178                  ; left,top,right,bottom
T_USEBR equ -180
T_PASS equ -182
T_INDEX equ -184
T_X0 equ -186
T_Y0 equ -188
T_X1 equ -190
T_Y1 equ -192
T_X equ -194
T_Y equ -196
T_END equ -198
T_MAJOR equ -200
T_MINOR equ -202
T_SIGN equ -204
T_REV equ -206
T_STEEP equ -208
T_LO equ -210
T_HI equ -212
T_ERROR equ -214
T_LEFT equ -216
T_MAPVALID equ -240
T_MAPX equ -242
T_MAPY equ -244
public Output

Output proc far
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
    mov ax,T_RESERVE
    call StackRoom
    jnc @F
    jmp OutputEarlyBad
@@:
    sub sp,T_LOCAL
    mov word ptr [bp+S_HELD],0
    mov word ptr [bp+T_USEBR],0
    mov ax,[bp+28]
    cmp ax,80
    jne @F
    jmp OutputOK
@@:
    cmp ax,81
    jne @F
    jmp OutputOK
@@:
    cmp ax,4
    jne @F
    jmp OutputStyleOK
@@:
    cmp ax,18
    je @F
    jmp OutputUnsupported
@@:
OutputStyleOK:
    mov ax,[bp+26]
    cmp ax,16383
    jbe @F
    jmp OutputBad
@@:
    cmp ax,2
    jae @F
    jmp OutputOK
@@:
    mov cx,ax
    shl cx,1
    shl cx,1
    mov ax,[bp+24]
    mov bx,[bp+22]
    call CheckReadSpan
    jnc @F
    jmp OutputBad
@@:
    mov ax,[bp+32]
    mov bx,[bp+30]
    call SnapshotSurface
    jnc @F
    jmp OutputBad
@@:
    call ClassifyFormat
    jnc @F
    jmp OutputBad
@@:
    cmp word ptr [bp+S_DESC+2],0
    jg @F
    jmp OutputBad
@@:
    cmp word ptr [bp+S_DESC+4],0
    jg @F
    jmp OutputBad
@@:
    mov word ptr [bp+T_CLIP],0
    mov word ptr [bp+T_CLIP+2],0
    mov ax,[bp+S_DESC+2]
    mov [bp+T_CLIP+4],ax
    mov ax,[bp+S_DESC+4]
    mov [bp+T_CLIP+6],ax
    mov ax,[bp+8]
    mov bx,[bp+6]
    or ax,bx
    jnz @F
    jmp OutputClipDone
@@:
    mov ax,[bp+8]
    mov cx,8
    call CheckReadSpan
    jnc @F
    jmp OutputBad
@@:
    mov es,ax
    mov ax,es:[bx]
    cmp ax,[bp+T_CLIP]
    jg @F
    jmp OutputClipTop
@@:
    mov [bp+T_CLIP],ax
OutputClipTop:
    mov ax,es:[bx+2]
    cmp ax,[bp+T_CLIP+2]
    jg @F
    jmp OutputClipRight
@@:
    mov [bp+T_CLIP+2],ax
OutputClipRight:
    mov ax,es:[bx+4]
    cmp ax,[bp+T_CLIP+4]
    jl @F
    jmp OutputClipBottom
@@:
    mov [bp+T_CLIP+4],ax
OutputClipBottom:
    mov ax,es:[bx+6]
    cmp ax,[bp+T_CLIP+6]
    jl @F
    jmp OutputClipDone
@@:
    mov [bp+T_CLIP+6],ax
OutputClipDone:
    mov ax,[bp+T_CLIP]
    cmp ax,[bp+T_CLIP+4]
    jl @F
    jmp OutputOK
@@:
    mov ax,[bp+T_CLIP+2]
    cmp ax,[bp+T_CLIP+6]
    jl @F
    jmp OutputOK
@@:
    mov ax,[bp+12]
    mov bx,[bp+10]
    mov cx,12
    lea di,[bp+T_DM]
    call OutputSnapshot
    jnc @F
    jmp OutputBad
@@:
    mov ax,[bp+T_DM]
    cmp ax,1
    jae @F
    jmp OutputBad
@@:
    cmp ax,16
    jbe @F
    jmp OutputBad
@@:
    mov [bp+S_ROP],ax
    cmp word ptr [bp+28],18
    jne @F
    jmp OutputPen
@@:
    mov ax,[bp+16]
    mov bx,[bp+14]
    or ax,bx
    jnz @F
    jmp OutputPen
@@:
    mov ax,[bp+16]
    mov cx,56
    lea di,[bp+T_BRUSH]
    call OutputSnapshot
    jnc @F
    jmp OutputBad
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jbe @F
    jmp OutputBad
@@:
    cmp word ptr [bp+T_BRUSH+48],1
    jne @F
    jmp OutputOK
@@:
    cmp word ptr [bp+T_BRUSH+48],2
    je @F
    jmp OutputBrushReady
@@:
    cmp word ptr [bp+T_DM+2],1
    jne @F
    jmp OutputBrushReady
@@:
    cmp word ptr [bp+T_DM+2],2
    je @F
    jmp OutputBad
@@:
OutputBrushReady:
    mov word ptr [bp+T_USEBR],1
    jmp OutputOwn
OutputPen:
    mov ax,[bp+20]
    mov bx,[bp+18]
    or ax,bx
    jnz @F
    jmp OutputOK
@@:
    mov ax,[bp+20]
    mov cx,6
    lea di,[bp+T_PEN]
    call OutputSnapshot
    jnc @F
    jmp OutputBad
@@:
    cmp word ptr [bp+T_PEN+4],5
    jne @F
    jmp OutputOK
@@:
    cmp word ptr [bp+T_PEN+4],0
    je @F
    jmp OutputUnsupported
@@:
OutputOwn:
OutputWalks:
    mov word ptr [bp+T_PASS],0
OutputWalk:
    mov word ptr [bp+T_INDEX],1
    cmp word ptr [bp+28],4
    je @F
    jmp OutputLines
@@:
    call WalkSpans
    jmp OutputWalkResult
OutputLines:
    call WalkLines
OutputWalkResult:
    jnc @F
    jmp OutputBad
@@:
    cmp word ptr [bp+T_PASS],0
    jne OutputAdvancePass
    cmp word ptr [bp+S_KIND],0
    je OutputAdvancePass
    mov ax,[bp+T_CLIP]
    mov bx,[bp+T_CLIP+2]
    mov cx,[bp+T_CLIP+4]
    mov dx,[bp+T_CLIP+6]
    call EnterDrawingRect
    jnc OutputClaimed
    jmp OutputBad
OutputClaimed:
    cmp word ptr [bp+28],4
    jne OutputPerfLine
    PERFINC PerfScans
    jmp OutputPerfDone
OutputPerfLine:
    PERFINC PerfLines
OutputPerfDone:
    mov word ptr [bp+S_HELD],1
OutputAdvancePass:
    inc word ptr [bp+T_PASS]
    cmp word ptr [bp+T_PASS],2
    jae @F
    jmp OutputWalk
@@:
OutputOK:
    mov ax,1
    jmp OutputRelease
OutputUnsupported:
    mov ax,0ffffh
    jmp OutputRelease
OutputBad:
    xor ax,ax
OutputRelease:
    cmp word ptr [bp+S_HELD],0
    jne @F
    jmp OutputReturn
@@:
    call LeaveDrawing
    jmp OutputReturn
OutputEarlyBad:
    xor ax,ax
OutputReturn:
    xor dx,dx
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf 28
Output endp

; AX:BX -> CX bytes to SS:DI. Used only for small fixed-size snapshots.
OutputSnapshot proc near
    cmp cx,8
    je OutputSnapshotEight
    call CheckReadSpan
    jnc @F
    jmp OutputSnapshotBad
@@:
    mov es,ax
    ; The complete source was checked. Copy its exact word span and odd
    ; tail to SS with REP. Preserve DS/SI/DX and the incoming direction flag;
    ; ES remains the checked source selector, BX/DI advance exactly as before.
    pushf
    cld
    push ds
    push si
    push es
    push cx
    mov si,bx
    mov ds,ax
    push ss
    pop es
    shr cx,1
    rep movsw
    pop cx
    test cl,1
    jz OutputSnapshotDone
    movsb
OutputSnapshotDone:
    mov bx,si
    xor cx,cx
    pop es
    pop si
    pop ds
    popf
    clc
    ret
OutputSnapshotBad:
    stc
    ret
OutputSnapshotEight:
    ; Clip rectangles / selected draw-mode fields: retain the full proof,
    ; exact four ordered word transfers, registers and incoming DF/IF.
    call CheckReadSpan
    jc OutputSnapshotBad
    mov es,ax
    pushf
    push ax
    mov ax,es:[bx]
    mov ss:[di],ax
    mov ax,es:[bx+2]
    mov ss:[di+2],ax
    mov ax,es:[bx+4]
    mov ss:[di+4],ax
    mov ax,es:[bx+6]
    mov ss:[di+6],ax
    add bx,8
    add di,8
    xor cx,cx
    pop ax
    popf
    clc
    ret
OutputSnapshot endp

; Draw one covered, clipped pixel, or preflight it without reading destination.
OutputPoint proc near
    mov ax,[bp+T_X]
    cmp ax,[bp+T_CLIP]
    jge @F
    jmp PointSkip
@@:
    cmp ax,[bp+T_CLIP+4]
    jl @F
    jmp PointSkip
@@:
    mov [bp+S_X],ax
    mov ax,[bp+T_Y]
    cmp ax,[bp+T_CLIP+2]
    jge @F
    jmp PointSkip
@@:
    cmp ax,[bp+T_CLIP+6]
    jl @F
    jmp PointSkip
@@:
    mov [bp+S_Y],ax
    cmp word ptr [bp+T_USEBR],0
    je @F
    jmp PointBrush
@@:
    mov al,byte ptr [bp+T_PEN+3]
    jmp PointPhysical
PointBrush:
    mov ax,[bp+T_X]
    sub al,byte ptr [bp+T_BRUSH+50]
    and ax,7
    mov cx,ax
    mov dl,80h
    shr dl,cl
    mov ax,[bp+T_Y]
    sub al,byte ptr [bp+T_BRUSH+51]
    and ax,7
    mov si,ax
    cmp word ptr [bp+T_BRUSH+48],2
    je @F
    jmp PointPattern
@@:
    cmp word ptr [bp+T_DM+2],1
    je @F
    jmp PointPattern
@@:
    test byte ptr [bp+T_BRUSH+40+si],dl
    jnz @F
    jmp PointSkip
@@:
PointPattern:
    cmp word ptr [bp+T_BRUSH+48],4
    je @F
    jmp PointFixed
@@:
    mov al,byte ptr [bp+T_DM+11]    ; raw zero selects current foreground
    test byte ptr [bp+T_BRUSH+32+si],dl
    jnz @F
    jmp PointPhysical
@@:
    mov al,byte ptr [bp+T_DM+7]     ; raw one selects current background
PointPhysical:
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp PointIndex
@@:
    mov cl,4
    shr al,cl
    and al,1
    jmp PointPenReady
PointIndex:
    and al,15
    jmp PointPenReady
PointFixed:
    xor al,al
    cmp word ptr [bp+S_PLANES],1
    je @F
    jmp PointRGB
@@:
    test byte ptr [bp+T_BRUSH+32+si],dl
    jnz @F
    jmp PointPenReady
@@:
    inc al
    jmp PointPenReady
PointRGB:
    test byte ptr [bp+T_BRUSH+si],dl
    jnz @F
    jmp PointGreen
@@:
    or al,1
PointGreen:
    test byte ptr [bp+T_BRUSH+8+si],dl
    jnz @F
    jmp PointBlue
@@:
    or al,2
PointBlue:
    test byte ptr [bp+T_BRUSH+16+si],dl
    jz PointHigh
    or al,4
PointHigh:
    test byte ptr [bp+T_BRUSH+24+si],dl
    jz PointPenReady
    or al,8
PointPenReady:
    mov byte ptr [bp+S_PEN],al
    call OutputMapPoint
    jnc @F
    jmp PointBad
@@:
    cmp word ptr [bp+T_PASS],0
    jne @F
    jmp PointSkip
@@:
    call PixelBits
PointSkip:
    clc
    ret
PointBad:
    stc
    ret
OutputPoint endp

; First PTTYPE's y is the scan Y. Remaining pairs are [left,right).
WalkSpans proc near
    mov es,[bp+24]
    mov bx,[bp+22]
    mov ax,es:[bx+2]
    mov [bp+T_Y],ax
    cmp ax,[bp+T_CLIP+2]
    jge @F
    jmp SpansDone
@@:
    cmp ax,[bp+T_CLIP+6]
    jl @F
    jmp SpansDone
@@:
    call CompileSpanRow
SpanNext:
    mov bx,[bp+T_INDEX]
    shl bx,1
    shl bx,1
    add bx,[bp+22]
    mov es,[bp+24]
    mov ax,es:[bx]
    mov dx,es:[bx+2]
    cmp ax,[bp+T_CLIP]
    jl @F
    jmp SpanLeft
@@:
    mov ax,[bp+T_CLIP]
SpanLeft:
    cmp dx,[bp+T_CLIP+4]
    jg @F
    jmp SpanRight
@@:
    mov dx,[bp+T_CLIP+4]
SpanRight:
    cmp ax,dx
    jl @F
    jmp SpanAdvance
@@:
    mov [bp+T_X],ax
    mov [bp+T_END],dx
    call PackedSpan
    jnc @F
    jmp SpansBad
@@:
SpanAdvance:
    inc word ptr [bp+T_INDEX]
    mov ax,[bp+T_INDEX]
    cmp ax,[bp+26]
    jae @F
    jmp SpanNext
@@:
SpansDone:
    clc
    ret
SpansBad:
    stc
    ret
WalkSpans endp

WalkLines proc near
LineNext:
    mov word ptr [bp+T_MAPVALID],0
    mov bx,[bp+T_INDEX]
    dec bx
    shl bx,1
    shl bx,1
    add bx,[bp+22]
    mov es,[bp+24]
    mov ax,es:[bx]
    mov dx,es:[bx+2]
    mov cx,es:[bx+4]
    mov si,es:[bx+6]
    mov word ptr [bp+T_REV],0
    cmp ax,cx
    jg @F
    jmp LineOrdered
@@:
    xchg ax,cx
    xchg dx,si
    mov word ptr [bp+T_REV],1
LineOrdered:
    mov [bp+T_X0],ax
    mov [bp+T_Y0],dx
    mov [bp+T_X1],cx
    mov [bp+T_Y1],si
    sub cx,ax                     ; unsigned X extent, 0..65535
    mov word ptr [bp+T_SIGN],1
    cmp si,dx
    jl @F
    jmp LinePositiveY
@@:
    xchg si,dx
    mov word ptr [bp+T_SIGN],-1
LinePositiveY:
    sub si,dx                     ; unsigned absolute Y extent
    mov word ptr [bp+T_STEEP],0
    cmp cx,si
    jb @F
    jmp LineAxes
@@:
    xchg cx,si
    mov word ptr [bp+T_STEEP],1
LineAxes:
    mov [bp+T_MAJOR],cx
    mov [bp+T_MINOR],si
    or cx,cx
    jnz @F
    jmp LineAdvance
@@:
    ; Inclusive canonical t range, excluding the caller's original endpoint.
    mov ax,[bp+T_REV]
    mov [bp+T_LO],ax
    or ax,ax
    jz @F
    jmp LineHigh
@@:
    dec cx
LineHigh:
    mov [bp+T_HI],cx
    ; Clip only the major axis here. Minor-axis testing preserves DDA phase.
    mov ax,[bp+T_X0]
    mov bx,[bp+T_CLIP]
    mov dx,[bp+T_CLIP+4]
    cmp word ptr [bp+T_STEEP],0
    jne @F
    jmp LineClipPositive
@@:
    mov ax,[bp+T_Y0]
    mov bx,[bp+T_CLIP+2]
    mov dx,[bp+T_CLIP+6]
    cmp word ptr [bp+T_SIGN],0
    jle @F
    jmp LineClipPositive
@@:
    cmp ax,bx
    jge @F
    jmp LineAdvance
@@:
    dec dx
    cmp ax,dx
    jg @F
    jmp LineNegativeHigh
@@:
    mov cx,ax
    sub cx,dx
    cmp cx,[bp+T_LO]
    ja @F
    jmp LineNegativeHigh
@@:
    mov [bp+T_LO],cx
LineNegativeHigh:
    sub ax,bx
    jmp LineClipHigh
LineClipPositive:
    cmp ax,dx
    jl @F
    jmp LineAdvance
@@:
    cmp ax,bx
    jl @F
    jmp LinePositiveHigh
@@:
    sub bx,ax
    cmp bx,[bp+T_LO]
    ja @F
    jmp LinePositiveHigh
@@:
    mov [bp+T_LO],bx
LinePositiveHigh:
    dec dx
    sub dx,ax
    mov ax,dx
LineClipHigh:
    cmp ax,[bp+T_HI]
    jb @F
    jmp LineClipReady
@@:
    mov [bp+T_HI],ax
LineClipReady:
    mov ax,[bp+T_LO]
    cmp ax,[bp+T_HI]
    jbe @F
    jmp LineAdvance
@@:
    mov cx,[bp+T_HI]
    sub cx,ax
    inc cx
    mov [bp+T_LEFT],cx
    cmp word ptr [bp+T_MINOR],0
    jne LineSlope
    call AxisLine
    jnc @F
    jmp LinesBad
@@:
    jmp LineAdvance
LineSlope:
    mul word ptr [bp+T_MINOR]
    mov cx,[bp+T_MAJOR]
    cmp word ptr [bp+T_SIGN],0
    jge @F
    jmp LineBias
@@:
    dec cx
LineBias:
    shr cx,1
    add ax,cx
    adc dx,0
    div word ptr [bp+T_MAJOR]      ; quotient minor(t), remainder for recurrence
    mov [bp+T_ERROR],dx
    mov bx,[bp+T_LO]
    cmp word ptr [bp+T_STEEP],0
    je @F
    jmp LineCoordinates
@@:
    xchg ax,bx                    ; AX major-X, BX minor-Y
LineCoordinates:
    add ax,[bp+T_X0]
    mov [bp+T_X],ax
    cmp word ptr [bp+T_SIGN],0
    jle @F
    jmp LineYAdd
@@:
    neg bx
LineYAdd:
    add bx,[bp+T_Y0]
    mov [bp+T_Y],bx
LinePixel:
    call OutputPoint
    jnc @F
    jmp LinesBad
@@:
    dec word ptr [bp+T_LEFT]
    jnz @F
    jmp LineAdvance
@@:
    mov ax,[bp+T_ERROR]
    add ax,[bp+T_MINOR]
    jnc @F
    jmp LineMinorStep
@@:
    cmp ax,[bp+T_MAJOR]
    jae @F
    jmp LineMajorStep
@@:
LineMinorStep:
    sub ax,[bp+T_MAJOR]
    cmp word ptr [bp+T_STEEP],0
    je @F
    jmp LineMinorX
@@:
    mov dx,[bp+T_SIGN]
    add [bp+T_Y],dx
    jmp LineMajorStep
LineMinorX:
    inc word ptr [bp+T_X]
LineMajorStep:
    mov [bp+T_ERROR],ax
    cmp word ptr [bp+T_STEEP],0
    je @F
    jmp LineMajorY
@@:
    inc word ptr [bp+T_X]
    jmp LinePixel
LineMajorY:
    mov dx,[bp+T_SIGN]
    add [bp+T_Y],dx
    jmp LinePixel
LineAdvance:
    inc word ptr [bp+T_INDEX]
    mov ax,[bp+T_INDEX]
    cmp ax,[bp+26]
    jae @F
    jmp LineNext
@@:
    clc
    ret
LinesBad:
    stc
    ret
WalkLines endp

; Complete polyline preflight still calls the checked point adapter. During
; output, reuse that same stable mapping for adjacent raster points. Huge
; memory surfaces retain checked mapping because a row can change selector.
public OutputMapPoint
OutputMapPoint proc near
    cmp word ptr [bp+T_PASS],0
    jne @F
    jmp OMPChecked
@@:
    cmp word ptr [bp+S_KIND],0
    jne OMPOrdinary
    cmp word ptr [bp+S_DESC+22],0
    je @F
    jmp OMPChecked
@@:
OMPOrdinary:
    cmp word ptr [bp+T_MAPVALID],0
    jne @F
    jmp OMPChecked
@@:
    mov ax,[bp+S_Y]
    sub ax,[bp+T_MAPY]
    cmp ax,-1
    jl OMPChecked
    cmp ax,1
    jg OMPChecked
    mov dx,ax
    cmp word ptr [bp+S_KIND],0
    jne OMPScreen
    imul word ptr [bp+S_ROW]
    mov dx,ax
    jmp OMPX
OMPScreen:
    or ax,ax
    jz OMPX
    mov cx,[bp+T_MAPY]
    and cx,3
    mov dx,2000h
    cmp ax,1
    jne OMPBack
    cmp cx,3
    jne OMPX
    mov dx,0a050h
    jmp OMPX
OMPBack:
    mov dx,0e000h
    or cx,cx
    jnz OMPX
    mov dx,5fb0h
OMPX:
    mov ax,[bp+S_X]
    mov cx,ax
    and cx,7
    shr ax,3
    mov bx,[bp+T_MAPX]
    shr bx,3
    sub ax,bx
    add dx,ax
    mov al,80h
    shr al,cl
    mov byte ptr [bp+S_MASK],al
    xor si,si
    mov cx,[bp+S_PLANES]
OMPPlane:
    add [bp+S_ADDR+si],dx
    add si,4
    loop OMPPlane
    jmp OMPRemember
OMPChecked:
    call PreparePoint
    jc OMPDone
OMPRemember:
    mov ax,[bp+S_X]
    mov [bp+T_MAPX],ax
    mov ax,[bp+S_Y]
    mov [bp+T_MAPY],ax
    mov word ptr [bp+T_MAPVALID],1
    clc
OMPDone:
    ret
OutputMapPoint endp
