; Phase 10: checked 0200h font access, layout/extent core and GetCharWidth.
; All state is SS:BP-relative; S_* space above -96 is reserved for the shared
; surface adapter in the future text renderer. No global font/string cache.
U_FONT equ -98
U_LIMIT equ -100
U_HEIGHT equ -102
U_FIXED equ -104
U_FIRST equ -106
U_LAST equ -108
U_DEFAULT equ -110
U_BREAK equ -112
U_FCOUNT equ -114
U_TABLEEND equ -116
U_WIDTH equ -118
U_GLYPH equ -120
U_INDEX equ -122
U_CHARX equ -124
U_ACTIVE equ -126
U_QUOT equ -128
U_REM equ -130
U_BCOUNT equ -132
U_ERR equ -134
U_INITIAL equ -136
U_POSITION equ -140
U_RIGHT equ -144
U_ADVANCE equ -148
U_STRING equ -152
U_VECTOR equ -156
U_DM equ -160
U_N equ -162
U_COMMIT equ -164
U_USEDV equ -166
U_SCAN equ -168
U_OUT equ -172
U_OUTCOUNT equ -174
U_CODE equ -176
U_CACHE equ -688                 ; 256 WORD intrinsic widths, -688..-177
TXT_LOCAL equ 224
TXT_RESERVE equ 352
WIDTH_LOCAL equ 736
WIDTH_RESERVE equ 864
public TextSpan, FontOpen, FontGlyph, FontSlot, FontBit
public TextBreak, TextAdvance, TextRun, TextMeasure, GetCharWidth
public TextRunPlainGlyph

; AX selector, BX offset, DX:CX positive length <=65536, DI=0 read/1 write.
; Preserve input registers. Full 64K is legal only at offset zero, checked in
; two 32K spans so zero never masquerades as a nonempty WORD length.
TextSpan proc near
    push bx
    push cx
    push dx
    cmp dx,0
    jne UTJump0
    jmp TSSmall
UTJump0:
    cmp dx,1
    je UTJump1
    jmp TSBad
UTJump1:
    or cx,cx
    jz UTJump2
    jmp TSBad
UTJump2:
    or bx,bx
    jz UTJump3
    jmp TSBad
UTJump3:
    mov cx,8000h
    call TSCheck
    jnc UTJump4
    jmp TSDone
UTJump4:
    mov bx,8000h
TSSmall:
    call TSCheck
    jmp TSDone
TSBad:
    stc
TSDone:
    pop dx
    pop cx
    pop bx
    ret
TSCheck:
    or di,di
    jz UTJump5
    jmp TSWrite
UTJump5:
    call CheckReadSpan
    ret
TSWrite:
    call CheckSpan
    ret
TextSpan endp

; AX:BX font selector:FONTINFO offset. Check the owned loaded allocation,
; table (including extra entry), standard in-segment raster type and metrics.
; Used glyph spans are subsequently checked by FontGlyph. ES/AX..DI volatile.
FontOpen proc near
    cmp bx,0042h
    je UTJump6
    jmp FOBad
UTJump6:
    mov [bp+U_FONT],ax
    xor bx,bx
    mov cx,118
    call CheckReadSpan
    jnc UTJump7
    jmp FOBad
UTJump7:
    mov es,ax
    cmp word ptr es:[0],0200h
    je UTJump8
    jmp FOBad
UTJump8:
    cmp word ptr es:[0042h],0
    je UTJump9
    jmp FOBad
UTJump9:
    ; GDI 16:028D copies dfSize unchanged when enlarging a 0200h font.
    ; Bound loaded fonts by their still-owned global allocation, not that
    ; on-disk header field. Query only; never acquire/release ownership.
    push word ptr [bp+U_FONT]
    call GlobalHandle
    or ax,ax
    jnz FOHandleFound
    jmp FOBad
FOHandleFound:
    cmp dx,[bp+U_FONT]
    je FOHandleMatches
    jmp FOBad
