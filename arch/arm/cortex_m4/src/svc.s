.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.section .text.SVC_Handler,"ax",%progbits
.global SVC_Handler
.type SVC_Handler, %function
.thumb_func
SVC_Handler:
.size SVC_Handler, .-SVC_Handler

.section .note.GNU-stack,"",%progbits
