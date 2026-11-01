; Source-free ROP truth bits 0,1,4,5 packed to a four-bit index.
; Per entry: constant DX then P-dependent mask DX (DL=XOR, DH=AND).
; Result coefficients are (replicate(P) AND mask) XOR constant.
; Immutable table; borrowed brush and caller destination remain unchanged.
even
public FillRopPairs
FillRopPairs label word
 dw 00000h,00000h ; truth 0
 dw 0ffffh,0ffffh ; truth 1
 dw 0ff00h,0ff00h ; truth 2
 dw 000ffh,000ffh ; truth 3
 dw 00000h,0ffffh ; truth 4
 dw 0ffffh,00000h ; truth 5
 dw 0ff00h,000ffh ; truth 6
 dw 000ffh,0ff00h ; truth 7
 dw 00000h,0ff00h ; truth 8
 dw 0ffffh,000ffh ; truth 9
 dw 0ff00h,00000h ; truth a
 dw 000ffh,0ffffh ; truth b
 dw 00000h,000ffh ; truth c
 dw 0ffffh,0ff00h ; truth d
 dw 0ff00h,0ffffh ; truth e
 dw 000ffh,00000h ; truth f

even
SourceKernelTable label word
 dw offset SW0, offset SB0
 dw offset SW1, offset SB1
 dw offset SW2, offset SB2
 dw offset SW3, offset SB3
 dw offset SW4, offset SB4
 dw offset SW5, offset SB5
 dw offset SW6, offset SB6
 dw offset SW7, offset SB7
 dw offset SW8, offset SB8
 dw offset SW9, offset SB9
 dw offset SW10, offset SB10
 dw offset SW11, offset SB11
 dw offset SW12, offset SB12
 dw offset SW13, offset SB13
 dw offset SW14, offset SB14
 dw offset SW15, offset SB15

; Generated from every operand pair in each ROP3 truth table.
; Index=ROP, bits D/S/P; immutable driver-owned data, no lifetime cache.
RopUseTable label byte
 db 0,7,7,6,7,5,7,7,7,7,5,7,6,7,7,4
 db 7,3,7,7,7,7,7,7,7,7,7,7,7,7,7,7
 db 7,7,3,7,7,7,7,7,7,7,7,7,7,7,7,7
 db 6,7,7,2,7,7,7,7,7,7,7,7,6,7,7,6
 db 7,7,7,7,3,7,7,7,7,7,7,7,7,7,7,7
 db 5,7,7,7,7,1,7,7,7,7,5,7,7,7,7,5
 db 7,7,7,7,7,7,3,7,7,7,7,7,7,7,7,7
 db 7,7,7,7,7,7,7,3,7,7,7,7,7,7,7,7
 db 7,7,7,7,7,7,7,7,3,7,7,7,7,7,7,7
 db 7,7,7,7,7,7,7,7,7,3,7,7,7,7,7,7
 db 5,7,7,7,7,5,7,7,7,7,1,7,7,7,7,5
 db 7,7,7,7,7,7,7,7,7,7,7,3,7,7,7,7
 db 6,7,7,6,7,7,7,7,7,7,7,7,2,7,7,6
 db 7,7,7,7,7,7,7,7,7,7,7,7,7,3,7,7
 db 7,7,7,7,7,7,7,7,7,7,7,7,7,7,3,7
 db 4,7,7,6,7,5,7,7,7,7,5,7,6,7,7,0

FillFirstMasks db 0ffh,07fh,03fh,01fh,00fh,007h,003h,001h
FillLastMasks db 080h,0c0h,0e0h,0f0h,0f8h,0fch,0feh,0ffh
