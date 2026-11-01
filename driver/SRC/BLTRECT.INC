; Phase 9 public Windows BitBlt entry; BlitRectangle is its diagnostic alias.
; Original DDK genlocal.blt's 32-byte Pascal BitBlt ABI.
; AX=1 completed/empty, 0 invalid/unavailable, FFFF unsupported mapping.
; Windows documents no return value. Diagnostic AX is NOT a retry contract.
; Supported callers: normal GDI surfaces and documented monochrome brute views.
; Physical desktop activation remains gated until cursor/text integration.
; Complete metadata/mapping preflight precedes all pixel reads and writes.
; Borrowed descriptors/backing selectors must remain stable for this call.
; No allocation, yielding, resident calls, port I/O or dynamic executable code.
; General injective mono aliases use the fixed-space ALIAS.INC scheduler.
X_DDESC equ -284
X_SDESC equ -320
X_DK equ -322
X_DP equ -324
X_SK equ -326
X_SP equ -328
X_DX equ -330
X_DY equ -332
X_SX equ -334
X_SY equ -336
X_W equ -338
X_H equ -340
X_ROP equ -342
X_USE equ -344
X_YDIR equ -346
X_XDIR equ -348
X_MODE equ -350
X_PASS equ -352
X_ROW equ -354
X_LEFT equ -356
X_POS equ -358
X_COUNT equ -360
X_PHASE equ -362
X_MASK equ -364
X_DPTR equ -380                 ; 4 validated row-origin far pointers
X_SPTR equ -396
X_SOURCE equ -400               ; ALL source planes staged before any write
X_DNORM equ -406
X_SNORM equ -410
X_DMIN equ -414
X_DMAX equ -418
X_SMIN equ -422
X_SMAX equ -426
X_PREV equ -430
X_VIEW equ -432
X_MIN equ -436
X_MAX equ -440
X_START equ -444
X_END equ -448
X_SPHASE equ -450
X_BYTE equ -452
A_DORDER equ -454
A_SORDER equ -456
A_MVIEW equ -458
A_UVIEW equ -460
A_SCAN equ -462
A_GROUP equ -464
A_IVIEW equ -466
A_QBYTE equ -470
A_QBIT equ -472
A_FOUND equ -474
A_RET equ -478
A_INDEX equ -482
A_N equ -486
A_CAND equ -490
A_NEXT equ -494
A_STEPS equ -498
A_CYCLE equ -500
A_OVERRIDE equ -502
A_SAVED equ -504
A_STATUS equ -506
A_UINDEX equ -510
A_UN equ -514
A_OUTNEXT equ -516
A_INROW equ -518
A_ENEXT equ -520
A_EFIRST equ -524
A_ELAST equ -528
A_OFIRST equ -532
A_OLAST equ -536
A_OUTROW equ -538
C_COUNT equ -540
C_CACHE equ -588                 ; eight {selector, base low, base high} entries
C_KEY equ -590
P_PATTERN equ -548             ; 0.13 per-plane constant brush bits, alias locals dead
F_RUN equ -592
F_SRC equ -594
F_DST equ -596
X_TRACE equ -598
X_DFMT equ -600
X_SFMT equ -602
X_CURBOX equ -610
; Screen-only source-free fill scratch; within the existing frame.
F_READY equ -612
F_PHASE equ -614
F_START equ -616
F_REMAIN equ -618
F_ROWS equ -620
F_PLANE equ -622
F_PTR equ -624
F_XBYTE equ -626
F_MIN equ -628
F_MAX equ -630
F_SCAN equ -632
F_ENDY equ -634
F_ROPCONST equ -636             ; invocation-local source-free ROP coefficients
F_ROPMASK equ -638
X_FAST equ -640                 ; prepared ordinary/screen row iterator
BC_HELD equ -642
BC_ENTRY equ -644
BC_BYTES equ -646
BC_PHASE equ -648
X_LOCAL equ 648
X_RESERVE equ 928                ; includes helper + interrupt headroom;
                                ; actual Windows kernel stack remains a gate
public BitBlt, BlitRectangle, BltMapRow, BltPreflight, BltSchedule, BltTile

BitBlt label far
BlitRectangle proc far
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
    mov ax,X_RESERVE
    call StackRoom
    jnc @F
    jmp BRReturnBad
@@:
    sub sp,X_LOCAL
    mov word ptr [bp+BC_HELD],0
    mov word ptr [bp+X_TRACE],2
    mov word ptr [bp+X_DFMT],0ffffh
    mov word ptr [bp+X_SFMT],0ffffh
    mov word ptr [bp+S_HELD],0
    mov word ptr [bp+C_COUNT],0
    mov word ptr [bp+F_READY],0
    mov ax,[bp+14]
    mov dx,[bp+16]
    call BlitDecode
    jnc @F
    jmp BRBad
@@:
    mov [bp+X_ROP],ax
    mov [bp+X_USE],bx
    mov cx,[bp+20]
    mov dx,[bp+18]
    or cx,cx
    jns @F
    jmp BRBad
@@:
    or dx,dx
    jns @F
    jmp BRBad
@@:
    or cx,cx
    jnz @F
    jmp BROK
@@:
    or dx,dx
    jnz @F
    jmp BROK
@@:
    cmp ax,0aah
    jne @F
    jmp BROK
@@:
    mov [bp+X_W],cx
    mov [bp+X_H],dx
    mov word ptr [bp+X_SK],0
    mov word ptr [bp+X_SP],0
    mov word ptr [bp+X_MODE],0
    mov word ptr [bp+X_PASS],0
    mov word ptr [bp+X_YDIR],1
    mov word ptr [bp+X_XDIR],1
    mov word ptr [bp+X_TRACE],3
    mov ax,[bp+36]
    mov bx,[bp+34]
    test word ptr [bp+X_USE],2
    jnz BRSnapshotDest
    ; Source-free screen fills never consume a descriptor snapshot. Prove
    ; the complete private identity directly; ordinary bitmaps reuse the
    ; original snapshot after this one two-byte tag proof, not a second one.
    mov cx,2
    call CheckReadSpan
    jnc @F
    jmp BRBad
