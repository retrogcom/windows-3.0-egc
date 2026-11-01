; Sixteen-colour desktop candidate, native 286 implementation.
; Public Enable is operational; Windows/M28 runtime qualification is pending.
.286p
incDevice = 1
.xlist
include cmacros.inc
include gdidefs.inc
include int31.inc
.list

externFP GetSelectorBase
externFP GlobalDosAlloc
externFP GlobalDosFree
externFP GlobalSize
externFP GlobalHandle
externFP GlobalReAlloc
externA VideoA000
externA VideoB000
externA VideoB800
externA WinFlags
externA HugeIncrement

INACTIVE equ 0
PREPARING equ 1
ACTIVE equ 2
QUIESCING equ 3
FAULTED equ 4
ERR_BUSY equ 1
ERR_RESIDENT equ 2
ERR_BASELINE equ 3
ERR_MAPPING equ 4
ERR_ALLOC equ 5
ERR_OWNER equ 6
ERR_BRIDGE equ 7
ERR_MODE equ 8
ERR_RELEASE equ 9
ERR_CLOSE equ 10
ERR_RESTORE equ 11
ERR_INCOMPLETE equ 12
ERR_ENVIRONMENT equ 13
ERR_ARGUMENT equ 14
ERR_STACK equ 15
ERR_PUBLISH equ 16

TRACE macro stage
    push ax
    mov ax,stage
    call TraceRecord
    pop ax
endm

include C:\SRC\DRAWDIAG.INC

sBegin Data
public DriverState, ProtectedMode, LastError, CleanupError, Generation
public PaletteSelector, PaletteReal, ResidentHandle, BaselineKnown
public HardwareDirty, BridgeBusy, ScreenGate, PlaneSelectors, PlaneBases
public CallFrame, GdiTable, GdiTableEnd, Palette16, DriverDataEnd
DriverState dw INACTIVE
ProtectedMode dw 0
LastError dw 0
CleanupError dw 0
Generation dw 0
PaletteSelector dw 0
PaletteReal dw 0
ResidentHandle dw 0ffffh
BaselineKnown dw 0
HardwareDirty dw 0
BridgeBusy dw 0
ScreenGate dw 0
public DrawDepth, PhysicalDevice, DeviceEnd
DrawDepth dw 0
; Private 36-byte screen descriptor. Never reinterpret as a memory BITMAP.
PhysicalDevice label word
dw 4745h,640,400,80,0104h,1,0,0
dw 0,0,0,8000h,0,0,0,0,0,36
DeviceEnd label byte
.errnz (DeviceEnd-PhysicalDevice)-36
PlaneSelectors dw 4 dup(0)
PlaneBases dw 0,8000h,0,0
CallFrame db 50 dup(0)
.errnz (size Real_Mode_Call_Struc)-50
ResidentName db 'DEBDRIVE',0
; API colour codes: resident performs its R/G translation exactly once.
Palette16 db 0,4,2,6,1,5,3,8,7,12,10,14,9,13,11,15
include C:\SRC\COLORS16.INC
include C:\SRC\OBTABLE.INC
; Fixed sixteen-colour profile; GDI supplies styled/wide lines and font
; transformation. No palette manager or hardware acceleration.
GdiTable label byte
dw 0300h,1,215,161,640,400,1,4
dw 0ffffh,80,0,0,16,36
dw 0,0002h,0008h,2004h,0001h,0299h
; Original ATTDEB GDIINFO aspect and mapping-mode numbers (2:0048 onward).
; Its GDIINFO reports 96/72 logical DPI; description text says 96/80.
; Preserve its actual metrics; medium-resolution font resources are linked.
dw 50,60,78,156
dw 2150,1613,640,-400
dw 21500,16125,640,-400
dw 5375,3225,4064,-2032
dw 26875,16125,2032,-1016
dw 9675,23220,508,-1016
dw 96,72,DC_IgnoreDFNP
dw 8 dup(0)
GdiTableEnd label byte
.errnz (GdiTableEnd-GdiTable)-(size GDIINFO)
even
include C:\SRC\CURDATA.INC
include C:\SRC\HELPTAB.INC
include C:\SRC\TRACEDEF.INC
include C:\SRC\DRAWDEF.INC
include C:\SRC\FILLDEF.INC
; Owned, bounded text strip. No external allocation or stack growth.
public TextBusy, TextInk, TextCover, TextBatchEnd
TextBusy dw 0
TextInk db 2560 dup(0)
TextCover db 2560 dup(0)
TextBatchEnd label byte
; Two content-validated, phase-aligned 32x32 bitmap entries (mono/colour).
public BitCacheBusy, BitCacheNext, BitCache, BitCacheEnd
BitCacheBusy dw 0
BitCacheNext dw 0
BitCache db 2624 dup(0)
BitCacheEnd label byte
DriverDataEnd label byte
sEnd Data

