; Source-independent BitBlt rows: compile each plane once, then stream bytes.
; Caller completed BltPreflight and BltMapRow for the complete destination.
; X_USE has no source bit; no source alias or conversion path can enter here.
; Keep clipping, partial-byte neighbours and drawing/cursor ownership intact.
; Ordinary CPU stores only: no EGC controller acceleration or port accesses.
public BltPatternRow
BltPatternRow proc near
    call BltFillGeometry
    xor si,si
    xor di,di
BFPlane:
    mov bl,byte ptr [bp+B_PAT+di]
    xor dx,dx
    mov ax,[bp+X_ROP]
    call BlitReduce              ; once per row/plane, not for every byte
    mov es,[bp+X_DPTR+si+2]
    mov bx,[bp+X_DPTR+si]
    add bx,[bp+F_DST]
    mov cx,[bp+B_BYTES]
    mov al,byte ptr [bp+B_FIRST]
    cmp cx,1
    jne BFMany
    and al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    jmp BFNext
BFMany:
    call SpanEdgeByte
    inc bx
    sub cx,2
    jcxz BFLast
    or dh,dh
    jz BFConstant
    ; Amortize the shared word-loop setup only over wide interiors.
    cmp cx,8
    jb BFInner
    call FillLogicalBytes
    jmp BFLast
BFInner:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    loop BFInner
    jmp BFLast
BFConstant:
    mov al,dl
    xchg bx,di
    cld
    rep stosb
    xchg bx,di
BFLast:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
BFNext:
    add si,4
    inc di
    cmp di,[bp+X_DP]
    jb BFPlane
    ret
BltPatternRow endp


; Screen-only, source-free rectangle path. No memory bitmap, screen copy,
; alias scheduler or source conversion can enter. Borrowed mappings are stable
; for this non-yielding invocation. Validate every touched row BEFORE pixels.
; The physical row offsets are monotonic within each of the four banks:
; the minimum is among the first four rows, maximum among the last four.
; One enclosing ordinary-data span per plane proves every complete 80-byte
; row, including the same padding checked by the generic BitBlt preflight.
public ScreenFillPreflight, ScreenFillDraw, ScreenFillOffset
ScreenFillPreflight proc near
    cmp ScreenGate,1
    jne SFPBad
    cmp DriverState,ACTIVE
    jne SFPBad
    cmp word ptr [bp+X_DP],4
    jne SFPBad
    mov ax,[bp+X_DY]
    cmp ax,400
    jae SFPBad
    cmp word ptr [bp+X_H],0
    je SFPBad
    add ax,[bp+X_H]
    jc SFPBad
    cmp ax,400
    ja SFPBad
    mov [bp+F_ENDY],ax
    mov ax,[bp+X_DX]
    cmp ax,640
    jae SFPBad
    cmp word ptr [bp+X_W],0
    je SFPBad
    add ax,[bp+X_W]
    jc SFPBad
    cmp ax,640
    ja SFPBad
    jmp SFPBounds
SFPBad:
    stc
    ret
SFPBounds:
    cmp word ptr [bp+X_H],1
    jne SFPMultiple
    mov ax,[bp+X_DY]
    call ScreenFillOffset
    mov [bp+F_MIN],ax
    mov [bp+F_MAX],ax
    jmp SFPCheckPlanes
SFPMultiple:
    cmp word ptr [bp+X_H],4
    jb SFPShort
    ; All four banks participate. Lowest address is the first bank-zero
    ; row; highest is the last bank-three row. Two offsets replace eight.
    mov ax,[bp+X_DY]
    add ax,3
    and ax,0fffch
    call ScreenFillOffset
    mov [bp+F_MIN],ax
    mov ax,[bp+F_ENDY]
    sub ax,4
    or ax,3
    call ScreenFillOffset
    mov [bp+F_MAX],ax
    jmp SFPCheckPlanes
SFPShort:
    mov word ptr [bp+F_MIN],0ffffh
    mov word ptr [bp+F_MAX],0
    mov ax,[bp+X_H]
    cmp ax,4
    jbe @F
    mov ax,4
@@:
    mov [bp+F_ROWS],ax
    mov [bp+F_REMAIN],ax
    mov ax,[bp+X_DY]
    mov [bp+F_SCAN],ax
SFPMin:
    mov ax,[bp+F_SCAN]
    call ScreenFillOffset
    cmp ax,[bp+F_MIN]
    jae @F
    mov [bp+F_MIN],ax
@@:
    cmp ax,[bp+F_MAX]
    jbe SFPMinMaxReady
    mov [bp+F_MAX],ax
SFPMinMaxReady:
    inc word ptr [bp+F_SCAN]
    dec word ptr [bp+F_REMAIN]
    jnz SFPMin
SFPCheckPlanes:
    mov cx,[bp+F_MAX]
    sub cx,[bp+F_MIN]
    add cx,80
    xor di,di
SFPPlane:
    mov ax,PlaneSelectors[di]
    mov bx,PlaneBases[di]
    add bx,[bp+F_MIN]
    jc SFPInvalid
    call CheckSpan
    jc SFPInvalid
    add di,2
    cmp di,8
    jb SFPPlane
    clc
    ret
SFPInvalid:
    stc
    ret
ScreenFillPreflight endp

; AX logical y -> AX row offset. BX/CX/DX scratch. No access to pixels.
ScreenFillOffset proc near
    ; The bank number is two bits: rotating right three equals <<13.
    ; This arithmetic is equivalent for every WORD input, not just 0..399.
    mov dx,ax
    and dx,3
    ror dx,3
    shr ax,2
    mov bx,ax
    shl ax,2
    add ax,bx
    shl ax,4
    add ax,dx
    ret
ScreenFillOffset endp

; All bounds/rights already proved; caller owns the drawing exclusion.
; Pattern repeats every eight logical rows. Visit each phase independently,
; then each plane; reduce its ROP once and advance 160 bytes per repeated row.
; This ordering is safe only for this source-free, fixed screen layout.
; Partial edge bytes preserve neighbours; interiors with A=0 never read D.
; Aligned word stores use the owner-measured M28 CPU transfer path.
; Masked edges remain bytes; never round a transfer beyond its proved span.
; Screen-only opaque four-plane brush row. Caller already copied and
; validated the brush and optional monochrome colours. No Output ROP2
; coefficients are needed: ScreenFillDraw applies the actual BitBlt ROP3.
; Pixel phase is absolute destination y and the saved brush origin.
public ScreenFillPattern
ScreenFillPattern proc near
    test word ptr [bp+X_USE],4
    jnz SFPBrush
    xor ax,ax
    mov [bp+B_PAT],ax
    mov [bp+B_PAT+2],ax
    ret
