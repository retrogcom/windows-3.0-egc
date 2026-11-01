; 0.11 ordinary/screen source engine. Enter only after complete preflight,
; X_MODE=0 plane independence and ordinary (non-huge) bitmap eligibility.
; Dead alias-scheduler locals hold per-call geometry; public frame is unchanged.
; Plane-major order preserves both selected copy directions. No shared state.
P_HEAD equ -480                 ; ten-byte edge records
P_TAIL equ -490
P_DBASE equ -502
P_SBASE equ -504
P_DY equ -506
P_SY equ -508
P_LEFT equ -510
P_COPY equ -512
P_DSTEP equ -514
P_SSTEP equ -516
P_EDGES equ -518
P_DSTEPS equ -528
P_SSTEPS equ -536
P_FAST equ -538
P_TOTAL equ -540
P_BAND equ -542
P_ROW0 equ -544
P_RINDEX equ -550
P_RAWD equ -558
P_RAWS equ -566
P_STEPS equ -568
P_LIMIT equ -546
P_ENTRY equ -570
P_HDELTA equ -572
P_FRONT equ -574
P_BACK equ -576
P_WORDS equ -578
public BltPlaneRows
BltPlaneRows proc near
    mov ax,[bp+X_H]
    mov [bp+P_TOTAL],ax
    mov [bp+P_LIMIT],ax
    ; Limit incomplete-colour exposure to eight rows on the four-plane
    ; screen. Full preflight and overlap scheduling still cover the rectangle.
    cmp word ptr [bp+X_DK],0
    je BPRBandReady
    cmp ax,8
    jbe BPRBandReady
    mov word ptr [bp+P_LIMIT],8
BPRBandReady:
    mov ax,[bp+P_LIMIT]
    mov [bp+P_BAND],ax
    mov ax,[bp+X_ROW]
    mov [bp+P_ROW0],ax
    xor ax,ax
    call BltCopyMapRow
    mov ax,1
    call BltCopyMapRow
    mov ax,[bp+X_DDESC+6]
    mul word ptr [bp+X_DP]
    imul word ptr [bp+X_YDIR]
    mov [bp+P_DSTEP],ax
    mov ax,[bp+X_SDESC+6]
    mul word ptr [bp+X_SP]
    imul word ptr [bp+X_YDIR]
    mov [bp+P_SSTEP],ax
    mov word ptr [bp+P_COPY],0
    cmp word ptr [bp+X_ROP],0cch
    jne BPRGeometry
    mov ax,[bp+X_DP]
    cmp ax,[bp+X_SP]
    jne BPRGeometry
    mov word ptr [bp+P_COPY],1
BPRGeometry:
    mov cx,[bp+X_DX]
    neg cx
    and cx,7
    cmp cx,[bp+X_W]
    jbe BPRHead
    mov cx,[bp+X_W]
BPRHead:
    mov [bp+X_POS],cx
    xor ax,ax
    lea di,[bp+P_HEAD]
    call BPREdgeGeometry
    mov ax,[bp+X_W]
    sub ax,[bp+X_POS]
    mov cx,ax
    and cx,7
    shr ax,3
    mov [bp+F_RUN],ax
    mov ax,[bp+X_W]
    sub ax,cx
    lea di,[bp+P_TAIL]
    call BPREdgeGeometry
    xor ax,ax
    mov al,byte ptr [bp+P_HEAD+7]
    or al,byte ptr [bp+P_TAIL+7]
    mov [bp+P_EDGES],ax
    mov ax,[bp+X_POS]
    add ax,[bp+X_DX]
    shr ax,3
    mov [bp+F_DST],ax
    mov ax,[bp+X_POS]
    add ax,[bp+X_SX]
    mov dx,ax
    and dx,7
    mov cl,dl
    mov dh,1
    shl dh,cl
    dec dh                  ; 0.13: high byte = (1 shl phase) - 1
    mov [bp+X_SPHASE],dx
    shr ax,3
    mov [bp+F_SRC],ax
    mov word ptr [bp+P_FAST],0
    mov word ptr [bp+P_STEPS],0
    cmp word ptr [bp+X_H],4
    jb BPRRawStepsReady
    call BPRPrepareRawSteps
    mov word ptr [bp+P_STEPS],1