@@:
    mov es,ax
    cmp word ptr es:[bx],4745h
    jne @F
    jmp BRDirectScreen
@@:
    call SnapshotTyped
    jmp BRDestSnapshotDone
BRSnapshotDest:
    call SnapshotSurface
BRDestSnapshotDone:
    jnc @F
    jmp BRBad
@@:
    call ClassifyFormat
    jnc @F
    jmp BRBad
@@:
    mov ax,[bp+S_DESC+8]
    mov [bp+X_DFMT],ax
    mov ax,[bp+S_KIND]
    mov [bp+X_DK],ax
    mov ax,[bp+S_PLANES]
    mov [bp+X_DP],ax
    jmp BRSaveDest
BRDirectScreen:
    mov cx,36
    call CheckReadSpan
    jnc @F
    jmp BRBad
@@:
    mov si,DataOFFSET PhysicalDevice
    mov di,bx
    mov cx,18
    cld
    repe cmpsw
    je @F
    jmp BRBad
@@:
    mov ax,es:[bx+8]
    cmp ax,0101h
    je BRDirectFormat
    cmp ax,0104h
    je BRDirectFormat
    jmp BRBad
BRDirectFormat:
    mov [bp+X_DFMT],ax
    xor ah,ah
    mov [bp+X_DP],ax
    mov word ptr [bp+X_DK],1
    mov ax,es:[bx+2]
    mov [bp+X_DDESC+2],ax
    mov dx,es:[bx+4]
    mov [bp+X_DDESC+4],dx
    ; Strict positive descriptor limits and positive request extents are
    ; required even on this cheap proof. Unsigned compares reject negative
    ; coordinates. Subtraction avoids a signed/unsigned endpoint wrap.
    or ax,ax
    jle BRDirectClipFallback
    or dx,dx
    jle BRDirectClipFallback
    mov cx,[bp+32]
    cmp cx,ax
    jae BRDirectClipFallback
    sub ax,cx
    cmp [bp+X_W],ax
    ja BRDirectClipFallback
    mov bx,[bp+30]
    cmp bx,dx
    jae BRDirectClipFallback
    sub dx,bx
    cmp [bp+X_H],dx
    ja BRDirectClipFallback
    mov [bp+X_DX],cx
    mov [bp+X_DY],bx
    jmp BRClipped
BRDirectClipFallback:
    jmp BRClip
BRSaveDest:
    lea si,[bp+S_DESC]
    lea di,[bp+X_DDESC]
    call BltCopyDesc
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BRClip
@@:
    mov word ptr [bp+X_TRACE],4
    mov ax,[bp+28]
    mov bx,[bp+26]
    call SnapshotSurface
    jnc @F
    jmp BRBad
@@:
    call ClassifyFormat
    jnc @F
    jmp BRBad
@@:
    mov ax,[bp+S_DESC+8]
    mov [bp+X_SFMT],ax
    mov ax,[bp+S_KIND]
    mov [bp+X_SK],ax
    mov ax,[bp+S_PLANES]
    mov [bp+X_SP],ax
    lea si,[bp+S_DESC]
    lea di,[bp+X_SDESC]
    call BltCopyDesc
BRClip:
    mov word ptr [bp+X_TRACE],5
    xor di,di
    xor si,si
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BRClipX
@@:
    inc di
    mov si,[bp+X_SDESC+2]
BRClipX:
    mov ax,[bp+32]
    mov bx,[bp+24]
    mov cx,[bp+X_W]
    mov dx,[bp+X_DDESC+2]
    call BlitClipAxis
    jnc @F
    jmp BRBad
@@:
    or cx,cx
    jnz @F
    jmp BROK
@@:
    mov [bp+X_DX],ax
    mov [bp+X_SX],bx
    mov [bp+X_W],cx
    xor si,si
    or di,di
    jnz @F
    jmp BRClipY
@@:
    mov si,[bp+X_SDESC+4]
BRClipY:
    mov ax,[bp+30]
    mov bx,[bp+22]
    mov cx,[bp+X_H]
    mov dx,[bp+X_DDESC+4]
    call BlitClipAxis
    jnc @F
    jmp BRBad
@@:
    or cx,cx
    jnz @F
    jmp BROK
@@:
    mov [bp+X_DY],ax
    mov [bp+X_SY],bx
    mov [bp+X_H],cx
BRClipped:
    mov word ptr [bp+X_TRACE],6
    ; P and drawing colours are borrowed only when they participate.
    test word ptr [bp+X_USE],4
    jnz @F
    jmp BRColours
@@:
    mov ax,[bp+12]
    mov bx,[bp+10]
    mov cx,56
    lea di,[bp+T_BRUSH]
    call OutputSnapshot
    jnc @F
    jmp BRBad
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jbe @F
    jmp BRBad
@@:
    cmp word ptr [bp+T_BRUSH+48],1
    jne @F
    jmp BRBad
@@:
BRColours:
    mov word ptr [bp+X_TRACE],6
    mov word ptr [bp+T_DM+6],0
    mov word ptr [bp+T_DM+10],0
    test word ptr [bp+X_USE],4
    jnz @F
    jmp BRSourceColours
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jne @F
    jmp BRReadColours
@@:
BRSourceColours:
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BRMapping
@@:
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    jne @F
    jmp BRMapping
@@:
BRReadColours:
    mov word ptr [bp+X_TRACE],6
    mov ax,[bp+8]
    mov bx,[bp+6]
    add bx,4                     ; skip unused ROP2 and background mode
    jnc @F
    jmp BRBad
@@:
    mov cx,8
    lea di,[bp+T_DM+4]
    call OutputSnapshot
    jnc @F
    jmp BRBad