SFPBrush:
    mov ax,[bp+X_DY]
    add ax,[bp+F_PHASE]
    sub al,byte ptr [bp+T_BRUSH+51]
    and ax,7
    mov si,ax
    mov cl,byte ptr [bp+T_BRUSH+50]
    and cl,7
    cmp word ptr [bp+T_BRUSH+48],4
    je SFPMMono
    ; Solid, colour-pattern and opaque hatched brushes share stored tiles.
    mov al,byte ptr [bp+T_BRUSH+si]
    ror al,cl
    mov ah,byte ptr [bp+T_BRUSH+si+8]
    ror ah,cl
    mov [bp+B_PAT],ax
    mov al,byte ptr [bp+T_BRUSH+si+16]
    ror al,cl
    mov ah,byte ptr [bp+T_BRUSH+si+24]
    ror ah,cl
    mov [bp+B_PAT+2],ax
    ret
SFPMMono:
    mov bl,byte ptr [bp+T_BRUSH+si+32]
    ror bl,cl
    mov dl,byte ptr [bp+T_DM+11] ; raw zero -> foreground
    mov dh,byte ptr [bp+T_DM+7]  ; raw one -> background
    shr dl,1
    sbb al,al
    shr dh,1
    sbb ah,ah
    xor ah,al
    and ah,bl
    xor al,ah
    mov byte ptr [bp+B_PAT+0],al
    shr dl,1
    sbb al,al
    shr dh,1
    sbb ah,ah
    xor ah,al
    and ah,bl
    xor al,ah
    mov byte ptr [bp+B_PAT+1],al
    shr dl,1
    sbb al,al
    shr dh,1
    sbb ah,ah
    xor ah,al
    and ah,bl
    xor al,ah
    mov byte ptr [bp+B_PAT+2],al
    shr dl,1
    sbb al,al
    shr dh,1
    sbb ah,ah
    xor ah,al
    and ah,bl
    xor al,ah
    mov byte ptr [bp+B_PAT+3],al
    ret
ScreenFillPattern endp

ScreenFillDraw proc near
    ; Compile source-free truth bits once, outside phase/plane loops.
    mov ax,[bp+X_ROP]
    mov bx,ax
    and ax,3
    shr bx,1
    shr bx,1
    and bx,0ch
    or bx,ax
    shl bx,1
    shl bx,1
    mov ax,FillRopPairs[bx]
    mov [bp+F_ROPCONST],ax
    mov ax,FillRopPairs[bx+2]
    mov [bp+F_ROPMASK],ax
    mov byte ptr [bp+B_COVER],0ffh
    ; Positive clipped bounds are already proved. Table indices are x mod 8.
    mov bx,[bp+X_DX]
    mov si,bx
    and si,7
    mov al,FillFirstMasks[si]
    mov byte ptr [bp+B_FIRST],al
    mov dx,bx
    add dx,[bp+X_W]
    dec dx
    mov si,dx
    and si,7
    mov al,FillLastMasks[si]
    mov byte ptr [bp+B_LAST],al
    shr bx,3
    mov [bp+F_XBYTE],bx
    shr dx,3
    sub dx,bx
    inc dx
    mov [bp+B_BYTES],dx
    ; Small row groups already have no distant-bank progression. Preserve
    ; the fast one-byte column writer for narrow borders/carets.
    cmp dx,1
    je SFOUnbanded
    cmp word ptr [bp+X_H],4
    jbe SFOUnbanded
    ; Prove and compile constant fills once for all ordered bands.
    call ScreenOrderedConstant
    jc SFOGeneral
    ret
SFOGeneral:
    push word ptr [bp+X_DY]
    push word ptr [bp+X_H]
    ; Descriptor scratch is dead on the admitted source-free screen path.
    mov ax,[bp+X_H]
    mov [bp+X_SDESC+68],ax
SFOBand:
    mov ax,[bp+X_SDESC+68]
    cmp ax,4
    jbe SFOHeight
    mov ax,4
SFOHeight:
    mov [bp+X_H],ax
    ; Inner helpers use descriptor scratch only for narrow >=8-row columns,
    ; which cannot be selected by this four-row schedule.
    ; The constant-pattern admission already failed for this invocation.
    ; Skip repeating it for every four-row group. Keep the one-row writer.
    cmp ax,1
    jne SFOBroadBand
    call SFDSingleRow
    jmp SFOBandDone
SFOBroadBand:
    call SFDBroad
SFOBandDone:
    mov ax,[bp+X_H]
    add [bp+X_DY],ax
    sub [bp+X_SDESC+68],ax
    jnz SFOBand
    pop word ptr [bp+X_H]
    pop word ptr [bp+X_DY]
    ret
SFOUnbanded:
    call ScreenFillBand
    ret
ScreenFillDraw endp

ScreenFillBand proc near
    cmp word ptr [bp+X_H],1
    jne @F
    jmp SFDSingleRow
@@:
    cmp word ptr [bp+X_H],32
    ja SFDShortDone
    cmp word ptr [bp+X_H],8
    jb SFDShortTry
    cmp dx,1
    je SFDShortDone
SFDShortTry:
    call ScreenShortConstant
    jc SFDShortDone
    ret
SFDShortDone:
    mov dx,[bp+B_BYTES]
    cmp dx,1
    je @F
    jmp SFDBroad
@@:
    call ScreenNarrowDraw
    ret
SFDSingleRow:
    ; One logical row has one phase and one offset, including patterned
    ; and destination-dependent ROPs. Reuse the exact existing row writer.
    mov word ptr [bp+F_PHASE],0
    call ScreenFillPattern
    ; Equal extrema prove that the complete preflight covered one physical
    ; row. Those locals survive every fill writer. A multi-row FastBorder
    ; envelope has distinct extrema, so each one-row strip still maps itself.
    mov ax,[bp+F_MIN]
    cmp ax,[bp+F_MAX]
    je SFDOneOffsetReady
    mov ax,[bp+X_DY]
    call ScreenFillOffset
SFDOneOffsetReady:
    add ax,[bp+F_XBYTE]
    mov [bp+F_START],ax
    cmp word ptr [bp+B_BYTES],1
    jne SFDSingleBroad
    ; One covered byte in each plane needs neither row nor bank scheduling.
    mov cl,byte ptr [bp+B_FIRST]
    and cl,byte ptr [bp+B_LAST]
    mov ch,cl
    not ch                       ; uncovered bits survive every plane
    xor si,si
    xor di,di
SFDTinyPlane:
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov es,PlaneSelectors[si]
    mov bx,PlaneBases[si]
    add bx,[bp+F_START]
    ; The clipped one-byte row has nonzero coverage. Compile that mask
    ; directly into the coefficients; retain the exact old read-elision.
    and dl,cl
    or dh,ch
    mov al,dl
    or dh,dh
    jz SFDTinyStore
    mov ah,es:[bx]
    and ah,dh
    xor al,ah
SFDTinyStore:
    mov es:[bx],al
    add si,2
    inc di
    cmp si,8
    jb SFDTinyPlane
    ret
SFDSingleBroad:
    ; One row per plane: preserve the existing edge and word writers while
    ; omitting F_ROWS/F_REMAIN/F_PTR/F_PLANE scheduling for nonexistent rows.
    cld
    xor si,si
