; 0.10: compose a complete screen text call into a bounded 32-row strip.
; Ink and coverage are distinct: gaps/clipping remain untouched, overlapping
; glyphs are ORed, and every background is composed before any foreground.
; The complete ordinary text/font preflight and screen ownership precede this
; path. Large regions and nested text calls retain the original local writer.
public TextBatchBegin, TextBatchRectangle, TextBatchFlush
TextBatchBegin proc near
    cmp word ptr [bp+V_CURBOX+4],640
    jbe @F
    jmp TBBNo
@@:
    cmp word ptr [bp+S_KIND],1
    je TBBLayoutReady
    cmp word ptr [bp+S_DESC+22],0
    je @F
    jmp TBBNo
@@:
TBBLayoutReady:
    mov ax,[bp+V_CURBOX+6]
    sub ax,[bp+V_CURBOX+2]
    ja @F
    jmp TBBNo
@@:
    cmp ax,32
    jbe @F
    jmp TBBNo
@@:
    mov bx,[bp+V_CURBOX+4]
    sub bx,[bp+V_CURBOX]
    ja @F
    jmp TBBNo
@@:
    pushf
    cli
    cmp TextBusy,0
    je @F
    jmp TBBBusy
@@:
    mov TextBusy,1
    popf
    cld
    mov word ptr [bp+V_BATCH],1
    test word ptr [bp+6],2
    jnz TBBModeReady
    cmp word ptr [bp+V_DM+2],2
    je TBBModeReady
    inc word ptr [bp+V_BATCH]
TBBModeReady:
    PERFINC PerfBatchText
    cmp word ptr [bp+S_KIND],0
    jne TBBCounted
    PERFINC PerfMemoryBatch
TBBCounted:
    ; Clear only the byte columns that this invocation may compose/read.
    ; Keep the established 80-byte pitch and exact coverage write contract.
    mov dx,ax
    mov si,[bp+V_CURBOX]
    shr si,3
    mov bx,[bp+V_CURBOX+4]
    dec bx
    shr bx,3
    sub bx,si
    inc bx
    push ds
    pop es
    xor ax,ax
TBBClearRow:
    mov di,DataOFFSET TextInk
    add di,si
    mov cx,bx
    shr cx,1
    rep stosw
    jnc TBBCover
    stosb
TBBCover:
    cmp word ptr [bp+V_BATCH],2
    je TBBNext
    mov di,DataOFFSET TextCover
    add di,si
    mov cx,bx
    shr cx,1
    rep stosw
    jnc TBBNext
    stosb
TBBNext:
    add si,80
    dec dx
    jnz TBBClearRow
TBBNo:
    ret
TBBBusy:
    popf
    ret
TextBatchBegin endp

; Geometry/first/last masks were prepared by TextRectangle. Visit columns
; outside rows so the glyph's vertical-strip multiply and shift are computed
; once per column. A source column outside the glyph is never dereferenced.
TextBatchRectangle proc near
    cmp word ptr [bp+V_BATCH],2
    jne @F
    jmp TextInkRectangle
@@:
    ; Ordinary opaque text has composed its complete clipped V_BOX before
    ; any glyph. U_RIGHT bounds every glyph without extent saturation.
    ; Coverage is therefore already set; only accumulate the glyph ink.
    ; ETO_OPAQUE with transparent text can have a disjoint explicit fill,
    ; so it must retain the combined ink/coverage compositor below.
    cmp word ptr [bp+V_KIND],0
    je TBRComposeBoth
    cmp word ptr [bp+V_DM+2],2
    jne TBRComposeBoth
    jmp TextInkRectangle
TBRComposeBoth:
    mov ax,[bp+V_REGION+2]
    sub ax,[bp+V_CURBOX+2]
    mov bx,80
    mul bx
    mov bx,[bp+V_REGION]
    shr bx,3
    add ax,bx
    mov [bp+V_BSTART],ax
    mov word ptr [bp+V_BYTE],0
TBRColumn:
    mov al,0ffh
    cmp word ptr [bp+V_BYTE],0
    jne TBRLastMask
    and al,byte ptr [bp+V_FIRST]
