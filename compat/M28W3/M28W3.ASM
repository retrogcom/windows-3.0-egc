; M28W3 1.3 - pattern-based DOSX return-frame correction.
; Tested on Olivetti M28 BIOS 1.08. No ROM check is performed.
; BIOS 2.12 has a different frame convention; this correction is unsuitable.
bits 16
cpu 286
org 100h
%define MAX_FILE 0d000h
%include "LAYOUT.INC"

start:
    cli
    mov ax,cs
    mov ss,ax
    mov sp,stack_top
    sti
    mov ds,ax
    mov es,ax
    cld
    xor cx,cx
    mov cl,[80h]
    mov si,81h
.args:
    jcxz .args_done
    lodsb
    dec cx
    cmp al,' '
    je .args
    cmp al,9
    je .args
    cmp al,'/'
    jne usage_error
    cmp byte [check_only],0
    jne usage_error
    test cx,cx
    jz usage_error
    lodsb
    dec cx
    or al,20h
    cmp al,'c'
    jne usage_error
    mov byte [check_only],1
    jcxz .args_done
    cmp byte [si],' '
    je .args
    cmp byte [si],9
    jne usage_error
    jmp .args
.args_done:
    mov ax,[2]
    mov bx,cs
    sub ax,bx
    cmp ax,(buffer+MAX_FILE+1+15)/16
    jb memory_error
    smsw ax
    test al,1
    jnz mode_error
    mov ah,30h
    int 21h
    cmp al,3
    jb mode_error
    mov dx,banner
    mov ah,9
    int 21h
    mov dx,original_name
    mov ax,4300h
    int 21h
    jc input_error
    test cx,18h
    jnz input_error
    mov [original_attrs],cx
    call load_file
    jc input_error
    mov ax,[file_time]
    mov [original_time],ax
    mov ax,[file_date]
    mov [original_date],ax
    mov ax,[file_size]
    mov [original_size],ax
    call locate_frame
    jnc .located
    mov dx,[match_error]
    jmp error
.located:
    call print_site
    cmp byte [check_only],0
    jne checked
    cmp byte [patch_state],1
    jne .needs_patch
    cmp byte [video_state],1
    je already_patched
.needs_patch:
    test word [original_attrs],1
    jnz readonly_error
    mov dx,backup_name
    call must_be_absent
    jc collision_error
    mov dx,work_name
    call must_be_absent
    jc collision_error
    mov si,new_bytes
    mov di,[patch_site]
    mov cx,PATCH_SIZE
    rep movsb
    mov si,video_new_bytes
    mov di,[video_site]
    mov cx,VIDEO_PATCH_SIZE
    rep movsb
    call crc32
    mov [expected_crc],ax
    mov [expected_crc+2],dx

    mov dx,work_name
    xor cx,cx
    mov ax,5b00h
    int 21h
    jc stage_error
    mov [handle],ax
    mov bx,ax
    mov dx,buffer
    mov cx,[file_size]
    mov ah,40h
    int 21h
    jc stage_close_error
    cmp ax,[file_size]
    jne stage_close_error
    mov bx,[handle]
    mov cx,[original_time]
    mov dx,[original_date]
    mov ax,5701h
    int 21h
    jc stage_close_error
    mov bx,[handle]
    mov ah,3eh
    int 21h
    jc stage_error
    mov dx,work_name
    call load_file
    jc stage_error
    mov ax,[file_size]
    cmp ax,[original_size]
    jne stage_error
    call crc32
    cmp ax,[expected_crc]
    jne stage_error
    cmp dx,[expected_crc+2]
    jne stage_error
    mov dx,work_name
    mov cx,[original_attrs]
    mov ax,4301h
    int 21h
    jc stage_error
    mov dx,original_name
    mov di,backup_name
    mov ah,56h
    int 21h
    jc stage_error
    mov dx,work_name
    mov di,original_name
    mov ah,56h
    int 21h
    jnc success
    mov dx,backup_name
    mov di,original_name
    mov ah,56h
    int 21h
    jc recovery_error
    jmp stage_error

; Validate an unpacked, non-overlay MZ, then find one complete return routine
; in the executable entry segment. No fixed file offset or checksum allowlist.
locate_frame:
    mov word [match_error],layout_text
    cmp word [buffer],5a4dh
    jne .bad
    cmp word [buffer+1ah],0
    jne .bad
    mov ax,[buffer+4]           ; pages, checked before the shift
    test ax,ax
    jz .bad
    cmp ax,(MAX_FILE+511)/512
    ja .bad
    mov cl,9
    shl ax,cl
    mov dx,[buffer+2]
    cmp dx,511
    ja .bad
    test dx,dx
    jz .length
    sub ax,512
    add ax,dx
