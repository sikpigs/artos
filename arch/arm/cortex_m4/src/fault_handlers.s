.syntax unified
.cpu cortex-m4
.thumb

/* A separate stack allows capture even when the interrupted MSP is corrupt.
 * Never return from a fault. No FP instructions: do not trigger lazy stacking.
 */
.section .bss.fault_stack,"aw",%nobits
.balign 8
fault_stack:
  .space 512
fault_stack_top:

.section .text.fault_entry,"ax",%progbits
.macro fault_handler name
.global \name
.type \name, %function
.thumb_func
\name:
  b fault_entry
.size \name, .-\name
.endm

fault_handler HardFault_Handler
fault_handler MemManage_Handler
fault_handler BusFault_Handler
fault_handler UsageFault_Handler

.type fault_entry, %function
.thumb_func
fault_entry:
  cpsid i
  mrs r2, msp
  mrs r3, psp
  tst lr, #4
  ite eq
  moveq r0, r2
  movne r0, r3
  mov r1, lr
  ldr r4, =fault_stack_top
  msr msp, r4
  isb
  b fault_capture
.size fault_entry, .-fault_entry

.section .note.GNU-stack,"",%progbits
