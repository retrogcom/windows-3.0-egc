; Sixteen-colour diagnostic schema 1. TraceEnabled gates all collection.
public DrawBlt
DrawBlt label word
 dw 0 ; Calls
 dw 0 ; Failures
 dw 0 ; StackFailures
 dw 0 ; Empty
 dw 0ffffh ; MinRoom
 dw 0 ; FirstStage
 dw 0 ; Return
 dw 0 ; SP
 dw 0 ; Limit
 dw 0 ; Watermark
 dw 0ffffh ; DstFormat
 dw 0ffffh ; SrcFormat
 dw 0ffffh ; Arg00
 dw 0ffffh ; Arg01
 dw 0ffffh ; Arg02
 dw 0ffffh ; Arg03
 dw 0ffffh ; Arg04
 dw 0ffffh ; Arg05
 dw 0ffffh ; Arg06
 dw 0ffffh ; Arg07
 dw 0ffffh ; Arg08
 dw 0ffffh ; Arg09
 dw 0ffffh ; Arg10
 dw 0ffffh ; Arg11
 dw 0ffffh ; Arg12
 dw 0ffffh ; Arg13
 dw 0ffffh ; Arg14
 dw 0ffffh ; Arg15
public DrawDib
DrawDib label word
 dw 0 ; Calls
 dw 0 ; Failures
 dw 0 ; StackFailures
 dw 0 ; Empty
 dw 0ffffh ; MinRoom
 dw 0 ; FirstStage
 dw 0 ; Return
 dw 0 ; SP
 dw 0 ; Limit
 dw 0 ; Watermark
 dw 0ffffh ; DstFormat
 dw 0ffffh ; SrcFormat
 dw 0ffffh ; Arg00
 dw 0ffffh ; Arg01
 dw 0ffffh ; Arg02
 dw 0ffffh ; Arg03
 dw 0ffffh ; Arg04
 dw 0ffffh ; Arg05
 dw 0ffffh ; Arg06
 dw 0ffffh ; Arg07
 dw 0ffffh ; Arg08
 dw 0ffffh ; Arg09
 dw 0ffffh ; Arg10
 dw 0ffffh ; Arg11
 dw 0ffffh ; Arg12
 dw 0ffffh ; Arg13
 dw 0ffffh ; Arg14
 dw 0ffffh ; Arg15
public DrawScreenDib
DrawScreenDib label word
 dw 0 ; Calls
 dw 0 ; Failures
 dw 0 ; StackFailures
 dw 0 ; Empty
 dw 0ffffh ; MinRoom
 dw 0 ; FirstStage
 dw 0 ; Return
 dw 0 ; SP
 dw 0 ; Limit
 dw 0 ; Watermark
 dw 0ffffh ; DstFormat
 dw 0ffffh ; SrcFormat
 dw 0ffffh ; Arg00
 dw 0ffffh ; Arg01
 dw 0ffffh ; Arg02
 dw 0ffffh ; Arg03
 dw 0ffffh ; Arg04
 dw 0ffffh ; Arg05
 dw 0ffffh ; Arg06
 dw 0ffffh ; Arg07
 dw 0ffffh ; Arg08
 dw 0ffffh ; Arg09
 dw 0ffffh ; Arg10
 dw 0ffffh ; Arg11
 dw 0ffffh ; Arg12
 dw 0ffffh ; Arg13
 dw 0ffffh ; Arg14
 dw 0ffffh ; Arg15
public DrawRecordsEnd
DrawRecordsEnd label byte
DrawRecordSize equ 56
DrawTraceLine db 'EGC16 diag=0001 op='
DrawOpText db '0000'
 db ' calls='
DrawText0 db '0000'
 db ' fail='
DrawText1 db '0000'
 db ' stack='
DrawText2 db '0000'
 db ' empty='
DrawText3 db '0000'
 db ' min='
DrawText4 db '0000'
 db ' first='
DrawText5 db '0000'
 db ' ret='
DrawText6 db '0000'
 db ' sp='
DrawText7 db '0000'
 db ' low='