SFDSinglePlane:
    mov di,si
    shr di,1
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov es,PlaneSelectors[si]
    mov bx,PlaneBases[si]
    add bx,[bp+F_START]
    mov cx,[bp+B_BYTES]
    or dh,dh
    jnz SFDSingleGeneral
    cmp cx,2
    jbe SFDSingleGeneral
    mov di,bx
    cmp byte ptr [bp+B_FIRST],0ffh
    je SFDSingleInterior
    mov al,dl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+B_FIRST]
    xor al,ah
    stosb
    dec cx
SFDSingleInterior:
    cmp byte ptr [bp+B_LAST],0ffh
    je SFDSingleAll
    dec cx
    mov al,dl
    call FillConstantBytes
    mov al,dl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+B_LAST]
    xor al,ah
    stosb
    jmp SFDSingleNext
SFDSingleAll:
    mov al,dl
    call FillConstantBytes
    jmp SFDSingleNext
SFDSingleGeneral:
    ; Preserve the two-byte legacy path and destination-dependent ROPs.
    ; SpanEdgeByte and FillConstantBytes preserve the plane index in SI.
    mov al,byte ptr [bp+B_FIRST]
    call SpanEdgeByte
    inc bx
    sub cx,2
    jcxz SFDSingleLast
    or dh,dh
    jz SFDSingleConstant
    ; Amortize the shared word-loop setup only over wide interiors.
    cmp cx,8
    jb SFDSingleInner
    call FillLogicalBytes
    jmp SFDSingleLast
SFDSingleInner:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    loop SFDSingleInner
    jmp SFDSingleLast
SFDSingleConstant:
    mov di,bx
    mov al,dl
    call FillConstantBytes
    mov bx,di
SFDSingleLast:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
SFDSingleNext:
    add si,2
    cmp si,8
    jae SFDSingleDone
    jmp SFDSinglePlane
SFDSingleDone:
    ret
SFDBroad:
    cld
    ; 0.10: preflight-only scratch is dead after entering drawing.
    ; F_SCAN now holds period; F_ENDY now holds physical stride.
    mov word ptr [bp+F_SCAN],8
    mov word ptr [bp+F_ENDY],160
    cmp word ptr [bp+X_H],4
    ja @F
    jmp SFDPeriodDone
@@:
    test word ptr [bp+X_USE],4
    jnz @F
    jmp SFDPeriodFour
@@:
    cmp word ptr [bp+T_BRUSH+48],4
    jne @F
    jmp SFDPeriodMono
@@:
    mov ax,[bp+T_BRUSH+0]
    cmp ax,[bp+T_BRUSH+4]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+2]
    cmp ax,[bp+T_BRUSH+6]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+8]
    cmp ax,[bp+T_BRUSH+12]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+10]
    cmp ax,[bp+T_BRUSH+14]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+16]
    cmp ax,[bp+T_BRUSH+20]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+18]
    cmp ax,[bp+T_BRUSH+22]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+24]
    cmp ax,[bp+T_BRUSH+28]
    je @F
    jmp SFDPeriodDone
@@:
    mov ax,[bp+T_BRUSH+26]
    cmp ax,[bp+T_BRUSH+30]
    je @F
    jmp SFDPeriodDone
@@:
    jmp SFDPeriodFour
SFDPeriodMono:
    mov ax,[bp+T_BRUSH+32]
    cmp ax,[bp+T_BRUSH+36]
    jne SFDPeriodDone
    mov ax,[bp+T_BRUSH+34]
    cmp ax,[bp+T_BRUSH+38]
    jne SFDPeriodDone
SFDPeriodFour:
    mov word ptr [bp+F_SCAN],4
    mov word ptr [bp+F_ENDY],80
SFDPeriodDone:
    mov word ptr [bp+F_READY],0
    cmp word ptr [bp+X_W],640
    jne SFDFullReady
    cmp word ptr [bp+F_ENDY],80
    jne SFDFullReady
    inc word ptr [bp+F_READY]
SFDFullReady:
    mov word ptr [bp+F_PHASE],0
SFDPhase:
    call ScreenFillPattern
    mov ax,[bp+X_DY]
    add ax,[bp+F_PHASE]
    call ScreenFillOffset
    add ax,[bp+F_XBYTE]
    mov [bp+F_START],ax
    mov ax,[bp+X_H]
    sub ax,[bp+F_PHASE]
    mov dx,[bp+F_SCAN]
    add ax,dx
    dec ax
    shr ax,1
    shr ax,1
    cmp dx,4
    je @F
    shr ax,1
@@:
    mov [bp+F_ROWS],ax
    mov word ptr [bp+F_PLANE],0
SFDPlane:
    mov si,[bp+F_PLANE]
    mov di,si
    shr di,1
    mov dl,byte ptr [bp+B_PAT+di]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov es,PlaneSelectors[si]
    mov bx,PlaneBases[si]
    add bx,[bp+F_START]
    mov [bp+F_PTR],bx
    mov ax,[bp+F_ROWS]
    mov [bp+F_REMAIN],ax
    cmp word ptr [bp+F_READY],0
    je SFDBulkDone
    or dh,dh
    jnz SFDBulkDone
    mov cx,ax
    shl cx,2
    add cx,ax
    shl cx,4
    mov di,bx
    mov al,dl
    call FillConstantBytes
    jmp SFDNextPlane
SFDBulkDone:
    ; Reduce once per plane/phase; the shared writer reuses edge coefficients.
    ; 0.10: avoid multi-row setup for a single physical row.
    cmp ax,1
    jne @F
    jmp SFDRow
@@:
    or dh,dh
    jz @F
    jmp SFDRow
@@:
    cmp word ptr [bp+B_BYTES],4
    jb SFDNarrow
SFDSharedRows:
    mov si,ax
    mov di,bx
    mov al,dl
    mov dl,byte ptr [bp+B_FIRST]
    mov dh,byte ptr [bp+B_LAST]
    mov cx,[bp+B_BYTES]
    mov bx,[bp+F_ENDY]
    call ScreenConstantRows
    jmp SFDNextPlane
SFDNarrow:
    cmp byte ptr [bp+B_FIRST],0ffh
    jne SFDNarrowMasked
    cmp byte ptr [bp+B_LAST],0ffh
    jne SFDNarrowMasked
    mov si,ax
    mov di,bx
    mov bx,[bp+F_ENDY]
    sub bx,[bp+B_BYTES]
    mov al,dl
SFDNarrowRows:
    mov cx,[bp+B_BYTES]
    rep stosb
    add di,bx
    dec si
    jnz SFDNarrowRows
    jmp SFDNextPlane
SFDNarrowMasked:
    cmp word ptr [bp+B_BYTES],1
    jne SFDSharedRows
SFDRow:
    or dh,dh
    jnz SFDLogicalDispatch
    cmp word ptr [bp+B_BYTES],2
    jbe SFDRowGeneral
    mov di,[bp+F_PTR]
    mov cx,[bp+B_BYTES]
    cmp byte ptr [bp+B_FIRST],0ffh
    je SFDOneInterior
    mov al,dl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+B_FIRST]
    xor al,ah
    stosb
    dec cx
SFDOneInterior:
    cmp byte ptr [bp+B_LAST],0ffh
    je SFDOneAll
    dec cx
    mov al,dl
    call FillConstantBytes
    mov al,dl
    mov ah,es:[di]
    xor al,ah
    and al,byte ptr [bp+B_LAST]
    xor al,ah
    stosb
    jmp SFDNextRow
