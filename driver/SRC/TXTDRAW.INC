; Phase 11: public 286 text, complete address preflight then packed writes.
; Stable borrowed GDI font/string/vector data, invocation-local snapshots.
; U_* holds the shared font/layout state; S_* holds the surface adapter.
V_BATCH equ -314                ; ownership of the bounded data-segment strip
V_GLYPHTOP equ -316
V_GLYPHBOTTOM equ -318
V_CLIPFAST equ -320             ; whole horizontal extent fits this clip
V_LOCAL equ 320
V_RESERVE equ 448
V_BASE equ -184
V_CLIP equ -192
V_FILL equ -200
V_BOX equ -208
V_DM equ -232
V_REGION equ -240
V_PASS equ -242
V_KIND equ -244
V_INK equ -246
V_PAPER equ -248
V_COLOR equ -250
V_ORGX equ -252
V_Y equ -254
V_BYTES equ -262
V_FIRST equ -264
V_LAST equ -266
V_MASK equ -268
V_BYTE equ -270
V_COL equ -272
V_SHIFT equ -274
V_RAW equ -276
V_SRCY equ -278
V_CURBOX equ -286
; Screen-only envelope, shared by every clipped glyph/background rectangle.
; All locals fit the existing frame and stack reservation.
V_PMIN equ -288
V_PMAX equ -290
V_MINROW equ -292
V_MAXROW equ -294
V_XBYTE equ -296
V_XLAST equ -298
V_SCAN equ -300
V_NSCAN equ -302
V_NROWS equ -304
V_BPHASE equ -306
V_BPLANE equ -308
V_BSTART equ -310
V_BROWS equ -312
public StrBlt, ExtTextOut, TextClipSpan, TextRectangle, TextMask
public TextRenderPass

; Original 30-byte StrBlt ABI: add the ten absent ExtTextOut argument bytes.
; Check that small stack expansion before changing the caller's return frame.
StrBlt proc far
    mov ax,ds
    nop
    mov ax,sp
    sub ax,10
    ja VXJump0
    jmp VSBad
VXJump0:
    cmp ax,ss:[000ah]
    jae VXJump1
    jmp VSBad
VXJump1:
    pop cx
    pop dx
    xor ax,ax
    push ax
    push ax
    push ax
    push ax
    push ax
    push dx
    push cx
    jmp ExtTextOut
VSBad:
    xor ax,ax
    mov dx,8000h
    cld
    ret 30
StrBlt endp

; Pascal: device,x,y,clip,string,count,font,drawmode,xform,dx,rect,options.
; 40 bytes. Negative count delegates to the checked measurement entry before
; allocating the drawing frame; clipping/device/colour operands are unused.
ExtTextOut proc far
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
    cmp word ptr [bp+28],0
    jge VXJump2
    jmp VTMeasure
VXJump2:
    mov ax,V_RESERVE
    call StackRoom
    jnc VXJump3
    jmp VTEarlyBad
VXJump3:
    sub sp,V_LOCAL
    mov word ptr [bp+V_BATCH],0
    mov word ptr [bp+S_HELD],0
    mov word ptr [bp+V_CURBOX],640
    mov word ptr [bp+V_CURBOX+2],400
    mov word ptr [bp+V_CURBOX+4],0
    mov word ptr [bp+V_CURBOX+6],0
    mov word ptr [bp+S_ROP],13 ; select write validation; text ignores ROP2
    mov ax,[bp+28]
    mov [bp+U_N],ax
    or ax,ax
    jz VXJump4
    jmp VTOptions
VXJump4:
    test word ptr [bp+6],2
    jnz VXJump5
    jmp VTSuccess
VXJump5:
VTOptions:
    test word ptr [bp+6],6
    jnz VXJump6
    jmp VTSurface
VXJump6:
    mov ax,[bp+8]
    or ax,[bp+10]
    jnz VXJump7
    jmp VTSuccess
VXJump7:
VTSurface:
    mov ax,[bp+44]
    mov bx,[bp+42]
    call SnapshotSurface
    jnc VXJump8
    jmp VTBad
VXJump8:
    call ClassifyFormat
    jnc VXJump9
    jmp VTBad
VXJump9:
    mov ax,[bp+S_DESC+2]
    or ax,ax
    jg VXJump10
    jmp VTBad
VXJump10:
    mov [bp+V_BASE+4],ax
    mov ax,[bp+S_DESC+4]
    or ax,ax
    jg VXJump11
    jmp VTBad
VXJump11:
    mov [bp+V_BASE+6],ax
    mov word ptr [bp+V_BASE],0
    mov word ptr [bp+V_BASE+2],0
    mov ax,[bp+36]
    mov bx,[bp+34]
    or ax,bx
    jnz VXJump12
    jmp VTBaseReady
VXJump12:
    mov ax,[bp+36]
    mov cx,8
    lea di,[bp+V_REGION]
    call OutputSnapshot
    jnc VXJump13
    jmp VTBad
VXJump13:
    lea si,[bp+V_REGION]
    lea di,[bp+V_BASE]
    call TextIntersect