DrawText8 db '0000'
 db ' water='
DrawText9 db '0000'
 db ' dst='
DrawText10 db '0000'
 db ' src='
DrawText11 db '0000'
 db ' a00='
DrawText12 db '0000'
 db ' a01='
DrawText13 db '0000'
 db ' a02='
DrawText14 db '0000'
 db ' a03='
DrawText15 db '0000'
 db ' a04='
DrawText16 db '0000'
 db ' a05='
DrawText17 db '0000'
 db ' a06='
DrawText18 db '0000'
 db ' a07='
DrawText19 db '0000'
 db ' a08='
DrawText20 db '0000'
 db ' a09='
DrawText21 db '0000'
 db ' a0A='
DrawText22 db '0000'
 db ' a0B='
DrawText23 db '0000'
 db ' a0C='
DrawText24 db '0000'
 db ' a0D='
DrawText25 db '0000'
 db ' a0E='
DrawText26 db '0000'
 db ' a0F='
DrawText27 db '0000'
 db 13,10
DrawTraceEnd label byte
even
DrawTraceFields label word
 dw DataOFFSET DrawText0
 dw DataOFFSET DrawText1
 dw DataOFFSET DrawText2
 dw DataOFFSET DrawText3
 dw DataOFFSET DrawText4
 dw DataOFFSET DrawText5
 dw DataOFFSET DrawText6
 dw DataOFFSET DrawText7
 dw DataOFFSET DrawText8
 dw DataOFFSET DrawText9
 dw DataOFFSET DrawText10
 dw DataOFFSET DrawText11
 dw DataOFFSET DrawText12
 dw DataOFFSET DrawText13
 dw DataOFFSET DrawText14
 dw DataOFFSET DrawText15
 dw DataOFFSET DrawText16
 dw DataOFFSET DrawText17
 dw DataOFFSET DrawText18
 dw DataOFFSET DrawText19
 dw DataOFFSET DrawText20
 dw DataOFFSET DrawText21
 dw DataOFFSET DrawText22
 dw DataOFFSET DrawText23
 dw DataOFFSET DrawText24
 dw DataOFFSET DrawText25
 dw DataOFFSET DrawText26
 dw DataOFFSET DrawText27
TraceWriteOffset dw 0
TraceWriteLength dw 0

; Performance schema 1: completed preflight/ownership, saturating call counts.
public PerfStart, PerfEnd
PerfStart label word
public PerfFill
PerfFill dw 0
public PerfAligned
PerfAligned dw 0
public PerfEdge
PerfEdge dw 0
public PerfLarge
PerfLarge dw 0
public PerfShort
PerfShort dw 0
public PerfBlack
PerfBlack dw 0
public PerfWhite
PerfWhite dw 0
public PerfPattern
PerfPattern dw 0
public PerfCopy
PerfCopy dw 0
public PerfScans
PerfScans dw 0
public PerfLines
PerfLines dw 0
public PerfText
PerfText dw 0
public PerfBackground
PerfBackground dw 0
public PerfBgLarge
PerfBgLarge dw 0
public PerfSource
PerfSource dw 0
public PerfMonoSource
PerfMonoSource dw 0
public PerfSrcCopy
PerfSrcCopy dw 0
public PerfSrcAnd
PerfSrcAnd dw 0
public PerfSrcXor
PerfSrcXor dw 0
public PerfSrcOr
PerfSrcOr dw 0
public PerfSrcOther
PerfSrcOther dw 0
public PerfMemoryDest
PerfMemoryDest dw 0
public PerfBatchText
PerfBatchText dw 0
public PerfWidth1
PerfWidth1 dw 0
public PerfWidth8
PerfWidth8 dw 0
public PerfWidth128
PerfWidth128 dw 0
public PerfWidthWide
PerfWidthWide dw 0
public PerfHeight1
PerfHeight1 dw 0
public PerfHeight7
PerfHeight7 dw 0
public PerfHeight31
PerfHeight31 dw 0
public PerfHeightTall
PerfHeightTall dw 0
public PerfBorder
PerfBorder dw 0
public PerfSourceAligned
PerfSourceAligned dw 0
public PerfSourceShifted
PerfSourceShifted dw 0
public PerfCacheHit
PerfCacheHit dw 0
public PerfCacheMiss
PerfCacheMiss dw 0
public PerfBorderDone
PerfBorderDone dw 0
public PerfMemoryBatch
PerfMemoryBatch dw 0
PerfEnd label byte
PerfLine db 'EGC16 perf=0004'
 db ' fill='