BPRRawStepsReady:
    cmp word ptr [bp+P_COPY],1
    jne BPRSetupDone
    cmp word ptr [bp+P_EDGES],0
    jne BPRSetupDone
    cmp word ptr [bp+X_SPHASE],0
    jne BPRSetupDone
    cmp word ptr [bp+X_H],4
    jb BPRSetupDone
    mov ax,[bp+P_DSTEP]
    or ax,[bp+P_SSTEP]
    test ax,1
    jnz BPRSetupDone
    test word ptr [bp+F_RUN],1
    jz BPRFastSteps
    ; Post020 E1: an odd run joins the fast stream only when its source
    ; and destination bytes differ in parity, so BPRAlignedRows selects
    ; the byte stream. Plane 0 decides here (plane strides are even);
    ; BPRAlignedOddStart re-checks every plane and keeps BPRRow otherwise.
    mov ax,[bp+X_DPTR]
    add ax,[bp+F_DST]
    mov dx,[bp+X_SPTR]
    add dx,[bp+F_SRC]
    xor ax,dx
    test ax,1
    jz BPRSetupDone
BPRFastSteps:
    call BPRPrepareSteps
    mov word ptr [bp+P_FAST],1
BPRSetupDone:
    ; Choose a plane entry once per rectangle. Eight-row bands keep the
    ; same bank phase and row increments, including their final short band.
    mov word ptr [bp+P_ENTRY],offset BPRRow
    cmp word ptr [bp+P_FAST],0
    je BPRSelectShifted
    mov word ptr [bp+P_ENTRY],offset BPRAlignedOddStart
    test word ptr [bp+F_RUN],1
    jnz BPRFastEntryReady
    mov word ptr [bp+P_ENTRY],offset BPRAlignedStart
BPRFastEntryReady:
    jmp BPRFusedReady
BPRSelectShifted:
BPRFusedGeometry:
    cmp word ptr [bp+X_XDIR],1
    je BPRForwardGeometry
    cmp word ptr [bp+P_STEPS],1
    jne BPRBackFallback
    cmp word ptr [bp+P_COPY],1
    jne BPRBackFallback
    cmp word ptr [bp+X_SPHASE],0
    jne BPRBackFallback
    cmp word ptr [bp+F_RUN],0
    je BPRBackFallback
    mov ax,[bp+P_DSTEP]
    or ax,[bp+P_SSTEP]
    test ax,1
    jnz BPRBackFallback
    call BPRPrepareSteps
    mov word ptr [bp+P_ENTRY],offset BPRBackwardStart
    jmp BPRFusedReady
BPRBackFallback:
    jmp BPRFusedReady
BPRForwardGeometry:
    cmp word ptr [bp+P_STEPS],1
    je @F
    jmp BPRFusedReady
@@:
    cmp word ptr [bp+F_RUN],0
    jne BPRHasInterior
    ; Edges are self-contained: never read the fused interior lookahead.
    mov word ptr [bp+P_ENTRY],offset BPREdgeOnlyStart
    jmp BPRFusedReady
BPRHasInterior:
    cmp word ptr [bp+X_SPHASE],0
    jne BPRHasShift
    cmp word ptr [bp+P_COPY],1
    jne BPRLogicGeometry
    cmp word ptr [bp+F_RUN],3
    ja BPRForwardCopyGeometry
    mov word ptr [bp+P_ENTRY],offset BPRCaptionStart
    jmp BPRFusedReady
BPRForwardCopyGeometry:
    mov ax,[bp+P_DSTEP]
    or ax,[bp+P_SSTEP]
    test ax,1
    jnz BPRFusedReady
    call BPRPrepareSteps
    mov word ptr [bp+P_ENTRY],offset BPRForwardCopyStart
    jmp BPRFusedReady
; Same-bit-phase logic preserves the ordinary forward alias schedule. Even
; row steps preserve byte parity, including each eight-row screen band.
BPRLogicGeometry:
    mov ax,[bp+P_DSTEP]
    or ax,[bp+P_SSTEP]
    test ax,1
    jnz BPRFusedReady
    call BPRPrepareSteps
    mov word ptr [bp+P_ENTRY],offset BPRLogicStart
    jmp BPRFusedReady
BPRHasShift:
    cmp word ptr [bp+P_COPY],1
    jne BPRLargeShift
    cmp word ptr [bp+F_RUN],3
    ja BPRLargeShift
    mov al,byte ptr [bp+P_HEAD+5]
    sub al,byte ptr [bp+P_HEAD+4]
    cmp byte ptr [bp+P_HEAD+8],0
    je @F
    neg al