FOHandleMatches:
    push ax
    call GlobalSize
    or dx,dx
    jz FOSizeSmall
    xor ax,ax                   ; first 64K only for the small-font convention
    mov dx,1
FOSizeSmall:
    mov cx,ax
    mov ax,[bp+U_FONT]
    xor bx,bx
    xor di,di
    call TextSpan
    jnc FOStorageValid
    jmp FOBad
FOStorageValid:
    or dx,dx
    jnz FOSize
    cmp cx,122
    jae FOSize
    jmp FOBad
FOSize:
    dec cx
    mov [bp+U_LIMIT],cx
    mov es,[bp+U_FONT]
    mov ax,es:[0058h]
    or ax,ax
    jg UTJump13
    jmp FOBad
UTJump13:
    mov [bp+U_HEIGHT],ax
    mov ax,es:[0056h]
    or ax,ax
    jns UTJump14
    jmp FOBad
UTJump14:
    mov [bp+U_FIXED],ax
    xor ax,ax
    mov al,es:[005fh]
    mov [bp+U_FIRST],ax
    xor bx,bx
    mov bl,es:[0060h]
    mov [bp+U_LAST],bx
    sub bx,ax
    jnc UTJump15
    jmp FOBad
UTJump15:
    inc bx
    mov [bp+U_FCOUNT],bx
    xor ax,ax
    mov al,es:[0061h]
    cmp ax,bx
    jb UTJump16
    jmp FOBad
UTJump16:
    mov [bp+U_DEFAULT],ax
    mov al,es:[0062h]
    cmp ax,bx
    jb UTJump17
    jmp FOBad
UTJump17:
    mov [bp+U_BREAK],ax
    mov ax,bx
    inc ax
    shl ax,1
    shl ax,1
    add ax,118
    mov [bp+U_TABLEEND],ax
    dec ax
    cmp ax,[bp+U_LIMIT]
    jbe UTJump18
    jmp FOBad
UTJump18:
    mov ax,bx                   ; validate the extra table entry too
    call FontSlot
    ret
FOBad:
    stc
    ret
FontOpen endp

; AX character code WORD -> AX intrinsic width, BX glyph offset, CX index.
; Map default before break matching. No glyph bytes are fetched here.
FontGlyph proc near
    ; FontOpen already proved FIRST/LAST/count/default in this invocation.
    ; Subtraction carries iff the WORD code is below FIRST; count rejects
    ; every code above LAST. The chosen index is therefore proved here.
    sub ax,[bp+U_FIRST]
    jc FGDefault
    cmp ax,[bp+U_FCOUNT]
    jb FSIndexReady
FGDefault:
    mov ax,[bp+U_DEFAULT]
    jmp FSIndexReady
FontGlyph endp

; AX index, 0..count inclusive (count is the extra entry).
FontSlot proc near
    cmp ax,[bp+U_FCOUNT]
    jbe FSIndexReady
    stc
    ret
FSIndexReady:
    mov [bp+U_INDEX],ax
    mov si,ax
    shl si,1
    shl si,1
    add si,118
    mov es,[bp+U_FONT]
    mov ax,es:[si]
    mov bx,es:[si+2]
    or ax,ax
    js FSBad
    mov [bp+U_WIDTH],ax
    mov [bp+U_GLYPH],bx
    cmp word ptr [bp+U_FIXED],0
    je FSOffset
    mov cx,[bp+U_INDEX]
    cmp cx,[bp+U_FCOUNT]
    je FSOffset
    cmp ax,[bp+U_FIXED]
    jne FSBad
FSOffset:
    cmp bx,[bp+U_TABLEEND]
    jb FSBad
    or ax,ax
    jz FSZero
    ; One byte per column for widths 1..8: retain the exact allocation
    ; extent and overflow proof without multiplying by one. No font cache.
    cmp ax,8
    ja FSWideGlyph
    mov ax,[bp+U_HEIGHT]
    xor dx,dx
    jmp FSGlyphSize