PerfText0 db '0000'
 db ' align='
PerfText1 db '0000'
 db ' edge='
PerfText2 db '0000'
 db ' large='
PerfText3 db '0000'
 db ' short='
PerfText4 db '0000'
 db ' black='
PerfText5 db '0000'
 db ' white='
PerfText6 db '0000'
 db ' pat='
PerfText7 db '0000'
 db ' copy='
PerfText8 db '0000'
 db ' scans='
PerfText9 db '0000'
 db ' lines='
PerfText10 db '0000'
 db ' text='
PerfText11 db '0000'
 db ' bg='
PerfText12 db '0000'
 db ' bglg='
PerfText13 db '0000'
 db ' src='
PerfText14 db '0000'
 db ' mono='
PerfText15 db '0000'
 db ' scopy='
PerfText16 db '0000'
 db ' sand='
PerfText17 db '0000'
 db ' sxor='
PerfText18 db '0000'
 db ' sor='
PerfText19 db '0000'
 db ' sother='
PerfText20 db '0000'
 db ' memdst='
PerfText21 db '0000'
 db ' batch='
PerfText22 db '0000'
 db ' w1='
PerfText23 db '0000'
 db ' w8='
PerfText24 db '0000'
 db ' w128='
PerfText25 db '0000'
 db ' wwide='
PerfText26 db '0000'
 db ' h1='
PerfText27 db '0000'
 db ' h7='
PerfText28 db '0000'
 db ' h31='
PerfText29 db '0000'
 db ' htall='
PerfText30 db '0000'
 db ' border='
PerfText31 db '0000'
 db ' salign='
PerfText32 db '0000'
 db ' sshift='
PerfText33 db '0000'
 db ' bchit='
PerfText34 db '0000'
 db ' bcmiss='
PerfText35 db '0000'
 db ' bdone='
PerfText36 db '0000'
 db ' mbatch='
PerfText37 db '0000'
 db 13,10
PerfLineEnd label byte
even
PerfFields label word
 dw DataOFFSET PerfText0
 dw DataOFFSET PerfText1
 dw DataOFFSET PerfText2
 dw DataOFFSET PerfText3
 dw DataOFFSET PerfText4
 dw DataOFFSET PerfText5
 dw DataOFFSET PerfText6
 dw DataOFFSET PerfText7
 dw DataOFFSET PerfText8
 dw DataOFFSET PerfText9
 dw DataOFFSET PerfText10
 dw DataOFFSET PerfText11
 dw DataOFFSET PerfText12
 dw DataOFFSET PerfText13
 dw DataOFFSET PerfText14
 dw DataOFFSET PerfText15
 dw DataOFFSET PerfText16
 dw DataOFFSET PerfText17
 dw DataOFFSET PerfText18
 dw DataOFFSET PerfText19
 dw DataOFFSET PerfText20
 dw DataOFFSET PerfText21
 dw DataOFFSET PerfText22

 dw DataOFFSET PerfText23
 dw DataOFFSET PerfText24
 dw DataOFFSET PerfText25
 dw DataOFFSET PerfText26
 dw DataOFFSET PerfText27
 dw DataOFFSET PerfText28
 dw DataOFFSET PerfText29
 dw DataOFFSET PerfText30
 dw DataOFFSET PerfText31
 dw DataOFFSET PerfText32
 dw DataOFFSET PerfText33
 dw DataOFFSET PerfText34
 dw DataOFFSET PerfText35
 dw DataOFFSET PerfText36
 dw DataOFFSET PerfText37
