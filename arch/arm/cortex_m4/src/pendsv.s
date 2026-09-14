.syntax unified
.cpu cortex-m4
.fpu fpv4-sp-d16
.thumb

.section .text.PendSV_Handler,"ax",%progbits
.global PendSV_Handler
.type PendSV_Handler, %function
.thumb_func
PendSV_Handler:
    /* Reserve eight bytes on MSP and preserve EXC_RETURN across the C++ call. R0's slot will later hold PRIMASK. */
    push {r0, lr}

    bl artos_context_started
    ldr lr, [sp, #4]
    cmp r0, #0
    beq .Lfirst_task

    /* Save outgoing context with interrupts enabled. Higher-priority ISRs must not modify scheduler state. */
    mrs r0, psp
    isb

    /* FPU Used? */
    tst r14, #0x10
    it eq
    vstmdbeq r0!, {s16-s31}
    stmdb r0!, {r4-r11, r14}

    mrs r1, primask
    cpsid i
    str r1, [sp]

    bl artos_context_switch
    b .Lrestore

.Lfirst_task:
    bl artos_context_start
.Lrestore:
    /* R0 is the incoming SP, Scheduler bookeeping is complete, so allow interrupts before restoring registers */
    ldr r1, [sp]
    msr primask, r1

    ldmia r0!, {r4-r11, r14}

    /* FPU Used? */
    tst r14, #0x10
    it eq
    vldmiaeq r0!, {s16-s31}

    msr psp, r0
    isb

    /* Restore the previous interrupt mask and discard the outgoing LR into
     * scratch R1. Keep the incoming EXC_RETURN in LR for exception return.
     */
    pop {r0, r1}
    bx lr
.size PendSV_Handler, .-PendSV_Handler

.section .text.start_first_task,"ax",%progbits
.global start_first_task
.type start_first_task, %function
.thumb_func
start_first_task:
    /* Read SCB->VTOR, then the vector table's initial stack pointer.
     * Reset MSP to discard the startup call stack; this function never returns.
     * The scheduler must already have selected next_thread.
     */
    ldr r0, =0xE000ED08
    ldr r0, [r0]
    ldr r0, [r0]
    msr msp, r0
    /* Clear CONTROL to select privileged Thread mode using MSP and clear
     * the FP-context-active flag. Unmask interrupts and faults, then
     * synchronize before requesting the first context switch.
     */
    mov r0, #0
    msr control, r0
    cpsie i
    cpsie f
    dsb
    isb
    /* Set PENDSVSET (bit 28) in SCB->ICSR. PendSV's startup path restores
     * the selected task's stack and enters it through exception return.
     */
    ldr r0, =0xE000ED04
    ldr r1, =0x10000000
    str r1, [r0]
    dsb
    isb
.Lwait_first_task:
    /* Wait for PendSV to take over; there is no startup context to resume. */
    b .Lwait_first_task
.size start_first_task, .-start_first_task

.section .note.GNU-stack,"",%progbits