SFDOneAll:
    mov al,dl
    call FillConstantBytes
    jmp SFDNextRow
SFDLogicalDispatch:
    ; Test width once on entering a logical plane/phase. Constants retain
    ; their original dispatch, including phases containing only one row.
    cmp word ptr [bp+B_BYTES],10
    jb SFDRowGeneral
SFDLogicalRows:
    mov bx,[bp+F_PTR]
    mov al,byte ptr [bp+B_FIRST]
    call SpanEdgeByte
    inc bx
    mov cx,[bp+B_BYTES]
    sub cx,2
    call FillLogicalBytes
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    mov bx,[bp+F_ENDY]
    add [bp+F_PTR],bx
    dec word ptr [bp+F_REMAIN]
    jnz SFDLogicalRows
    jmp SFDNextPlane
SFDRowGeneral:
    mov bx,[bp+F_PTR]
    mov cx,[bp+B_BYTES]
    mov al,byte ptr [bp+B_FIRST]
    cmp cx,1
    jne SFDMany
    and al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    jmp SFDScalarNextRow
SFDMany:
    call SpanEdgeByte
    inc bx
    sub cx,2
    jcxz SFDLast
    or dh,dh
    jz SFDConstant
SFDInner:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    loop SFDInner
    jmp SFDLast
SFDConstant:
    mov al,dl
    mov di,bx
    call FillConstantBytes
    mov bx,di
SFDLast:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
SFDScalarNextRow:
    mov bx,[bp+F_ENDY]
    add [bp+F_PTR],bx
    dec word ptr [bp+F_REMAIN]
    jz SFDNextPlane
    jmp SFDRowGeneral
SFDNextRow:
    mov bx,[bp+F_ENDY]
    add [bp+F_PTR],bx
    dec word ptr [bp+F_REMAIN]
    jz @F
    jmp SFDRow
@@:
SFDNextPlane:
    add word ptr [bp+F_PLANE],2
    cmp word ptr [bp+F_PLANE],8
    jae @F
    jmp SFDPlane
@@:
    inc word ptr [bp+F_PHASE]
    mov ax,[bp+F_PHASE]
    cmp ax,[bp+F_SCAN]
    jae SFDDone
    cmp ax,[bp+X_H]
    jae SFDDone
    jmp SFDPhase
SFDDone:
    ret
ScreenFillBand endp

; Destination-dependent full bytes only: ES:BX, CX >= 8, DL constant,
; DH mask. Compute (D AND mask) XOR constant without widening the footprint.
; Return BX advanced, CX zero; AX/flags scratch; preserve DX/SI/DI/BP/DS/ES.
; No string instructions: valid with either incoming direction flag.
public FillLogicalBytes
FillLogicalBytes proc near
    push dx
    push si
    mov al,dl
    mov ah,al
    mov si,ax
    mov dl,dh
    test bx,1
    jz FLBWords
    mov al,es:[bx]
    and al,dl
    xor ax,si
    mov es:[bx],al
    inc bx
    dec cx
FLBWords:
    push cx
    shr cx,1
FLBLoop:
    mov ax,es:[bx]
    and ax,dx
    xor ax,si
    mov es:[bx],ax
    add bx,2
    loop FLBLoop
    pop ax
    test al,1
    jz FLBDone
    mov al,es:[bx]
    and al,dl
    xor ax,si
    mov es:[bx],al
    inc bx
FLBDone:
    pop si
    pop dx
    ret
FillLogicalBytes endp

; AL constant, ES:DI destination, CX positive byte count. Preserve BX/DX/SI.
; Align an odd start with one byte, write words, then the exact trailing byte.
; SHRs carry survives REP STOSW, including a zero word count. No video reads.
public FillConstantBytes
FillConstantBytes proc near
    cld
    test di,1
    jz FCBWords
    stosb
    dec cx
FCBWords:
    mov ah,al
    shr cx,1
    rep stosw
    jnc FCBDone
    stosb
FCBDone:
    ret
FillConstantBytes endp

; Shared screen-only constant rows. Caller proved the complete address span
; and owns cursor exclusion. ES:DI first byte, AL constant, DL/DH first/last
; coverage, CX byte count (>0), SI row count (>0), BX even physical stride.
; Only BP/DS/ES are preserved. Private 8-byte frame; no allocation or service.
; Word alignment and tails are selected ONCE for all rows of this bank/plane.
; Full edges never read destination; partial edges preserve exact neighbours.
public ScreenConstantRows
ScreenConstantRows proc near
    push bp
    mov bp,sp
    sub sp,8
    cld
    mov ah,al
    mov [bp-2],ax
    sub bx,cx
    cmp dx,0ffffh
    jne @F
    jmp SCRAligned
@@:
    ; Reject wide rows in the same two visits as the old single-byte test.
    cmp cx,3
    ja SCRWideMasked
    cmp cx,1
    jne @F
    jmp SCRSingle
@@:
    ; Post-0.14 experiment: narrow constant rows with two partial edges.
    ; Keep full-byte writes free of reads, and preserve exact byte coverage.
    ; AL=00/FF allows each partial edge to use one memory AND/OR.
    cmp dh,0ffh
    je SCRWideMasked
    cmp dl,0ffh
    je SCRWideMasked
    cmp si,2
    jb SCRWideMasked
    or al,al
    jz SCRTinyClear
    cmp al,0ffh
    je @F
    jmp SCRTinyPattern
@@:
    add bx,cx                    ; restore physical stride for indexed rows
    cmp cx,2
    jne @F
    jmp SCRTinySet2
@@:
    jmp SCRTinySet3
SCRTinyClear:
    not dx
    add bx,cx
    cmp cx,2
    jne @F
    jmp SCRTinyClear2
@@:
    jmp SCRTinyClear3
SCRWideMasked:
    cmp si,4
    jb SCRWideGeneric
    cmp dl,0ffh
    je SCRWideGeneric
    cmp dh,0ffh
    je SCRWideGeneric
    or al,al
    jnz @F
    jmp SCRSolidClear
@@:
    cmp al,0ffh
    jne SCRWideGeneric
    jmp SCRSolidSet
SCRWideGeneric:
    sub cx,2
    not dx
    mov ah,dl
    not ah
    and ah,al
    mov byte ptr [bp-6],ah
    mov ah,dh
    not ah
    and ah,al
    mov byte ptr [bp-8],ah
    ; DI advances over the left edge before the interior: odd DI -> even.
    test di,1
    jnz SCRMaskedEven
    jcxz SCRMaskedEven
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SCRMasked11
@@:
    jmp SCRMasked10
SCRMaskedEven:
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SCRMasked01
@@:
    jmp SCRMasked00
SCRMasked00:
    mov al,byte ptr [bp-2]
    or dl,dl
    jz SCRMLeft00
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SCRMLeft00:
    stosb
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SCRMRight00
    mov al,es:[di]
    and al,dh
    xor al,byte ptr [bp-8]