TBRLastMask:
    mov bx,[bp+V_BYTE]
    inc bx
    cmp bx,[bp+V_BYTES]
    jne TBRMaskReady
    and al,byte ptr [bp+V_LAST]
TBRMaskReady:
    mov byte ptr [bp+V_MASK],al
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_REGION+2]
    mov [bp+V_BROWS],ax
    mov di,[bp+V_BSTART]
    add di,[bp+V_BYTE]
    cmp word ptr [bp+V_KIND],0
    jne TBRGlyph
    cmp word ptr [bp+V_BYTE],0
    jne TBRPaperCounted
    PERFINC PerfBackground
    mov ax,[bp+V_REGION+4]
    sub ax,[bp+V_REGION]
    cmp ax,128
    jb TBRPaperCounted
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_REGION+2]
    cmp ax,64
    jb TBRPaperCounted
    PERFINC PerfBgLarge
TBRPaperCounted:
    mov al,byte ptr [bp+V_MASK]
TBRPaper:
    or TextCover[di],al
    add di,80
    dec word ptr [bp+V_BROWS]
    jnz TBRPaper
    jmp TBRNextColumn
TBRGlyph:
    mov ax,[bp+V_REGION]
    and ax,0fff8h
    mov bx,[bp+V_BYTE]
    shl bx,3
    add ax,bx
    sub ax,[bp+V_ORGX]
    mov cx,ax
    and cx,7
    mov [bp+V_SHIFT],cx
    sar ax,3
    mov word ptr [bp+V_BPLANE],0
    mov bx,ax
    or ax,ax
    js TBRCheckSecond
    shl bx,3
    cmp bx,[bp+U_WIDTH]
    jae TBRCheckSecond
    or word ptr [bp+V_BPLANE],1
TBRCheckSecond:
    mov bx,ax
    inc bx
    js TBRStripReady
    cmp word ptr [bp+V_SHIFT],0
    je TBRStripReady
    shl bx,3
    cmp bx,[bp+U_WIDTH]
    jae TBRStripReady
    or word ptr [bp+V_BPLANE],2
TBRStripReady:
    mul word ptr [bp+U_HEIGHT]
    add ax,[bp+U_GLYPH]
    add ax,[bp+V_REGION+2]
    sub ax,[bp+38]
    mov si,ax
    mov bx,[bp+U_HEIGHT]
    mov es,[bp+U_FONT]
    mov cx,[bp+V_SHIFT]
    ; Select the proved source-column footprint once, outside glyph rows.
    ; An aligned column needs neither a second-byte test nor a bit shift.
    cmp word ptr [bp+V_BPLANE],3
    jne @F
    jmp TBRBothRows
@@:
    cmp word ptr [bp+V_BPLANE],2
    jne @F
    jmp TBRSecondRows
@@:
    cmp word ptr [bp+V_BPLANE],1
    je @F
    jmp TBRNextColumn
@@:
    or cx,cx
    jz TBRAlignedRows
    jmp TBRFirstRows
TBRAlignedRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TBRAlignedRowsPairs
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
TBRAlignedRowsPairs:
    or dx,dx
    jz TBRAlignedRowsDone
TBRAlignedRowsLoop:
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    dec dx
    jnz TBRAlignedRowsLoop
TBRAlignedRowsDone:
    jmp TBRNextColumn
TBRFirstRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TBRFirstRowsPairs
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
TBRFirstRowsPairs:
    or dx,dx
    jz TBRFirstRowsDone
TBRFirstRowsLoop:
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    dec dx
    jnz TBRFirstRowsLoop
TBRFirstRowsDone:
    jmp TBRNextColumn
TBRSecondRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TBRSecondRowsPairs
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
TBRSecondRowsPairs:
    or dx,dx
    jz TBRSecondRowsDone
TBRSecondRowsLoop:
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    dec dx
    jnz TBRSecondRowsLoop
TBRSecondRowsDone:
    jmp TBRNextColumn
TBRBothRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TBRBothRowsPairs
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
TBRBothRowsPairs:
    or dx,dx
    jz TBRBothRowsDone
TBRBothRowsLoop:
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    or TextCover[di],ah
    inc si
    add di,80
    dec dx
    jnz TBRBothRowsLoop
TBRBothRowsDone:
    jmp TBRNextColumn
TBRNextColumn:
    inc word ptr [bp+V_BYTE]
    mov ax,[bp+V_BYTE]
    cmp ax,[bp+V_BYTES]
    jae TBRDone
    jmp TBRColumn
TBRDone:
    clc
    ret
TextBatchRectangle endp

; Stream the composed strip. A full coverage byte/word is a plain store;
; transparent or clipped coverage preserves only the uncovered destination.
; Each byte of a word is inside the preflighted region; words require even
; destination alignment. No font/string re-read is needed during emission.
TextBatchFlush proc near
    cmp word ptr [bp+V_BATCH],2
    jne @F
    jmp TextTransparentFlush
@@:
    mov ax,[bp+V_CURBOX]
    shr ax,3
    mov [bp+V_XBYTE],ax
    mov ax,[bp+V_CURBOX+4]
    dec ax
    shr ax,3
    sub ax,[bp+V_XBYTE]
    inc ax
    mov [bp+V_BYTES],ax
    mov word ptr [bp+V_BPLANE],0
TBFPlane:
    mov ax,[bp+V_CURBOX+2]
    mov [bp+V_Y],ax
    cmp word ptr [bp+S_KIND],0
    je TBFMemoryStart
    call ScreenFillOffset
    add ax,[bp+V_XBYTE]
    mov si,[bp+V_BPLANE]
    mov es,PlaneSelectors[si]
    add ax,PlaneBases[si]
    jmp TBFStartReady
TBFMemoryStart:
    mul word ptr [bp+S_ROW]
    add ax,[bp+S_DESC+10]
    mov bx,ax
    mov ax,[bp+V_BPLANE]
    shr ax,1
    mul word ptr [bp+S_STRIDE]
    add ax,bx
    add ax,[bp+V_XBYTE]
    mov es,[bp+S_DESC+12]
TBFStartReady:
    mov [bp+V_BSTART],ax
    and ax,1
    mov [bp+V_NSCAN],ax       ; one leading byte on an odd address
    mov bx,[bp+V_BYTES]
    sub bx,ax
    mov ax,bx
    and ax,1
    mov [bp+V_BPHASE],ax      ; trailing byte
    shr bx,1
    mov [bp+V_NROWS],bx       ; aligned words per row
    mov si,[bp+V_BPLANE]
    mov ax,[bp+V_XBYTE]
    mov [bp+V_SCAN],ax
    mov cx,si
    shr cx,1
    mov al,byte ptr [bp+V_INK]
    shr al,cl
    and al,1
    neg al
    mov ah,al
    mov [bp+V_RAW],ax
    mov al,byte ptr [bp+V_PAPER]
    shr al,cl
    and al,1
    neg al
    mov ah,al
    xor [bp+V_RAW],ax          ; repeated fg XOR bg
    mov [bp+V_COLOR],ax       ; repeated background
    ; The foreground XOR background is constant for this whole plane.
    ; BX is dead after row-layout preparation; edge helpers preserve it.
    mov bx,[bp+V_RAW]
TBFRow:
    mov di,[bp+V_BSTART]
    mov si,[bp+V_SCAN]
    cmp word ptr [bp+V_NSCAN],0
    je TBFWords
    call TBFOne
    inc si
    inc di
TBFWords:
    mov cx,[bp+V_NROWS]
    jcxz TBFTail
TBFWord:
    mov dx,word ptr TextCover[si]
    cmp dx,0ffffh
    je TBFFullWord
    or dx,dx
    jz TBFWordAdvance
    ; Each accessed destination byte must have coverage, even for words.
    or dl,dl
    jz TBFRightOnly
    or dh,dh
    jz TBFLeftOnly
    mov ax,word ptr TextInk[si]
    and ax,bx
    xor ax,[bp+V_COLOR]
    and ax,dx
    not dx
    or dx,dx
    jz TBFWordStore
    and dx,es:[di]
    or ax,dx
