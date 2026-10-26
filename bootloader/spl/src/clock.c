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

    uintptr_t addr =  CCU_BASE + CCU_APB1_CLK;
    uint32_t val = read32(addr);
    val &= ~((0x3u << 24) | (0x3u << 8) | 0x1fu);
    write32(addr, val);

    // Open Clock Gate for UART1

    // UART_BGR_REG (CCU + 0x90C), bit 1 = UART1_GATING, 1 = pass (manual p.120)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 1;
    write32(addr, val);

    // release UART1 reset (first!)

    // UART_BGR_REG (CCU + 0x90C), bit 17 = UART1_RST, 1 = de-assert (manual p.119)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 17;
    write32(addr, val);
}