.length:
    cmp ax,[file_size]
    jne .bad
    mov ax,[buffer+8]
    cmp ax,2
    jb .bad
    cmp ax,MAX_FILE/16
    ja .bad
    mov cl,4
    shl ax,cl
    cmp ax,[file_size]
    jae .bad
    mov [header_size],ax
    mov cx,[buffer+6]           ; relocation table must fit inside header
    cmp cx,MAX_FILE/4
    ja .bad
    shl cx,1
    shl cx,1
    mov bx,[buffer+18h]
    cmp bx,1ch
    jb .bad
    add cx,bx
    jc .bad
    cmp cx,ax
    ja .bad
    mov ax,[buffer+16h]         ; entry CS -> file offset
    cmp ax,MAX_FILE/16
    ja .bad
    mov cl,4
    shl ax,cl
    add ax,[header_size]
    jc .bad
    cmp ax,[file_size]
    jae .bad
    mov [code_base],ax
    mov bx,ax
    add bx,[buffer+14h]
    jc .bad
    cmp bx,[file_size]
    jae .bad
    add bx,buffer
    mov [entry_site],bx
    mov dx,[file_size]
    sub dx,CONTEXT_SIZE
    jc .bad
    cmp ax,dx
    ja .bad
    add ax,buffer
    add dx,buffer
    mov [scan_last],dx
    mov bp,ax
    mov byte [matches],0
    mov word [match_error],pattern_text
.scan:
    mov si,bp
    mov di,old_context
    call match_context
    jnc .original
    mov si,bp
    mov di,patched_context
    call match_context
    jc .next
    mov byte [patch_state],1
    jmp .found
.original:
    mov byte [patch_state],0
.found:
    inc byte [matches]
    cmp byte [matches],1
    jne .ambiguous
    mov [context_site],bp
.next:
    inc bp
    cmp bp,[scan_last]
    jbe .scan
    cmp byte [matches],1
    jne .bad

    ; The PUSH return IP must point to this routine's own CLI/XOR/MOV ES.
    mov word [match_error],layout_text
    mov bx,[context_site]
    mov ax,bx
    sub ax,buffer
    sub ax,[code_base]
    mov [routine_ip],ax
    add ax,RESUME_DELTA
    cmp ax,[bx+RESUME_IMM_DELTA]
    jne .bad
    add bx,PATCH_DELTA
    mov [patch_site],bx

    ; Require the characteristic startup CALL followed by DOSX's error flag.
    ; Its relative target must be the matched routine, not unrelated data.
    mov bp,[entry_site]
    mov dx,[context_site]
    sub dx,8
    jc .bad
    cmp bp,dx
    ja .bad
    xor cx,cx
.caller:
    cmp byte [ds:bp],0e8h
    jne .caller_next
    cmp word [ds:bp+3],06c6h
    jne .caller_next
    cmp word [ds:bp+5],0044h
    jne .caller_next
    cmp byte [ds:bp+7],1
    jne .caller_next
    mov ax,bp
    sub ax,buffer
    sub ax,[code_base]
    add ax,3
    add ax,[ds:bp+1]           ; 16-bit near-call semantics
    cmp ax,[routine_ip]
    jne .caller_next
    inc cx
.caller_next:
    inc bp
    cmp bp,dx
    jbe .caller
    cmp cx,1
    jne .bad

    ; Validate every relocation target and reject any fixup overlapping the
    ; matched instructions. Such fixups would change the code after loading.
    mov word [match_error],reloc_text
    mov si,[buffer+18h]
    add si,buffer
    mov cx,[buffer+6]
    mov bp,[context_site]
    sub bp,buffer
    mov di,bp
    dec bp                    ; a word starting one byte earlier overlaps
    add di,CONTEXT_SIZE
    mov bx,[file_size]
    dec bx
.reloc:
    jcxz .good
    mov ax,[si+2]
    cmp ax,MAX_FILE/16
    ja .bad
    push cx
    mov cl,4
    shl ax,cl
    pop cx
    add ax,[si]
    jc .bad
    add ax,[header_size]
    jc .bad
    cmp ax,bx
    jae .bad
    cmp ax,bp
    jb .reloc_next
    cmp ax,di
    jb .bad
.reloc_next:
    add si,4
    loop .reloc