VTBaseReady:
    lea si,[bp+V_BASE]
    lea di,[bp+V_CLIP]
    call TextRectCopy
    lea si,[bp+V_BASE]
    lea di,[bp+V_FILL]
    call TextRectCopy
    test word ptr [bp+6],6
    jnz VXJump14
    jmp VTMode
VXJump14:
    mov ax,[bp+10]
    mov bx,[bp+8]
    mov cx,8
    lea di,[bp+V_REGION]
    call OutputSnapshot
    jnc VXJump15
    jmp VTBad
VXJump15:
    lea si,[bp+V_REGION]
    lea di,[bp+V_FILL]
    call TextIntersect
    test word ptr [bp+6],4
    jnz VXJump16
    jmp VTMode
VXJump16:
    lea si,[bp+V_FILL]
    lea di,[bp+V_CLIP]
    call TextRectCopy
VTMode:
    mov ax,[bp+22]
    mov bx,[bp+20]
    mov cx,8                 ; zero count needs only the background colour
    cmp word ptr [bp+U_N],0
    jne VXJump17
    jmp VTModeSize
VXJump17:
    mov cx,24
VTModeSize:
    lea di,[bp+V_DM]
    call OutputSnapshot
    jnc VXJump18
    jmp VTBad
VXJump18:
    mov al,byte ptr [bp+V_DM+7]
    call TextColour
    mov [bp+V_PAPER],ax
    cmp word ptr [bp+U_N],0
    jne VXJump19
    jmp VTOwn
VXJump19:
    cmp word ptr [bp+V_DM+2],1
    jne VXJump20
    jmp VTTextMode
VXJump20:
    cmp word ptr [bp+V_DM+2],2
    je VXJump21
    jmp VTBad
VXJump21:
VTTextMode:
    mov al,byte ptr [bp+V_DM+11]
    call TextColour
    mov [bp+V_INK],ax
    call TextLoad
    jnc VXJump22
    jmp VTBad
VXJump22:
    call TextRun             ; validates ALL used glyphs before any output
    jnc VXJump23
    jmp VTBad
VXJump23:
    call TextBackgroundBox
VTOwn:
VTPasses:
    mov word ptr [bp+V_PMIN],0ffffh
    mov word ptr [bp+V_PMAX],0
    mov word ptr [bp+V_PASS],0
    call TextRenderPass          ; no framebuffer access; collect touched bounds
    jnc VTPreflightDone
    jmp VTBad
VTPreflightDone:
    cmp word ptr [bp+S_KIND],0
    jne VTScreenPreflight
    cmp word ptr [bp+S_DESC+22],0
    jne VTWrite
    call TextMemoryCheck
    jnc VTWrite
    jmp VTBad
VTScreenPreflight:
    call TextScreenCheck
    jnc VTScreenChecked
    jmp VTBad
VTScreenChecked:
    mov ax,[bp+V_CURBOX]
    mov bx,[bp+V_CURBOX+2]
    mov cx,[bp+V_CURBOX+4]
    mov dx,[bp+V_CURBOX+6]
    call EnterDrawingRect
    jnc VTClaimed
    jmp VTBad
VTClaimed:
    PERFINC PerfText
    mov word ptr [bp+S_HELD],1
VTWrite:
    mov word ptr [bp+V_PASS],1
    call TextRenderPass
    jnc VTSuccess
    jmp VTBad
VTSuccess:
    xor ax,ax
    xor dx,dx
    jmp VTRelease
VTBad:
    xor ax,ax
    mov dx,8000h
VTRelease:
    cmp word ptr [bp+V_BATCH],0
    je VTBatchReleased
    mov TextBusy,0
    mov word ptr [bp+V_BATCH],0
VTBatchReleased:
    cmp word ptr [bp+S_HELD],0
    jne VXJump28
    jmp VTReturn
VXJump28:
    call LeaveDrawing
    jmp VTReturn
VTEarlyBad:
    xor ax,ax
    mov dx,8000h
    jmp VTReturn
VTMeasure:
    ; No drawing locals yet. TextMeasure performs its own stack reservation.
    ; Reserve the pushed arguments/return as well before constructing them.
    mov ax,32
    call StackRoom
    jnc VXJump29
    jmp VTEarlyBad
VXJump29:
    push word ptr [bp+26]
    push word ptr [bp+24]
    push word ptr [bp+32]
    push word ptr [bp+30]
    push word ptr [bp+28]
    push word ptr [bp+14]
    push word ptr [bp+12]
    push word ptr [bp+22]
    push word ptr [bp+20]
    call TextMeasure
VTReturn:
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    cld
    ret 40
ExtTextOut endp

; Canonical physical metadata -> one monochrome or four palette index bits.
TextColour proc near
    cmp word ptr [bp+S_PLANES],1
    je VXJump30
    jmp VCColour
VXJump30:
    mov cl,4
    shr al,cl
    and al,1
    jmp VCDone
VCColour:
    and al,15
VCDone:
    xor ah,ah
    ret
TextColour endp

; SS:SI source, SS:DI destination rectangle. Both pointers are local.
TextRectCopy proc near
    mov cx,4