sBegin Code
assumes cs,Code
assumes ds,Data
assumes es,nothing
public Activate, Deactivate, CheckSpan, ValidateVideo, PlaneAddress
public CallVideo, CodeEnd, ProbeResident, ReleasePalette

; Windows loader entry: no display I/O/allocation; optional startup log only.
cProc DriverInit,<FAR,PUBLIC>,<si,di>
cBegin
    mov ax,WinFlags
    mov StartupFlags,ax
    test ax,0020h                 ; Enhanced mode is outside this target
    jnz InitFail
    test ax,000eh                 ; 286, 386 or 486 (separate WinFlags bits)
    jz InitFail
    and ax,1
    mov ProtectedMode,ax
    TRACE 0001h
    mov ax,1
    jmp short InitDone
InitFail:
    mov LastError,ERR_ENVIRONMENT
    TRACE 0002h
    xor ax,ax
InitDone:
cEnd

; Standard DDK Pascal entry layout and Windows entry prologue.
cProc Enable,<FAR,PUBLIC,WIN,PASCAL>,<si,di>
    parmD lpDevice
    parmW Style
    parmD lpDeviceType
    parmD lpOutputFile
    parmD lpStuff
cBegin
    push ds
    mov ax,seg DriverState
    mov ds,ax
    mov LastError,0
    mov ax,Style
    and ax,7fffh
    cmp ax,1
    je Inquiry
    or ax,ax
    jnz InquiryFail
    TRACE 0020h
    mov ax,128
    call StackRoom
    jnc EnableStackOK
    mov LastError,ERR_STACK
    jmp EnableRefused
EnableStackOK:
    mov bx,word ptr lpDevice
    mov ax,word ptr lpDevice+2
    mov cx,36
    call CheckSpan
    jc InquiryFail
    call Activate
    or ax,ax
    jz EnableDone
    mov bx,word ptr lpDevice
    mov ax,word ptr lpDevice+2
    call ClonePhysical
    jnc EnableDone
    ; Borrowed storage remains stable across this synchronous call. If its
    ; second validation nevertheless fails, relinquish acquired hardware.
    mov LastError,ERR_PUBLISH
    call Deactivate
    xor ax,ax
    jmp short EnableDone
Inquiry:
    TRACE 0010h
    mov bx,word ptr lpDevice
    mov ax,word ptr lpDevice+2
    mov cx,size GDIINFO
    call CheckSpan
    jc InquiryFail
    mov es,ax
    mov di,bx
    mov si,DataOFFSET GdiTable
    mov cx,size GDIINFO
    cld
    rep movsb
    mov ax,size GDIINFO
    jmp short EnableDone
InquiryFail:
    mov LastError,ERR_ARGUMENT
EnableRefused:
    TRACE 00f0h
    xor ax,ax
EnableDone:
    mov TraceReturn,ax
    TRACE 0050h
    pop ds
cEnd

cProc Disable,<FAR,PUBLIC,WIN,PASCAL>,<si,di>
    parmD lpDevice
cBegin
    push ds
    mov ax,seg DriverState
    mov ds,ax
    call Deactivate
    pop ds
    mov ax,0ffffh                ; historical ABI; inspect retained fault state
cEnd

; AX segment/selector, BX offset, CX positive length. Preserve AX/BX/CX.
; CF clear only for a complete non-wrapping writable ordinary data span.
; Does not load the supplied selector until access and bounds are checked.
CheckSpan proc near
    push dx
    push si
    or ax,ax
    jz SpanBad
    jcxz SpanBad
    mov si,cx
    dec si
    add si,bx
    jc SpanBad
    cmp ProtectedMode,0
    jne SpanProtected
    cmp ax,0f000h
    jbe SpanGood
    mov dx,0ffffh
    sub dx,ax
    shl dx,1
    shl dx,1
    shl dx,1
    shl dx,1
    or dx,15
    cmp si,dx
    ja SpanBad
    jmp short SpanGood
