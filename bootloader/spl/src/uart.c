/*
 * uart.c - first-light test: sends "HELLO WORLD" over UART1 forever
 *
 * Runs on the board, freestanding (no libc!), started by start.S via main()
 *
 * Hardware: UART1 on PG6 (TX) / PG7 (RX), 3.3V levels (VCC-PG = 3V3)
 * Setting:  115200 baud, 8N1, polling (no interrupts)
 *
 * What it does:
 *   1. clocks: cl_init_uart() in clock.c (APB1 -> 24 MHz, UART1 clock gate + reset)
 *   2. PG6/PG7 -> function 2 (UART1)
 *   3. UART setup: interrupts off, FIFO on, divisor 13 (24 MHz / 16 / 13 = 115385 baud, +0.16%), 8N1
 *   4. loop: wait until TX FIFO has space, write the char
 *
 * Terminal: screen /dev/tty.usbserial-XXXX 115200
 */

#include "../include/uart.h"
#include "../include/clock.h"
#include "../include/gpio.h"
#include "../include/io.h"
#include <stdint.h>

void uart_init(void) {
    // APB1 to 24MHz + UART1 clock gate open + out of reset, all in clock.c
    cl_init_uart();

    gpio_set_func(6, 2); // UART1_TX
    gpio_set_func(7, 2); // UART1_RX

    // Configure:
    // - UART1 Baudrate too 115200
    // - UART Config 8N1 (8data, 1stop, no parity)
    // - FIFO

    uintptr_t addr = UART1_BASE;
    write32(addr + 0x04, 0x0);                 // disable Interrupt by writing 0 in 0x02500404; because we send via polling, not Interrupt
    write32(addr + 0x08, 0xf7);                // enable FIFO Puffer for MSGs
    uint32_t val = read32(addr + 0x0c);        // addr + 0x0c means go on 12bytes to another register; 1 << 7 means go 7Bit in 1 Register
    val |= (1 << 7);                           // DLAB to 1 (DLAB = a Switch, that selects the 'function' of 2 adresses (0x00 & 0x04); depending on state they do something different)
    write32(addr + 0x0c, val);                 // enable 'Baud-Rate division Register' to select Baude Rate later
    write32(addr + 0x00, 0xd & 0xff);          // write first half od 'Baud-Rate divisor'
    write32(addr + 0x04, (0xd >> 8) & 0xff);   // write second half od 'Baud-Rate divisor'
    val = read32(addr + 0x0c);                 // read LCR-state (LCR = Line Control Register; the register where the UART Settings are made)
    val &= ~(1 << 7);                          // DLAB to 0
    write32(addr + 0x0c, val);                 // update LCR
    val = read32(addr + 0x0c);                 // read new LCR state
    val &= ~0x1f;                              // 0x1f = 31 = 00011111; we ~ them so just set 5last bits to 0
    val |= (0x3 << 0) | (0 << 2) | (0x0 << 3); // Set UART frame format to 8 data bits, 1 stop bit, no parity (8N1); I set exactly those bits, bc theyre 'defined' that way in the T113's User Manual
    write32(addr + 0x0c, val);
}

void uart_putc(char c) {
    // Put whatever you wanna send into the Send-Register if it's not occupied already

    uintptr_t addr = UART1_BASE;
    while ((read32(addr + 0x7c) & (0x1 << 1)) == 0)
        ;
    write32(addr + 0x00, c);
}

void uart_puts(const char *s) {
    while (*s != 0) {
        uart_putc(*s);
        s++;
    }
}

uint32_t nibble(uint32_t val, int k) { return (val >> (4 * k)) & 0x0F; }

void uart_puthex32(uint32_t val) {
    const char *lookup = "0123456789ABCDEF";

    for (int i = 7; i >= 0; i--) {
        uint32_t n = nibble(val, i);
        uart_putc(lookup[n]);
    }
}