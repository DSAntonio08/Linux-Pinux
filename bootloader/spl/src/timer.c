/*
 * timer.c - microsecond delay using the ARM Generic Timer of the Cortex-A7
 *
 * The Generic Timer has a 64-bit counter (CNTPCT) that counts up all the time, fed by the 24 MHz crystal
 * It is part of the CPU, not a T113 peripheral, so it is read over CP15 (like VBAR/SCTLR in start.S)
 * and needs no setup: 24 ticks = 1 us
 *
 * Why not a for-loop that counts: its speed depends on the CPU clock, which changes once clock.c raises PLL_CPU
 *
 * Only multiply, never divide: a 64-bit division needs libgcc (__aeabi_uldivmod), which -nostdlib doesn't link
 *
 */

#include "timer.h"

#define TICKS_PER_US 24u // 24 MHz crystal -> 24 counter ticks per microsecond

static uint64_t read_cntpct(void) {
    uint32_t low, high;
    // mrrc --> move to 2 ARM Registers from Coprocessor; CNTPCT is 64 bit, so it comes in 2 halves
    __asm__ volatile("mrrc p15, 0, %0, %1, c14" : "=r"(low), "=r"(high));
    return ((uint64_t)high << 32) | low; // glue the 2 halves together
}

void timer_udelay(uint32_t us) {
    uint64_t start = read_cntpct();
    uint64_t ticks = (uint64_t)us * TICKS_PER_US; // cast first, so the multiplication can't overflow 32 bit

    // subtracting (instead of comparing to start + ticks) still works if the counter ever wraps around
    while (read_cntpct() - start < ticks)
        ;
}
