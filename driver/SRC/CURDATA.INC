; Resident cursor ownership, masks, save-under and bounded service scratch.
public CurIndex, CurSaveEnd
public CurBusy, CurPending, CurLogical, CurDrawn, CurSlot, CurEpoch
public CurX, CurY, CurWorkX, CurWorkY, CurSave, CurShape0, CurShape1
public CurLeft, CurTop, CurRight, CurBottom, CurSaveX, CurSaveY
public CurSaveW, CurSaveH, CurOriginX, CurOriginY, CurMaskBytes
CurBusy dw 0
CurPending dw 0
CurLogical dw 0
CurDrawn dw 0
CurSlot dw DataOFFSET CurShape0
CurEpoch dw 0
CurX dw 0
CurY dw 0
CurWorkX dw 0
CurWorkY dw 0
CurOriginX dw 0
CurOriginY dw 0
CurLeft dw 0
CurTop dw 0
CurRight dw 0
CurBottom dw 0
CurSaveX dw 0
CurSaveY dw 0
CurSaveW dw 0
CurSaveH dw 0
CurMaskBytes dw 0
CurRow dw 0
CurByte dw 0
CurPlane dw 0
CurRowOffset dw 0
CurAndRow dw 0
CurXorRow dw 0
CurIndex dw 0
CurRowsLeft dw 0
CurLead dw 0
CurWords dw 0
CurTail dw 0
CurBytesLeft dw 0
CurOp dw 0
CurNewSlot dw 0
CurAnd db 0ffh
CurXor db 0
CurBit db 0
CurPad db 0
CurShape0 db 268 dup(0)
CurShape1 db 268 dup(0)
CurNextX dw 0
CurNextY dw 0
CurNextW dw 0
CurNextH dw 0
CurMaskRow dw 0
CurPreparedAnd db 160 dup(0)
CurPreparedXor db 160 dup(0)
; Eight lazily populated phase entries, invalidated after every accepted shape.
; Each entry: valid, relative byte X, source row, width, height, right edge;
; followed by 160 AND and 160 XOR bytes. Clipping is part of the cache key.
public CurCache, CurCacheOffset
CurCacheOffset dw 0
CurCache db 2656 dup(0)

; 0.11 geometry compiled once on a cache miss, owned by CurBusy.
CurMaskSource dw 0
CurMaskShift dw 0
CurMaskCover db 5 dup(0)
CurMaskAlign db 0
CurAndPad db 6 dup(0ffh)
CurXorPad db 6 dup(0)
CurSave db 640 dup(0)
CurSaveEnd label byte
.errnz (CurSaveEnd-CurSave)-640