VRCopy:
    mov ax,ss:[si]
    mov ss:[di],ax
    add si,2
    add di,2
    loop VRCopy
    ret
TextRectCopy endp

; Intersect SS:DI rectangle with SS:SI, signed WORD half-open coordinates.
TextIntersect proc near
    mov ax,ss:[si]
    cmp ax,ss:[di]
    jg VXJump31
    jmp VILeft
VXJump31:
    mov ss:[di],ax
VILeft:
    mov ax,ss:[si+2]
    cmp ax,ss:[di+2]
    jg VXJump32
    jmp VITop
VXJump32:
    mov ss:[di+2],ax
VITop:
    mov ax,ss:[si+4]
    cmp ax,ss:[di+4]
    jl VXJump33
    jmp VIRight
VXJump33:
    mov ss:[di+4],ax
VIRight:
    mov ax,ss:[si+6]
    cmp ax,ss:[di+6]
    jl VXJump34
    jmp VIDone
VXJump34:
    mov ss:[di+6],ax
VIDone:
    ret
TextIntersect endp

; DX:AX signed origin, CX unsigned width <=65535, BX/DI clipped bounds.
; Origin is >=-32768; clip is within nonnegative destination dimensions.
; AX/BX -> clipped start/end, CF empty. No coordinate truncation before clip.
TextClipSpan proc near
    cmp bx,di
    jl VXJump35
    jmp VCSEmpty
VXJump35:
    push si
    mov si,ax
    or dx,dx
    jns VXJump36
    jmp VCSLeftNegative
VXJump36:
    jz VXJump37
    jmp VCSPopEmpty
VXJump37:
    cmp ax,di
    jb VXJump38
    jmp VCSPopEmpty
VXJump38:
    cmp ax,bx
    jb VXJump39
    jmp VCSLeftReady
VXJump39:
VCSLeftNegative:
    mov ax,bx
VCSLeftReady:
    push ax
    add si,cx
    adc dx,0
    or dx,dx
    jns VXJump40
    jmp VCSBothEmpty
VXJump40:
    jz VXJump41
    jmp VCSRightBound
VXJump41:
    cmp si,di
    jae VXJump42
    jmp VCSRightReady
VXJump42:
VCSRightBound:
    mov si,di
VCSRightReady:
    mov bx,si
    pop ax
    pop si
    cmp ax,bx
    jb VXJump43
    jmp VCSEmpty
VXJump43:
    clc
    ret
VCSBothEmpty:
    pop ax
VCSPopEmpty:
    pop si
VCSEmpty:
    stc
    ret
TextClipSpan endp

; Bounding background uses the UNSATURATED extent. If its high word is
; nonzero, 65535 already covers the entire possible destination after X>=-32768.
TextBackgroundBox proc near
    mov ax,[bp+40]
    cwd
    mov cx,[bp+U_RIGHT]
    cmp word ptr [bp+U_RIGHT+2],0
    jne VXJump44
    jmp VBWidth
VXJump44:
    mov cx,0ffffh
VBWidth:
    mov bx,[bp+V_CLIP]
    mov di,[bp+V_CLIP+4]
    call TextClipSpan
    jnc VXJump45
    jmp VBEmpty
VXJump45:
    mov [bp+V_BOX],ax
    mov [bp+V_BOX+4],bx
    mov ax,[bp+38]
    cwd
    mov cx,[bp+U_HEIGHT]
    mov bx,[bp+V_CLIP+2]
    mov di,[bp+V_CLIP+6]
    call TextClipSpan
    jnc VXJump46
    jmp VBEmpty
VXJump46:
    mov [bp+V_BOX+2],ax
    mov [bp+V_BOX+6],bx
    ret
VBEmpty:
    mov word ptr [bp+V_BOX],0
    mov word ptr [bp+V_BOX+4],0
    ret
TextBackgroundBox endp

; In each pass explicit fill and complete text background precede all ink.
TextRenderPass proc near
    cmp word ptr [bp+V_PASS],1
    jne VRBatchReady
    call TextBatchBegin
VRBatchReady:
    mov ax,[bp+V_PAPER]
    mov [bp+V_COLOR],ax
    mov word ptr [bp+V_KIND],0
    test word ptr [bp+6],2
    jnz VXJump47
    jmp VRText
VXJump47:
    lea si,[bp+V_FILL]
    lea di,[bp+V_REGION]
    call TextRectCopy
    call TextRectangle
    jnc VXJump48
    jmp VRBad
VXJump48:
VRText:
    cmp word ptr [bp+U_N],0
    jne VXJump49
    jmp VRDone
VXJump49:
    cmp word ptr [bp+V_DM+2],2
    je VXJump50
    jmp VRInk
VXJump50:
    lea si,[bp+V_BOX]
    lea di,[bp+V_REGION]
    call TextRectCopy
    call TextRectangle
    jnc VXJump51
    jmp VRBad
VXJump51:
VRInk:
    mov ax,[bp+V_INK]
    mov [bp+V_COLOR],ax
    mov word ptr [bp+V_KIND],1
    call TextInkWalk
    jc VRBad
VRDone:
    cmp word ptr [bp+V_BATCH],0
    je VRBatchDone
    call TextBatchFlush
