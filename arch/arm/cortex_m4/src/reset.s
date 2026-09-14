.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.global Reset_Handler

.extern __libc_init_array

.word _sidata
.word _sdata
.word _edata
.word _sbss
.word _ebss

.section .text.Reset_Handler
.weak Reset_Handler
.type Reset_Handler, %function
Reset_Handler:
  /* Enable CP10 and CP11 so code built with the hard-float ABI can use the FPU. */
  ldr r0, =0xE000ED88
  ldr r1, [r0]
  orr r1, r1, #(0xFUL << 20)
  str r1, [r0]
  dsb
  isb

  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
1:
  cmp r0, r1
  bcc 2f
  b 3f
2:
  ldr r3, [r2], #4
  str r3, [r0], #4
  b 1b
3:
  ldr r0, =_sbss
  ldr r1, =_ebss
  movs r2, #0
4:
  cmp r0, r1
  bcc 5f
  b 6f
5:
  str r2, [r0], #4
  b 4b
6:
  bl __libc_init_array
  bl main
7:
  wfi
  b 7b
.size Reset_Handler, .-Reset_Handler


.section .note.GNU-stack,"",%progbits