SCRMRight00:
    stosb
    add di,bx
    dec si
    jnz SCRMasked00
    jmp SCRDone
SCRMasked01:
    mov al,byte ptr [bp-2]
    or dl,dl
    jz SCRMLeft01
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SCRMLeft01:
    stosb
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SCRMRight01
    mov ah,es:[di+1]
    and ah,dh
    xor ah,byte ptr [bp-8]
SCRMRight01:
    stosw
    add di,bx
    dec si
    jnz SCRMasked01
    jmp SCRDone
SCRMasked10:
    mov ax,[bp-2]
    or dl,dl
    jz SCRMLeft10
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SCRMLeft10:
    stosw
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SCRMRight10
    mov al,es:[di]
    and al,dh
    xor al,byte ptr [bp-8]
SCRMRight10:
    stosb
    add di,bx
    dec si
    jnz SCRMasked10
    jmp SCRDone
SCRMasked11:
    mov ax,[bp-2]
    or dl,dl
    jz SCRMLeft11
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SCRMLeft11:
    stosw
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SCRMRight11
    mov ah,es:[di+1]
    and ah,dh
    xor ah,byte ptr [bp-8]
SCRMRight11:
    stosw
    add di,bx
    dec si
    jnz SCRMasked11
    jmp SCRDone
SCRAligned:
    test di,1
    jz SCRAlignedEven
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SCRAligned11
@@:
    jmp SCRAligned10
SCRAlignedEven:
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SCRAligned01
@@:
    jmp SCRAligned00
SCRAligned00:
    mov cx,[bp-4]
    rep stosw
    add di,bx
    dec si
    jnz SCRAligned00
    jmp SCRDone
SCRAligned01:
    mov cx,[bp-4]
    rep stosw
    stosb
    add di,bx
    dec si
    jnz SCRAligned01
    jmp SCRDone
SCRAligned10:
    stosb
    mov cx,[bp-4]
    rep stosw
    add di,bx
    dec si
    jnz SCRAligned10
    jmp SCRDone
SCRAligned11:
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    add di,bx
    dec si
    jnz SCRAligned11
    jmp SCRDone
SCRTinyClear2:
    and es:[di],dl
    and es:[di+1],dh
    add di,bx
    dec si
    jnz SCRTinyClear2
    jmp SCRDone
SCRTinyClear3:
    and es:[di],dl
    mov es:[di+1],al
    and es:[di+2],dh
    add di,bx
    dec si
    jnz SCRTinyClear3
    jmp SCRDone
SCRTinySet2:
    or es:[di],dl
    or es:[di+1],dh
    add di,bx
    dec si
    jnz SCRTinySet2
    jmp SCRDone
SCRTinySet3:
    or es:[di],dl
    mov es:[di+1],al
    or es:[di+2],dh
    add di,bx
    dec si
    jnz SCRTinySet3
    jmp SCRDone
SCRTinyPattern:
    ; Both edges partial, two/three bytes, at least two rows.
    ; Precompute covered pattern bits; do not read the full middle byte.
    mov ah,al
    and ah,dl
    mov byte ptr [bp-6],ah
    mov ah,al
    and ah,dh
    mov byte ptr [bp-8],ah
    not dx
    add bx,cx
    cmp cx,2
    jne SCRTinyPattern3
SCRTinyPattern2:
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
    mov es:[di],al
    mov al,es:[di+1]
    and al,dh
    xor al,byte ptr [bp-8]
    mov es:[di+1],al
    add di,bx
    dec si
    jnz SCRTinyPattern2
    jmp SCRDone
SCRTinyPattern3:
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
    mov es:[di],al
    mov al,byte ptr [bp-2]
    mov es:[di+1],al
    mov al,es:[di+2]
    and al,dh
    xor al,byte ptr [bp-8]
    mov es:[di+2],al
    add di,bx
    dec si
    jnz SCRTinyPattern3
    jmp SCRDone
SCRSingle:
    and dl,dh
    mov dh,al
    and dh,dl
    not dl
SCRSingleRow:
    mov al,es:[di]
    and al,dl
    xor al,dh
    stosb
    add di,bx
    dec si
    jnz SCRSingleRow
SCRDone:
    mov sp,bp
    pop bp
    ret

; Both partial edges and constant 00/FF: full interior bytes remain write-only.
SCRSolidClear:
    ; Include the unstored final edge byte in the per-row stride once.
    inc bx
    not dx
    sub cx,2
    test di,1
    jnz SCRSolidClearFront0
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc SCRSolidClear10
    jmp SCRSolidClear11
SCRSolidClearFront0:
    shr cx,1
    mov [bp-4],cx
    jnc SCRSolidClear00
    jmp SCRSolidClear01
SCRSolidClear00:
    and es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    and es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidClear00
    jmp SCRDone
SCRSolidClear01:
    and es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidClear01
    jmp SCRDone
SCRSolidClear10:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    and es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidClear10
    jmp SCRDone
SCRSolidClear11:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidClear11
    jmp SCRDone
SCRSolidSet:
    ; Include the unstored final edge byte in the per-row stride once.
    inc bx
    sub cx,2
    test di,1
    jnz SCRSolidSetFront0
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc SCRSolidSet10
    jmp SCRSolidSet11
SCRSolidSetFront0:
    shr cx,1
    mov [bp-4],cx
    jnc SCRSolidSet00
    jmp SCRSolidSet01
SCRSolidSet00:
    or es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    or es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidSet00
    jmp SCRDone
SCRSolidSet01:
    or es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidSet01
    jmp SCRDone
SCRSolidSet10:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    or es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidSet10
    jmp SCRDone
SCRSolidSet11:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec si
    jnz SCRSolidSet11
    jmp SCRDone
ScreenConstantRows endp

public BltFillGeometry
BltFillGeometry proc near
    mov byte ptr [bp+B_COVER],0ffh
    ; Positive clipped bounds are already proved. Table indices are x mod 8.
    mov bx,[bp+X_DX]
    mov si,bx
    and si,7
    mov al,FillFirstMasks[si]
    mov byte ptr [bp+B_FIRST],al
    mov dx,bx
    add dx,[bp+X_W]
    dec dx
    mov si,dx
    and si,7
    mov al,FillLastMasks[si]
    mov byte ptr [bp+B_LAST],al
    shr bx,3
    mov [bp+F_DST],bx
    shr dx,3
    sub dx,bx
    inc dx
    mov [bp+B_BYTES],dx
    ret
BltFillGeometry endp

; Ordinary memory-bitmap fill. BltPreflight already validated every complete
; row and plane. Map once per pattern phase, reduce once per phase/plane,
; then stream at the proved interleaved stride. No source or alias dependence.
public BltMemoryFillRows
BltMemoryFillRows proc near
    call BltFillGeometry
    mov word ptr [bp+F_SCAN],1
    test word ptr [bp+X_USE],4
    jz BMFPeriodReady
    mov word ptr [bp+F_SCAN],8
BMFPeriodReady:
    mov ax,[bp+X_DDESC+6]
    mul word ptr [bp+X_DP]
    mul word ptr [bp+F_SCAN]
    mov [bp+F_ENDY],ax
    mov word ptr [bp+X_ROW],0