VRBatchDone:
    clc
    ret
VRBad:
    stc
    ret
TextRenderPass endp

; Replay layout from the original error each time. Positions above 65535
; cannot intersect any supported destination even with the minimum X origin.
TextInkWalk proc near
    ; Every glyph-right is bounded by TextRun's UNSATURATED U_RIGHT.
    ; If the complete extent fits, all later horizontal intersections are
    ; identities. A zero-width glyph still produces no rectangle. Re-prove
    ; once per preflight/render pass; retain the original clipped fallback.
    mov word ptr [bp+V_CLIPFAST],0
    cmp word ptr [bp+U_RIGHT+2],0
    jne VWClipReady
    mov ax,[bp+40]
    cmp ax,[bp+V_CLIP]
    jl VWClipReady
    mov dx,[bp+V_CLIP+4]
    sub dx,ax
    jbe VWClipReady
    cmp [bp+U_RIGHT],dx
    ja VWClipReady
    mov word ptr [bp+V_CLIPFAST],1
VWClipReady:
    ; Vertical clipping is identical for every glyph in this call.
    mov ax,[bp+38]
    cwd
    mov cx,[bp+U_HEIGHT]
    mov bx,[bp+V_CLIP+2]
    mov di,[bp+V_CLIP+6]
    call TextClipSpan
    jnc @F
    clc
    ret
@@:
    mov [bp+V_GLYPHTOP],ax
    mov [bp+V_GLYPHBOTTOM],bx
    mov ax,[bp+U_INITIAL]
    mov [bp+U_ERR],ax
    mov word ptr [bp+U_SCAN],0
    mov word ptr [bp+U_POSITION],0
    mov word ptr [bp+U_POSITION+2],0
VWNext:
    mov bx,[bp+U_STRING]
    add bx,[bp+U_SCAN]
    mov es,[bp+U_STRING+2]
    xor ax,ax
    mov al,es:[bx]
    call TextReplayGlyph
    jnc VXJump52
    jmp VWBad
VXJump52:
    cmp word ptr [bp+U_USEDV],0
    jne VXJump53
    jmp VWAdvance
VXJump53:
    mov bx,[bp+U_SCAN]
    shl bx,1
    add bx,[bp+U_VECTOR]
    mov es,[bp+U_VECTOR+2]
    mov ax,es:[bx]
VWAdvance:
    call TextAdvance
    mov [bp+U_ADVANCE],ax
    mov [bp+U_ADVANCE+2],dx
    cmp word ptr [bp+U_POSITION+2],0
    je VXJump54
    jmp VWStep
VXJump54:
    mov ax,[bp+40]
    cwd
    add ax,[bp+U_POSITION]
    adc dx,0
    mov [bp+V_ORGX],ax
    mov cx,[bp+U_WIDTH]
    cmp word ptr [bp+V_CLIPFAST],0
    je VWClipGlyph
    jcxz VWStep
    mov bx,ax
    add bx,cx
    jmp VXJump55
VWClipGlyph:
    mov bx,[bp+V_CLIP]
    mov di,[bp+V_CLIP+4]
    call TextClipSpan
    jnc VXJump55
    jmp VWStep
VXJump55:
    mov [bp+V_REGION],ax
    mov [bp+V_REGION+4],bx
    mov ax,[bp+V_GLYPHTOP]
    mov [bp+V_REGION+2],ax
    mov ax,[bp+V_GLYPHBOTTOM]
    mov [bp+V_REGION+6],ax
    call TextRectangle
    jnc VXJump57
    jmp VWBad
VXJump57:
VWStep:
    mov ax,[bp+U_ADVANCE]
    mov dx,[bp+U_ADVANCE+2]
    add [bp+U_POSITION],ax
    adc [bp+U_POSITION+2],dx
    inc word ptr [bp+U_SCAN]
    mov ax,[bp+U_SCAN]
    cmp ax,[bp+U_N]
    jae VXJump58
    jmp VWNext
VXJump58:
    clc
    ret
VWBad:
    stc
    ret
TextInkWalk endp

; Packed half-open V_REGION. Pass zero validates every row/plane without
; reading destination bits. Stable inputs/selector mappings are a call contract.
TextRectangle proc near
    mov ax,[bp+V_REGION]
    cmp ax,[bp+V_REGION+4]
    jl VXJump59
    jmp VRCDone
VXJump59:
    mov ax,[bp+V_REGION+2]
    cmp ax,[bp+V_REGION+6]
    jl VXJump60
    jmp VRCDone
VXJump60:
    cmp word ptr [bp+V_PASS],0
    jne VRCBoundsDone
    mov ax,[bp+V_REGION]
    mov bx,[bp+V_REGION+2]
    mov cx,[bp+V_REGION+4]
    mov dx,[bp+V_REGION+6]
    lea si,[bp+V_CURBOX]
    call CursorRectUnion
VRCBoundsDone:
    cmp word ptr [bp+V_PASS],0
    jne VRCSetup
    cmp word ptr [bp+S_KIND],0
    jne VRCBoundsCollected
    cmp word ptr [bp+S_DESC+22],0
    jne VRCSetup
