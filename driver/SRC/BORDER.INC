; 0.13: source-free screen borders with one descriptor/brush snapshot,
; one mapping preflight and one drawing exclusion. The four DDK windmill
; rectangles are disjoint, preserving XOR/invert corner semantics.
; Thick/overflowing or unsupported calls return 0 before writing for GDI.
FB_OUTER equ -480
FB_CLIP equ -488
FB_SEGS equ -528
FB_INDEX equ -530
FB_THICKX equ -534
FB_THICKY equ -536
public BorderLoadSegment
FastBorder proc far
    H_ENTER
    PERFINC PerfBorder
    mov ax,X_RESERVE
    call StackRoom
    jnc FBBStack
    jmp FBBEarly
FBBStack:
    sub sp,X_LOCAL
    mov word ptr [bp+S_HELD],0
    mov ax,[bp+28]
    or ax,ax
    js FBBBad
    mov [bp+FB_THICKX],ax
    mov ax,[bp+26]
    or ax,ax
    js FBBBad
    mov [bp+FB_THICKY],ax
    mov ax,[bp+22]
    mov dx,[bp+24]
    call BlitDecode
    jc FBBBad
    test bx,2
    jnz FBBBad
    mov [bp+X_ROP],ax
    mov [bp+X_USE],bx
    cmp ax,0aah
    je FBBOk
    mov ax,[bp+32]
    mov bx,[bp+30]
    mov cx,8
    lea di,[bp+FB_OUTER]
    call OutputSnapshot
    jc FBBBad
    mov ax,[bp+FB_OUTER+4]
    sub ax,[bp+FB_OUTER]
    jo FBBBad
    or ax,ax
    jle FBBOk
    shr ax,1
    cmp ax,[bp+FB_THICKX]
    jb FBBBad
    mov ax,[bp+FB_OUTER+6]
    sub ax,[bp+FB_OUTER+2]
    jo FBBBad
    or ax,ax
    jle FBBOk
    shr ax,1
    cmp ax,[bp+FB_THICKY]
    jb FBBBad
    jmp FBBClip
FBBBad:
    xor ax,ax
    jmp FBBRelease
FBBOk:
    mov ax,0ffffh
    jmp FBBRelease
FBBClip:
    mov ax,[bp+8]
    mov bx,[bp+6]
    mov cx,8
    lea di,[bp+FB_CLIP]
    call OutputSnapshot
    jc FBBBad
    cmp word ptr [bp+FB_CLIP],0
    jge FBBClipTop
    mov word ptr [bp+FB_CLIP],0
FBBClipTop:
    cmp word ptr [bp+FB_CLIP+2],0
    jge FBBClipRight
    mov word ptr [bp+FB_CLIP+2],0
FBBClipRight:
    cmp word ptr [bp+FB_CLIP+4],640
    jle FBBClipBottom
    mov word ptr [bp+FB_CLIP+4],640
FBBClipBottom:
    cmp word ptr [bp+FB_CLIP+6],400
    jle FBBClipReady
    mov word ptr [bp+FB_CLIP+6],400
FBBClipReady:
    mov word ptr [bp+X_CURBOX],640
    mov word ptr [bp+X_CURBOX+2],400
    mov word ptr [bp+X_CURBOX+4],0
    mov word ptr [bp+X_CURBOX+6],0
    mov word ptr [bp+FB_INDEX],0
FBBGeometry:
    mov ax,[bp+FB_OUTER]
    mov bx,[bp+FB_OUTER+2]
    mov cx,[bp+FB_OUTER+4]
    mov dx,[bp+FB_OUTER+6]
    mov si,[bp+FB_INDEX]
    cmp si,8
    je FBBRight
    cmp si,16
    je FBBBottom
    cmp si,24
    je FBBLeft
    sub cx,[bp+FB_THICKX]
    mov dx,bx
    add dx,[bp+FB_THICKY]
    jmp FBBIntersect
FBBRight:
    mov ax,cx
    sub ax,[bp+FB_THICKX]
    sub dx,[bp+FB_THICKY]
    jmp FBBIntersect
FBBBottom:
    add ax,[bp+FB_THICKX]
    mov bx,dx
    sub bx,[bp+FB_THICKY]
    jmp FBBIntersect
FBBLeft:
    add bx,[bp+FB_THICKY]
    mov cx,ax
    add cx,[bp+FB_THICKX]
