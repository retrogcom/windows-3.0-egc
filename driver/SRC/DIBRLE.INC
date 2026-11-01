; Bounded RLE4/RLE8 pixel iterator. Reset requires an already-open read stream.
; Each Next returns CF=0, AX=index, BX=x, DX=bottom-up band row; AX=FFFF means
; EOB. CF=1 is malformed/truncated input. No surface is read or written here.
; Preflight must consume to EOB before replay starts any destination writes.
D_WIDTH equ -98
D_ROWS equ -100
D_BPP equ -102
D_USED equ -104
D_RX equ -106
D_RY equ -108
D_RLEFT equ -110
D_RVALUE equ -112
D_RMODE equ -114
D_RPAD equ -116
D_RPHASE equ -118
D_RSTATE equ -120
D_RCOUNT equ -122
D_RDX equ -124
public DibRleReset,DibRleNext
DREAD macro
 push cs
 call near ptr DibStreamRead
endm
DibRleReset proc far
 mov word ptr [bp+D_RSTATE],2
 cmp word ptr [bp+ST_WRITE],0
 DNEED e,DRResetBad
 cmp word ptr [bp+D_WIDTH],0
 DNEED g,DRResetBad
 cmp word ptr [bp+D_ROWS],0
 DNEED g,DRResetBad
 mov ax,[bp+D_BPP]
 cmp ax,4
 je DRReset4
 cmp ax,8
 DNEED e,DRResetBad
 mov ax,256
 jmp DRResetUsed
DRReset4:
 mov ax,16
DRResetUsed:
 cmp word ptr [bp+D_USED],0
 DNEED ne,DRResetBad
 cmp [bp+D_USED],ax
 DNEED be,DRResetBad
 mov word ptr [bp+D_RSTATE],0
 mov word ptr [bp+D_RX],0
 mov word ptr [bp+D_RY],0
 mov word ptr [bp+D_RLEFT],0
 mov word ptr [bp+D_RPAD],0
 clc
 retf
DRResetBad:
 stc
 retf
DibRleReset endp

DibRleNext proc far
 cmp word ptr [bp+D_RSTATE],0
 je DRActive
 cmp word ptr [bp+D_RSTATE],1
 DNEED e,DRError
 jmp DREnd
DRActive:
 cmp word ptr [bp+D_RLEFT],0
 DNEED e,DRPixel
 cmp word ptr [bp+D_RPAD],0
 je DRPacket
 DREAD
 DNEED nc,DRError
 mov word ptr [bp+D_RPAD],0
DRPacket:
 DREAD
 DNEED nc,DRError
 mov [bp+D_RCOUNT],ax
 DREAD
 DNEED nc,DRError
 mov [bp+D_RVALUE],ax
 ; Past the final EOL, only immediate EOB is legal.
 mov ax,[bp+D_RY]
 cmp ax,[bp+D_ROWS]
 jb DRDispatch
 cmp word ptr [bp+D_RCOUNT],0
 DNEED e,DRError
 cmp word ptr [bp+D_RVALUE],1
 DNEED e,DRError
 jmp DREnd
DRDispatch:
 mov ax,[bp+D_RCOUNT]
 or ax,ax
 jz DREscape
 mov word ptr [bp+D_RMODE],0
 jmp DRRun
DREscape:
 mov ax,[bp+D_RVALUE]
 or ax,ax
 jnz DRNotEOL
 mov word ptr [bp+D_RX],0
 inc word ptr [bp+D_RY]
 jmp DRPacket
DRNotEOL:
 cmp ax,1
 DNEED ne,DREnd
 cmp ax,2
 jne DRAbsolute
 DREAD
 DNEED nc,DRError
 mov [bp+D_RDX],ax
 DREAD
 DNEED nc,DRError
 ; A zero delta is a valid no-op: the bounded stream consumed four bytes.
 add ax,[bp+D_RY]
 cmp ax,[bp+D_ROWS]
 DNEED b,DRError
 mov dx,ax
 mov ax,[bp+D_RX]
 add ax,[bp+D_RDX]
 cmp ax,[bp+D_WIDTH]
 DNEED be,DRError
 mov [bp+D_RX],ax
 mov [bp+D_RY],dx
 jmp DRPacket
DRAbsolute:
 mov word ptr [bp+D_RMODE],1
 mov bx,ax
 cmp word ptr [bp+D_BPP],8
 je DRAbsPad
 inc bx
 shr bx,1
DRAbsPad:
 and bx,1
 mov [bp+D_RPAD],bx
DRRun:
 mov [bp+D_RLEFT],ax
 mov word ptr [bp+D_RPHASE],0
 add ax,[bp+D_RX]
 cmp ax,[bp+D_WIDTH]
 DNEED be,DRError
DRPixel:
 cmp word ptr [bp+D_RMODE],0
 je DREncoded
 cmp word ptr [bp+D_BPP],8
 je DRLiteralByte
 cmp word ptr [bp+D_RPHASE],0
 jne DRPacked
DRLiteralByte:
 DREAD
 DNEED nc,DRError
 mov [bp+D_RVALUE],ax
DREncoded:
 cmp word ptr [bp+D_BPP],8
 je DRByte
DRPacked:
 mov ax,[bp+D_RVALUE]
 cmp word ptr [bp+D_RPHASE],0
 jne DRLow
 shr ax,4
DRLow:
 and ax,15
 xor word ptr [bp+D_RPHASE],1
 jmp DRIndex
DRByte:
 mov ax,[bp+D_RVALUE]
DRIndex:
 cmp ax,[bp+D_USED]
 DNEED b,DRError
 mov bx,[bp+D_RX]
 mov dx,[bp+D_RY]
 inc word ptr [bp+D_RX]
 dec word ptr [bp+D_RLEFT]
 clc
 retf
DREnd:
 mov word ptr [bp+D_RSTATE],1
 mov ax,0ffffh
 clc
 retf
DRError:
 mov word ptr [bp+D_RSTATE],2
 mov ax,0ffffh
 stc
 retf
DibRleNext endp