VRCBoundsCollected:
    clc
    ret
VRCSetup:
    mov ax,[bp+V_REGION+2]
    mov [bp+V_Y],ax
    mov ax,[bp+V_REGION]
    mov bx,ax
    and ax,7
    mov cx,ax
    mov al,0ffh
    shr al,cl
    mov byte ptr [bp+V_FIRST],al
    mov ax,[bp+V_REGION+4]
    dec ax
    mov dx,ax
    and ax,7
    mov cx,7
    sub cx,ax
    mov al,0ffh
    shl al,cl
    mov byte ptr [bp+V_LAST],al
    mov cl,3
    shr bx,cl
    shr dx,cl
    sub dx,bx
    inc dx
    mov [bp+V_BYTES],dx
    cmp word ptr [bp+V_BATCH],0
    je VRCUnbatched
    call TextBatchRectangle
    ret
VRCUnbatched:
    cmp word ptr [bp+S_KIND],0
    je VRCRow
    cmp word ptr [bp+V_KIND],0
    jne VRCMapInk
    PERFINC PerfBackground
    pushf
    push ax
    mov ax,[bp+V_REGION+4]
    sub ax,[bp+V_REGION]
    cmp ax,128
    jb VRCBgSmall
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_REGION+2]
    cmp ax,64
    jb VRCBgSmall
    PERFINC PerfBgLarge
VRCBgSmall:
    pop ax
    popf
    call TextScreenSolid
    ret
VRCMapInk:
    call TextScreenMap
VRCRow:
    mov ax,[bp+V_REGION]
    mov [bp+S_X],ax
    mov ax,[bp+V_Y]
    mov [bp+S_Y],ax
    sub ax,[bp+38]
    mov [bp+V_SRCY],ax
    cmp word ptr [bp+S_KIND],0
    je VRCMemoryPrepare
    jmp VRCValidated
VRCMemoryPrepare:
    call PreparePoint
    jnc VXJump61
    jmp VRCBad
VXJump61:
    xor si,si
    mov di,[bp+S_PLANES]
VRCCheck:
    mov ax,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov cx,[bp+V_BYTES]
    call CheckSpan
    jnc VXJump62
    jmp VRCBad
VXJump62:
    add si,4
    dec di
    jz VXJump63
    jmp VRCCheck
VXJump63:
VRCValidated:
    cmp word ptr [bp+V_PASS],0
    jne VXJump64
    jmp VRCNextRow
VXJump64:
    cmp word ptr [bp+V_KIND],0
    jne VRCInkBytes
    call TextSolidRow
    jmp VRCNextRow
VRCInkBytes:
    mov word ptr [bp+V_BYTE],0
VRCByte:
    mov al,0ffh
    cmp word ptr [bp+V_KIND],0
    jne VXJump65
    jmp VRCEdges
VXJump65:
    mov ax,[bp+V_BYTE]
    shl ax,1
    shl ax,1
    shl ax,1
    mov bx,[bp+V_REGION]
    and bx,0fff8h
    add ax,bx
    call TextMask
VRCEdges:
    cmp word ptr [bp+V_BYTE],0
    je VXJump66
    jmp VRCLast
VXJump66:
    and al,byte ptr [bp+V_FIRST]
VRCLast:
    mov bx,[bp+V_BYTE]
    inc bx
    cmp bx,[bp+V_BYTES]
    je VXJump67
    jmp VRCMask
VXJump67:
    and al,byte ptr [bp+V_LAST]
VRCMask:
    mov byte ptr [bp+V_MASK],al
    or al,al
    jnz VXJump68
    jmp VRCNextByte
VXJump68:
    xor si,si
    xor di,di
VRCPlane:
    mov es,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    add bx,[bp+V_BYTE]
    mov dx,[bp+V_COLOR]
    mov cx,di
    shr dl,cl
    and dl,1
    neg dl
    and dl,byte ptr [bp+V_MASK]
    mov al,byte ptr [bp+V_MASK]
    not al
    or al,al
    jnz VXJump69
    jmp VRCStore
VXJump69:
    and al,es:[bx]
    or dl,al
VRCStore:
    mov es:[bx],dl
    add si,4
    inc di
    cmp di,[bp+S_PLANES]
    jae VXJump70
    jmp VRCPlane
VXJump70:
VRCNextByte:
    inc word ptr [bp+V_BYTE]
    mov ax,[bp+V_BYTE]
    cmp ax,[bp+V_BYTES]
    jae VXJump71
    jmp VRCByte
VXJump71:
VRCNextRow:
    cmp word ptr [bp+S_KIND],0
    je VRCStepY
    mov ax,[bp+V_Y]
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne VRCStepPointers
    mov dx,0a050h             ; bank 3 -> next bank 0: -6000h + 80
VRCStepPointers:
    add [bp+S_ADDR],dx
    add [bp+S_ADDR+4],dx
    add [bp+S_ADDR+8],dx
    add [bp+S_ADDR+12],dx
VRCStepY:
    inc word ptr [bp+V_Y]
    mov ax,[bp+V_Y]
    cmp ax,[bp+V_REGION+6]
    jge VXJump72
    jmp VRCRow
