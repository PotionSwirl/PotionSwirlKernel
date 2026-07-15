BITS 64

%MACRO MAKE_ISR 1
GLOBAL ISR_%1
EXTERN IDT_Vector%1InterruptHandler

ISR_%1:
    PUSH RAX
    PUSH RCX
    PUSH RDX
    PUSH R8
    PUSH R9
    PUSH R10
    PUSH R11
    CALL IDT_Vector%1InterruptHandler
    POP R11
    POP R10
    POP R9
    POP R8
    POP RDX
    POP RCX
    POP RAX
    IRETQ

%ENDMACRO

MAKE_ISR 33
