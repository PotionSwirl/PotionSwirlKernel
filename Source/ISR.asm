BITS 64


%MACRO MAKE_ISR 1

GLOBAL ISR_%1
EXTERN IDT_Vector%1InterruptHandler

ISR_%1:
    PUSH RAX
    PUSH RCX
    PUSH RDX
    PUSH RSI
    PUSH RDI
    PUSH R8
    PUSH R9
    PUSH R10
    PUSH R11
    CALL IDT_Vector%1InterruptHandler
    POP R11
    POP R10
    POP R9
    POP R8
    POP RDI
    POP RSI
    POP RDX
    POP RCX
    POP RAX
    IRETQ

%ENDMACRO


MAKE_ISR 33
MAKE_ISR 64


; Special case for exceptions

GLOBAL ISR_14
EXTERN IDT_Vector14ExceptionHandler

ISR_14:

    PUSH RAX
    PUSH RCX
    PUSH RDX
    PUSH R8
    PUSH R9
    PUSH R10
    PUSH R11

    ; Pass error code as argument into C function
    MOV RDI, [RSP + 72]
    CALL IDT_Vector14ExceptionHandler

    POP R11
    POP R10
    POP R9
    POP R8
    POP RDX
    POP RCX
    POP RAX

    ADD RSP, 8      ; Clean up error code that was pushed to stack

    IRETQ