VXJump72:
VRCDone:
    clc
    ret
VRCBad:
    stc
    ret
TextRectangle endp

; Solid text-background row after the complete text preflight. Keep the
; background-before-all-ink ordering; V_KIND=1 still uses the glyph writer.
; Full bytes are constant stores, partial edges preserve neighbouring pixels.
; Use only existing V/S locals; no recursive public drawing call or allocation.
public TextSolidRow
TextSolidRow proc near
    xor si,si
    xor di,di
TSPlane:
    mov dx,[bp+V_COLOR]
    mov cx,di
    shr dl,cl
    and dl,1
    neg dl
    mov es,[bp+S_ADDR+si+2]
    mov bx,[bp+S_ADDR+si]
    mov cx,[bp+V_BYTES]
    mov al,byte ptr [bp+V_FIRST]
    cmp cx,1
    jne TSMany
    and al,byte ptr [bp+V_LAST]
    call TextSolidEdge
    jmp TSNext
TSMany:
    call TextSolidEdge
    inc bx
    sub cx,2
    jcxz TSLast
    mov al,dl
    xchg bx,di
    cmp word ptr [bp+S_KIND],0
    je TSByteFill
    call FillConstantBytes
    jmp TSFilled
TSByteFill:
    cld
    rep stosb
TSFilled:
    xchg bx,di
TSLast:
    mov al,byte ptr [bp+V_LAST]
    call TextSolidEdge
TSNext:
    add si,4
    inc di
    cmp di,[bp+S_PLANES]
    jb TSPlane
    ret
TextSolidRow endp

; AL coverage, DL constant; ES:BX destination. Preserve all but AX/flags.
TextSolidEdge proc near
    mov ah,al
    not ah
    and al,dl
    or ah,ah
    jz TSEStore
    and ah,es:[bx]
    or al,ah
TSEStore:
    mov es:[bx],al
    ret
TextSolidEdge endp

; Destination byte's aligned pixel X -> AL eight coverage bits. At most two
; vertical strip bytes are loaded, including left/right edge zero extension.
TextMask proc near
    sub ax,[bp+V_ORGX]
    mov cx,ax
    and cx,7
    mov [bp+V_SHIFT],cx
    mov cl,3
    sar ax,cl
    mov [bp+V_COL],ax
    call TextStrip
    cmp word ptr [bp+V_SHIFT],0
    jne VXJump73
    jmp VMDone
VXJump73:
    mov byte ptr [bp+V_RAW],al
    mov ax,[bp+V_COL]
    inc ax
    call TextStrip
    mov ah,byte ptr [bp+V_RAW]
    mov cx,[bp+V_SHIFT]
    shl ax,cl
    mov al,ah
VMDone:
    ret
TextMask endp

; AX signed strip index -> AL byte or zero outside current glyph.
TextStrip proc near
    or ax,ax
    jns VXJump74
    jmp VStripZero
VXJump74:
    mov bx,ax
    shl bx,1
    shl bx,1
    shl bx,1
    cmp bx,[bp+U_WIDTH]
    jb VXJump75
    jmp VStripZero
VXJump75:
    mul word ptr [bp+U_HEIGHT]
    add ax,[bp+U_GLYPH]
    add ax,[bp+V_SRCY]
    mov bx,ax
    mov es,[bp+U_FONT]
    mov al,es:[bx]
    ret
VStripZero:
    xor al,al
    ret
TextStrip endp

; Checked positive-count input setup, matching internal TextMeasure.
TextLoad proc near
    mov ax,[bp+26]
    mov bx,[bp+24]
    call FontOpen
    jc VXJump76
    jmp XLJump54
VXJump76:
    jmp XLBad
XLJump54:
    mov ax,[bp+32]
    mov bx,[bp+30]
    mov [bp+U_STRING],bx
    mov [bp+U_STRING+2],ax
    mov cx,[bp+U_N]
    call CheckReadSpan
    jc VXJump77
    jmp XLJump55
VXJump77:
    jmp XLBad
XLJump55:
    mov ax,[bp+14]
    mov bx,[bp+12]
    mov [bp+U_VECTOR],bx
    mov [bp+U_VECTOR+2],ax
    or ax,bx
    mov [bp+U_USEDV],ax
    jz VXJump78
    jmp XLJump56
VXJump78:
    jmp XLMode
XLJump56:
    mov ax,[bp+14]
    xor dx,dx
    shl cx,1
    rcl dx,1
    xor di,di
    call TextSpan
    jc VXJump79
    jmp XLJump57
VXJump79:
    jmp XLBad
XLJump57:
XLMode:
    mov ax,[bp+22]
    mov bx,[bp+20]
    mov [bp+U_DM],bx
    mov [bp+U_DM+2],ax
    mov cx,24
    call CheckReadSpan
    jc VXJump80
    jmp XLJump58
VXJump80:
    jmp XLBad
