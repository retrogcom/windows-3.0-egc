; Bounded, failure-only request capture. DS is driver data; BP is the
; standard public-entry frame. AX (return value), ES and caller FLAGS survive.
; BX/CX/DX/SI/DI may be changed; public epilogues restore saved SI/DI.
; No far argument is dereferenced and no DOS/kernel/BIOS call is made.
; One FLAGS word is the only extra stack use, including low-stack refusal.
; IRQs are masked only while updating the fixed record; cursor IRQ code never
; calls these public GDI entries. Records accumulate for this DLL lifetime.
DRAWREPORT macro record, stage, dfmt, sfmt, words
 local disabled, countdone, roomok, minok, failure, emptydone
 local failcount, stackdone, firstdone, enabled, capture
 pushf
 cmp TraceEnabled,0
 jne enabled
 jmp disabled
enabled:
 cli
 mov si,DataOFFSET record
 cmp word ptr [si],0ffffh
 je countdone
 inc word ptr [si]
countdone:
 mov dx,bp
 sub dx,6
 sub dx,ss:[000ah]
 jnc roomok
 xor dx,dx
roomok:
 cmp dx,[si+8]
 jae minok
 mov [si+8],dx
minok:
 mov bx,stage
 or bx,bx
 jnz failure
 or ax,ax
 jnz disabled
 cmp word ptr [si+6],0ffffh
 je disabled
 inc word ptr [si+6]
 jmp disabled
failure:
 cmp word ptr [si+2],0ffffh
 je failcount
 inc word ptr [si+2]
failcount:
 cmp bx,1
 jne stackdone
 cmp word ptr [si+4],0ffffh
 je stackdone
 inc word ptr [si+4]
stackdone:
 cmp word ptr [si+10],0
 je capture
 jmp disabled
capture:
 mov [si+10],bx
 mov [si+12],ax
 mov dx,bp
 sub dx,6
 mov [si+14],dx
 mov dx,ss:[000ah]
 mov [si+16],dx
 mov dx,ss:[000ch]
 mov [si+18],dx
 mov dx,dfmt
 mov [si+20],dx
 mov dx,sfmt
 mov [si+22],dx
 lea bx,[bp+6]
 lea di,[si+24]
 mov cx,words
firstdone:
 mov dx,ss:[bx]
 mov [di],dx
 add bx,2
 add di,2
 loop firstdone
 ; Unused argument slots remain FFFFh in the fixed data initializer.
disabled:
 popf
endm

; Invocation counters only; never log while rendering. FFFF means saturated.
PERFINC macro counter
 local done
 pushf
 cmp TraceEnabled,0
 je done
 cli
 cmp counter,0ffffh
 je done
 inc counter
done:
 popf
endm