@@:
    mov byte ptr [bp+P_FRONT],al
    mov ax,[bp+P_HEAD+2]
    sub ax,[bp+F_SRC]
    mov [bp+P_HDELTA],ax
    mov word ptr [bp+P_ENTRY],offset BPRSmallShiftStart
    jmp BPRFusedReady
BPRLargeShift:
    mov ax,[bp+P_DSTEP]
    or ax,[bp+P_SSTEP]
    test ax,1
    jnz BPRFusedReady
    call BPRShiftPrepare
    mov word ptr [bp+P_ENTRY],offset BPRFusedStart
BPRFusedReady:
    mov word ptr [bp+F_PLANE],0
BPRPlane:
    cmp word ptr [bp+P_COPY],0
    jne BPRPlaneCoeffs
    call BltSourceCoefficients
BPRPlaneCoeffs:
    mov si,[bp+F_PLANE]
    mov es,[bp+X_DPTR+si+2]
    mov ax,[bp+X_DPTR+si]
    mov [bp+P_DBASE],ax
    cmp word ptr [bp+X_SP],1
    jne BPRSource
    xor si,si
BPRSource:
    mov dx,[bp+X_SPTR+si+2]
    mov ax,[bp+X_SPTR+si]
    mov [bp+P_SBASE],ax
    mov ax,[bp+X_ROW]
    add ax,[bp+X_DY]
    mov [bp+P_DY],ax
    mov ax,[bp+X_ROW]
    add ax,[bp+X_SY]
    mov [bp+P_SY],ax
    mov ax,[bp+P_BAND]
    mov [bp+P_LEFT],ax
    mov word ptr [bp+P_RINDEX],0
    push ds
    mov ds,dx
    jmp word ptr [bp+P_ENTRY]
BPRFusedStart:
    cmp word ptr [bp+P_COPY],1
    je BPRFusedPointers
    cmp word ptr [bp+S_WORD],offset SW12
    je BPRFusedPointers
    cmp word ptr [bp+S_WORD],offset SW8
    je BPRFusedPointers
    cmp word ptr [bp+S_WORD],offset SW6
    je BPRFusedPointers
    cmp word ptr [bp+S_WORD],offset SW11
    je BPRFusedPointers
    cmp word ptr [bp+S_WORD],offset SW14
    je BPRFusedPointers
    jmp BPRRow
BPRFusedPointers:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRShiftRows
    jmp BPRPlaneDone
BPRForwardCopyStart:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRForwardCopyRows
    jmp BPRPlaneDone
BPRLogicStart:
    ; Coefficients are already expanded for mono colours and pattern plane.
    ; Dispatch on their effective function, never merely the caller's ROP.
    cmp word ptr [bp+S_WORD],offset SW8
    je BPRLogicPointers
    cmp word ptr [bp+S_WORD],offset SW6
    je BPRLogicPointers
    jmp BPRRow
BPRLogicPointers:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRLogicRows
    jmp BPRPlaneDone
BPRSmallShiftStart:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRSmallShiftRows
    jmp BPRPlaneDone
BPRBackwardStart:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRBackwardRows
    jmp BPRPlaneDone
BPRCaptionStart:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    call BPRCaptionRows
    jmp BPRPlaneDone
BPRAlignedStart:
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
BPRUseStream:
    call BPRAlignedRows
    jmp BPRPlaneDone
BPRAlignedOddStart:
    ; Post020 E1: odd run. Only the byte stream copies it exactly; equal
    ; parity would select a word stream, so such a plane keeps BPRRow.
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    mov ax,si
    xor ax,di
    test ax,1
    jnz BPRUseStream
    jmp BPRRow
BPRRow:
    cmp word ptr [bp+P_EDGES],0
    je BPRInterior
    lea bx,[bp+P_HEAD]
    cmp word ptr [bp+X_XDIR],1
    je BPRFirst
    lea bx,[bp+P_TAIL]
BPRFirst:
    call BPREdge
