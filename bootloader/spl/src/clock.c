/*
 * clock.c - everything that touches the CCU (Clock Controller Unit)
 *
 * cl_init_uart():
 *   1. APB1 clock -> 24 MHz source, no dividers (UART1 hangs on APB1, the baud divisor in uart.c relies on this)
 *   2. open UART1 clock gate + take it out of reset (UART_BGR_REG, p.118-119)
 */

#include "../include/clock.h"
#include "../include/io.h"

void cl_init_uart(void) {
    // Set APB1 (Advanced Peripheral Bus 1) to 24MHz for sure

    uintptr_t addr = CCU_BASE + CCU_APB1_CLK;
    uint32_t val = read32(addr);
    val &= ~((0x3u << 24) | (0x3u << 8) | 0x1fu);
    write32(addr, val);

    // release UART1 reset (first!)

    // UART_BGR_REG (CCU + 0x90C), bit 17 = UART1_RST, 1 = de-assert (manual p.119)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 17;
    write32(addr, val);

    // Open Clock Gate for UART1

    // UART_BGR_REG (CCU + 0x90C), bit 1 = UART1_GATING, 1 = pass (manual p.120)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 1;
    write32(addr, val);
}

void enPLL() {
    /*
    Bit 31: PLL_EN, the main switch.
    Bit 30: PLL_LDO_EN, the PLL’s power supply, which defaults to 1.
    Bit 29: LOCK_ENABLE, which tells the PLL to start tracking its lock.
    Bit 28: LOCK, read-only. It is 1 once the output is stable.
    Bit 27: PLL_OUTPUT_GATE, where 1 lets the clock out to the chip.
    */

    /*
    The function takes the register’s offset, so one function serves several PLLs. The steps are:

    Already on? If PLL_EN and LOCK are both set, return success without touching anything.
    This is my addition, not the manual’s.
    If the BROM already enabled the PLL and something runs from it, closing the gate would glitch that clock.

    Set PLL_EN and LDO_EN, and clear the output gate, so no clock leaves the PLL while it settles.

    Set LOCK_ENABLE.

    Wait for LOCK, with a timeout. On timeout, return an error code.
    clock.c should not print, so the caller (spl.c) decides what to do.

    Wait 20 us (the manual says so).

    Set the output gate to 1.
    */
}