; 0.16 transient planar-byte caches. Preflight interval scratch is dead during
; conversion; no allocation, global cache, extra public frame or selector is kept.
DG_X equ D_CURSTART
DG_Y equ D_CURSTART+2
DG_BITS equ D_CUREND
DG_COVER equ D_OTHERSTART+2
DG_VALID equ D_OTHERSTART
public DibGetPackedIndex, DibRleAccumulate, DibRleFlush
DibGetPackedIndex proc near
    push si
    push di
    call DibLocate
    DNEED nc,DGPOutside
    mov ax,[bp+S_X]
    shr ax,3
    cmp word ptr [bp+DG_VALID],0
    je DGPLoad
    cmp ax,[bp+DG_X]
    jne DGPLoad
    mov dx,[bp+S_Y]
    cmp dx,[bp+DG_Y]
    je DGPExtract
DGPLoad:
    mov [bp+DG_X],ax
    mov ax,[bp+S_Y]
    mov [bp+DG_Y],ax
    call DibPoint
    jc DGPBad
    xor si,si
    xor di,di
    mov cx,[bp+S_PLANES]
DGPLoadPlane:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov al,es:[bx]
    mov [bp+DG_BITS+di],al
    inc di
    add si,4
    loop DGPLoadPlane
    mov word ptr [bp+DG_VALID],1
DGPExtract:
    mov cx,[bp+S_X]
    and cx,7
    mov dl,80h
    shr dl,cl
    xor ax,ax
    mov dh,1
    xor si,si
    mov cx,[bp+S_PLANES]
DGPBit:
    test byte ptr [bp+DG_BITS+si],dl
    jz DGPZero
    or al,dh
DGPZero:
    shl dh,1
    inc si
    loop DGPBit
    cmp word ptr [bp+S_PLANES],1
    je DGPDone
    cmp word ptr [bp+D_BPP],1
    jne DGPDone
    call DibIndex
    mov al,dh
    shr al,4
    and ax,1
    jmp DGPDone
DGPOutside:
    xor ax,ax
DGPDone:
    pop di
    pop si
    clc
    ret
DGPBad:
    pop di
    pop si
    stc
    ret
DibGetPackedIndex endp

; AL colour/mono metadata from the validated RLE palette cache.
DibRleAccumulate proc near
    push si
    push di
    mov [bp+D_PIXEL],ax
    call DibLocate
    DNEED nc,DRADone
    mov ax,[bp+S_X]
    shr ax,3
    cmp word ptr [bp+DG_VALID],0
    je DRANew
    cmp ax,[bp+DG_X]
    jne DRAFlush
    mov dx,[bp+S_Y]
    cmp dx,[bp+DG_Y]
    je DRAPack
DRAFlush:
    call DibRleFlush
    mov ax,[bp+S_X]
    shr ax,3
DRANew:
    mov [bp+DG_X],ax
    mov ax,[bp+S_Y]
    mov [bp+DG_Y],ax
    call DibPoint
    jc DRABad
    mov word ptr [bp+DG_BITS],0
    mov word ptr [bp+DG_BITS+2],0
    mov word ptr [bp+DG_COVER],0
    mov word ptr [bp+DG_VALID],1
DRAPack:
    mov cx,[bp+S_X]
    and cx,7
    mov dl,80h
    shr dl,cl
    or byte ptr [bp+DG_COVER],dl
    mov dh,dl
    not dh
    mov ax,[bp+D_PIXEL]
    cmp word ptr [bp+S_PLANES],1
    jne DRAIndex
    shr al,4
DRAIndex:
    xor si,si
    mov cx,[bp+S_PLANES]
DRAPlane:
    and byte ptr [bp+DG_BITS+si],dh
    shr al,1
    jnc DRAZero
    or byte ptr [bp+DG_BITS+si],dl
DRAZero:
    inc si
    loop DRAPlane
DRADone:
    pop di
    pop si
    clc
    ret
DRABad:
    pop di
    pop si
    stc
    ret
DibRleAccumulate endp

; Only recorded coverage is touched. DELTA holes and clipped pixels stay intact.
DibRleFlush proc near
    cmp word ptr [bp+DG_VALID],0
    je DRFDone16
    xor si,si
    xor di,di
    mov cx,[bp+S_PLANES]
    mov dl,byte ptr [bp+DG_COVER]
    not dl
DRFPlane16:
    mov bx,[bp+S_ADDR+si]
    mov es,[bp+S_ADDR+si+2]
    mov al,byte ptr [bp+DG_BITS+di]
    or dl,dl
    jz DRFStore16
    mov ah,es:[bx]
    and ah,dl
    or al,ah
DRFStore16:
    mov es:[bx],al
    add si,4
    inc di
    loop DRFPlane16
    mov word ptr [bp+DG_VALID],0
DRFDone16:
    ret
DibRleFlush endp

; Fast write within an already-proved chunk; fallback establishes each new
; selector/budget span. End/wrap cannot bypass the original stream mapping.
DibFastWrite proc near
    cmp word ptr [bp+ST_WRITE],1
    jne DFWSlow
    cmp word ptr [bp+ST_VALID],0
    je DFWSlow
    cmp word ptr [bp+ST_WRAP],0
    jne DFWSlow
    mov bx,[bp+ST_OFF]
    cmp bx,[bp+ST_END]
    ja DFWSlow
    mov es,[bp+ST_SEL]
    mov es:[bx],al
    call DibStreamAdvance
    clc
    ret
DFWSlow:
    push cs
    call near ptr DibStreamWrite
    ret
DibFastWrite endp