@@:
BRMapping:
    mov word ptr [bp+X_TRACE],7
    cmp word ptr [bp+X_DK],0
    je BRGenericMapping
    test word ptr [bp+X_USE],2
    jnz BRGenericMapping
    call ScreenFillPreflight
    jnc @F
    jmp BRBad
@@:
    mov word ptr [bp+F_READY],1
    jmp BRClaim
BRGenericMapping:
    xor ax,ax
    call BltPreflight
    jnc @F
    jmp BRRelease
@@:
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BRClaim
@@:
    mov word ptr [bp+X_TRACE],8
    mov ax,1
    call BltPreflight
    jnc @F
    jmp BRRelease
@@:
    mov word ptr [bp+X_TRACE],9
    call BltSchedule
    jnc @F
    jmp BRRelease
@@:
BRClaim:
    mov word ptr [bp+X_TRACE],10
    mov ax,[bp+X_DK]
    or ax,[bp+X_SK]
    jnz @F
    jmp BRDraw
@@:
    call BltEnterDrawing
    jnc @F
    jmp BRBad
@@:
    mov word ptr [bp+S_HELD],1
BRDraw:
    cmp TraceEnabled,0
    je BRSourceCounted
    call BltSourceCounters
BRSourceCounted:
    mov word ptr [bp+X_TRACE],11
    cmp word ptr [bp+F_READY],0
    jne @F
    jmp BRGenericDraw
@@:
    cmp TraceEnabled,0
    jne @F
    jmp BRPerfDraw
@@:
    PERFINC PerfFill
    call BltFillHistogram
    mov ax,[bp+X_DX]
    or ax,[bp+X_W]
    test ax,7
    jnz BRPerfEdge
    PERFINC PerfAligned
    jmp BRPerfSize
BRPerfEdge:
    PERFINC PerfEdge
BRPerfSize:
    cmp word ptr [bp+X_W],128
    jb BRPerfShort
    cmp word ptr [bp+X_H],64
    jb BRPerfShort
    PERFINC PerfLarge
BRPerfShort:
    cmp word ptr [bp+X_H],8
    jae BRPerfROP
    PERFINC PerfShort
BRPerfROP:
    cmp word ptr [bp+X_ROP],0
    jne BRPerfWhite
    PERFINC PerfBlack
BRPerfWhite:
    cmp word ptr [bp+X_ROP],0ffh
    jne BRPerfPattern
    PERFINC PerfWhite
BRPerfPattern:
    test word ptr [bp+X_USE],4
    jz BRPerfDraw
    PERFINC PerfPattern
BRPerfDraw:
    call ScreenFillDraw
    jmp BROK
BRGenericDraw:
    call BitCacheTry
    cmp word ptr [bp+X_DK],0
    je BRPerfCopyDone
    cmp word ptr [bp+X_ROP],0cch
    jne BRPerfCopyDone
    PERFINC PerfCopy
BRPerfCopyDone:
    cmp word ptr [bp+X_MODE],2
    jne @F
    call AliasGeneral
    jnc BROKGeneral
    jmp BRBad
BROKGeneral:
    jmp BROK
@@:
    call BltRows
    jnc @F
    jmp BRBad
@@:
    cmp word ptr [bp+X_MODE],1
    je @F
    jmp BROK
@@:
    mov word ptr [bp+X_PASS],1
    mov word ptr [bp+X_XDIR],-1
    mov word ptr [bp+X_YDIR],-1
    call BltRows
    jnc @F
    jmp BRBad
@@:
BROK:
    mov word ptr [bp+X_TRACE],0
    mov ax,1
    jmp BRRelease
BRBad:
    xor ax,ax
BRRelease:
    cmp word ptr [bp+BC_HELD],0
    je BRCacheReleased
    mov BitCacheBusy,0
    mov word ptr [bp+BC_HELD],0
BRCacheReleased:
    DRAWREPORT DrawBlt, [bp+X_TRACE], [bp+X_DFMT], [bp+X_SFMT], 16
    cmp word ptr [bp+S_HELD],0
    jne @F
    jmp BRReturn
@@:
    call LeaveDrawing
    jmp BRReturn
BRReturnBad:
    xor ax,ax
    DRAWREPORT DrawBlt, 1, 0ffffh, 0ffffh, 16
BRReturn:
    cld
    lea sp,[bp-6]
    pop di
    pop si
    pop ds
    pop bp
    dec bp
    retf 32
BlitRectangle endp

; SS:SI -> SS:DI, 36 bytes; DS remains driver data.
BltCopyDesc proc near
    ; 0.10: fixed-size private copy, exact 36-byte coverage.
    mov ax,ss:[si+0]
    mov ss:[di+0],ax
    mov ax,ss:[si+2]
    mov ss:[di+2],ax
    mov ax,ss:[si+4]
    mov ss:[di+4],ax
    mov ax,ss:[si+6]
    mov ss:[di+6],ax
    mov ax,ss:[si+8]
    mov ss:[di+8],ax
    mov ax,ss:[si+10]
    mov ss:[di+10],ax
    mov ax,ss:[si+12]
    mov ss:[di+12],ax
    mov ax,ss:[si+14]
    mov ss:[di+14],ax
    mov ax,ss:[si+16]
    mov ss:[di+16],ax
    mov ax,ss:[si+18]
    mov ss:[di+18],ax
    mov ax,ss:[si+20]
    mov ss:[di+20],ax
    mov ax,ss:[si+22]
    mov ss:[di+22],ax
    mov ax,ss:[si+24]
    mov ss:[di+24],ax
    mov ax,ss:[si+26]
    mov ss:[di+26],ax
    mov ax,ss:[si+28]
    mov ss:[di+28],ax
    mov ax,ss:[si+30]
    mov ss:[di+30],ax
    mov ax,ss:[si+32]
    mov ss:[di+32],ax
    mov ax,ss:[si+34]
    mov ss:[di+34],ax
    add si,36
    add di,36
    xor cx,cx
    ret
