bits 64

%macro MAKE_ISR 1
    global ISR_%1
    extern IDT_Vector%1InterruptHandler
    ISR_%1:
        push rax
        push rcx
        push rdx
        push r8
        push r9
        push r10
        push r11
        call IDT_Vector%1InterruptHandler
        pop r11
        pop r10
        pop r9
        pop r8
        pop rdx
        pop rcx
        pop rax
        iretq
%endmacro

MAKE_ISR 33