SpanProtected:
    verw ax
    jnz SpanBad
    lar dx,ax
    jnz SpanBad
    test dx,8000h                ; VERW/LSL do not establish presence
    jz SpanBad
    and dx,1c00h                 ; ordinary data, no code or expand-down
    cmp dx,1000h
    jne SpanBad
    lsl dx,ax
    jnz SpanBad
    cmp si,dx
    ja SpanBad
SpanGood:
    pop si
    pop dx
    clc
    ret
SpanBad:
    pop si
    pop dx
    stc
    ret
CheckSpan endp

ValidateVideo proc near
    mov ax,VideoA000
    mov PlaneSelectors,ax
    mov PlaneSelectors+2,ax
    xor bx,bx
    mov cx,8000h
    call CheckSpan
    jc VideoBad
    mov bx,8000h
    call CheckSpan
    jc VideoBad
    mov ax,VideoB000
    mov PlaneSelectors+4,ax
    xor bx,bx
    call CheckSpan
    jc VideoBad
    mov ax,VideoB800
    mov PlaneSelectors+6,ax
    call CheckSpan
    ret
VideoBad:
    stc
    ret
ValidateVideo endp

; AX=x, DX=y, BX=plane 0..3 -> AX=selector, DI=offset, CL=bitmask.
; All four planes are used by the sixteen-colour kernel.
; This only forms a pointer; it does not read or write video memory.
PlaneAddress proc near
    cmp ScreenGate,1
    jne AddressBad
    cmp DriverState,ACTIVE
    jne AddressBad
    cmp ax,640
    jae AddressBad
    cmp dx,400
    jae AddressBad
    cmp bx,4
    jae AddressBad
    push si
    push ax
    mov si,ax
    shr si,1
    shr si,1
    shr si,1
    mov ax,dx
    and ax,3
    mov cl,13
    shl ax,cl
    mov di,ax
    mov ax,dx
    shr ax,1
    shr ax,1
    mov cx,80
    mul cx
    add di,ax
    add di,si
    shl bx,1
    add di,PlaneBases[bx]
    pop ax
    and ax,7
    mov cl,al
    mov al,80h
    shr al,cl
    mov cl,al
    mov ax,PlaneSelectors[bx]
    pop si
    clc
    ret
AddressBad:
    stc
    ret
PlaneAddress endp

; Serialized BIOS bridge. AX=request; other inner input registers zero.
; For 0B05h, ES:SI is PaletteReal:0. Returns queried AX/BX and bridge CF.
; The inner BIOS FLAGS are deliberately not interpreted as success/failure.
; Returned real ES:DI is retained only as bytes in CallFrame, never loaded PM.
CallVideo proc near
    pushf
    cli
    cmp BridgeBusy,0
    je @F
    jmp BridgeTaken
@@:
    mov BridgeBusy,1
    popf
    push ds
    push es
    push si
    push di
    push bp
    push ax
    push ds
    pop es
    mov di,DataOFFSET CallFrame
    xor ax,ax
    mov cx,25
    cld
    rep stosw
    pop ax
    mov word ptr CallFrame.RealMode_EAX,ax
    ; Inner flags have IF set; BH=0 never requests PIC/A20 reset.
    mov word ptr CallFrame.RealMode_Flags,0202h
    cmp ax,0b05h
    jne CallPrepared
    mov dx,PaletteReal
    mov word ptr CallFrame.RealMode_ES,dx
CallPrepared:
    cmp ProtectedMode,0
    je RealVideo
    mov ax,0300h
    mov bx,0010h
    xor cx,cx
    mov di,DataOFFSET CallFrame
    int 31h
    jc BridgeFailed
    mov ax,word ptr CallFrame.RealMode_EAX
    mov bx,word ptr CallFrame.RealMode_EBX
    jmp short BridgeSuccess
RealVideo:
    xor bx,bx
    xor cx,cx
    xor dx,dx
    xor si,si
    xor di,di
    xor bp,bp
    mov es,word ptr CallFrame.RealMode_ES
    mov ax,word ptr CallFrame.RealMode_EAX
    push ax
    xor ax,ax
    mov ds,ax
    pop ax
    int 10h
    ; Recover DS before touching any shared data, even if BIOS destroyed it.
    push ax
    mov ax,seg DriverState
    mov ds,ax
    pop ax
