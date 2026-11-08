/*
 * clock.c - everything that touches the CCU (Clock Controller Unit)
 *
 * clock_init_uart():
 *   1. APB1 clock -> 24 MHz source, no dividers (UART1 hangs on APB1, the baud divisor in uart.c relies on this)
 *   2. open UART1 clock gate + take it out of reset (UART_BGR_REG, p.118-119)
 */

#include "../include/clock.h"
#include "../include/io.h"
#include "../include/timer.h"

void clock_init_uart(void) {
    // Set APB1 (Advanced Peripheral Bus 1) to 24MHz for sure

    uintptr_t addr = CCU_BASE + CCU_APB1_CLK;
    io_clearbits_le32(addr, (0x3u << 24) | (0x3u << 8) | 0x1fu);

    // release UART1 reset (first!)

    // UART_BGR_REG (CCU + 0x90C), bit 17 = UART1_RST, 1 = de-assert (manual p.119)
    addr = CCU_BASE + CCU_UART_BGR;
    io_setbits_le32(addr, 1 << 17);

    // Open Clock Gate for UART1

    // UART_BGR_REG (CCU + 0x90C), bit 1 = UART1_GATING, 1 = pass (manual p.120)
    io_setbits_le32(addr, 1 << 1);
}

/*
 * PLL = Phase-Locked Loop: a clock multiplier
 * the board only has a 24 MHz crystal, a PLL turns that into a much faster clock (e.g. PLL_DDR -> DRAM clock)
 * inside it an oscillator is compared to the 24 MHz over and over and corrected until both stay in step ("locked")
 * LOCK bit = 1 means the output is stable and safe to use
 */
int clock_enable_pll(uint32_t offset) {

    uintptr_t addr = CCU_BASE + offset;
    uint32_t val = io_read32(addr);

    // if the BROM already enabled it and something runs from it, closing the gate below would glitch that clock
    // BOTH bits must be set, so compare against both: (val & (A | B)) alone would be true if just one is set
    if ((val & (PLL_EN | PLL_LOCK)) == (PLL_EN | PLL_LOCK))
        return 0;

    // PLL + its power on, output gate closed, so no clock leaves the PLL while it settles
    io_clearsetbits_le32(addr, PLL_OUTPUT_GATE, PLL_EN | PLL_LDO_EN);

    // start lock detection
    io_setbits_le32(addr, PLL_LOCK_EN);

    // wait for LOCK, max 2 ms (PLL_CPU needs up to 1.5 ms, the others 500 us, p.60)
    uint32_t timeout = 2000;
    while ((io_read32(addr) & PLL_LOCK) == 0) {
        if (timeout == 0)
            return 1;
        timer_udelay(1);
        timeout--;
    }

    timer_udelay(20); // manual: wait 20 us after LOCK

    // open the output gate, now the PLL is usable
    io_setbits_le32(addr, PLL_OUTPUT_GATE);

    return 0;
}