BMFPhase:
    xor ax,ax
    call BltCopyMapRow
    call BltPattern
    mov ax,[bp+X_H]
    sub ax,[bp+X_ROW]
    cmp word ptr [bp+F_SCAN],1
    je BMFCountReady
    add ax,7
    shr ax,3
BMFCountReady:
    mov [bp+F_ROWS],ax
    mov word ptr [bp+F_PLANE],0
BMFPlane:
    mov si,[bp+F_PLANE]
    mov di,si
    shr di,2
    mov bl,byte ptr [bp+B_PAT+di]
    xor dx,dx
    mov ax,[bp+X_ROP]
    call BlitReduce
    mov es,[bp+X_DPTR+si+2]
    mov di,[bp+X_DPTR+si]
    add di,[bp+F_DST]
    mov [bp+F_PTR],di
    or dh,dh
    jnz BMFGeneral
    mov al,dl
    mov dl,byte ptr [bp+B_FIRST]
    mov dh,byte ptr [bp+B_LAST]
    mov cx,[bp+B_BYTES]
    mov si,[bp+F_ROWS]
    mov bx,[bp+F_ENDY]
    call ScreenConstantRows
    jmp BMFNextPlane
BMFGeneral:
    mov ax,[bp+F_ROWS]
    mov [bp+F_REMAIN],ax
BMFRow:
    mov bx,[bp+F_PTR]
    mov cx,[bp+B_BYTES]
    mov al,byte ptr [bp+B_FIRST]
    cmp cx,1
    jne BMFMany
    and al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    jmp BMFNextRow
BMFMany:
    call SpanEdgeByte
    inc bx
    sub cx,2
    jcxz BMFLast
    test bx,1
    jz BMFWide
    call BMFByte
    dec cx
    jz BMFLast
BMFWide:
    cmp cx,2
    jb BMFTail
    mov ax,es:[bx]
    and al,dh
    and ah,dh
    xor al,dl
    xor ah,dl
    mov es:[bx],ax
    add bx,2
    sub cx,2
    jnz BMFWide
    jmp BMFLast
BMFTail:
    call BMFByte
BMFLast:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
BMFNextRow:
    mov ax,[bp+F_ENDY]
    add [bp+F_PTR],ax
    dec word ptr [bp+F_REMAIN]
    jnz BMFRow
BMFNextPlane:
    add word ptr [bp+F_PLANE],4
    mov ax,[bp+X_DP]
    shl ax,2
    cmp [bp+F_PLANE],ax
    jae @F
    jmp BMFPlane
@@:
    inc word ptr [bp+X_ROW]
    mov ax,[bp+X_ROW]
    cmp ax,[bp+F_SCAN]
    jae BMFDone
    cmp ax,[bp+X_H]
    jae @F
    jmp BMFPhase
@@:
BMFDone:
    cld
    clc
    ret
BltMemoryFillRows endp
BMFByte proc near
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    ret
BMFByte endp

; Small/medium fills share coefficients, ES and the row kernel across
; at most 32 logical rows. Single-byte columns at eight rows and above keep
; their dedicated writer. Pattern content must prove constant; D-dependent
; or nonconstant-pattern calls fall back with their original semantics.
public ScreenShortConstant
ScreenShortConstant proc near
    test word ptr [bp+X_USE],1
    jz @F
    jmp SSCNo
@@:
    test word ptr [bp+X_USE],4
    jz SSCReady
    call BltConstantPattern
    jnc @F
    jmp SSCNo
@@:
SSCReady:
    mov word ptr [bp+F_PHASE],0
    call ScreenFillPattern
    mov ax,[bp+X_DY]
    and ax,3
    mov bx,4
    sub bx,ax
    cmp bx,[bp+X_H]
    jbe SSCFirstCount
    mov bx,[bp+X_H]
SSCFirstCount:
    mov [bp+F_ROWS],bx
    mov ax,[bp+X_H]
    sub ax,bx
    mov [bp+F_REMAIN],ax
    mov ax,[bp+X_DY]
    call ScreenFillOffset
    add ax,[bp+F_XBYTE]
    mov [bp+F_START],ax
SSCPlanes:
    mov word ptr [bp+F_PLANE],0
SSCPlane:
    mov di,[bp+F_PLANE]
    mov si,di
    shr si,1
    mov al,byte ptr [bp+B_PAT+si]
    and al,byte ptr [bp+F_ROPMASK]
    xor al,byte ptr [bp+F_ROPCONST]
    mov byte ptr [bp+F_RUN],al
    mov es,PlaneSelectors[di]
    mov di,PlaneBases[di]
    add di,[bp+F_START]
    mov dl,byte ptr [bp+B_FIRST]
    mov dh,byte ptr [bp+B_LAST]
    mov cx,[bp+B_BYTES]
    mov bx,2000h
    cmp word ptr [bp+F_REMAIN],0
    jne SSCWrapped
    mov si,[bp+F_ROWS]
    call ScreenConstantRows
    jmp SSCNext
SSCWrapped:
    mov si,[bp+X_H]
    push word ptr [bp+X_DY]
    call ScreenShortRows
SSCNext:
    add word ptr [bp+F_PLANE],2
    cmp word ptr [bp+F_PLANE],8
    jae @F
    jmp SSCPlane
@@:
    clc
    ret
SSCNo:
    stc
    ret
ScreenShortConstant endp

; Select the constant row kernel once across the admitted bank fragments.
public ScreenShortRows
ScreenShortRows proc near
    push bp
    mov bp,sp
    sub sp,10
    push ax
    mov ax,[bp+4]                 ; first logical Y, passed by the caller
    and ax,3
    neg ax
    add ax,4
    mov [bp-10],ax
    pop ax
    cld
    mov ah,al
    mov [bp-2],ax
    sub bx,cx
    cmp dx,0ffffh
    jne @F
    jmp SSRAligned
@@:
    ; Reject wide rows in the same two visits as the old single-byte test.
    cmp cx,3
    ja SSRWideMasked
    cmp cx,1
    jne @F
    jmp SSRSingle
@@:
    ; Post-0.14 experiment: narrow constant rows with two partial edges.
    ; Keep full-byte writes free of reads, and preserve exact byte coverage.
    ; AL=00/FF allows each partial edge to use one memory AND/OR.
    cmp dh,0ffh
    je SSRWideMasked
    cmp dl,0ffh
    je SSRWideMasked
    cmp si,2
    jb SSRWideMasked
    or al,al
    jz SSRTinyClear
    cmp al,0ffh
    je @F
    jmp SSRTinyPattern
@@:
    add bx,cx                    ; restore physical stride for indexed rows
    cmp cx,2
    jne @F
    jmp SSRTinySet2
@@:
    jmp SSRTinySet3
SSRTinyClear:
    not dx
    add bx,cx
    cmp cx,2
    jne @F
    jmp SSRTinyClear2
@@:
    jmp SSRTinyClear3
SSRWideMasked:
    cmp si,4
    jb SSRWideGeneric
    cmp dl,0ffh
    je SSRWideGeneric
    cmp dh,0ffh
    je SSRWideGeneric
    or al,al
    jnz @F
    jmp SSRSolidClear