BPRInterior:
    cmp word ptr [bp+F_RUN],0
    je BPRLast
    mov si,[bp+P_SBASE]
    add si,[bp+F_SRC]
    mov di,[bp+P_DBASE]
    add di,[bp+F_DST]
    mov cx,[bp+F_RUN]
    cmp word ptr [bp+P_COPY],0
    je BPRGeneral
    cmp word ptr [bp+X_SPHASE],0
    je BPRAligned
    call BltShiftPlane
    jmp BPRLast
BPRAligned:
    call BltCopyAlignedPlane
    jmp BPRLast
BPRGeneral:
    mov bx,cx
    call BltSourcePlane
BPRLast:
    cmp word ptr [bp+P_EDGES],0
    je BPRNextRow
    lea bx,[bp+P_TAIL]
    cmp word ptr [bp+X_XDIR],1
    je BPRSecond
    lea bx,[bp+P_HEAD]
BPRSecond:
    call BPREdge
BPRNextRow:
    dec word ptr [bp+P_LEFT]
    jz BPRPlaneDone
    cmp word ptr [bp+P_STEPS],0
    je BPRShortStep
    mov si,[bp+P_RINDEX]
    mov dx,[bp+P_RAWD+si]
    add [bp+P_DBASE],dx
    mov dx,[bp+P_RAWS+si]
    add [bp+P_SBASE],dx
    add si,2
    and si,6
    mov [bp+P_RINDEX],si
    jmp BPRRow
BPRShortStep:
    mov dx,[bp+P_DSTEP]
    cmp word ptr [bp+X_DK],0
    je BPRDStep
    mov ax,[bp+P_DY]
    call BPRScreenStep
BPRDStep:
    add [bp+P_DBASE],dx
    mov dx,[bp+P_SSTEP]
    cmp word ptr [bp+X_SK],0
    je BPRSStep
    mov ax,[bp+P_SY]
    call BPRScreenStep
BPRSStep:
    add [bp+P_SBASE],dx
    mov ax,[bp+X_YDIR]
    add [bp+P_DY],ax
    add [bp+P_SY],ax
    jmp BPRRow
BPRPlaneDone:
    pop ds
    add word ptr [bp+F_PLANE],4
    mov ax,[bp+X_DP]
    shl ax,2
    cmp [bp+F_PLANE],ax
    jae @F
    jmp BPRPlane
@@:
    mov ax,[bp+P_BAND]
    sub [bp+P_TOTAL],ax
    jz BPRFinished
    imul word ptr [bp+X_YDIR]
    add [bp+X_ROW],ax
    ; Post020 E3: bands exist only for the four-plane screen destination
    ; and a completed band is always eight rows, so every destination
    ; plane start moves by two interleave rows (160 bytes) in the band
    ; direction; the row offset is (y mod 4)*8192+(y div 4)*80 and no
    ; clipped row reaches a bank end. A screen source moves the same way,
    ; an ordinary memory source by eight plane-group strides (the
    ; sixteen-bit product BltCopyMapRow forms); a segmented source is
    ; still remapped.
    mov ax,160
    cmp word ptr [bp+X_YDIR],1
    je @F
    neg ax
@@:
    add [bp+X_DPTR],ax
    add [bp+X_DPTR+4],ax
    add [bp+X_DPTR+8],ax
    add [bp+X_DPTR+12],ax
    cmp word ptr [bp+X_SK],0
    jne BPRBandSource
    cmp word ptr [bp+X_SDESC+22],0
    jne BPRBandRemap
    mov ax,[bp+P_SSTEP]
    shl ax,3
BPRBandSource:
    add [bp+X_SPTR],ax
    cmp word ptr [bp+X_SP],1
    je BPRBandMapped
    add [bp+X_SPTR+4],ax
    add [bp+X_SPTR+8],ax
    add [bp+X_SPTR+12],ax
    jmp BPRBandMapped
BPRBandRemap:
    mov ax,1
    call BltCopyMapRow
BPRBandMapped:
    mov ax,[bp+P_TOTAL]
    cmp ax,[bp+P_LIMIT]
    jbe BPRNextBand
    mov ax,[bp+P_LIMIT]
BPRNextBand:
    mov [bp+P_BAND],ax
    jmp BPRFusedReady
BPRFinished:
    mov ax,[bp+P_ROW0]
    mov [bp+X_ROW],ax
    cld
    clc
    ret
BltPlaneRows endp

; Four row increments, prepared once. Every screen bank increment and ordinary
; row stride is even. Includes the pointer movement performed by REP MOVSW.
BPRPrepareSteps proc near
    xor si,si