BltCopyDesc endp

; AX view (0 destination, 1 source), X_ROW relative to clipped origins.
; Return validated row pointers, physical first-plane row origin and complete
; row envelope. Memory rows include padding; screen checks all 80 bytes/plane.
; No bitmap bytes are fetched. Carry on invalid mapping, AX=0.
BltMapRow proc near
    mov [bp+X_VIEW],ax
    or ax,ax
    jnz BMSource
    mov ax,[bp+X_DK]
    mov [bp+S_KIND],ax
    mov ax,[bp+X_DP]
    mov [bp+S_PLANES],ax
    mov ax,[bp+X_DY]
    mov word ptr [bp+S_ROP],1
    lea si,[bp+X_DDESC]
    jmp BMPrepare
BMSource:
    mov ax,[bp+X_SK]
    mov [bp+S_KIND],ax
    mov ax,[bp+X_SP]
    mov [bp+S_PLANES],ax
    mov ax,[bp+X_SY]
    mov word ptr [bp+S_ROP],0
    lea si,[bp+X_SDESC]
BMPrepare:
    add ax,[bp+X_ROW]
    mov [bp+S_Y],ax
    mov word ptr [bp+S_X],0
    cmp word ptr [bp+S_KIND],0
    jne BMScreen
    ; S_DESC/stride/huge fields are needed only by the memory/alias path.
    lea di,[bp+S_DESC]
    call BltCopyDesc
    call PreparePoint
    jnc @F
    jmp BMBad
@@:
    mov ax,[bp+S_SELECTOR]
    mov bx,[bp+S_OFFSET]
    mov cx,[bp+S_ROW]
    mov di,[bp+S_ROP]
    call BltNormalizeRow
    jnc @F
    jmp BMBad
@@:
    mov [bp+X_START],ax
    mov [bp+X_START+2],dx
    dec cx
    add ax,cx
    adc dx,0
    jmp BMEnd
BMScreen:
    mov cx,80
    call PrepareScreenSpan
    jnc @F
    jmp BMBad
@@:
    mov ax,[bp+S_ADDR]
    mov [bp+X_START],ax
    mov word ptr [bp+X_START+2],000ah
    mov ax,[bp+S_ADDR+12]
    add ax,8079h
    mov dx,000bh
BMEnd:
    mov [bp+X_END],ax
    mov [bp+X_END+2],dx
    lea di,[bp+X_DPTR]
    mov bx,X_DNORM
    cmp word ptr [bp+X_VIEW],0
    jne @F
    jmp BMSave
@@:
    lea di,[bp+X_SPTR]
    mov bx,X_SNORM
BMSave:
    mov si,bx
    mov ax,[bp+X_START]
    mov [bp+si],ax
    mov ax,[bp+X_START+2]
    mov [bp+si+2],ax
    lea si,[bp+S_ADDR]
    mov cx,8
BMSaveWord:
    mov ax,ss:[si]
    mov ss:[di],ax
    add si,2
    add di,2
    loop BMSaveWord
    clc
    ret
BMBad:
    xor ax,ax
    stc
    ret
BltMapRow endp

; Validate ALL rows before pixel access. Record increasing physical order.
; AliasUnique then proves uniqueness, using exact mono inverse checks when
; overlapping segment envelopes prevent the cheaper structural proof.
BltPreflight proc near
    ; Ordinary views prove one complete envelope; huge/irregular views keep
    ; the original per-row and alias-uniqueness proof on any declined case.
    push ax
    call BltRangePreflight
    pop ax
    jc @F
    ret
@@:
    mov [bp+X_VIEW],ax
    mov si,A_DORDER
    or ax,ax
    jz @F
    mov si,A_SORDER
@@:
    mov word ptr [bp+si],1
    mov word ptr [bp+X_ROW],0
BPRow:
    mov ax,[bp+X_VIEW]
    call BltMapRow
    jnc @F
    jmp BPReturn
@@:
    cmp word ptr [bp+X_ROW],0
    jne @F
    jmp BPFirst
@@:
    cmp word ptr [bp+S_KIND],0
    je @F
    jmp BPExtend
@@:
    mov dx,[bp+X_START+2]
    cmp dx,[bp+X_PREV+2]
    jbe @F
    jmp BPExtend
@@:
    jae @F
    jmp BPUnordered
@@:
    mov ax,[bp+X_START]
    cmp ax,[bp+X_PREV]
    ja @F
    jmp BPUnordered
@@:
    jmp BPExtend
BPUnordered:
    mov si,A_DORDER
    cmp word ptr [bp+X_VIEW],0
    je @F
    mov si,A_SORDER
@@:
    mov word ptr [bp+si],0
BPExtend:
    mov dx,[bp+X_START+2]
    cmp dx,[bp+X_MIN+2]
    jae @F
    jmp BPSetMin
@@:
    jbe @F
    jmp BPMax
@@:
    mov ax,[bp+X_START]
    cmp ax,[bp+X_MIN]
    jb @F
    jmp BPMax
@@:
BPSetMin:
    mov ax,[bp+X_START]
    mov [bp+X_MIN],ax
    mov [bp+X_MIN+2],dx
BPMax:
    mov dx,[bp+X_END+2]
    cmp dx,[bp+X_MAX+2]
    jbe @F
    jmp BPSetMax
@@:
    jae @F
    jmp BPNext
@@:
    mov ax,[bp+X_END]
    cmp ax,[bp+X_MAX]
    ja @F
    jmp BPNext
@@:
BPSetMax:
    mov ax,[bp+X_END]
    mov [bp+X_MAX],ax
    mov [bp+X_MAX+2],dx
    jmp BPNext