TBFWordStore:
    mov es:[di],ax
TBFWordAdvance:
    add si,2
    add di,2
    loop TBFWord
TBFTail:
    cmp word ptr [bp+V_BPHASE],0
    je TBFNextRow
    call TBFOne
    jmp TBFNextRow
TBFLeftOnly:
    mov al,TextInk[si]
    and al,bl
    xor al,byte ptr [bp+V_COLOR]
    and al,dl
    not dl
    or dl,dl
    jz TBFLeftStore
    and dl,es:[di]
    or al,dl
TBFLeftStore:
    mov es:[di],al
    jmp TBFWordAdvance
TBFRightOnly:
    mov al,TextInk[si+1]
    and al,bl
    xor al,byte ptr [bp+V_COLOR]
    and al,dh
    not dh
    or dh,dh
    jz TBFRightStore
    and dh,es:[di+1]
    or al,dh
TBFRightStore:
    mov es:[di+1],al
    jmp TBFWordAdvance
TBFFullWord:
    ; Both bytes are wholly owned: no edge tests or destination read.
    ; Layout above has already established even destination alignment.
    mov ax,word ptr TextInk[si]
    and ax,bx
    xor ax,[bp+V_COLOR]
    mov es:[di],ax
    jmp TBFWordAdvance
TBFNextRow:
    cmp word ptr [bp+S_KIND],0
    je TBFMemoryStep
    mov ax,[bp+V_Y]
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne TBFStep
    mov dx,0a050h
    jmp TBFStep
TBFMemoryStep:
    mov dx,[bp+S_ROW]
TBFStep:
    add [bp+V_BSTART],dx
    add word ptr [bp+V_SCAN],80
    inc word ptr [bp+V_Y]
    mov ax,[bp+V_Y]
    cmp ax,[bp+V_CURBOX+6]
    jae @F
    jmp TBFRow
@@:
    add word ptr [bp+V_BPLANE],2
    mov ax,[bp+S_PLANES]
    shl ax,1
    cmp [bp+V_BPLANE],ax
    jae @F
    jmp TBFPlane
@@:
TBFEnd:
    ret
TextBatchFlush endp

; Single covered byte for a row's leading/trailing edge. Preserve CX.
TBFOne proc near
    mov dl,TextCover[si]
    or dl,dl
    jz TBFOEnd
    mov al,TextInk[si]
    and al,bl
    xor al,byte ptr [bp+V_COLOR]
    and al,dl
    not dl
    or dl,dl
    jz TBFOStore
    and dl,es:[di]
    or al,dl
TBFOStore:
    mov es:[di],al
TBFOEnd:
    ret
TBFOne endp

; 0.16 ink-only compositor. Admission is fixed before background/ink passes.
TextInkRectangle proc near
    mov ax,[bp+V_REGION+2]
    sub ax,[bp+V_CURBOX+2]
    mov bx,80
    mul bx
    mov bx,[bp+V_REGION]
    shr bx,3
    add ax,bx
    mov [bp+V_BSTART],ax
    mov word ptr [bp+V_BYTE],0
TIRColumn:
    mov al,0ffh
    cmp word ptr [bp+V_BYTE],0
    jne TIRLastMask
    and al,byte ptr [bp+V_FIRST]
TIRLastMask:
    mov bx,[bp+V_BYTE]
    inc bx
    cmp bx,[bp+V_BYTES]
    jne TIRMaskReady
    and al,byte ptr [bp+V_LAST]
TIRMaskReady:
    mov byte ptr [bp+V_MASK],al
    mov ax,[bp+V_REGION+6]
    sub ax,[bp+V_REGION+2]
    mov [bp+V_BROWS],ax
    mov di,[bp+V_BSTART]
    add di,[bp+V_BYTE]
