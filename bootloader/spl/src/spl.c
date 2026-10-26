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
#include "../include/uart.h"

int main(void) {
    cl_init_uart();

    // everything except clock
    uart_init();
    while (1) {
        uart_transmit('H');
        uart_transmit('E');
        uart_transmit('L');
        uart_transmit('L');
        uart_transmit('O');
        uart_transmit(' ');
        uart_transmit('W');
        uart_transmit('O');
        uart_transmit('R');
        uart_transmit('L');
        uart_transmit('D');
        uart_transmit('\r'); // carriage return
        uart_transmit('\n'); // new line
    }
}
