.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.section .text.SVC_Handler,"ax",%progbits

.weak service_call
.type service_call, %function
service_call:
  bx lr
.size service_call, .-service_call

.global SVC_Handler

.type SVC_Handler, %function
.thumb_func
SVC_Handler:
    tst lr, #4
    ite eq
    mrseq r1, msp
    mrsne r1, psp

    tst lr, #0x10
    bne 1f
    add r1, r1, #72
1:
    ldr r2, [r1, #24]
    ldrb r0, [r2, #-2]
    b service_call

.size SVC_Handler, .-SVC_Handler

.section .note.GNU-stack,"",%progbits
