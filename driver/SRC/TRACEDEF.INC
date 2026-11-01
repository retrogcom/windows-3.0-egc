; Startup-only diagnostic state. Renderer/cursor paths never call DOS logging.
public TraceEnabled, TraceBusy, TraceHandle, TraceIoError, TraceStage
public StartupFlags, StartupMode, StartupPage, TraceReturn, LastVideoAX
TraceEnabled dw 1
TraceBusy dw 0
TraceHandle dw 0ffffh
TraceIoError dw 0
TraceStage dw 0
StartupFlags dw 0
StartupMode dw 0ffffh
StartupPage dw 0ffffh
TraceReturn dw 0ffffh
LastVideoAX dw 0ffffh
TraceName db 'C:\EGC16.LOG',0
TraceLine db 'EGC16 0.21 st='
TraceStageText db '0000 wf='
TraceFlagsText db '0000 mode='
TraceModeText db '0000 page='
TracePageText db '0000 err='
TraceErrorText db '0000 clean='
TraceCleanupText db '0000 state='
TraceStateText db '0000 pal='
TracePaletteText db '0000 ret='
TraceReturnText db '0000 bios='
TraceVideoText db '0000 io='
TraceIoText db '0000',13,10
TraceLineEnd label byte
even
TraceFields dw DataOFFSET TraceStage, DataOFFSET TraceStageText
 dw DataOFFSET StartupFlags, DataOFFSET TraceFlagsText
 dw DataOFFSET StartupMode, DataOFFSET TraceModeText
 dw DataOFFSET StartupPage, DataOFFSET TracePageText
 dw DataOFFSET LastError, DataOFFSET TraceErrorText
 dw DataOFFSET CleanupError, DataOFFSET TraceCleanupText
 dw DataOFFSET DriverState, DataOFFSET TraceStateText
 dw DataOFFSET PaletteSelector, DataOFFSET TracePaletteText
 dw DataOFFSET TraceReturn, DataOFFSET TraceReturnText
 dw DataOFFSET LastVideoAX, DataOFFSET TraceVideoText
 dw DataOFFSET TraceIoError, DataOFFSET TraceIoText