BridgeSuccess:
    mov LastVideoAX,ax
    mov BridgeBusy,0
    pop bp
    pop di
    pop si
    pop es
    pop ds
    cld
    clc
    ret
BridgeFailed:
    mov BridgeBusy,0
    pop bp
    pop di
    pop si
    pop es
    pop ds
    cld
    stc
    ret
BridgeTaken:
    popf
    stc
    ret
CallVideo endp

; DOS handle retained until close really succeeds, including error unwind.
ProbeResident proc near
    mov dx,DataOFFSET ResidentName
    mov ax,3d00h
    int 21h
    jc ResidentBad
    mov ResidentHandle,ax
    mov bx,ax
    mov ax,4400h
    int 21h
    jc ResidentCloseBad
    test dl,80h
    jz ResidentCloseBad
    call CloseResident
    ret
ResidentCloseBad:
    call CloseResident
ResidentBad:
    stc
    ret
ProbeResident endp

CloseResident proc near
    mov bx,ResidentHandle
    cmp bx,0ffffh
    je Closed
    mov ah,3eh
    int 21h
    jc CloseFailed
    mov ResidentHandle,0ffffh
Closed:
    clc
    ret
CloseFailed:
    mov CleanupError,ERR_CLOSE
    stc
    ret
CloseResident endp

ReleasePalette proc near
    mov ax,PaletteSelector
    or ax,ax
    jz Released
    xor bx,bx
    mov es,bx                    ; no lingering palette selector in ES
    cCall GlobalDosFree,<ax>
    or ax,ax
    jnz ReleaseFailed
    mov PaletteSelector,0
    mov PaletteReal,0
Released:
    clc
    ret
ReleaseFailed:
    mov CleanupError,ERR_RELEASE
    stc
    ret
ReleasePalette endp

; Internal foundation lifecycle. No hooks, drawing callbacks or allocations
; are permitted concurrently. State publication is interrupt-atomic.
Activate proc near
    pushf
    cli
    cmp DriverState,ACTIVE
    jne @F
    jmp AlreadyActive
@@:
    cmp DriverState,INACTIVE
    je @F
    jmp ActivateBusy
@@:
    mov DriverState,PREPARING
    mov CurDrawn,0
    mov ScreenGate,0
    popf
    mov LastError,0
    mov CleanupError,0
    mov BaselineKnown,0
    mov StartupMode,0ffffh
    mov StartupPage,0ffffh
    call ProbeResident
    jnc ResidentOK
    mov LastError,ERR_RESIDENT
    jmp ActivateFailed
ResidentOK:
    TRACE 0021h
    mov ax,0f00h
    call CallVideo
    jnc BaselineReturned
    mov LastError,ERR_BRIDGE
    jmp ActivateFailed
BaselineReturned:
    mov StartupMode,ax
    mov dx,bx
    mov dl,dh
    xor dh,dh
    mov StartupPage,dx
    TRACE 0022h
    cmp ax,5003h                 ; native 80-column mode 3
    je BaselineModeOK
    cmp ax,5006h                 ; legacy CGA logo handoff
    je BaselineModeOK
    cmp ax,5042h                 ; EGCLOGO native 640x400 colour handoff
    jne BaselineBad
BaselineModeOK:
    cmp bh,0                     ; initial scope is active page zero
    jne BaselineBad
    mov BaselineKnown,1
    call ValidateVideo
    jnc MappingOK
    mov LastError,ERR_MAPPING
    jmp ActivateFailed
BaselineBad:
    mov LastError,ERR_BASELINE
    jmp ActivateFailed
MappingOK:
    xor ax,ax
    mov bx,257
    cCall GlobalDosAlloc,<ax,bx>
    mov PaletteSelector,ax       ; acquire ownership before later failure
    mov PaletteReal,dx
    or ax,ax
    jnz @F
    jmp AllocBad
@@:
    or dx,dx
    jnz @F
    jmp AllocBad
@@:
    xor bx,bx
    mov cx,257
    call CheckSpan
    jnc @F
    jmp AllocBad
@@:
    cCall GlobalHandle,<ax>
    or ax,ax
    jnz @F
    jmp OwnerBad
