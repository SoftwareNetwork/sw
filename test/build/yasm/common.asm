global main
extern ExitProcess  ; external functions in system libraries
extern MessageBoxA

section .data
title:  db 'Win64', 0
msg:    db 'Hello world!', 0

section .text
main:
    sub rsp, 28h
    mov rcx, 0       ; hWnd = HWND_DESKTOP
    lea rdx,[msg]    ; LPCSTR lpText
    lea r8,[title]   ; LPCSTR lpCaption
    mov r9d, 0       ; uType = MB_OK
    call MessageBoxA
    add rsp, 28h

    mov  ecx,eax
    call ExitProcess

    hlt     ; never here