.good:
    call locate_video
    ret
.ambiguous:
    mov word [match_error],ambiguous_text
.bad:
    stc
    ret

; Independently locate the GDT/video descriptor sequence and validate fixups.
; Either original or corrected form is accepted, including upgrades from 1.2.
locate_video:
    mov word [match_error],video_pattern_text
    mov bp,[code_base]
    add bp,buffer
    mov dx,[file_size]
    sub dx,VIDEO_CONTEXT_SIZE
    jc .bad
    add dx,buffer
    cmp bp,dx
    ja .bad
    mov [scan_last],dx
    mov byte [matches],0
.scan:
    mov si,bp
    mov di,video_old_context
    call match_video
    jnc .original
    mov si,bp
    mov di,video_new_context
    call match_video
    jc .next
    mov byte [video_state],1
    jmp .found
.original:
    mov byte [video_state],0
.found:
    inc byte [matches]
    cmp byte [matches],1
    jne .ambiguous
    mov [video_context_site],bp
.next:
    inc bp
    cmp bp,[scan_last]
    jbe .scan
    cmp byte [matches],1
    jne .bad
    mov ax,[video_context_site]
    add ax,VIDEO_PATCH_DELTA
    mov [video_site],ax

    mov word [match_error],reloc_text
    mov si,[buffer+18h]
    add si,buffer
    mov cx,[buffer+6]
    mov bp,[video_context_site]
    sub bp,buffer
    mov di,bp
    dec bp
    add di,VIDEO_CONTEXT_SIZE
.reloc:
    jcxz .good
    mov ax,[si+2]
    shl ax,4
    add ax,[si]
    add ax,[header_size]   ; bounds and overflow already checked by locate_frame
    cmp ax,bp
    jb .reloc_next
    cmp ax,di
    jb .bad
.reloc_next:
    add si,4
    loop .reloc
.good:
    clc
    ret
.ambiguous:
    mov word [match_error],ambiguous_text
.bad:
    stc
    ret

match_video:
    push si
    push di
    push cx
    mov cx,VIDEO_CONTEXT_SIZE
    repe cmpsb
    je .equal
    stc
    jmp .done
.equal:
    clc
.done:
    pop cx
    pop di
    pop si
    ret

; 181-byte reviewed instruction context. Only the saved resume IP is masked;
; its value is independently checked against MZ entry CS and candidate offset.
match_context:
    push si
    push di
    push bx
    push cx
    mov bx,context_mask
    mov cx,CONTEXT_SIZE
.byte:
    lodsb
    xor al,[di]
    and al,[bx]
    jnz .different
    inc di
    inc bx
    loop .byte
    clc
    jmp .done
.different:
    stc
.done:
    pop cx
    pop bx
    pop di
    pop si
    ret

print_site:
    mov bx,[patch_site]
    mov di,offset_digits
    mov dx,offset_text
    call print_offset
    mov bx,[video_site]
    mov di,video_offset_digits
    mov dx,video_offset_text
    call print_offset
    ret
print_offset:
    sub bx,buffer
    mov cx,4
.hex:
    rol bx,4
    mov al,bl
    and al,15
    add al,'0'
    cmp al,'9'
    jbe .digit
    add al,7
.digit:
    stosb
    loop .hex
    mov ah,9
    int 21h
    ret

must_be_absent:
    mov ax,4300h
    int 21h
    jnc .exists
    cmp ax,2
    jne .exists
    clc
    ret
.exists:
    stc
    ret

load_file:
    mov ax,3d00h
    int 21h
    jc .return
    mov [handle],ax
    mov bx,ax
    mov dx,buffer
    mov cx,MAX_FILE+1
    mov ah,3fh
    int 21h
    jc .failed
    cmp ax,40h
    jb .failed
    cmp ax,MAX_FILE
    ja .failed
    mov [file_size],ax
    mov bx,[handle]
    mov ax,5700h
    int 21h
    jc .failed
    mov [file_time],cx
    mov [file_date],dx
    mov bx,[handle]
    mov ah,3eh
    int 21h
.return:
    ret
.failed:
    mov bx,[handle]
    mov ah,3eh
    int 21h
    stc
    ret

crc32:
    mov dx,0ffffh
    mov ax,dx
    mov si,buffer
    mov cx,[file_size]
.byte:
    xor al,[si]
    inc si
    mov bl,8
.bit:
    shr dx,1
    rcr ax,1
    jnc .next
    xor ax,8320h
    xor dx,0edb8h