@@:
    xor bx,bx
    mov cx,2080h                 ; GMEM_MODIFY | GMEM_DDESHARE
    cCall GlobalReAlloc,<ax,bx,bx,cx>
    or ax,ax
    jnz @F
    jmp OwnerBad
@@:
    mov ax,PaletteSelector
    mov es,ax
    xor di,di
    mov al,4
    cld
    stosb
    mov bx,16
PaletteLoop:
    mov si,DataOFFSET Palette16
    mov cx,16
    rep movsb
    dec bx
    jnz PaletteLoop
    xor ax,ax
    mov es,ax
    TRACE 0023h
public StartEgcHardware
StartEgcHardware label near
    mov HardwareDirty,1         ; mutation boundary precedes INT 10h
    TRACE 0030h
    mov ax,0042h
    call CallVideo
    jnc @F
    jmp ModeBridgeBad
@@:
    TRACE 0031h
    mov ax,0b05h
    call CallVideo
    jnc @F
    jmp ModeBridgeBad
@@:
    TRACE 0032h
    mov ax,0f00h
    call CallVideo
    jnc @F
    jmp ModeBridgeBad
@@:
    TRACE 0033h
    cmp al,42h
    je @F
    jmp ModeWrong
@@:
    ; Clear all four complete planes, including padding.
    mov ax,PlaneSelectors
    mov es,ax
    xor ax,ax
    xor di,di
    mov cx,8000h
    cld
    rep stosw
    mov ax,PlaneSelectors+4
    mov es,ax
    xor ax,ax
    xor di,di
    mov cx,4000h
    rep stosw
    mov ax,PlaneSelectors+6
    mov es,ax
    xor ax,ax
    xor di,di
    mov cx,4000h
    rep stosw
    xor ax,ax
    mov es,ax
    pushf
    cli
    inc Generation
    mov ax,Generation
    mov PhysicalDevice+12,ax
    mov ax,ds
    mov PhysicalDevice+14,ax
    mov ax,PlaneSelectors
    mov PhysicalDevice+16,ax
    mov PhysicalDevice+20,ax
    mov ax,PlaneSelectors+4
    mov PhysicalDevice+24,ax
    mov ax,PlaneSelectors+6
    mov PhysicalDevice+28,ax
    mov DriverState,ACTIVE
    mov ScreenGate,1
    mov CurDrawn,0
    mov CurPending,1
    popf
    call CursorService
    TRACE 0040h
    mov ax,1
    clc
    ret
AllocBad:
    mov LastError,ERR_ALLOC
    jmp short ActivateFailed
OwnerBad:
    mov LastError,ERR_OWNER
    jmp short ActivateFailed
ModeBridgeBad:
    mov LastError,ERR_BRIDGE
    jmp short ActivateFailed
ModeWrong:
    mov LastError,ERR_MODE
ActivateFailed:
    TRACE 00e0h
    call Cleanup
    TRACE 00e1h
    xor ax,ax
    stc
    ret
AlreadyActive:
    popf
    ; Reuse only a fully owned active context; never reset a live desktop.
    cmp ScreenGate,1
    jne ActiveInconsistent
    cmp PaletteSelector,0
    je ActiveInconsistent
    cmp HardwareDirty,1
    jne ActiveInconsistent
    mov ax,1
    clc
    ret
ActiveInconsistent:
    mov ScreenGate,0
    mov DriverState,FAULTED
    mov LastError,ERR_BUSY
    xor ax,ax
    stc
    ret
ActivateBusy:
    popf
    xor ax,ax
    stc
    ret
Activate endp

Deactivate proc near
    pushf
    cli
    cmp DriverState,PREPARING
    je DeactivateBusy
    cmp DriverState,QUIESCING
    je DeactivateBusy
    cmp BridgeBusy,0
    jne DeactivateBusy
    cmp DrawDepth,0
    jne DeactivateBusy
    cmp CurBusy,0
    jne DeactivateBusy
    mov CurDrawn,0
    mov CurPending,1
    mov ScreenGate,0
    mov DriverState,QUIESCING
    popf
    mov CleanupError,0
    TRACE 0060h
    call Cleanup
    ; Dump only at normal teardown, after hardware/resource cleanup.
    TRACE 0070h
    TRACE 0071h
    TRACE 0072h
    TRACE 0073h
    TRACE 0061h
    ret
DeactivateBusy:
    popf
    stc
    ret