@@:
    cmp al,0ffh
    jne SSRWideGeneric
    jmp SSRSolidSet
SSRWideGeneric:
    sub cx,2
    not dx
    mov ah,dl
    not ah
    and ah,al
    mov byte ptr [bp-6],ah
    mov ah,dh
    not ah
    and ah,al
    mov byte ptr [bp-8],ah
    ; DI advances over the left edge before the interior: odd DI -> even.
    test di,1
    jnz SSRMaskedEven
    jcxz SSRMaskedEven
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SSRMasked11
@@:
    jmp SSRMasked10
SSRMaskedEven:
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SSRMasked01
@@:
    jmp SSRMasked00
SSRMasked00:
    mov al,byte ptr [bp-2]
    or dl,dl
    jz SSRMLeft00
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SSRMLeft00:
    stosb
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SSRMRight00
    mov al,es:[di]
    and al,dh
    xor al,byte ptr [bp-8]
SSRMRight00:
    stosb
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRMasked00
    jmp SSRDone
SSRMasked01:
    mov al,byte ptr [bp-2]
    or dl,dl
    jz SSRMLeft01
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SSRMLeft01:
    stosb
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SSRMRight01
    mov ah,es:[di+1]
    and ah,dh
    xor ah,byte ptr [bp-8]
SSRMRight01:
    stosw
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRMasked01
    jmp SSRDone
SSRMasked10:
    mov ax,[bp-2]
    or dl,dl
    jz SSRMLeft10
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SSRMLeft10:
    stosw
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SSRMRight10
    mov al,es:[di]
    and al,dh
    xor al,byte ptr [bp-8]
SSRMRight10:
    stosb
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRMasked10
    jmp SSRDone
SSRMasked11:
    mov ax,[bp-2]
    or dl,dl
    jz SSRMLeft11
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
SSRMLeft11:
    stosw
    mov ax,[bp-2]
    mov cx,[bp-4]
    rep stosw
    or dh,dh
    jz SSRMRight11
    mov ah,es:[di+1]
    and ah,dh
    xor ah,byte ptr [bp-8]
SSRMRight11:
    stosw
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRMasked11
    jmp SSRDone
SSRAligned:
    test di,1
    jz SSRAlignedEven
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SSRAligned11
@@:
    jmp SSRAligned10
SSRAlignedEven:
    shr cx,1
    mov [bp-4],cx
    jnc @F
    jmp SSRAligned01
@@:
    jmp SSRAligned00
SSRAligned00:
    mov cx,[bp-4]
    rep stosw
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRAligned00
    jmp SSRDone
SSRAligned01:
    mov cx,[bp-4]
    rep stosw
    stosb
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRAligned01
    jmp SSRDone
SSRAligned10:
    stosb
    mov cx,[bp-4]
    rep stosw
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRAligned10
    jmp SSRDone
SSRAligned11:
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRAligned11
    jmp SSRDone
SSRTinyClear2:
    and es:[di],dl
    and es:[di+1],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinyClear2
    jmp SSRDone
SSRTinyClear3:
    and es:[di],dl
    mov es:[di+1],al
    and es:[di+2],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinyClear3
    jmp SSRDone
SSRTinySet2:
    or es:[di],dl
    or es:[di+1],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinySet2
    jmp SSRDone
SSRTinySet3:
    or es:[di],dl
    mov es:[di+1],al
    or es:[di+2],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinySet3
    jmp SSRDone
SSRTinyPattern:
    ; Both edges partial, two/three bytes, at least two rows.
    ; Precompute covered pattern bits; do not read the full middle byte.
    mov ah,al
    and ah,dl
    mov byte ptr [bp-6],ah
    mov ah,al
    and ah,dh
    mov byte ptr [bp-8],ah
    not dx
    add bx,cx
    cmp cx,2
    jne SSRTinyPattern3
SSRTinyPattern2:
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
    mov es:[di],al
    mov al,es:[di+1]
    and al,dh
    xor al,byte ptr [bp-8]
    mov es:[di+1],al
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinyPattern2
    jmp SSRDone
SSRTinyPattern3:
    mov al,es:[di]
    and al,dl
    xor al,byte ptr [bp-6]
    mov es:[di],al
    mov al,byte ptr [bp-2]
    mov es:[di+1],al
    mov al,es:[di+2]
    and al,dh
    xor al,byte ptr [bp-8]
    mov es:[di+2],al
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRTinyPattern3
    jmp SSRDone
SSRSingle:
    and dl,dh
    mov dh,al
    and dh,dl
    not dl
SSRSingleRow:
    mov al,es:[di]
    and al,dl
    xor al,dh
    stosb
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h                 ; 2000h + 8050h = next logical bank-zero row
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSingleRow
SSRDone:
    mov sp,bp
    pop bp
    ret 2

; Both partial edges and constant 00/FF: full interior bytes remain write-only.
SSRSolidClear:
    ; Include the unstored final edge byte in the per-row stride once.
    inc bx
    not dx
    sub cx,2
    test di,1
    jnz SSRSolidClearFront0
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc SSRSolidClear10
    jmp SSRSolidClear11
SSRSolidClearFront0:
    shr cx,1
    mov [bp-4],cx
    jnc SSRSolidClear00
    jmp SSRSolidClear01
SSRSolidClear00:
    and es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    and es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidClear00
    jmp SSRDone
SSRSolidClear01:
    and es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidClear01
    jmp SSRDone
SSRSolidClear10:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    and es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidClear10
    jmp SSRDone
SSRSolidClear11:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidClear11
    jmp SSRDone
SSRSolidSet:
    ; Include the unstored final edge byte in the per-row stride once.
    inc bx
    sub cx,2
    test di,1
    jnz SSRSolidSetFront0
    dec cx
    shr cx,1
    mov [bp-4],cx
    jnc SSRSolidSet10
    jmp SSRSolidSet11
SSRSolidSetFront0:
    shr cx,1
    mov [bp-4],cx
    jnc SSRSolidSet00
    jmp SSRSolidSet01
SSRSolidSet00:
    or es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    or es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidSet00
    jmp SSRDone
SSRSolidSet01:
    or es:[di],dl
    inc di
    mov cx,[bp-4]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidSet01
    jmp SSRDone
SSRSolidSet10:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    or es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidSet10
    jmp SSRDone
SSRSolidSet11:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp-4]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec word ptr [bp-10]
    jnz @F
    add di,8050h
    mov word ptr [bp-10],4
@@:
    dec si
    jnz SSRSolidSet11
    jmp SSRDone
ScreenShortRows endp

; Source-free area fills: finish at most four consecutive logical rows in
; every plane, then advance. Constant coefficients/pattern are proved once.
public ScreenOrderedConstant
ScreenOrderedConstant proc near
    test word ptr [bp+X_USE],4
    jz SOCReady
    call BltConstantPattern
    jnc SOCReady
    stc
    ret