TIRGlyph:
    mov ax,[bp+V_REGION]
    and ax,0fff8h
    mov bx,[bp+V_BYTE]
    shl bx,3
    add ax,bx
    sub ax,[bp+V_ORGX]
    mov cx,ax
    and cx,7
    mov [bp+V_SHIFT],cx
    sar ax,3
    mov word ptr [bp+V_BPLANE],0
    mov bx,ax
    or ax,ax
    js TIRCheckSecond
    shl bx,3
    cmp bx,[bp+U_WIDTH]
    jae TIRCheckSecond
    or word ptr [bp+V_BPLANE],1
TIRCheckSecond:
    mov bx,ax
    inc bx
    js TIRStripReady
    cmp word ptr [bp+V_SHIFT],0
    je TIRStripReady
    shl bx,3
    cmp bx,[bp+U_WIDTH]
    jae TIRStripReady
    or word ptr [bp+V_BPLANE],2
TIRStripReady:
    mul word ptr [bp+U_HEIGHT]
    add ax,[bp+U_GLYPH]
    add ax,[bp+V_REGION+2]
    sub ax,[bp+38]
    mov si,ax
    mov bx,[bp+U_HEIGHT]
    mov es,[bp+U_FONT]
    mov cx,[bp+V_SHIFT]
    ; Select the proved source-column footprint once, outside glyph rows.
    ; An aligned column needs neither a second-byte test nor a bit shift.
    cmp word ptr [bp+V_BPLANE],3
    jne @F
    jmp TIRBothRows
@@:
    cmp word ptr [bp+V_BPLANE],2
    jne @F
    jmp TIRSecondRows
@@:
    cmp word ptr [bp+V_BPLANE],1
    je @F
    jmp TIRNextColumn
@@:
    or cx,cx
    jz TIRAlignedRows
    jmp TIRFirstRows
TIRAlignedRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TIRAlignedRowsPairs
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
TIRAlignedRowsPairs:
    or dx,dx
    jz TIRAlignedRowsDone
TIRAlignedRowsLoop:
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    dec dx
    jnz TIRAlignedRowsLoop
TIRAlignedRowsDone:
    jmp TIRNextColumn
TIRFirstRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TIRFirstRowsPairs
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
TIRFirstRowsPairs:
    or dx,dx
    jz TIRFirstRowsDone
TIRFirstRowsLoop:
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    xor al,al
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    dec dx
    jnz TIRFirstRowsLoop
TIRFirstRowsDone:
    jmp TIRNextColumn
TIRSecondRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TIRSecondRowsPairs
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
TIRSecondRowsPairs:
    or dx,dx
    jz TIRSecondRowsDone
TIRSecondRowsLoop:
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    xor ah,ah
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    dec dx
    jnz TIRSecondRowsLoop
TIRSecondRowsDone:
    jmp TIRNextColumn
TIRBothRows:
    mov dx,[bp+V_BROWS]
    shr dx,1
    jnc TIRBothRowsPairs
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
TIRBothRowsPairs:
    or dx,dx
    jz TIRBothRowsDone
TIRBothRowsLoop:
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    mov ah,es:[si]
    mov al,es:[si+bx]
    shl ax,cl
    and ah,byte ptr [bp+V_MASK]
    or TextInk[di],ah
    inc si
    add di,80
    dec dx
    jnz TIRBothRowsLoop
TIRBothRowsDone:
    jmp TIRNextColumn
TIRNextColumn:
    inc word ptr [bp+V_BYTE]
    mov ax,[bp+V_BYTE]
    cmp ax,[bp+V_BYTES]
    jae TIRDone
    jmp TIRColumn
TIRDone:
    clc
    ret
TextInkRectangle endp

TextTransparentFlush proc near
    mov si,[bp+V_CURBOX]
    shr si,3
    mov bx,[bp+V_CURBOX+4]
    dec bx
    shr bx,3
    sub bx,si
    inc bx
    mov dx,[bp+V_CURBOX+6]
    sub dx,[bp+V_CURBOX+2]
TTFEmptyRow:
    mov di,si
    mov cx,bx
    shr cx,1
    jcxz TTFEmptyTail
TTFEmptyWords:
    cmp word ptr TextInk[di],0
    jne TTFHasInk
    add di,2
    loop TTFEmptyWords