BPRStepPrepare:
    mov ax,[bp+si+P_RAWD]
    mov [bp+si+P_DSTEPS],ax
    mov ax,[bp+si+P_RAWS]
    mov [bp+si+P_SSTEPS],ax
    mov ax,[bp+F_RUN]
    cmp word ptr [bp+X_XDIR],1
    jne BPRStepAdjust
    neg ax
BPRStepAdjust:
    add [bp+si+P_DSTEPS],ax
    add [bp+si+P_SSTEPS],ax
    add si,2
    cmp si,8
    jb BPRStepPrepare
    ret
BPRPrepareSteps endp

; ES:DI and DS:SI even, run positive/even, row increments precompiled.
; AX and BX walk the four-entry tables; DX counts rows. DF restored on return.
BPRAlignedRows proc near
    mov ax,si
    xor ax,di
    test ax,1
    jnz BPRBytes
    test si,1
    jz BPRBothEven
    jmp BPROddRows
BPRBothEven:
    cmp word ptr [bp+X_XDIR],1
    je BPRStreamForward
    mov ax,[bp+F_RUN]
    sub ax,2
    add si,ax
    add di,ax
    std
    jmp BPRStreamReady
BPRStreamForward:
    cld
BPRStreamReady:
    lea bx,[bp+P_DSTEPS]
    lea ax,[bp+P_DSTEPS+8]
    mov dx,[bp+P_LEFT]
BPRStreamRow:
    mov cx,[bp+F_RUN]
    shr cx,1
    rep movsw
    dec dx
    jz BPRStreamDone
    add di,ss:[bx]
    add si,ss:[bx-8]
    add bx,2
    cmp bx,ax
    jb BPRStreamRow
    sub bx,8
    jmp BPRStreamRow
BPRStreamDone:
    cld
    ret
BPRBytes:
    cmp word ptr [bp+X_XDIR],1
    je BPRBytesForward
    mov ax,[bp+F_RUN]
    dec ax
    add si,ax
    add di,ax
    std
    jmp BPRBytesReady
BPRBytesForward:
    cld
BPRBytesReady:
    lea bx,[bp+P_DSTEPS]
    lea ax,[bp+P_DSTEPS+8]
    mov dx,[bp+P_LEFT]
BPRByteRow:
    mov cx,[bp+F_RUN]
    rep movsb
    dec dx
    jz BPRStreamDone
    add di,ss:[bx]
    add si,ss:[bx-8]
    add bx,2
    cmp bx,ax
    jb BPRByteRow
    sub bx,8
    jmp BPRByteRow
; Equal odd addresses, even run and even strides. Each row copies one
; leading byte, aligned interior words and one trailing byte. Backward
; pointers still advance by exactly -run, matching the prepared steps.
BPROddRows:
    lea bx,[bp+P_DSTEPS]
    lea ax,[bp+P_DSTEPS+8]
    mov dx,[bp+P_LEFT]
    cmp word ptr [bp+X_XDIR],1
    jne BPROddBackward
    cld
BPROddForwardRow:
    mov cx,[bp+F_RUN]
    shr cx,1
    dec cx
    movsb
    rep movsw
    movsb
    dec dx
    jz BPROddDone
    add di,ss:[bx]
    add si,ss:[bx-8]
    add bx,2
    cmp bx,ax
    jb BPROddForwardRow
    sub bx,8
    jmp BPROddForwardRow
BPROddBackward:
    mov cx,[bp+F_RUN]
    dec cx
    add si,cx
    add di,cx
    std
BPROddBackwardRow:
    mov cx,[bp+F_RUN]
    shr cx,1
    dec cx
    movsb
    dec si
    dec di
    rep movsw
    inc si
    inc di
    movsb
    dec dx
    jz BPROddDone
    add di,ss:[bx]
    add si,ss:[bx-8]
    add bx,2
    cmp bx,ax
    jb BPROddBackwardRow
    sub bx,8
    jmp BPROddBackwardRow
BPROddDone:
    cld
    ret
BPRAlignedRows endp

