/* ARM adaptation of Steve Wozniak's Apple-1 monitor (1976).
   Provenance and licensing status: ../PROVENANCE.md. */

.syntax unified
.cpu cortex-m4
.thumb

/* Byte offsets into C++ monitor_workspace; monitor.cpp checks them with offsetof.
   LDR/STR access 32-bit pointers/values; LDRB/STRB access individual bytes. */
.equ WS_CTX, 0
.equ WS_GETCHAR, 4
.equ WS_PUTCHAR, 8
.equ WS_IN, 12
.equ WS_XAM, 140
.equ WS_ST, 144
.equ WS_HEX, 148
.equ WS_YSAV, 152
.equ WS_MODE, 153

.equ XAM, 0x00
/* Mode values match the shifts of seven-bit ASCII ':' and '.' below.
   Future mode handling must use these values or explicit bit masks. */
.equ STOR, 0x74
.equ BLOCK_XAM, 0xB8

.section .text.wozmon, "ax", %progbits
.global wozmon
.type wozmon, %function
.thumb_func

/* Entry: r0 = monitor_workspace*.
   Persistent registers (preserved by the C++ callbacks):
     r4 = workspace, r5 = input buffer, r6 = editing/parsing index.
   Scratch registers:
     r0 = character/digit, r2 = hex accumulator, r3 = callback/temporary.
   r0-r3 and flags may change across callbacks. Our putchar helper explicitly
   preserves r0 so the caller can test the character after echo. */
wozmon:
    push {r4-r6, lr}
    mov r4, r0

    b escape

notcr:
    /* Character already stored and echoed. Editing characters adjust the
       index so the next input overwrites their buffer slot. The explicit
       128-byte limit replaces the 6502's eight-bit sign test. */
    cmp r0, #0x08                   /* backspace? */
    beq backspace
    cmp r0, #0x1B                   /* ESC? */
    beq escape
    add r6, r6, #1
    cmp r6, #0x80
    blo nextchar

escape:
    movs r0, #0x5C                  /* Backslash prompt. */
    bl putchar

getline:
    /* Input ends on CR; output uses CR/LF. Start with index 1, then fall
       through backspace to index 0. Backspace at index 0 restarts the line. */
    movs r0, #0x0D                  /* CR */
    bl putchar
    movs r0, #0x0A                  /* LF */
    bl putchar

    mov r6, #1

backspace:
    subs r6, r6, #1
    bmi getline