TTFEmptyTail:
    test bl,1
    jz TTFEmptyNext
    cmp byte ptr TextInk[di],0
    jne TTFHasInk
TTFEmptyNext:
    add si,80
    dec dx
    jnz TTFEmptyRow
    ret
TTFHasInk:
    mov ax,[bp+V_CURBOX]
    shr ax,3
    mov [bp+V_XBYTE],ax
    mov ax,[bp+V_CURBOX+4]
    dec ax
    shr ax,3
    sub ax,[bp+V_XBYTE]
    inc ax
    mov [bp+V_BYTES],ax
    mov word ptr [bp+V_BPLANE],0
TTFPlane:
    mov ax,[bp+V_CURBOX+2]
    mov [bp+V_Y],ax
    cmp word ptr [bp+S_KIND],0
    je TTFMemoryStart
    call ScreenFillOffset
    add ax,[bp+V_XBYTE]
    mov si,[bp+V_BPLANE]
    mov es,PlaneSelectors[si]
    add ax,PlaneBases[si]
    jmp TTFStartReady
TTFMemoryStart:
    mul word ptr [bp+S_ROW]
    add ax,[bp+S_DESC+10]
    mov bx,ax
    mov ax,[bp+V_BPLANE]
    shr ax,1
    mul word ptr [bp+S_STRIDE]
    add ax,bx
    add ax,[bp+V_XBYTE]
    mov es,[bp+S_DESC+12]
TTFStartReady:
    mov [bp+V_BSTART],ax
    and ax,1
    mov [bp+V_NSCAN],ax       ; one leading byte on an odd address
    mov bx,[bp+V_BYTES]
    sub bx,ax
    mov ax,bx
    and ax,1
    mov [bp+V_BPHASE],ax      ; trailing byte
    shr bx,1
    mov [bp+V_NROWS],bx       ; aligned words per row
    mov si,[bp+V_BPLANE]
    mov ax,[bp+V_XBYTE]
    mov [bp+V_SCAN],ax
    mov cx,si
    shr cx,1
    mov al,byte ptr [bp+V_INK]
    shr al,cl
    and al,1
    ; A physical plane's ink is always either all clear or all set.
    ; Choose once per plane; zero/partial/full masks keep their old footprints.
    or al,al
    jz TTFClRow
    jmp TTFSetRow
TTFClRow:
    mov di,[bp+V_BSTART]
    mov si,[bp+V_SCAN]
    cmp word ptr [bp+V_NSCAN],0
    je TTFClWords
    call TTFClOne
    inc si
    inc di
TTFClWords:
    mov cx,[bp+V_NROWS]
    jcxz TTFClTail
TTFClWord:
    mov dx,word ptr TextInk[si]
    or dx,dx
    jz TTFClAdvance
    ; Preserve exact byte coverage, word alignment and destination traffic.
    or dl,dl
    jz TTFClRightOnly
    or dh,dh
    jz TTFClLeftOnly
    cmp dx,0ffffh
    je TTFClFullWord
    not dx
    and word ptr es:[di],dx
    jmp TTFClAdvance
TTFClFullWord:
    mov word ptr es:[di],0
TTFClAdvance:
    add si,2
    add di,2
    loop TTFClWord
TTFClTail:
    cmp word ptr [bp+V_BPHASE],0
    je TTFClNextRow
    call TTFClOne
    jmp TTFClNextRow
TTFClLeftOnly:
    cmp dl,0ffh
    je TTFClLeftFull
    not dl
    and byte ptr es:[di],dl
    jmp TTFClAdvance
TTFClLeftFull:
    mov byte ptr es:[di],0
    jmp TTFClAdvance
TTFClRightOnly:
    cmp dh,0ffh
    je TTFClRightFull
    not dh
    and byte ptr es:[di+1],dh
    jmp TTFClAdvance
TTFClRightFull:
    mov byte ptr es:[di+1],0
    jmp TTFClAdvance