; AX relative x, CX count 0..7, SS:DI record: dst/src byte offsets,
; source/destination shifts, coverage, count and second-source-byte flag.
BPREdgeGeometry proc near
    mov ss:[di+7],cl
    add ax,[bp+X_DX]
    mov bx,ax
    and bx,7
    mov ss:[di+5],bl
    sub ax,[bp+X_DX]
    add ax,[bp+X_SX]
    mov dx,ax
    and dx,7
    mov ss:[di+4],dl
    add dx,cx
    mov word ptr ss:[di+8],0
    cmp dx,8
    jbe BPREdgeOne
    mov word ptr ss:[di+8],1
BPREdgeOne:
    push ax
    mov al,ss:[di+4]
    cmp al,ss:[di+5]
    jne BPREdgePhaseReady
    mov byte ptr ss:[di+9],1
BPREdgePhaseReady:
    pop ax
    shr ax,3
    mov ss:[di+2],ax
    mov ax,cx
    mov cx,8
    sub cx,ax
    mov al,0ffh
    shl al,cl
    mov cx,bx
    shr al,cl
    mov ss:[di+6],al
    ; Recover relative x from the source byte plus its bit phase.
    mov ax,ss:[di+2]
    shl ax,3
    xor dx,dx
    mov dl,ss:[di+4]
    add ax,dx
    sub ax,[bp+X_SX]
    add ax,[bp+X_DX]
    shr ax,3
    mov ss:[di],ax
    ret
BPREdgeGeometry endp

BPREdge proc near
    cmp byte ptr ss:[bx+7],0
    je BPREdgeDone
    mov si,[bp+P_SBASE]
    add si,ss:[bx+2]
    mov di,[bp+P_DBASE]
    add di,ss:[bx]
    cmp byte ptr ss:[bx+9],1
    jne BPREdgeShifted
    mov al,[si]
    jmp BPREdgeValue
BPREdgeShifted:
    mov ah,[si]
    xor al,al
    cmp byte ptr ss:[bx+8],0
    je BPREdgeRead
    mov al,[si+1]
BPREdgeRead:
    mov cl,ss:[bx+4]
    shl ax,cl
    mov al,ah
    mov cl,ss:[bx+5]
    shr al,cl
BPREdgeValue:
    cmp word ptr [bp+P_COPY],0
    je BPREdgeRop
    mov ah,es:[di]
    xor al,ah
    and al,ss:[bx+6]
    xor al,ah
    mov es:[di],al
    ret
BPREdgeRop:
    mov dl,al
    and al,byte ptr [bp+S_C1]
    xor al,byte ptr [bp+S_C0]
    and dl,byte ptr [bp+S_M1]
    xor dl,byte ptr [bp+S_M0]
    mov cl,ss:[bx+6]
    and al,cl
    not cl
    or dl,cl
    and dl,es:[di]
    xor al,dl
    mov es:[di],al
BPREdgeDone:
    ret
BPREdge endp

BPRScreenStep proc near
    and ax,3
    cmp word ptr [bp+X_YDIR],1
    jne BPRStepBack
    mov dx,2000h
    cmp ax,3
    jne BPRStepDone
    mov dx,0a050h
    ret
BPRStepBack:
    mov dx,0e000h
    or ax,ax
    jnz BPRStepDone
    mov dx,5fb0h
BPRStepDone:
    ret
BPRScreenStep endp

; Prepare the bank/linear increments once for shifted and masked rows too.
BPRPrepareRawSteps proc near
    xor si,si
BPRRawPrepare:
    mov ax,si
    shr ax,1
    imul word ptr [bp+X_YDIR]
    add ax,[bp+X_ROW]
    mov bx,ax
    add ax,[bp+X_DY]
    mov dx,[bp+P_DSTEP]
    cmp word ptr [bp+X_DK],0
    je BPRRawD
    call BPRScreenStep
BPRRawD:
    mov [bp+si+P_RAWD],dx
    mov ax,bx
    add ax,[bp+X_SY]
    mov dx,[bp+P_SSTEP]
    cmp word ptr [bp+X_SK],0
    je BPRRawS
    call BPRScreenStep
BPRRawS:
    mov [bp+si+P_RAWS],dx
    add si,2
    cmp si,8
    jb BPRRawPrepare
    ret
BPRPrepareRawSteps endp