FBBIntersect:
    cmp ax,[bp+FB_CLIP]
    jge FBBTop
    mov ax,[bp+FB_CLIP]
FBBTop:
    cmp bx,[bp+FB_CLIP+2]
    jge FBBRightClip
    mov bx,[bp+FB_CLIP+2]
FBBRightClip:
    cmp cx,[bp+FB_CLIP+4]
    jle FBBBottomClip
    mov cx,[bp+FB_CLIP+4]
FBBBottomClip:
    cmp dx,[bp+FB_CLIP+6]
    jle FBBStore
    mov dx,[bp+FB_CLIP+6]
FBBStore:
    cmp ax,cx
    jge FBBEmpty
    cmp bx,dx
    jge FBBEmpty
    mov [bp+FB_SEGS+si],ax
    mov [bp+FB_SEGS+si+2],bx
    mov [bp+FB_SEGS+si+4],cx
    mov [bp+FB_SEGS+si+6],dx
    lea si,[bp+X_CURBOX]
    call CursorRectUnion
    jmp FBBNextGeometry
FBBEmpty:
    mov word ptr [bp+FB_SEGS+si+4],0
FBBNextGeometry:
    add word ptr [bp+FB_INDEX],8
    cmp word ptr [bp+FB_INDEX],32
    jae @F
    jmp FBBGeometry
@@:
    cmp word ptr [bp+X_CURBOX+4],0
    jne @F
    jmp FBBOk
@@:
    mov ax,[bp+20]
    mov bx,[bp+18]
    call SnapshotSurface
    jnc @F
    jmp FBBBad
@@:
    call ClassifyFormat
    jnc @F
    jmp FBBBad
@@:
    cmp word ptr [bp+S_KIND],1
    je @F
    jmp FBBBad
@@:
    mov word ptr [bp+X_DK],1
    mov ax,[bp+S_PLANES]
    mov [bp+X_DP],ax
    test word ptr [bp+X_USE],4
    jz FBBPreflight
    mov ax,[bp+16]
    mov bx,[bp+14]
    mov cx,56
    lea di,[bp+T_BRUSH]
    call OutputSnapshot
    jnc @F
    jmp FBBBad
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jbe @F
    jmp FBBBad
@@:
    cmp word ptr [bp+T_BRUSH+48],1
    jne @F
    jmp FBBBad
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jne FBBPreflight
    mov ax,[bp+12]
    mov bx,[bp+10]
    add bx,4
    jnc @F
    jmp FBBBad
@@:
    mov cx,8
    lea di,[bp+T_DM+4]
    call OutputSnapshot
    jnc @F
    jmp FBBBad
@@:
FBBPreflight:
    lea si,[bp+X_CURBOX]
    call BorderLoadSegment
    call ScreenFillPreflight
    jnc @F
    jmp FBBBad
@@:
    mov ax,[bp+X_CURBOX]
    mov bx,[bp+X_CURBOX+2]
    mov cx,[bp+X_CURBOX+4]
    mov dx,[bp+X_CURBOX+6]
    call EnterDrawingRect
    jnc @F
    jmp FBBBad
@@:
    mov word ptr [bp+S_HELD],1
    mov word ptr [bp+FB_INDEX],0
FBBRender:
    mov si,[bp+FB_INDEX]
    cmp word ptr [bp+FB_SEGS+si+4],0
    je FBBNextRender
    lea si,[bp+FB_SEGS+si]
    call BorderLoadSegment
    call ScreenFillDraw
FBBNextRender:
    add word ptr [bp+FB_INDEX],8
    cmp word ptr [bp+FB_INDEX],32
    jb FBBRender
    PERFINC PerfBorderDone
    mov ax,0ffffh
FBBRelease:
    cmp word ptr [bp+S_HELD],0
    je FBBReturn
    call LeaveDrawing
    jmp FBBReturn
FBBEarly:
    xor ax,ax
FBBReturn:
    H_EXIT 28
FastBorder endp
; SS:SI rectangle -> existing fill geometry. Other frame locals stay intact.
BorderLoadSegment proc near
    mov ax,ss:[si]
    mov [bp+X_DX],ax
    mov cx,ss:[si+4]
    sub cx,ax
    mov [bp+X_W],cx
    mov ax,ss:[si+2]
    mov [bp+X_DY],ax
    mov cx,ss:[si+6]
    sub cx,ax
    mov [bp+X_H],cx
    ret
BorderLoadSegment endp