TTFClNextRow:
    cmp word ptr [bp+S_KIND],0
    je TTFClMemoryStep
    mov ax,[bp+V_Y]
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne TTFClStep
    mov dx,0a050h
    jmp TTFClStep
TTFClMemoryStep:
    mov dx,[bp+S_ROW]
TTFClStep:
    add [bp+V_BSTART],dx
    add word ptr [bp+V_SCAN],80
    inc word ptr [bp+V_Y]
    mov ax,[bp+V_Y]
    cmp ax,[bp+V_CURBOX+6]
    jae @F
    jmp TTFClRow
@@:
    jmp TTFNextPlane
TTFSetRow:
    mov di,[bp+V_BSTART]
    mov si,[bp+V_SCAN]
    cmp word ptr [bp+V_NSCAN],0
    je TTFSetWords
    call TTFSetOne
    inc si
    inc di
TTFSetWords:
    mov cx,[bp+V_NROWS]
    jcxz TTFSetTail
TTFSetWord:
    mov dx,word ptr TextInk[si]
    or dx,dx
    jz TTFSetAdvance
    ; Preserve exact byte coverage, word alignment and destination traffic.
    or dl,dl
    jz TTFSetRightOnly
    or dh,dh
    jz TTFSetLeftOnly
    cmp dx,0ffffh
    je TTFSetFullWord
    or word ptr es:[di],dx
    jmp TTFSetAdvance
TTFSetFullWord:
    mov word ptr es:[di],0ffffh
TTFSetAdvance:
    add si,2
    add di,2
    loop TTFSetWord
TTFSetTail:
    cmp word ptr [bp+V_BPHASE],0
    je TTFSetNextRow
    call TTFSetOne
    jmp TTFSetNextRow
TTFSetLeftOnly:
    cmp dl,0ffh
    je TTFSetLeftFull
    or byte ptr es:[di],dl
    jmp TTFSetAdvance
TTFSetLeftFull:
    mov byte ptr es:[di],0ffh
    jmp TTFSetAdvance
TTFSetRightOnly:
    cmp dh,0ffh
    je TTFSetRightFull
    or byte ptr es:[di+1],dh
    jmp TTFSetAdvance
TTFSetRightFull:
    mov byte ptr es:[di+1],0ffh
    jmp TTFSetAdvance
TTFSetNextRow:
    cmp word ptr [bp+S_KIND],0
    je TTFSetMemoryStep
    mov ax,[bp+V_Y]
    and ax,3
    mov dx,2000h
    cmp ax,3
    jne TTFSetStep
    mov dx,0a050h
    jmp TTFSetStep
TTFSetMemoryStep:
    mov dx,[bp+S_ROW]
TTFSetStep:
    add [bp+V_BSTART],dx
    add word ptr [bp+V_SCAN],80
    inc word ptr [bp+V_Y]
    mov ax,[bp+V_Y]
    cmp ax,[bp+V_CURBOX+6]
    jae @F
    jmp TTFSetRow
@@:
    jmp TTFNextPlane
TTFNextPlane:
    add word ptr [bp+V_BPLANE],2
    mov ax,[bp+S_PLANES]
    shl ax,1
    cmp [bp+V_BPLANE],ax
    jae @F
    jmp TTFPlane
@@:
TTFEnd:
    ret
TextTransparentFlush endp

; Covered leading/trailing byte. Preserve CX and BX.
TTFClOne proc near
    mov dl,TextInk[si]
    or dl,dl
    jz TTFClOneEnd
    cmp dl,0ffh
    je TTFClOneFull
    not dl
    and byte ptr es:[di],dl
    ret
TTFClOneFull:
    mov byte ptr es:[di],0
TTFClOneEnd:
    ret
TTFClOne endp
; Covered leading/trailing byte. Preserve CX and BX.
TTFSetOne proc near
    mov dl,TextInk[si]
    or dl,dl
    jz TTFSetOneEnd
    cmp dl,0ffh
    je TTFSetOneFull
    or byte ptr es:[di],dl
    ret
TTFSetOneFull:
    mov byte ptr es:[di],0ffh
TTFSetOneEnd:
    ret
TTFSetOne endp