Deactivate endp

; Recovery failures keep hardware-dirty/resource records for a later retry.
; No interrupt hook or private pointer survives publication by this phase.
Cleanup proc near
    mov CurDrawn,0
    mov CurPending,1
    mov ScreenGate,0
    cmp HardwareDirty,0
    jne CleanupText
    cmp BaselineKnown,1
    jne CleanupResources
    cmp StartupMode,5006h
    je CleanupText
    cmp StartupMode,5042h
    jne CleanupResources
CleanupText:
    ; A recognized Windows logo entry also owns recovery to native text if
    ; allocation fails before EGC activation. Retain dirty on restore failure.
    mov HardwareDirty,1
    mov ax,0003h
    call CallVideo
    jc RestoreBad
    mov ax,0f00h
    call CallVideo
    jc RestoreBad
    cmp ax,5003h
    jne RestoreBad
    cmp bh,0
    jne RestoreBad
    mov HardwareDirty,0
    mov BaselineKnown,0
    jmp short CleanupResources
RestoreBad:
    mov CleanupError,ERR_RESTORE
CleanupResources:
    call ReleasePalette
    call CloseResident
    cmp HardwareDirty,0
    jne CleanupFault
    cmp PaletteSelector,0
    jne CleanupFault
    cmp ResidentHandle,0ffffh
    jne CleanupFault
    mov DriverState,INACTIVE
    mov BaselineKnown,0
    clc
    ret
CleanupFault:
    mov DriverState,FAULTED
    stc
    ret
Cleanup endp
include C:\SRC\PIXEL.INC
include C:\SRC\SCREEN.INC
include C:\SRC\OBJECT.INC
include C:\SRC\OUTPUT.INC
include C:\SRC\SPANS.INC
include C:\SRC\AXIS.INC
include C:\SRC\BLTCORE.INC
include C:\SRC\BLTMAP.INC
include C:\SRC\BLTRECT.INC
include C:\SRC\ALIAS.INC
include C:\SRC\MAPCACHE.INC
include C:\SRC\RANGE.INC
include C:\SRC\COPY.INC
include C:\SRC\BITCACHE.INC
include C:\SRC\SOURCE.INC
include C:\SRC\PLANES.INC
include C:\SRC\ROWCOPY.INC
include C:\SRC\FILL.INC
include C:\SRC\NARROW.INC
include C:\SRC\TXTCORE.INC
include C:\SRC\TXTDRAW.INC
include C:\SRC\TEXTFAST.INC
include C:\SRC\CURSOR.INC
include C:\SRC\HELPERS.INC
; Synchronous far bridges to the fixed checked surface/address helpers.
public DibNormalize, DibSpan
DibNormalize proc far
 call BlitNormalize
 retf
DibNormalize endp
DibSpan proc far
 call TextSpan
 retf
DibSpan endp
public DibColor, DibIndex
DibColor proc far
 call LogicalToPhysical
 retf
DibColor endp
DibIndex proc far
 call IndexToPhysical
 retf
DibIndex endp
public DibSnapshot
DibSnapshot proc far
 call SnapshotSurface
 retf
DibSnapshot endp
public DibClassify
DibClassify proc far
 call ClassifyFormat
 retf
DibClassify endp
public DibPoint
DibPoint proc far
 call PreparePoint
 retf
DibPoint endp
public DibPixel
DibPixel proc far
 call PixelBits
 retf
DibPixel endp
public DibScreenSpan
DibScreenSpan proc far
 call PrepareScreenSpan
 retf
DibScreenSpan endp
public DibEnter
DibEnter proc far
 call EnterDrawing
 retf
DibEnter endp
public DibLeave
DibLeave proc far
 call LeaveDrawing
 retf
DibLeave endp
public DibRoom
DibRoom proc far
 call StackRoom
 retf
DibRoom endp
include C:\SRC\TRACE.INC
CodeEnd label byte
sEnd Code
_DIB segment word public 'CODE'
assume cs:_DIB,ds:DGROUP
include C:\SRC\DIBSTRM.INC
include C:\SRC\DIBRLE.INC
include C:\SRC\DIBHEAD.INC
include C:\SRC\DIBSURF.INC
include C:\SRC\DIBPACK.INC
include C:\SRC\DIBCONV.INC
include C:\SRC\DIBENTRY.INC
_DIB ends
end DriverInit
