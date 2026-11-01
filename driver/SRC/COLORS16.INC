; Canonical Windows RGB representatives, not monitor measurements.
; DWORD order: R,G,B,metadata (index bits 0..3, mono bit 4, pure bit 5).
public CanonicalColours
CanonicalColours label byte
db 0,0,0,32
db 128,0,0,1
db 0,128,0,2
db 128,128,0,3
db 0,0,128,4
db 128,0,128,5
db 0,128,128,6
db 128,128,128,23
db 192,192,192,24
db 255,0,0,9
db 0,255,0,10
db 255,255,0,27
db 0,0,255,12
db 255,0,255,29
db 0,255,255,30
db 255,255,255,63
.errnz ($-CanonicalColours)-64