FSWideGlyph:
    add ax,7
    mov cl,3
    shr ax,cl
    mul word ptr [bp+U_HEIGHT]
    or dx,dx
    jnz FSBad
FSGlyphSize:
    dec ax
    add ax,bx
    jc FSBad
    cmp ax,[bp+U_LIMIT]
    ja FSBad
FSGood:
    mov ax,[bp+U_WIDTH]
    mov bx,[bp+U_GLYPH]
    mov cx,[bp+U_INDEX]
    clc
    ret
FSZero:
    mov ax,[bp+U_LIMIT]
    cmp ax,0ffffh
    je FSGood
    inc ax
    cmp bx,ax
    jbe FSGood
FSBad:
    stc
    ret
FontSlot endp

; Current validated glyph, AX=x DX=y -> AX=0/1, CF error. Preserve others.
FontBit proc near
    push bx
    push cx
    push dx
    push si
    push di
    push es
    cmp ax,[bp+U_WIDTH]
    jb UTJump33
    jmp FBFail
UTJump33:
    cmp dx,[bp+U_HEIGHT]
    jb UTJump34
    jmp FBFail
UTJump34:
    mov di,dx
    mov si,ax
    and si,7
    mov cl,3
    shr ax,cl
    mul word ptr [bp+U_HEIGHT]
    add ax,di
    add ax,[bp+U_GLYPH]
    mov bx,ax
    mov es,[bp+U_FONT]
    mov dl,80h
    mov cx,si
    shr dl,cl
    xor ax,ax
    test es:[bx],dl
    jnz UTJump35
    jmp FBOK
UTJump35:
    inc ax
FBOK:
    clc
    jmp FBReturn
FBFail:
    xor ax,ax
    stc
FBReturn:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret
FontBit endp

; Validated local DDA -> signed DX:AX break adjustment. Only local error moves.
TextBreak proc near
    xor ax,ax
    xor dx,dx
    cmp word ptr [bp+U_ACTIVE],0
    jne UTJump36
    jmp TBReturn
UTJump36:
    mov ax,[bp+U_QUOT]
    cwd
    mov bx,[bp+U_REM]
    or bx,bx
    jnz UTJump37
    jmp TBReturn
UTJump37:
    js UTJump38
    jmp TBMagnitude
UTJump38:
    neg bx
TBMagnitude:
    sub [bp+U_ERR],bx
    jle UTJump39
    jmp TBReturn
UTJump39:
    mov bx,[bp+U_BCOUNT]
    add [bp+U_ERR],bx
    cmp word ptr [bp+U_REM],0
    jge UTJump40
    jmp TBMinus
UTJump40:
    add ax,1
    adc dx,0
    ret
TBMinus:
    sub ax,1
    sbb dx,0
TBReturn:
    ret
TextBreak endp

; AX natural/vector advance (signed); current glyph already selected.
; DX:AX clamped nonnegative advance, preserving 32-bit intermediate sums.
TextAdvance proc near
    ; Without character extra or break DDA, only a negative explicit vector
    ; needs clamping. The natural-width preflight bypasses this helper, but
    ; both rendering passes and explicit vectors still benefit.
    mov bx,[bp+U_CHARX]
    or bx,[bp+U_ACTIVE]
    jnz TAGeneral
    cwd
    or dx,dx
    jns TAPlainDone
    xor ax,ax
    xor dx,dx
TAPlainDone:
    ret
TAGeneral:
    cwd
    mov bx,[bp+U_CHARX]
    xor cx,cx
    or bx,bx
    jns TACharSign
    dec cx
TACharSign:
    add ax,bx
    adc dx,cx
    mov bx,[bp+U_INDEX]
    cmp bx,[bp+U_BREAK]
    je UTJump41
    jmp TAClamp
