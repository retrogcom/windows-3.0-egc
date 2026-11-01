; A bounded cache of complete 32x32 ordinary mono bitmaps. No GDI
; allocation handles are retained or assumed immutable. Every hit compares
; ALL live source bytes, after ordinary preflight, before using shifted data.
; Caller/DGROUP/VRAM aliasing and reentry decline before changing the source.
; Cursor never accesses this cache. BC_HELD retains exclusive ownership
; until BitBlt releases, so nested calls cannot replace a borrowed entry.
public BitCacheTry, BitCacheHit, BitCacheMiss
BitCacheTry proc near
    mov ax,[bp+X_USE]
    and ax,6
    cmp ax,2
    je @F
    jmp BCTNo
@@:
    cmp word ptr [bp+X_DK],1
    je @F
    jmp BCTNo
@@:
    cmp word ptr [bp+X_SK],0
    je @F
    jmp BCTNo
@@:
    cmp word ptr [bp+X_MODE],0
    je @F
    jmp BCTNo
@@:
    ; 0.14: direct colour rows now beat cache hits on the common ROPs.
    ; Colour entries formerly populated only on SRCCOPY; stop admitting
    ; those costly cold copies. Retain live-content caching for mono work.
    cmp word ptr [bp+X_SP],1
    je BCTMonoPolicy
    ret
BCTMonoPolicy:
    mov ax,[bp+X_ROP]
    cmp ax,0cch
    je BCTDirectColours
    cmp ax,088h
    je BCTDirectColours
    cmp ax,066h
    jne BCTGeometry
BCTDirectColours:
    ; Raw zero -> foreground, raw one -> background. Only the measured
    ; identity black/white expansion bypasses all cache lookup/preparation.
    mov al,byte ptr [bp+T_DM+11]
    and al,15
    jnz BCTGeometry
    mov al,byte ptr [bp+T_DM+7]
    and al,15
    cmp al,15
    jne BCTGeometry
    ret
BCTGeometry:
    cmp word ptr [bp+X_W],32
    jne BCTNo
    cmp word ptr [bp+X_H],32
    jne BCTNo
    cmp word ptr [bp+X_SX],0
    jne BCTNo
    cmp word ptr [bp+X_SY],0
    jne BCTNo
    cmp word ptr [bp+X_SDESC+2],32
    jne BCTNo
    cmp word ptr [bp+X_SDESC+4],32
    jne BCTNo
    cmp word ptr [bp+X_SDESC+6],4
    jne BCTNo
    cmp word ptr [bp+X_SDESC+22],0
    jne BCTNo
    mov ax,[bp+X_DX]
    and ax,15
    test ax,7
    jz BCTNo
    mov [bp+BC_PHASE],ax
    ; A memory view of video memory needs the ordinary alias/cursor path.
    cmp word ptr [bp+X_SMIN+2],000ch
    jae BCTPrivateCheck
    cmp word ptr [bp+X_SMAX+2],000ah
    jae BCTNo
BCTPrivateCheck:
    mov ax,ds
    xor bx,bx
    mov cx,DataOFFSET DriverDataEnd
    xor di,di
    call BlitNormalize
    jc BCTNo
    cmp [bp+X_SMAX+2],dx
    jb BCTClaim
    ja BCTPrivateEnd
    cmp [bp+X_SMAX],ax
    jb BCTClaim
BCTPrivateEnd:
    add ax,DataOFFSET DriverDataEnd-1
    adc dx,0
    cmp [bp+X_SMIN+2],dx
    ja BCTClaim
    jb BCTNo
    cmp [bp+X_SMIN],ax
    ja BCTClaim
BCTNo:
    ret
BCTClaim:
    pushf
    cli
    cmp BitCacheBusy,0
    jne BCTBusy
    mov BitCacheBusy,1
    mov word ptr [bp+BC_HELD],1
    popf
    cld
    mov ax,[bp+X_SP]
    mov cl,7
    shl ax,cl
    mov [bp+BC_BYTES],ax
    mov bx,DataOFFSET BitCache
    call BCTMatch
    jnc BitCacheHit
    mov bx,DataOFFSET BitCache+1312
    call BCTMatch
    jnc BitCacheHit
    jmp BitCacheMiss
BCTBusy:
    popf
    ret
BitCacheHit:
    PERFINC PerfCacheHit
    mov [bp+BC_ENTRY],bx
    jmp BCTUse
BitCacheMiss:
    PERFINC PerfCacheMiss
    mov bx,BitCacheNext
    xor BitCacheNext,1312
    add bx,DataOFFSET BitCache
    mov [bp+BC_ENTRY],bx
    mov word ptr [bx],0
    mov ax,[bp+X_SDESC+12]
    mov [bx+2],ax
    mov ax,[bp+X_SDESC+10]
    mov [bx+4],ax
    mov ax,[bp+BC_PHASE]
    mov [bx+6],ax
    shr ax,3
    add ax,bx
    add ax,528
    mov [bx+10],ax
    mov ax,[bp+X_SP]
    mov [bx+8],ax
    push ds
    pop es
    lea di,[bx+16]
    mov si,[bp+X_SDESC+10]
    mov ax,[bp+X_SDESC+12]
    mov cx,[bp+BC_BYTES]
    shr cx,1
    push ds
    mov ds,ax
    rep movsw
    pop ds
    mov bx,[bp+BC_ENTRY]
    lea si,[bx+16]
    mov di,[bx+10]
    mov ax,[bp+X_SP]
    shl ax,5
    mov cx,[bp+BC_PHASE]
    and cx,7
BCTShiftRow:
    push ax
    xor dx,dx
    mov bx,4
BCTShiftByte:
    lodsb
    mov ah,dl
    mov dl,al
    shr ax,cl
    stosb
    dec bx
    jnz BCTShiftByte
    xor al,al
    mov ah,dl
    shr ax,cl
    stosb
    xor al,al
    stosb
    pop ax
    dec ax
    jnz BCTShiftRow
    mov bx,[bp+BC_ENTRY]
    mov word ptr [bx],1
BCTUse:
    mov ax,[bx+10]
    mov [bp+X_SDESC+10],ax
    mov ax,ds
    mov [bp+X_SDESC+12],ax
    mov word ptr [bp+X_SDESC+2],48
    mov word ptr [bp+X_SDESC+6],6
    mov ax,[bp+BC_PHASE]
    and ax,7
    mov [bp+X_SX],ax
    ret
BitCacheTry endp

; BX entry; CF clear only after the current source is byte-identical.
; ES and volatile registers may change; DS/BX/frame are preserved.
BCTMatch proc near
    cmp word ptr [bx],1
    jne BCTMismatch
    mov ax,[bp+X_SDESC+12]
    cmp [bx+2],ax
    jne BCTMismatch
    mov ax,[bp+X_SDESC+10]
    cmp [bx+4],ax
    jne BCTMismatch
    mov ax,[bp+BC_PHASE]
    cmp [bx+6],ax
    jne BCTMismatch
    mov ax,[bp+X_SP]
    cmp [bx+8],ax
    jne BCTMismatch
    push ds
    pop es
    lea di,[bx+16]
    mov si,[bp+X_SDESC+10]
    mov ax,[bp+X_SDESC+12]
    mov cx,[bp+BC_BYTES]
    shr cx,1
    push ds
    mov ds,ax
    repe cmpsw
    pop ds
    jne BCTMismatch
    clc
    ret
BCTMismatch:
    stc
    ret
BCTMatch endp