BPFirst:
    mov ax,[bp+X_START]
    mov [bp+X_MIN],ax
    mov ax,[bp+X_START+2]
    mov [bp+X_MIN+2],ax
    mov ax,[bp+X_END]
    mov [bp+X_MAX],ax
    mov ax,[bp+X_END+2]
    mov [bp+X_MAX+2],ax
BPNext:
    mov ax,[bp+X_END]
    mov [bp+X_PREV],ax
    mov ax,[bp+X_END+2]
    mov [bp+X_PREV+2],ax
    inc word ptr [bp+X_ROW]
    mov ax,[bp+X_ROW]
    cmp ax,[bp+X_H]
    jae @F
    jmp BPRow
@@:
    mov di,X_DMIN
    mov si,X_DMAX
    cmp word ptr [bp+X_VIEW],0
    jne @F
    jmp BPSave
@@:
    mov di,X_SMIN
    mov si,X_SMAX
BPSave:
    mov ax,[bp+X_MIN]
    mov [bp+di],ax
    mov ax,[bp+X_MIN+2]
    mov [bp+di+2],ax
    mov ax,[bp+X_MAX]
    mov [bp+si],ax
    mov ax,[bp+X_MAX+2]
    mov [bp+si+2],ax
    mov ax,[bp+X_VIEW]
    call AliasUnique
BPReturn:
    ret
BPUnsupported:
    mov ax,0ffffh
    stc
    ret
BltPreflight endp

; Physical envelopes first. Equal small row layouts may have different
; segment/selector identities; compare normalized bases, not selector values.
; Both-screen traversal uses logical rows, never EGC physical bank order.
; Other increasing mono aliases use the proven two-direction bit schedule.
BltSchedule proc near
    mov ax,[bp+X_DMAX+2]
    cmp ax,[bp+X_SMIN+2]
    jae @F
    jmp BSReady
@@:
    jbe @F
    jmp BSSecond
@@:
    mov ax,[bp+X_DMAX]
    cmp ax,[bp+X_SMIN]
    jae @F
    jmp BSReady
@@:
BSSecond:
    mov ax,[bp+X_SMAX+2]
    cmp ax,[bp+X_DMIN+2]
    jae @F
    jmp BSReady
@@:
    jbe @F
    jmp BSSame
@@:
    mov ax,[bp+X_SMAX]
    cmp ax,[bp+X_DMIN]
    jae @F
    jmp BSReady
@@:
BSSame:
    cmp word ptr [bp+X_DK],1
    je @F
    jmp BSSmall
@@:
    cmp word ptr [bp+X_SK],1
    jne @F
    jmp BSDirection
@@:
    jmp BSUnsupported
BSSmall:
    cmp word ptr [bp+X_SK],0
    je @F
    jmp BSUnsupported
@@:
    cmp word ptr [bp+X_DDESC+22],0
    je @F
    jmp BSHuge
@@:
    cmp word ptr [bp+X_SDESC+22],0
    je @F
    jmp BSHuge
@@:
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    je @F
    jmp BSMono
@@:
    mov ax,[bp+X_DDESC+6]
    cmp ax,[bp+X_SDESC+6]
    je @F
    jmp BSMono
@@:
    mul word ptr [bp+X_DP]
    mov cx,ax
    mul word ptr [bp+X_DY]
    mov bx,[bp+X_DMIN]
    mov si,[bp+X_DMIN+2]
    sub bx,ax
    sbb si,dx
    mov ax,cx
    mul word ptr [bp+X_SY]
    mov cx,[bp+X_SMIN]
    mov di,[bp+X_SMIN+2]
    sub cx,ax
    sbb di,dx
    cmp bx,cx
    je @F
    jmp BSMono
@@:
    cmp si,di
    je @F
    jmp BSMono
@@:
    jmp BSDirection
BSHuge:
    call AliasSameHuge
    jnc BSDirection
    jmp BSMono
BSDirection:
    mov ax,[bp+X_DY]
    cmp ax,[bp+X_SY]
    ja @F
    jmp BSX
@@:
    mov word ptr [bp+X_YDIR],-1
BSX:
    mov ax,[bp+X_DX]
    cmp ax,[bp+X_SX]
    ja @F
    jmp BSReady
@@:
    mov word ptr [bp+X_XDIR],-1
BSReady:
    clc
    ret
BSMono:
    cmp word ptr [bp+X_DP],1
    je @F
    jmp BSUnsupported
@@:
    cmp word ptr [bp+X_SP],1
    je @F
    jmp BSUnsupported
@@:
    mov word ptr [bp+X_MODE],1
    cmp word ptr [bp+A_DORDER],1
    jne BSGeneral
    cmp word ptr [bp+A_SORDER],1
    je @F
BSGeneral:
    mov word ptr [bp+X_MODE],2
@@:
    clc
    ret
BSUnsupported:
    mov ax,0ffffh
    stc
    ret
BltSchedule endp

; Pattern phase is always based on logical destination coordinates. BitBlt
; is opaque regardless of caller ROP2/background mode. CompileSpanRow's ROP2
; coefficients are unused; its row colour realization is shared with Output.
BltPattern proc near
    mov word ptr [bp+B_PAT],0
    mov byte ptr [bp+B_PAT+2],0
    test word ptr [bp+X_USE],4
    jnz @F
    jmp BPatternDone
@@:
    mov ax,[bp+X_ROW]
    add ax,[bp+X_DY]
    mov [bp+T_Y],ax
    mov word ptr [bp+T_USEBR],1
    mov word ptr [bp+T_DM+2],2
    mov word ptr [bp+S_ROP],13
    mov ax,[bp+X_DP]
    mov [bp+S_PLANES],ax
    ; Used colour source -> mono converts a colour P by background match,
    ; not Output's cached intensity tile. Mono P still recolours normally.
    cmp ax,1
    je @F
    jmp BPatternCompile
@@:
    cmp word ptr [bp+X_SP],4
    je @F
    jmp BPatternCompile
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jne @F
    jmp BPatternCompile