SOCReady:
    cld
    mov word ptr [bp+X_SDESC+16],0
    mov word ptr [bp+X_SDESC+18],0
    cmp word ptr [bp+B_BYTES],4
    jb SOCGeometryDone
    cmp byte ptr [bp+B_FIRST],0ffh
    je SOCGeometryDone
    cmp byte ptr [bp+B_LAST],0ffh
    je SOCGeometryDone
    mov cx,[bp+B_BYTES]
    mov ax,2001h
    sub ax,cx
    mov [bp+F_RUN],ax
    sub cx,2
    test word ptr [bp+F_XBYTE],1
    jnz SOCFrontZero
    dec cx
    mov ax,offset SOClear10
    mov dx,offset SOSet10
    test cl,1
    jz SOCGeometryReady
    mov ax,offset SOClear11
    mov dx,offset SOSet11
    jmp SOCGeometryReady
SOCFrontZero:
    mov ax,offset SOClear00
    mov dx,offset SOSet00
    test cl,1
    jz SOCGeometryReady
    mov ax,offset SOClear01
    mov dx,offset SOSet01
SOCGeometryReady:
    mov [bp+X_SDESC+16],ax
    mov [bp+X_SDESC+18],dx
    shr cx,1
    mov [bp+F_DST],cx
SOCGeometryDone:
    mov word ptr [bp+F_PHASE],0
    call ScreenFillPattern
    ; The source descriptor is dead on this source-free screen path.
    ; Four DX coefficient pairs are invocation-local, never cached globally.
    xor si,si
    xor di,di
SOCPattern:
    mov dl,byte ptr [bp+B_PAT+si]
    mov dh,dl
    and dx,[bp+F_ROPMASK]
    xor dx,[bp+F_ROPCONST]
    mov [bp+di+X_SDESC],dx
    xor ax,ax
    or dh,dh
    jnz SOCPointerReady
    or dl,dl
    jz SOCClearPointer
    cmp dl,0ffh
    jne SOCPointerReady
    mov ax,[bp+X_SDESC+18]
    jmp SOCPointerReady
SOCClearPointer:
    mov ax,[bp+X_SDESC+16]
SOCPointerReady:
    mov [bp+di+X_SDESC+8],ax
    inc si
    add di,2
    cmp si,4
    jb SOCPattern
    mov ax,[bp+X_DY]
    call ScreenFillOffset
    add ax,[bp+F_XBYTE]
    mov [bp+F_START],ax
    mov ax,[bp+X_H]
    mov [bp+F_REMAIN],ax
    ; H > 4 proves the first bank-boundary group fits. All later groups
    ; advance 80 bytes; only the initial partial group needs bank arithmetic.
    mov ax,[bp+X_DY]
    and ax,3
    neg ax
    add ax,4
    mov [bp+F_ROWS],ax
    mov cl,13
    shl ax,cl
    add ax,8050h
    mov [bp+F_ENDY],ax
SOCBand:
    mov word ptr [bp+F_PLANE],0
SOCPlane:
    mov si,[bp+F_PLANE]
    mov dx,[bp+si+X_SDESC]
    mov es,PlaneSelectors[si]
    mov di,PlaneBases[si]
    add di,[bp+F_START]
    mov ax,[bp+si+X_SDESC+8]
    mov si,[bp+F_ROWS]
    or ax,ax
    jz SOCLegacy
    mov [bp+F_READY],ax
    mov al,dl
    mov ah,al
    mov dl,byte ptr [bp+B_FIRST]
    mov dh,byte ptr [bp+B_LAST]
    or al,al
    jnz SOCSolidMasks
    not dx
SOCSolidMasks:
    mov bx,[bp+F_RUN]
    call word ptr [bp+F_READY]
    jmp SOCNextPlane
SOCLegacy:
    or dh,dh
    jnz SOCLogical
    mov al,dl
    mov dl,byte ptr [bp+B_FIRST]
    mov dh,byte ptr [bp+B_LAST]
    mov cx,[bp+B_BYTES]
    mov bx,2000h
    call ScreenConstantRows
    jmp SOCNextPlane
SOCLogical:
    call ScreenOrderedLogicalRows
SOCNextPlane:
    add word ptr [bp+F_PLANE],2
    cmp word ptr [bp+F_PLANE],8
    jb SOCPlane
    mov ax,[bp+F_ROWS]
    sub [bp+F_REMAIN],ax
    jz SOCFinished
    mov ax,[bp+F_ENDY]
    add [bp+F_START],ax
    mov word ptr [bp+F_ENDY],80
    mov ax,4
    cmp ax,[bp+F_REMAIN]
    jbe SOCCount
    mov ax,[bp+F_REMAIN]
SOCCount:
    mov [bp+F_ROWS],ax
    jmp SOCBand
SOCFinished:
    clc
    ret
ScreenOrderedConstant endp

; Uniform logical coefficients DX, ES:DI first row, SI 1..4 rows.
; All rows and edge neighbors were proved before drawing. This helper uses
; the same exact edge/word operations as SFDBroad, with 2000h bank stride.
ScreenOrderedLogicalRows proc near
SOLRow:
    push di
    mov bx,di
    mov al,byte ptr [bp+B_FIRST]
    call SpanEdgeByte
    inc bx
    mov cx,[bp+B_BYTES]
    sub cx,2
    jcxz SOLLast
    cmp cx,8
    jb SOLBytes
    call FillLogicalBytes
    jmp SOLLast
SOLBytes:
    mov al,es:[bx]
    and al,dh
    xor al,dl
    mov es:[bx],al
    inc bx
    loop SOLBytes
SOLLast:
    mov al,byte ptr [bp+B_LAST]
    call SpanEdgeByte
    pop di
    add di,2000h
    dec si
    jnz SOLRow
    ret
ScreenOrderedLogicalRows endp

SOClear00:
    and es:[di],dl
    inc di
    mov cx,[bp+F_DST]
    rep stosw
    and es:[di],dh
    add di,bx
    dec si
    jnz SOClear00
    ret

SOClear01:
    and es:[di],dl
    inc di
    mov cx,[bp+F_DST]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec si
    jnz SOClear01
    ret

SOClear10:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp+F_DST]
    rep stosw
    and es:[di],dh
    add di,bx
    dec si
    jnz SOClear10
    ret

SOClear11:
    and es:[di],dl
    inc di
    stosb
    mov cx,[bp+F_DST]
    rep stosw
    stosb
    and es:[di],dh
    add di,bx
    dec si
    jnz SOClear11
    ret

SOSet00:
    or es:[di],dl
    inc di
    mov cx,[bp+F_DST]
    rep stosw
    or es:[di],dh
    add di,bx
    dec si
    jnz SOSet00
    ret

SOSet01:
    or es:[di],dl
    inc di
    mov cx,[bp+F_DST]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec si
    jnz SOSet01
    ret

SOSet10:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp+F_DST]
    rep stosw
    or es:[di],dh
    add di,bx
    dec si
    jnz SOSet10
    ret

SOSet11:
    or es:[di],dl
    inc di
    stosb
    mov cx,[bp+F_DST]
    rep stosw
    stosb
    or es:[di],dh
    add di,bx
    dec si
    jnz SOSet11
    ret