UTJump41:
    push ax
    push dx
    call TextBreak
    mov bx,ax
    mov cx,dx
    pop dx
    pop ax
    add ax,bx
    adc dx,cx
TAClamp:
    or dx,dx
    js UTJump42
    jmp TADone
UTJump42:
    xor ax,ax
    xor dx,dx
TADone:
    ret
TextAdvance endp

; Full layout, no caller writes. Inputs/pointers/state are validated by caller.
; Keep exact unsigned 32-bit positions (maximum <2^32 for 32768 characters),
; track glyph-right and advance endpoints, and continue DDA after saturation.
TextRun proc near
    mov word ptr [bp+U_SCAN],0
    mov word ptr [bp+U_POSITION],0
    mov word ptr [bp+U_POSITION+2],0
    mov word ptr [bp+U_RIGHT],0
    mov word ptr [bp+U_RIGHT+2],0
    ; Natural widths with no extra spacing or break DDA are nonnegative.
    ; In that case every glyph-right equals the next position. Validate every
    ; glyph, accumulate exact 32-bit widths, and saturate only the return.
    ; No extra stack, retained font data, or relaxed font/span proof.
    mov ax,[bp+U_USEDV]
    or ax,[bp+U_CHARX]
    or ax,[bp+U_ACTIVE]
    jnz TRNext
TRPlainNext:
    mov bx,[bp+U_STRING]
    add bx,[bp+U_SCAN]
    mov es,[bp+U_STRING+2]
    xor ax,ax
    mov al,es:[bx]
    call FontGlyph
    jnc TextRunPlainGlyph
    jmp TRBad
TextRunPlainGlyph:
    add [bp+U_POSITION],ax
    adc word ptr [bp+U_POSITION+2],0
    inc word ptr [bp+U_SCAN]
    mov ax,[bp+U_SCAN]
    cmp ax,[bp+U_N]
    jb TRPlainNext
    mov ax,[bp+U_WIDTH]
    mov [bp+U_ADVANCE],ax
    mov word ptr [bp+U_ADVANCE+2],0
    mov ax,[bp+U_POSITION]
    mov dx,[bp+U_POSITION+2]
    mov [bp+U_RIGHT],ax
    mov [bp+U_RIGHT+2],dx
    jmp TRFinish
TRNext:
    mov bx,[bp+U_STRING]
    add bx,[bp+U_SCAN]
    mov es,[bp+U_STRING+2]
    xor ax,ax
    mov al,es:[bx]
    call FontGlyph
    jnc UTJump43
    jmp TRBad
UTJump43:
    cmp word ptr [bp+U_USEDV],0
    jne UTJump44
    jmp TRAdvance
UTJump44:
    mov bx,[bp+U_SCAN]
    shl bx,1
    add bx,[bp+U_VECTOR]
    mov es,[bp+U_VECTOR+2]
    mov ax,es:[bx]
TRAdvance:
    call TextAdvance
    mov [bp+U_ADVANCE],ax
    mov [bp+U_ADVANCE+2],dx
    mov ax,[bp+U_POSITION]
    mov dx,[bp+U_POSITION+2]
    add ax,[bp+U_WIDTH]
    adc dx,0
    call TRMax
    mov ax,[bp+U_POSITION]
    mov dx,[bp+U_POSITION+2]
    add ax,[bp+U_ADVANCE]
    adc dx,[bp+U_ADVANCE+2]
    mov [bp+U_POSITION],ax
    mov [bp+U_POSITION+2],dx
    call TRMax
    inc word ptr [bp+U_SCAN]
    mov ax,[bp+U_SCAN]
    cmp ax,[bp+U_N]
    jae UTJump45
    jmp TRNext
UTJump45:
TRFinish:
    mov ax,[bp+U_RIGHT]
    cmp word ptr [bp+U_RIGHT+2],0
    je UTJump46
    jmp TRSaturated
UTJump46:
    cmp ax,32767
    ja UTJump47
    jmp TRDone