XLJump58:
    mov es,ax
    mov ax,es:[bx+12]
    mov [bp+U_ACTIVE],ax
    mov ax,es:[bx+14]
    mov [bp+U_QUOT],ax
    mov ax,es:[bx+16]
    mov [bp+U_ERR],ax
    mov [bp+U_INITIAL],ax
    mov ax,es:[bx+18]
    mov [bp+U_REM],ax
    mov ax,es:[bx+20]
    mov [bp+U_BCOUNT],ax
    mov ax,es:[bx+22]
    mov [bp+U_CHARX],ax
    cmp word ptr [bp+U_ACTIVE],0
    je VXJump81
    jmp XLJump59
VXJump81:
    jmp XLOK
XLJump59:
    mov ax,[bp+U_BCOUNT]
    or ax,ax
    jle VXJump82
    jmp XLJump60
VXJump82:
    jmp XLBad
XLJump60:
    cmp word ptr [bp+U_ERR],1
    jl VXJump83
    jmp XLJump61
VXJump83:
    jmp XLBad
XLJump61:
    cmp [bp+U_ERR],ax
    ja VXJump84
    jmp XLJump62
VXJump84:
    jmp XLBad
XLJump62:
    mov ax,[bp+U_ACTIVE]
    cwd
    idiv word ptr [bp+U_BCOUNT]
    cmp ax,[bp+U_QUOT]
    jne VXJump85
    jmp XLJump63
VXJump85:
    jmp XLBad
XLJump63:
    cmp dx,[bp+U_REM]
    jne VXJump86
    jmp XLJump64
VXJump86:
    jmp XLBad
XLJump64:
XLOK:
    clc
    ret
XLBad:
    stc
    ret
TextLoad endp

; Collect an exact physical envelope from a clipped nonempty screen region.
; A bank's rows increase monotonically, so only first/last four rows matter.
; No borrowed selectors are loaded and no framebuffer is read in this pass.
public TextScreenCollect, TextScreenCheck, TextScreenMap
TextScreenCollect proc near
    mov ax,[bp+V_REGION]
    cmp ax,640
    jb @F
    jmp TSCBad
@@:
    mov bx,[bp+V_REGION+4]
    cmp bx,640
    jbe @F
    jmp TSCBad
@@:
    dec bx
    mov cl,3
    shr ax,cl
    shr bx,cl
    mov [bp+V_XBYTE],ax
    mov [bp+V_XLAST],bx
    mov ax,[bp+V_REGION+2]
    cmp ax,400
    jb @F
    jmp TSCBad
@@:
    mov bx,[bp+V_REGION+6]
    cmp bx,400
    jbe @F
    jmp TSCBad
@@:
    sub bx,ax
    cmp bx,4
    jbe TSCCount
    mov bx,4
TSCCount:
    mov [bp+V_NROWS],bx
    mov [bp+V_NSCAN],bx
    mov [bp+V_SCAN],ax
    mov word ptr [bp+V_MINROW],0ffffh
    mov word ptr [bp+V_MAXROW],0
TSCMin:
    mov ax,[bp+V_SCAN]
    call ScreenFillOffset
    cmp ax,[bp+V_MINROW]
    jae TSCMinNext
    mov [bp+V_MINROW],ax
TSCMinNext:
    inc word ptr [bp+V_SCAN]
    dec word ptr [bp+V_NSCAN]
    jnz TSCMin
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_NROWS]
    mov [bp+V_SCAN],ax
TSCMax:
    mov ax,[bp+V_SCAN]
    call ScreenFillOffset
    cmp ax,[bp+V_MAXROW]
    jbe TSCMaxNext
    mov [bp+V_MAXROW],ax
TSCMaxNext:
    inc word ptr [bp+V_SCAN]
    dec word ptr [bp+V_NROWS]
    jnz TSCMax
    mov ax,[bp+V_MINROW]
    add ax,[bp+V_XBYTE]
    cmp ax,[bp+V_PMIN]
    jae TSCMaximum
    mov [bp+V_PMIN],ax
TSCMaximum:
    mov ax,[bp+V_MAXROW]
    add ax,[bp+V_XLAST]
    cmp ax,[bp+V_PMAX]
    jbe TSCDone
    mov [bp+V_PMAX],ax
TSCDone:
    clc
    ret
TSCBad:
    stc
    ret
TextScreenCollect endp

; Validate every plane once, after ALL rectangles, before cursor exclusion.
; Ordinary data selectors have contiguous valid offsets; min/max proves each
; touched span even when the logical union contains gaps. Nothing is cached
; across calls. Empty output requires no screen selector access.
TextScreenCheck proc near
    mov ax,[bp+V_CURBOX]
    cmp ax,[bp+V_CURBOX+4]
    jae TSCheckDone
    mov [bp+V_REGION],ax
    mov ax,[bp+V_CURBOX+2]
    cmp ax,[bp+V_CURBOX+6]
    jae TSCheckDone
    mov [bp+V_REGION+2],ax
    mov ax,[bp+V_CURBOX+4]
    mov [bp+V_REGION+4],ax
    mov ax,[bp+V_CURBOX+6]
    mov [bp+V_REGION+6],ax
    call TextScreenCollect
    jc TSCheckBad
    cmp ScreenGate,1
    jne TSCheckBad
    cmp DriverState,ACTIVE
    jne TSCheckBad
    cmp word ptr [bp+S_PLANES],4
    jne TSCheckBad
    mov cx,[bp+V_PMAX]
    sub cx,[bp+V_PMIN]
    inc cx
    xor di,di
