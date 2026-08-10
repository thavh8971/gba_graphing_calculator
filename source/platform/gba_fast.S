/*
 * ARM7TDMI Mode 3 span fill.
 *
 * C ABI:
 *   void gcalcArmFill16(volatile u16 *destination, u16 value, u32 count);
 *
 * The public drawing API performs all clipping before reaching this routine.
 * It handles halfword-aligned destinations, aligns to a word boundary once,
 * and then writes six pixels per STMIA iteration.  Keeping this tiny routine
 * in IWRAM avoids ROM wait states in the framebuffer hot path.
 */

    .syntax unified
    .cpu arm7tdmi
    .arm
    .section .iwram, "ax", %progbits
    .align 2
    .global gcalcArmFill16
    .type gcalcArmFill16, %function

gcalcArmFill16:
    cmp     r2, #0
    bxeq    lr

    /* A halfword-aligned odd pixel address needs one store before STR/STM. */
    tst     r0, #2
    beq     1f
    strh    r1, [r0], #2
    subs    r2, r2, #1
    bxeq    lr

1:
    /* Sanitize the narrow ABI argument, then duplicate RGB15 in both halves. */
    mov     r1, r1, lsl #16
    orr     r1, r1, r1, lsr #16
    mov     r3, r1
    mov     r12, r1

2:
    cmp     r2, #6
    blo     3f
    stmia   r0!, {r1, r3, r12}
    subs    r2, r2, #6
    b       2b

3:
    cmp     r2, #2
    blo     4f
    str     r1, [r0], #4
    subs    r2, r2, #2
    b       3b

4:
    tst     r2, #1
    strhne  r1, [r0]
    bx      lr

    .size gcalcArmFill16, . - gcalcArmFill16

    .section .note.GNU-stack, "", %progbits