UTJump47:
TRSaturated:
    mov ax,32767
TRDone:
    mov dx,[bp+U_HEIGHT]
    clc
    ret
TRBad:
    stc
    ret
TRMax:
    cmp dx,[bp+U_RIGHT+2]
    jae UTJump48
    jmp TRMaxDone
UTJump48:
    jbe UTJump49
    jmp TRStore
UTJump49:
    cmp ax,[bp+U_RIGHT]
    ja UTJump50
    jmp TRMaxDone
UTJump50:
TRStore:
    mov [bp+U_RIGHT],ax
    mov [bp+U_RIGHT+2],dx
TRMaxDone:
    ret
TextRun endp

; Internal measurement/validation ABI, NOT a Windows text export:
; Pascal (lpFont,lpString,signed Count,lpWidths,lpDrawMode), 18 argument bytes.
; Both count signs return DX=height AX=extent; only negative commits BreakErr.
; Positive prepares/replays layout for the future drawing entry; it draws none.
TextMeasure proc far
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
    mov ax,TXT_RESERVE
    call StackRoom
    jnc UTJump51
    jmp TMReturnBad
UTJump51:
    sub sp,TXT_LOCAL
    mov ax,[bp+14]
    mov word ptr [bp+U_COMMIT],0
    or ax,ax
    jnz UTJump52
    jmp TMZero
UTJump52:
    js UTJump53
    jmp TMCount
UTJump53:
    neg ax                       ; 8000h remains unsigned 32768
    mov word ptr [bp+U_COMMIT],1
TMCount:
    mov [bp+U_N],ax
    mov ax,[bp+22]
    mov bx,[bp+20]
    call FontOpen
    jnc UTJump54
    jmp TMBad
UTJump54:
    mov ax,[bp+18]
    mov bx,[bp+16]
    mov [bp+U_STRING],bx
    mov [bp+U_STRING+2],ax
    mov cx,[bp+U_N]
    call CheckReadSpan
    jnc UTJump55
    jmp TMBad
UTJump55:
    mov ax,[bp+12]
    mov bx,[bp+10]
    mov [bp+U_VECTOR],bx
    mov [bp+U_VECTOR+2],ax
    or ax,bx
    mov [bp+U_USEDV],ax
    jnz UTJump56
    jmp TMMode
UTJump56:
    mov ax,[bp+12]
    xor dx,dx
    shl cx,1
    rcl dx,1
    xor di,di
    call TextSpan
    jnc UTJump57
    jmp TMBad
UTJump57:
TMMode:
    mov ax,[bp+8]
    mov bx,[bp+6]
    mov [bp+U_DM],bx
    mov [bp+U_DM+2],ax
    mov cx,24
    call CheckReadSpan
    jnc UTJump58
    jmp TMBad
UTJump58:
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
    jne UTJump59
    jmp TMRun
UTJump59:
    mov ax,[bp+U_BCOUNT]
    or ax,ax
    jg UTJump60
    jmp TMBad
UTJump60:
    cmp word ptr [bp+U_ERR],1
    jge UTJump61
    jmp TMBad
UTJump61:
    cmp [bp+U_ERR],ax
    jbe UTJump62
    jmp TMBad
UTJump62:
    mov ax,[bp+U_ACTIVE]
    cwd
    idiv word ptr [bp+U_BCOUNT]
    cmp ax,[bp+U_QUOT]
    je UTJump63
    jmp TMBad
UTJump63:
    cmp dx,[bp+U_REM]
    je UTJump64
    jmp TMBad
UTJump64:
    cmp word ptr [bp+U_COMMIT],0
    jne UTJump65
    jmp TMRun
UTJump65:
    mov ax,[bp+U_DM+2]
    mov bx,[bp+U_DM]
    add bx,16
    mov cx,2
    call CheckSpan
    jnc UTJump66
    jmp TMBad
