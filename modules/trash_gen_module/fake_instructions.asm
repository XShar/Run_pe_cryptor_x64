format MS64 COFF

include 'win64ax.inc'

public  do_Random_EAX as 'do_Random_EAX'
public  do_fake_instr as 'do_fake_instr'


section '.text' code readable executable

extrn debug_print ;Для дебага, обычный std_out в си, меняет регистры, не забывайте сохранять и восстанавливать их.)))
;Пример использования ccall _debug_print,38 (Напечатает "Debug 38" в консоле)

;---------------------------------------------
; Интерфейсная функция, для получения случайного числа в нужном интервале
; stdcall do_Random_EAX,min,max
; на выходе EAX - случайное число    
;---------------------------------------------
proc    do_Random_EAX rmin:qword,rmax:qword

        ;Сохранение регистров (На всякий случай)
       push rbx
       push rcx

       ;Инициализация генератора
       stdcall  WRandomInit

       mov   [value_min],rcx
       mov   [value_max],rdx

       ;Получить случайное число
       stdcall WIRandom,[value_min],[value_max]

       ;Восстановление регистров
       pop rcx
       pop rbx

       ret
endp

;---------------------------------------------
; Инициализация генератора случайных чисел
; stdcall WRandomInit 
;---------------------------------------------
proc    WRandomInit
        push    rax rdx
        rdtsc
        xor     rax,rdx
        mov     [random_seed],rax
        pop     rdx rax
        ret
endp

;---------------------------------------------
; Park Miller random number algorithm
; Получить случайное число 0 ... 99999
; stdcall Random_EAX
; на выходе EAX - случайное число 
;---------------------------------------------
Random_EAX:
        push    rdx rcx
        mov     rax,[random_seed]
        xor     rdx,rdx
        mov     rcx,127773
        div     rcx
        mov     rcx,rax
        mov     rax,16807
        mul     rdx
        mov     rdx,rcx
        mov     rcx,rax
        mov     rax,2836
        mul     rdx
        sub     rcx,rax
        xor     rdx,rdx
        mov     rax,rcx
        mov     [random_seed],rcx
        mov     rcx,100000
        div     rcx
        mov     rax,rdx
        pop     rcx rdx
        ret

proc    WIRandom rmin:dword,rmax:dword
        push    rdx rcx
        mov   [value_min],rcx
        mov   [value_max],rdx
        mov     rcx,[value_max]
        sub     rcx,[value_min]
        inc     rcx
        stdcall Random_EAX
        xor     rdx,rdx
        div     rcx
        mov     rax,rdx
        add     rax,[value_min]
        pop     rcx rdx
        ret
endp

;________________________________________________________
;Генерация фейковых инструкций
;________________________________________________________

proc do_fake_instr

        push rax
        push rsi
        push rdi
        push rdx
        push rbp
        push rcx

       ;Инициализация генератора
        stdcall  WRandomInit

       ;Получить случайное число от 0 до19
       stdcall WIRandom,0,19

      ;Подготовка

       db 0x48, 0xC7, 0xC1, 0xA0, 0x86, 0x01, 0x00 ;mov rcx,100000
       db 0x48, 0xFF, 0xC1  ;inc rcx
       db 0x48, 0xF7, 0xE2 ;mul rdx
       db 0x48, 0xc7, 0xc1, 0x1d, 0xf3, 0x01, 0x00 ;mov rcx,0x1f31d
       db 0x48, 0xf7, 0xf1 ;div rcx
       db 0x48, 0x29, 0xc1 ;sub rcx,rax
       db 0x48, 0x31, 0xd2 ;xor rdx,rdx

       .if rax=0
            db 0x48,0x89, 0xc1  ;mov rcx,rax
        .elseif rax=1
            db 0x48, 0xC7, 0xC0, 0xA7, 0x41, 0x00, 0x00 ;mov rax,0x41a7
        .elseif rax=2
            db 0x48, 0xC7, 0xC0, 0xCF, 0x6E, 0x00, 0x00 ;mov rax,0x6ecf
        .elseif rax=3
            db 0x48, 0x29, 0xC1 ;sub rcx,rax
        .elseif rax=4
            db 0x48, 0x31, 0xD2 ;xor rdx,rdx
        .elseif rax=5
            db 0x48, 0xC7, 0xC1, 0xA0, 0x86, 0x01, 0x00 ;mov rcx,100000
        .elseif rax=6
            db 0x48, 0xFF, 0xC1  ;inc rcx
        .elseif rax=7
            db 0x48, 0x31, 0xd0    ;xor rax,rdx
        .elseif rax=8
            db 0x48, 0xF7, 0xE2 ;mul rdx
        .elseif rax=9
            db 0x48, 0xc7, 0xc1, 0x1d, 0xf3, 0x01, 0x00 ;mov rcx,0x1f31d
        .elseif rax=10
            db 0x48, 0xf7, 0xf1 ;div rcx
        .elseif rax=11
            db 0x48, 0x29, 0xc1 ;sub rcx,rax
        .elseif rax=12
            db 0x48, 0x31, 0xd2 ;xor rdx,rdx

        .endif

        pop rcx
        pop rbp
        pop rdx
        pop rdi
        pop rsi
        pop rax
        ret
endp

section '.data' data readable writeable
random_seed     dq 0
value_min       dq 0
value_max       dq 0
value_min2       dd 0