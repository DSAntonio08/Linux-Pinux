/*
 * spl.c - Secondary Program Loader, main() of the SPL (runs in SRAM / Static RAM)
 *
 * Runs on the board, freestanding (no libc!), started by start.S via main()
 *
 * SPLs tasks:
 *   1. establish a Stack and Initialize C globals (start.S)
 *   2. setup essential clocks and pins
 *   3. enable Serial-Output for debugging
 *   4. initialize DRAM / Dynamic RAM
 *   5. read following 'actual' Bootloader into DRAM
 *   6. jump to actual Bootloader
 *
 * Right now: only 1-3, then sends "HELLO WORLD" over UART1 forever
 */

#include "../include/clock.h"
#include "../include/io.h"
#include "../include/t113_regs.h"
#include "../include/uart.h"

void dump_clock_state();

int main(void) {
    cl_init_uart();
    // everything UART except clock
    uart_init();

    while (1) {
        uart_puts("Hello World\r\n");
    }
}

void dump_clock_state() {
    /*
    read each clock register at startup and print it next to its manual reset value, so you can see what the BROM changed.
    */

    struct clock_reg {
        const char *name;
        uint32_t offset;
        uint32_t reset_value;
    };

    static const struct clock_reg clock_regs[] = {
        {"PLL_CPU_CTRL", CCU_PLL_CPU, 0x4A001000}, 
        {"PLL_DDR_CTRL", CCU_PLL_DDR, 0x48002301}, 
        {"PLL_PERI_CTRL", CCU_PLL_PERI, 0x48216300}, 
        {"CPU_AXI_CFG", CCU_CPU_AXI_CFG, 0x00000301},
        {"PSI_CLK", CCU_PSI_CLK, 0x00000000},      
        {"APB0_CLK", CCU_APB0_CLK, 0x00000000}, 
        {"APB1_CLK", CCU_APB1_CLK, 0x00000000},
    };

    for (int i = 0; i < sizeof(clock_regs) / sizeof(clock_regs[0]); i++) {
        uint32_t val = read32(CCU_BASE + clock_regs[i].offset);
        if (val != clock_regs[i].reset_value) {
            uart_puts(clock_regs[i].name);
            uart_puts(": ");
            uart_puthex32(val);
            uart_puts(" (reset ");
            uart_puthex32(clock_regs[i].reset_value);
            uart_puts(")");
            if (val != clock_regs[i].reset_value)
                uart_puts(" CHANGED");
            uart_puts("\r\n");
        }
    }
}