@@:
    mov word ptr [bp+S_PLANES],4
    call CompileSpanRow
    mov ax,[bp+B_PAT]
    mov dx,word ptr [bp+B_PAT+2]
    mov bl,byte ptr [bp+T_DM+7]
    and bl,15
    call BlitColourToMono
    mov byte ptr [bp+B_PAT],al
    jmp BPatternDone
BPatternCompile:
    call CompileSpanRow
BPatternDone:
    ret
BltPattern endp

BltRows proc near
    mov word ptr [bp+X_FAST],0
    cmp word ptr [bp+X_MODE],0
    jne BRowsIteratorReady
    cmp word ptr [bp+X_DK],0
    jne BRowsSourceIterator
    cmp word ptr [bp+X_DDESC+22],0
    jne BRowsIteratorReady
BRowsSourceIterator:
    test word ptr [bp+X_USE],2
    jz BRowsIteratorUse
    cmp word ptr [bp+X_SK],0
    jne BRowsIteratorUse
    cmp word ptr [bp+X_SDESC+22],0
    jne BRowsIteratorReady
BRowsIteratorUse:
    mov word ptr [bp+X_FAST],1
BRowsIteratorReady:
    xor ax,ax
    cmp word ptr [bp+X_YDIR],1
    jne @F
    jmp BRowsInit
@@:
    mov ax,[bp+X_H]
    dec ax
BRowsInit:
    mov [bp+X_ROW],ax
    cmp word ptr [bp+X_FAST],1
    jne BRowsSourceCheck
    test word ptr [bp+X_USE],2
    jnz BRowsSourceCheck
    cmp word ptr [bp+X_DK],0
    jne BRowsSourceCheck
    jmp BltMemoryFillRows
BRowsSourceCheck:
    cmp word ptr [bp+X_FAST],1
    jne BRowsNext
    mov ax,[bp+X_USE]
    and ax,6
    cmp ax,2
    je BRowsSourceReady
    cmp ax,6
    jne BRowsNext
    cmp word ptr [bp+X_DP],4
    jne BRowsNext
    call BltConstantPattern
    jc BRowsNext
BRowsSourceReady:
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    je BRowsPlanes
    cmp word ptr [bp+X_SP],1
    jne BRowsNext
BRowsPlanes:
    jmp BltPlaneRows
BRowsNext:
    cmp word ptr [bp+X_FAST],0
    je BRowsOriginal
    cmp word ptr [bp+X_FAST],2
    je BRowsPrepared
    xor ax,ax
    call BltCopyMapRow
    test word ptr [bp+X_USE],2
    jz BRowsPrepared
    mov ax,1
    call BltCopyMapRow
BRowsPrepared:
    mov word ptr [bp+X_FAST],2
    jmp BRowsPattern
BRowsOriginal:
    cmp word ptr [bp+X_ROP],0cch
    jne BRowsChecked
    cmp word ptr [bp+X_MODE],0
    jne BRowsChecked
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    jne BRowsChecked
    xor ax,ax
    call BltCopyMapRow
    jc BRowsMapBad
    mov ax,1
    call BltCopyMapRow
    jc BRowsMapBad
    call BltCopyRow
    jmp BRowsAdvance
BRowsMapBad:
    jmp BRowsReturn
BRowsChecked:
    xor ax,ax
    call BltMapRow
    jnc @F
    jmp BRowsReturn
@@:
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BRowsPattern
@@:
    mov ax,1
    call BltMapRow
    jnc @F
    jmp BRowsReturn
@@:
BRowsPattern:
    test word ptr [bp+X_USE],2
    jnz BRowsSource
    call BltPattern
    call BltPatternRow
    jmp BRowsAdvance
BRowsSource:
    cmp word ptr [bp+X_MODE],0
    jne BRowsSlow
    ; All source/destination-only truth tables share the row engine. A mono
    ; source is expanded using the current foreground/background per plane.
    test word ptr [bp+X_USE],4
    jnz BRowsSlow
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    je BRowsSourceFast
    cmp word ptr [bp+X_SP],1
    jne BRowsSlow
BRowsSourceFast:
    call BltCopyRow
    jmp BRowsAdvance
BRowsSlow:
    call BltPattern
    mov ax,[bp+X_W]
    mov [bp+X_LEFT],ax
    mov word ptr [bp+X_POS],0
BChunk:
    cmp word ptr [bp+X_XDIR],1
    je @F
    jmp BChunkReverse
@@:
    mov ax,[bp+X_POS]
    add ax,[bp+X_DX]
    and ax,7
    mov cx,8
    sub cx,ax
    cmp cx,[bp+X_LEFT]
    ja @F
    jmp BChunkCount
@@:
    mov cx,[bp+X_LEFT]
    jmp BChunkCount
BChunkReverse:
    mov ax,[bp+X_LEFT]
    add ax,[bp+X_DX]
    dec ax
    and ax,7
    inc ax
    mov cx,ax
    cmp cx,[bp+X_LEFT]
    ja @F
    jmp BChunkCount
@@:
    mov cx,[bp+X_LEFT]
BChunkCount:
    cmp word ptr [bp+X_MODE],1
    je @F
    jmp BChunkPacked
@@:
    mov cx,1                    ; two-direction alias proof is for ONE bit
BChunkPacked:
    mov [bp+X_COUNT],cx
    cmp word ptr [bp+X_XDIR],1
    jne @F
    jmp BChunkPositioned
@@:
    mov ax,[bp+X_LEFT]
    sub ax,cx
    mov [bp+X_POS],ax
BChunkPositioned:
    call BltTile
    mov ax,[bp+X_COUNT]
    sub [bp+X_LEFT],ax
    add [bp+X_POS],ax
    cmp word ptr [bp+X_LEFT],0
    je @F
    jmp BChunk