UTJump66:
TMRun:
    call TextRun
    jnc UTJump67
    jmp TMBad
UTJump67:
    cmp word ptr [bp+U_COMMIT],0
    jne UTJump68
    jmp TMReturn
UTJump68:
    cmp word ptr [bp+U_ACTIVE],0
    jne UTJump69
    jmp TMReturn
UTJump69:
    mov es,[bp+U_DM+2]
    mov bx,[bp+U_DM]
    mov cx,[bp+U_ERR]
    mov es:[bx+16],cx
    jmp TMReturn
TMZero:
    xor ax,ax
    xor dx,dx
    jmp TMReturn
TMBad:
    xor ax,ax
    mov dx,8000h
TMReturn:
    lea sp,[bp-6]
    jmp TMRestore
TMReturnBad:
    xor ax,ax
    mov dx,8000h
TMRestore:
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    cld
    ret 18
TextMeasure endp

; Public Windows GetCharWidth ordinal 15, original 24-byte Pascal ABI.
; Device/DrawMode/TextXForm are unused for intrinsic 286 raster widths.
; Snapshot <=256 widths before output, so output aliasing the font is safe.
GetCharWidth proc far
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
    mov ax,WIDTH_RESERVE
    call StackRoom
    jnc UTJump70
    jmp GWReturnBad
UTJump70:
    sub sp,WIDTH_LOCAL
    mov bx,[bp+20]
    mov ax,[bp+18]
    sub ax,bx
    jnc UTJump71
    jmp GWBad
UTJump71:
    inc ax
    jnz UTJump72
    jmp GWBad
UTJump72:
    cmp ax,8000h
    jbe UTJump73
    jmp GWBad
UTJump73:
    mov [bp+U_OUTCOUNT],ax
    mov [bp+U_CODE],bx
    mov cx,ax
    xor dx,dx
    shl cx,1
    rcl dx,1
    mov ax,[bp+24]
    mov bx,[bp+22]
    mov [bp+U_OUT],bx
    mov [bp+U_OUT+2],ax
    mov di,1
    call TextSpan
    jnc UTJump74
    jmp GWBad
UTJump74:
    mov ax,[bp+16]
    mov bx,[bp+14]
    call FontOpen
    jnc UTJump75
    jmp GWBad
UTJump75:
    mov word ptr [bp+U_SCAN],0
GWCache:
    mov ax,[bp+U_SCAN]
    call FontSlot
    jnc UTJump76
    jmp GWBad
UTJump76:
    mov di,[bp+U_SCAN]
    shl di,1
    add di,U_CACHE
    mov [bp+di],ax
    inc word ptr [bp+U_SCAN]
    mov ax,[bp+U_SCAN]
    cmp ax,[bp+U_FCOUNT]
    jae UTJump77
    jmp GWCache
UTJump77:
    mov es,[bp+U_OUT+2]
    mov di,[bp+U_OUT]
    mov cx,[bp+U_OUTCOUNT]
    mov bx,[bp+U_CODE]
GWOutput:
    mov ax,bx
    cmp ax,[bp+U_FIRST]
    jae UTJump78
    jmp GWDefault
UTJump78:
    cmp ax,[bp+U_LAST]
    jbe UTJump79
    jmp GWDefault
UTJump79:
    sub ax,[bp+U_FIRST]
    jmp GWIndex
GWDefault:
    mov ax,[bp+U_DEFAULT]
GWIndex:
    mov si,ax
    shl si,1
    add si,U_CACHE
    mov ax,[bp+si]
    mov es:[di],ax
    add di,2
    inc bx
    dec cx
    jz UTJump80
    jmp GWOutput
UTJump80:
    mov ax,1
    jmp GWReturn
GWBad:
    xor ax,ax
GWReturn:
    lea sp,[bp-6]
    jmp GWRestore
GWReturnBad:
    xor ax,ax
GWRestore:
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    cld
    ret 24
GetCharWidth endp