TSCheckPlane:
    mov ax,PlaneSelectors[di]
    mov bx,PlaneBases[di]
    add bx,[bp+V_PMIN]
    jc TSCheckBad
    call CheckSpan
    jc TSCheckBad
    add di,2
    cmp di,8
    jb TSCheckPlane
TSCheckDone:
    clc
    ret
TSCheckBad:
    stc
    ret
TextScreenCheck endp

; First row of a proved rectangle. Later rows advance the four pointers.
TextScreenMap proc near
    mov ax,[bp+V_REGION+2]
    call ScreenFillOffset
    mov bx,[bp+V_REGION]
    shr bx,1
    shr bx,1
    shr bx,1
    add ax,bx
    xor si,si
    xor di,di
TSMapPlane:
    mov bx,PlaneBases[di]
    add bx,ax
    mov [bp+S_ADDR+si],bx
    mov bx,PlaneSelectors[di]
    mov [bp+S_ADDR+si+2],bx
    add si,4
    add di,2
    cmp di,8
    jb TSMapPlane
    ret
TextScreenMap endp

; Screen-only solid text background. TextScreenCheck proved all rectangles
; before the write pass. Keep background-before-ink ordering and ownership.
public TextScreenSolid
TextScreenSolid proc near
    mov word ptr [bp+V_BPHASE],0
TSSPhase:
    mov ax,[bp+V_REGION+2]
    add ax,[bp+V_BPHASE]
    call ScreenFillOffset
    mov bx,[bp+V_REGION]
    shr bx,1
    shr bx,1
    shr bx,1
    add ax,bx
    mov [bp+V_BSTART],ax
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_REGION+2]
    sub ax,[bp+V_BPHASE]
    add ax,3
    shr ax,1
    shr ax,1
    mov [bp+V_BROWS],ax
    mov word ptr [bp+V_BPLANE],0
TSSPlane:
    mov si,[bp+V_BPLANE]
    mov es,PlaneSelectors[si]
    mov di,PlaneBases[si]
    add di,[bp+V_BSTART]
    mov ax,[bp+V_COLOR]
    mov cx,si
    shr cx,1
    shr al,cl
    and al,1
    neg al
    mov dl,byte ptr [bp+V_FIRST]
    mov dh,byte ptr [bp+V_LAST]
    mov cx,[bp+V_BYTES]
    mov si,[bp+V_BROWS]
    mov bx,80
    call ScreenConstantRows
    add word ptr [bp+V_BPLANE],2
    cmp word ptr [bp+V_BPLANE],8
    jb TSSPlane
    inc word ptr [bp+V_BPHASE]
    mov ax,[bp+V_BPHASE]
    cmp ax,4
    jae TSSDone
    add ax,[bp+V_REGION+2]
    cmp ax,[bp+V_REGION+6]
    jae TSSDone
    jmp TSSPhase
TSSDone:
    clc
    ret
TextScreenSolid endp

; Ordinary interleaved bitmaps have monotonic complete-row addresses.
; Prove the first and last touched full row with the original validator:
; both validate format, arithmetic, selector rights and ALL plane padding.
; Every intervening full row is then within the same stable selector span.
; Huge bitmaps retain the original per-glyph/per-row preflight.
public TextMemoryCheck
TextMemoryCheck proc near
    mov ax,[bp+V_CURBOX]
    cmp ax,[bp+V_CURBOX+4]
    jae TMCEmpty
    mov [bp+S_X],ax
    mov ax,[bp+V_CURBOX+2]
    cmp ax,[bp+V_CURBOX+6]
    jae TMCEmpty
    mov [bp+S_Y],ax
    call PreparePoint
    jc TMCDone
    mov ax,[bp+V_CURBOX+4]
    dec ax
    mov [bp+S_X],ax
    mov ax,[bp+V_CURBOX+6]
    dec ax
    mov [bp+S_Y],ax
    jmp PreparePoint
TMCEmpty:
    clc
TMCDone:
    ret
TextMemoryCheck endp

; Only TextInkWalk calls this, after TextLoad + the complete successful TextRun.
; That invocation has already checked every used/default glyph and font bound.
; Borrowed font/string storage is stable through both passes, as required by
; the existing all-preflight-before-output contract. No persistent cache.
TextReplayGlyph proc near
    cmp ax,[bp+U_FIRST]
    jb TRGDefault
    cmp ax,[bp+U_LAST]
    ja TRGDefault
    sub ax,[bp+U_FIRST]
    jmp TRGSlot
TRGDefault:
    mov ax,[bp+U_DEFAULT]
TRGSlot:
    mov [bp+U_INDEX],ax
    mov cx,ax
    shl ax,2
    mov si,ax
    mov es,[bp+U_FONT]
    mov ax,es:[si+118]
    mov bx,es:[si+120]
    mov [bp+U_WIDTH],ax
    mov [bp+U_GLYPH],bx
    clc
    ret
TextReplayGlyph endp