@@:
BRowsAdvance:
    cmp word ptr [bp+X_FAST],2
    jne BRowsAdvanceIndex
    xor ax,ax
    call BltStepRow
    test word ptr [bp+X_USE],2
    jz BRowsAdvanceIndex
    mov ax,1
    call BltStepRow
BRowsAdvanceIndex:
    mov ax,[bp+X_YDIR]
    add [bp+X_ROW],ax
    mov ax,[bp+X_ROW]
    cmp ax,[bp+X_H]
    jae @F
    jmp BRowsNext
@@:
    clc
BRowsReturn:
    ret
BltRows endp

; At most one destination byte per plane, no source word loads. A second
; source byte is fetched ONLY if participating source bits cross its boundary.
; All source planes are staged and converted before writing any destination.
BltTile proc near
    mov ax,[bp+X_POS]
    add ax,[bp+X_DX]
    mov bx,ax
    and bx,7
    mov [bp+X_PHASE],bx
    mov cl,3
    shr ax,cl
    mov [bp+X_BYTE],ax
    mov ax,[bp+X_POS]
    add ax,[bp+X_SX]
    mov dx,ax
    and dx,7
    mov [bp+X_SPHASE],dx
    cmp word ptr [bp+X_MODE],1
    je @F
    jmp BTileAccess
@@:
    ; Compare physical bit identities without truncating 24-bit byte bases.
    ; Bit position is 0 for MSB; compare position only when bytes are equal.
    mov ax,[bp+X_DNORM]
    mov dx,[bp+X_DNORM+2]
    add ax,[bp+X_BYTE]
    adc dx,0
    mov bx,[bp+X_POS]
    add bx,[bp+X_SX]
    mov cl,3
    shr bx,cl
    mov si,[bp+X_SNORM]
    mov di,[bp+X_SNORM+2]
    add si,bx
    adc di,0
    cmp dx,di
    jae @F
    jmp BTileLower
@@:
    jbe @F
    jmp BTileHigher
@@:
    cmp ax,si
    jae @F
    jmp BTileLower
@@:
    jbe @F
    jmp BTileHigher
@@:
    mov ax,[bp+X_PHASE]
    cmp ax,[bp+X_SPHASE]
    ja @F
    jmp BTileLower
@@:
BTileHigher:
    cmp word ptr [bp+X_PASS],1
    je @F
    jmp BTileDone
@@:
    jmp BTileAccess
BTileLower:
    cmp word ptr [bp+X_PASS],0
    je @F
    jmp BTileDone
@@:
BTileAccess:
    xor ax,ax
    mov bx,[bp+X_PHASE]
    xor dx,dx
    mov cx,[bp+X_COUNT]
    call BlitAlign
    mov byte ptr [bp+X_MASK],ah
    mov word ptr [bp+X_SOURCE],0
    mov word ptr [bp+X_SOURCE+2],0
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BTileWrite
@@:
    xor si,si
    xor di,di
BTileReadPlane:
    mov bx,[bp+X_SPTR+si]
    mov ax,[bp+X_POS]
    add ax,[bp+X_SX]
    mov cl,3
    shr ax,cl
    add bx,ax
    mov ax,[bp+X_SPTR+si+2]
    mov es,ax
    xor ax,ax
    mov ah,es:[bx]
    mov cx,[bp+X_SPHASE]
    add cx,[bp+X_COUNT]
    cmp cx,8
    ja @F
    jmp BTileWindow
@@:
    mov al,es:[bx+1]
BTileWindow:
    mov dx,[bp+X_SPHASE]
    mov bx,[bp+X_PHASE]
    mov cx,[bp+X_COUNT]
    call BlitAlign
    mov byte ptr [bp+X_SOURCE+di],al
    add si,4
    inc di
    cmp di,[bp+X_SP]
    jae @F
    jmp BTileReadPlane
@@:
    mov ax,[bp+X_SP]
    cmp ax,[bp+X_DP]
    jne @F
    jmp BTileWrite
@@:
    cmp ax,1
    je @F
    jmp BTileToMono
@@:
    mov al,byte ptr [bp+X_SOURCE]
    mov bl,byte ptr [bp+T_DM+11]
    mov bh,byte ptr [bp+T_DM+7]
    and bx,0f0fh
    call BlitMonoToColour
    mov [bp+X_SOURCE],ax
    mov word ptr [bp+X_SOURCE+2],dx
    jmp BTileWrite
BTileToMono:
    mov ax,[bp+X_SOURCE]
    mov dx,word ptr [bp+X_SOURCE+2]
    mov bl,byte ptr [bp+T_DM+7]
    and bl,15
    call BlitColourToMono
    mov byte ptr [bp+X_SOURCE],al
BTileWrite:
    ; 0.10: source bytes are already staged across ALL planes and
    ; converted to the destination format. A copy needs no ROP truth reducer.
    cmp word ptr [bp+X_ROP],0cch
    jne BTileGeneralWrite
    xor si,si
    xor bx,bx
BTileCopyWrite:
    mov di,bx
    mov dl,byte ptr [bp+X_SOURCE+di]
    mov di,[bp+X_DPTR+si]
    add di,[bp+X_BYTE]
    mov es,[bp+X_DPTR+si+2]
    mov cl,byte ptr [bp+X_MASK]
    mov al,dl
    cmp cl,0ffh
    je BTileCopyStore
    mov ah,es:[di]
    xor al,ah
    and al,cl
    xor al,ah
BTileCopyStore:
    mov es:[di],al
    inc bx
    add si,4
    cmp bx,[bp+X_DP]
    jb BTileCopyWrite
    ret
BTileGeneralWrite:
    test word ptr [bp+X_USE],4
    jnz BTilePatternWrite
    xor si,si
    xor bx,bx