.next:
    dec bl
    jnz .bit
    loop .byte
    not ax
    not dx
    ret

stage_close_error:
    mov bx,[handle]
    mov ah,3eh
    int 21h
stage_error: mov dx,stage_text
    jmp error
usage_error: mov dx,usage_text
    jmp error
memory_error: mov dx,memory_text
    jmp error
mode_error: mov dx,mode_text
    jmp error
input_error: mov dx,input_text
    jmp error
readonly_error: mov dx,readonly_text
    jmp error
collision_error: mov dx,collision_text
    jmp error
recovery_error: mov dx,recovery_text
    jmp error
checked:
    mov dx,check_text
    cmp byte [patch_state],1
    jne good_exit
    cmp byte [video_state],1
    jne good_exit
already_patched: mov dx,already_text
    jmp good_exit
success: mov dx,success_text
good_exit:
    mov ah,9
    int 21h
    mov ax,4c00h
    int 21h
error:
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h

check_only db 0
matches db 0
patch_state db 0
video_state db 0
video_site dw 0
video_context_site dw 0
file_size dw 0
original_size dw 0
header_size dw 0
code_base dw 0
entry_site dw 0
routine_ip dw 0
scan_last dw 0
context_site dw 0
patch_site dw 0
match_error dw 0
expected_crc dd 0
handle dw 0
file_time dw 0
file_date dw 0
original_time dw 0
original_date dw 0
original_attrs dw 0
original_name db 'DOSX.EXE',0
backup_name db 'DOSX.B13',0
work_name db 'DOSX.NEW',0
old_context: incbin "OLDCTX.BIN"
patched_context: incbin "NEWCTX.BIN"
context_mask: incbin "CTXMASK.BIN"
new_bytes: incbin "PATCH.BIN"
video_old_context: incbin "VIDOLD.BIN"
video_new_context: incbin "VIDNEW.BIN"
video_new_bytes: incbin "VIDPATCH.BIN"
banner db 'M28W3 1.3 - tested on Olivetti M28 BIOS 1.08; ROM identity is not checked.',13,10
 db 'BIOS 2.12 uses a different return stack: do not apply this fix to it.',13,10,'$'
offset_text db 'Matched DOSX return frame at file offset '
offset_digits db '0000h.',13,10,'$'
video_offset_text db 'Matched DOSX video query at file offset '
video_offset_digits db '0000h.',13,10,'$'
video_pattern_text db 'Compatible DOSX video/GDT routine not found. No files changed.',13,10,'$'
success_text db 'DOSX.EXE patched and verified (17+30 byte regions). Previous file saved as DOSX.B13.',13,10,'$'
already_text db 'Both matching routines are already patched. No files changed.',13,10,'$'
check_text db 'Compatible routines found. Check only: no files changed.',13,10,'$'
usage_text db 'Usage: M28W3 [/C] from WINDOWS\SYSTEM, outside Windows.',13,10
 db '/C finds and validates the patch location without changing files.',13,10,'$'
memory_text db 'Not enough DOS memory. No files changed.',13,10,'$'
mode_text db 'Run from real-mode DOS 3.0 or later, outside Windows (286+).',13,10,'$'
input_text db 'Cannot read DOSX.EXE, or file exceeds 52 KiB. No files changed.',13,10,'$'
readonly_text db 'DOSX.EXE is read-only. No files changed.',13,10,'$'
layout_text db 'Unsupported MZ layout or inconsistent return/caller address. No files changed.',13,10,'$'
pattern_text db 'Compatible complete DOSX return routine not found. No files changed.',13,10,'$'
ambiguous_text db 'Multiple matching routines: placement is ambiguous. No files changed.',13,10,'$'
reloc_text db 'Invalid relocation or relocation overlaps a matched routine. No files changed.',13,10,'$'
collision_text db 'DOSX.B13 or DOSX.NEW exists or cannot be checked. No files changed.',13,10,'$'
stage_text db 'Disk operation failed. Original remains DOSX.EXE.',13,10
 db 'DOSX.NEW may be incomplete; remove it before retrying.',13,10,'$'
recovery_text db 'Rename and rollback failed. Original is saved as DOSX.B13.',13,10
 db 'Before Windows: restore DOSX.B13 to DOSX.EXE. Keep this backup.',13,10,'$'
align 2,db 0
    times 256 db 0
stack_top:
buffer equ ((stack_top-$$+100h+15)&~15)
%if buffer+MAX_FILE+1>0fff0h
%error COM buffer would overflow segment
%endif