nextchar:
    /* Blocking input. Store the low byte at in[index], including the final
       CR. The parser relies on that CR to terminate its buffer scan. */
    bl getchar
    add r5, r4, #WS_IN
    strb r0, [r5, r6]
   
    bl putchar                     /* Helper preserves the character in r0. */
    cmp r0, #0x0D
    bne notcr

    /* Each line starts in examine mode at in[0]. */
    mov r0, #XAM
    strb r0, [r4, #WS_MODE]
    mov r6, #0
    b nextitem
setblock:
    lsl r0, r0, #1
setstor:
    lsl r0, r0, #1
setmode:
    strb r0, [r4, #WS_MODE]
blskip:
    /* Consume one separator/mode character. A completed number instead
       returns to nextitem so its terminating character gets processed. */
    adds r6, r6, #1
nextitem:
    add r5, r4, #WS_IN
    ldrb r0, [r5, r6]
    cmp r0, #0x0D                   /* CR? */
    beq getline
    cmp r0, #0x2E                   /* "."? */
    bcc blskip
    beq setblock
    cmp r0, #0x3A                   /* ":"? */
    beq setstor
    cmp r0, #0x52                   /* "R"? */
    beq runprog
    /* Reset the accumulator and full 32-bit hex field for each number.
       Save its starting index to detect an empty digit sequence. */
    mov r2, #0
    str r2, [r4, #WS_HEX]
    strb r6, [r4, #WS_YSAV]
nexthex:
    /* Decode uppercase ASCII '0'..'9' or 'A'..'F' into the nibble 0..15.
       Unsigned BLO/BLS/BHI implement the inclusive character ranges. */
    ldrb r0, [r5, r6]
    cmp r0, #0x30
    blo nothex
    cmp r0, #0x39
    bls decimal_digit

    cmp r0, #0x41
    blo nothex
    cmp r0, #0x46
    bhi nothex

    subs r0, r0, #0x37             /* 'A' (0x41) - 0x37 = 10. */
    b dig

decimal_digit:
    /* TODO: subtract from r0 (the ASCII character), not r9.
       Use subs r0, r0, #0x30 to turn '0'..'9' into 0..9. */
    subs r0, r0, #0x30
dig:
    /* Append the decoded nibble: value = (value << 4) | digit.
       "2AF" should accumulate as 0x2, then 0x2A, then 0x2AF.
       A 32-bit accumulator discards overflow beyond eight hex digits. */
    lsl r2, r2, #4
    orr r2, r2, r0
    adds r6, r6, #1
    b nexthex

nothex:
    /* Leave the first non-hex character unconsumed. An unchanged index means
       no digit was accepted, so abandon the malformed command. */
    ldrb r3, [r4, #WS_YSAV]
    cmp r6, r3
    beq escape
    
    str r2, [r4, #WS_HEX]

tonextitem:
    ldrb r0, [r4, #WS_MODE]
    cmp r0, #STOR
    beq storebyte
    cmp r0, #BLOCK_XAM
    beq examnext

    /* A new address establishes both examine and store cursors. */
    str r2, [r4, #WS_XAM]
    str r2, [r4, #WS_ST]
    b examheader

storebyte:
    ldr r3, [r4, #WS_ST]
    strb r2, [r3], #1
    str r3, [r4, #WS_ST]
    b nextitem

examheader:
    movs r0, #0x0D
    bl putchar
    movs r0, #0x0A
    bl putchar
    ldr r0, [r4, #WS_XAM]
    bl printaddress
    movs r0, #0x3A
    bl putchar
exambyte:
    movs r0, #0x20
    bl putchar
    ldr r3, [r4, #WS_XAM]
    ldrb r0, [r3]
    bl printbyte
examnext:
    /* Examine through the inclusive endpoint, with eight bytes per row.
       Reload cursors after output because callbacks may clobber r0-r3. */
    ldr r3, [r4, #WS_XAM]
    ldr r2, [r4, #WS_HEX]
    cmp r3, r2
    bhs examdone
    adds r3, r3, #1
    str r3, [r4, #WS_XAM]
    tst r3, #7
    beq examheader
    b exambyte
examdone:
    movs r0, #XAM
    strb r0, [r4, #WS_MODE]
    b nextitem

runprog:
    ldr r3, [r4, #WS_XAM]
    orr r3, r3, #1
    blx r3
    b getline
    
    /* Current behavior: if the called program returns, return to main.
       Decide whether it should instead resume the monitor. */
    pop {r4-r6, pc}

.size wozmon, .-wozmon

.type printbyte, %function
.thumb_func
printbyte:
    lsls r0, r0, #24
    movs r1, #2
    b printhex
.size printbyte, .-printbyte

.type printaddress, %function
.thumb_func
printaddress:
    movs r1, #8
printhex:
    /* Preserve the input buffer and parser index across callback output. */
    push {r5-r7, lr}
    mov r5, r0
    mov r6, r1
printdigit:
    lsrs r0, r5, #28
    lsls r5, r5, #4
    cmp r0, #10
    ite lo
    addlo r0, r0, #0x30
    addhs r0, r0, #0x37
    bl putchar
    subs r6, r6, #1
    bne printdigit
    pop {r5-r7, pc}
.size printaddress, .-printaddress

.type getchar, %function
.thumb_func
getchar:
    /* Private helper: r4 supplies workspace; return the callback's int in r0.
       Contract: blocking input returning a byte (the C++ adapter does this).
       Saving r3 supplies padding for eight-byte stack alignment. */
    push {r3, lr}                  /* Keep SP eight-byte aligned for callback. */

    ldr r0, [r4, #WS_CTX]
    ldr r3, [r4, #WS_GETCHAR]
    blx r3

    pop {r3, pc}
.size getchar, .-getchar

.type putchar, %function
.thumb_func
putchar:
    /* Private helper: accept character in r0 and preserve it across output.
       r1-r3 and flags remain scratch; BLX invokes the C++ function pointer. */
    push {r0, lr}                  /* Preserve character and align the stack. */

    mov r1, r0                     /* Callback arguments: r0 = ctx, r1 = char. */
    ldr r0, [r4, #WS_CTX]
    ldr r3, [r4, #WS_PUTCHAR]
    blx r3

    pop {r0, pc}
.size putchar, .-putchar