BTileSDWrite:
    mov di,bx
    mov dl,byte ptr [bp+X_SOURCE+di]
    push bx
    mov bx,[bp+X_ROP]
    and bx,15
    shl bx,2
    mov dh,dl
    and dx,FillRopPairs[bx+2]
    xor dx,FillRopPairs[bx]
    mov al,byte ptr [bp+X_MASK]
    and dl,al
    not al
    or dh,al
    mov di,[bp+X_DPTR+si]
    add di,[bp+X_BYTE]
    mov es,[bp+X_DPTR+si+2]
    mov al,dl
    or dh,dh
    jz BTileSDStore
    mov ah,es:[di]
    and ah,dh
    xor al,ah
BTileSDStore:
    mov es:[di],al
    pop bx
    inc bx
    add si,4
    cmp bx,[bp+X_DP]
    jb BTileSDWrite
    ret
BTilePatternWrite:
    xor si,si
    xor bx,bx
    xor bx,bx
BTileWritePlane:
    push bx
    mov di,bx
    mov dl,byte ptr [bp+X_SOURCE+di]
    mov bl,byte ptr [bp+B_PAT+di]
    mov di,[bp+X_DPTR+si]
    add di,[bp+X_BYTE]
    mov ax,[bp+X_DPTR+si+2]
    mov es,ax
    mov cl,byte ptr [bp+X_MASK]
    mov ax,[bp+X_ROP]
    call BlitWriteByte
    pop bx
    inc bx
    add si,4
    cmp bx,[bp+X_DP]
    jae @F
    jmp BTileWritePlane
@@:
BTileDone:
    ret
BltTile endp

; Exclude only the screen source/destination bounding rectangle. A memory
; operand whose physical envelope can alias VRAM retains full exclusion.
; Both rectangles contribute: a screen read must not copy the pointer.
public BltEnterDrawing
BltEnterDrawing proc near
    mov word ptr [bp+X_CURBOX],640
    mov word ptr [bp+X_CURBOX+2],400
    mov word ptr [bp+X_CURBOX+4],0
    mov word ptr [bp+X_CURBOX+6],0
    lea si,[bp+X_CURBOX]
    cmp word ptr [bp+X_DK],0
    jne BEDest
    cmp word ptr [bp+X_DMIN+2],000ch
    jae BESource
    cmp word ptr [bp+X_DMAX+2],000ah
    jb BESource
    jmp EnterDrawing
BEDest:
    mov ax,[bp+X_DX]
    mov bx,[bp+X_DY]
    mov cx,ax
    mov dx,bx
    add cx,[bp+X_W]
    add dx,[bp+X_H]
    call CursorRectUnion
BESource:
    test word ptr [bp+X_USE],2
    jz BEClaim
    cmp word ptr [bp+X_SK],0
    jne BEScreen
    cmp word ptr [bp+X_SMIN+2],000ch
    jae BEClaim
    cmp word ptr [bp+X_SMAX+2],000ah
    jb BEClaim
    jmp EnterDrawing
BEScreen:
    mov ax,[bp+X_SX]
    mov bx,[bp+X_SY]
    mov cx,ax
    mov dx,bx
    add cx,[bp+X_W]
    add dx,[bp+X_H]
    call CursorRectUnion
BEClaim:
    mov ax,[bp+X_CURBOX]
    mov bx,[bp+X_CURBOX+2]
    mov cx,[bp+X_CURBOX+4]
    mov dx,[bp+X_CURBOX+6]
    jmp EnterDrawingRect
BltEnterDrawing endp

; Schema 2 distinguishes the formerly unclassified source operations. Counts
; describe a complete session, not one window or a presumed DrawIcon trace.
BltSourceCounters proc near
    cmp word ptr [bp+X_DK],0
    jne BPCSource
    PERFINC PerfMemoryDest
BPCSource:
    test word ptr [bp+X_USE],2
    jnz @F
    jmp BPCDone
@@:
    PERFINC PerfSource
    mov ax,[bp+X_DX]
    xor ax,[bp+X_SX]
    test ax,7
    jnz BFSourceShifted
    PERFINC PerfSourceAligned
    jmp BFSourceCounted
BFSourceShifted:
    PERFINC PerfSourceShifted
BFSourceCounted:
    cmp word ptr [bp+X_SP],1
    jne BPCRop
    PERFINC PerfMonoSource
BPCRop:
    cmp word ptr [bp+X_ROP],0cch
    jne BPCAnd
    PERFINC PerfSrcCopy
    ret
BPCAnd:
    cmp word ptr [bp+X_ROP],088h
    jne BPCXor
    PERFINC PerfSrcAnd
    ret
BPCXor:
    cmp word ptr [bp+X_ROP],066h
    jne BPCOr
    PERFINC PerfSrcXor
    ret
BPCOr:
    cmp word ptr [bp+X_ROP],0eeh
    jne BPCOther
    PERFINC PerfSrcOr
    ret
BPCOther:
    PERFINC PerfSrcOther
BPCDone:
    ret
BltSourceCounters endp

; Marginal dimension counts after clipping/preflight. Trace-disabled calls
; bypass this routine. No drawing-time I/O or persistent geometry cache.
BltFillHistogram proc near
    mov ax,[bp+X_W]
    cmp ax,1
    jne BFWidth8
    PERFINC PerfWidth1
    jmp BFHeight
BFWidth8:
    cmp ax,8
    ja BFWidth128
    PERFINC PerfWidth8
    jmp BFHeight
BFWidth128:
    cmp ax,128
    ja BFWidthWide
    PERFINC PerfWidth128
    jmp BFHeight
BFWidthWide:
    PERFINC PerfWidthWide
BFHeight:
    mov ax,[bp+X_H]
    cmp ax,1
    jne BFHeight7
    PERFINC PerfHeight1
    ret
BFHeight7:
    cmp ax,7
    ja BFHeight31
    PERFINC PerfHeight7
    ret
BFHeight31:
    cmp ax,31
    ja BFHeightTall
    PERFINC PerfHeight31
    ret
BFHeightTall:
    PERFINC PerfHeightTall
    ret
BltFillHistogram endp