; 0.15 edge-only rows, forward horizontal order, preflight and raw row steps
; already proved. Reuse each exact edge record; load a second source byte
; only if its covered source pixels cross the byte boundary. Full-byte and
; backward copies retain their existing loops. No additional stack frame.
BPREdgeOnlyStart:
    mov word ptr [bp+P_FRONT],offset BPECopy
    cmp word ptr [bp+P_COPY],1
    je BPEStart
    cmp word ptr [bp+S_WORD],offset SW12
    je BPEStart
    mov word ptr [bp+P_FRONT],offset BPEAnd
    cmp word ptr [bp+S_WORD],offset SW8
    je BPEStart
    mov word ptr [bp+P_FRONT],offset BPEXor
    cmp word ptr [bp+S_WORD],offset SW6
    je BPEStart
    jmp BPRRow
BPEStart:
    lea bx,[bp+P_HEAD]
BPEEdge:
    cmp byte ptr ss:[bx+7],0
    je BPENextEdge
    mov si,[bp+P_SBASE]
    add si,ss:[bx+2]
    mov di,[bp+P_DBASE]
    add di,ss:[bx]
    mov ah,[si]
    xor al,al
    cmp byte ptr ss:[bx+8],0
    je BPELoaded
    mov al,[si+1]
BPELoaded:
    mov cl,ss:[bx+4]
    shl ax,cl
    mov al,ah
    mov cl,ss:[bx+5]
    shr al,cl
    mov ah,es:[di]
    jmp word ptr [bp+P_FRONT]
BPEAnd:
    and al,ah
BPECopy:
    xor al,ah
BPEXor:
    and al,ss:[bx+6]
    xor al,ah
    mov es:[di],al
BPENextEdge:
    lea ax,[bp+P_TAIL]
    cmp bx,ax
    je BPENextRow
    mov bx,ax
    jmp BPEEdge
BPENextRow:
    dec word ptr [bp+P_LEFT]
    jnz @F
    jmp BPRPlaneDone
@@:
    mov si,[bp+P_RINDEX]
    mov ax,[bp+P_RAWD+si]
    add [bp+P_DBASE],ax
    mov ax,[bp+P_RAWS+si]
    add [bp+P_SBASE],ax
    add si,2
    and si,6
    mov [bp+P_RINDEX],si
    jmp BPEStart

; Post-0.19: same-bit-phase forward SRCCOPY, any interior width > 3.
; Complete preflight, independent planes and even row steps are required.
; The selected byte/word accesses and head/interior/tail order match the
; general row writer. No new read, wider bus access, or persistent state.
public BPRForwardCopyRows
BPRForwardCopyRows proc near
    cld
    mov word ptr [bp+P_WORDS],0ffffh
    mov ax,si
    xor ax,di
    test ax,1
    jnz BPRFCReady
    mov ax,si
    and ax,1
    mov [bp+P_FRONT],ax
    mov cx,[bp+F_RUN]
    sub cx,ax
    mov ax,cx
    and ax,1
    mov [bp+P_BACK],ax
    shr cx,1
    mov [bp+P_WORDS],cx
BPRFCReady:
    mov dx,[bp+P_LEFT]
BPRFCRow:
    cmp byte ptr [bp+P_HEAD+7],0
    je BPRFCInterior
    mov al,[si-1]
    mov ah,es:[di-1]
    xor al,ah
    and al,byte ptr [bp+P_HEAD+6]
    xor al,ah
    mov es:[di-1],al
BPRFCInterior:
    mov cx,[bp+P_WORDS]
    cmp cx,0ffffh
    je BPRFCBytes
    cmp word ptr [bp+P_FRONT],0
    je BPRFCWords
    movsb
BPRFCWords:
    rep movsw
    cmp word ptr [bp+P_BACK],0
    je BPRFCTail
    movsb
    jmp BPRFCTail
BPRFCBytes:
    mov cx,[bp+F_RUN]
    rep movsb
BPRFCTail:
    cmp byte ptr [bp+P_TAIL+7],0
    je BPRFCNext
    mov al,[si]
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+P_TAIL+6]
    xor al,ah
    mov es:[di],al
BPRFCNext:
    dec dx
    jz BPRFCDone
    mov bx,[bp+P_RINDEX]
    add word ptr [bp+P_RINDEX],2
    and word ptr [bp+P_RINDEX],6
    add bx,bp
    add di,ss:[bx+P_DSTEPS]
    add si,ss:[bx+P_SSTEPS]
    jmp BPRFCRow
BPRFCDone:
    ret
BPRForwardCopyRows endp
