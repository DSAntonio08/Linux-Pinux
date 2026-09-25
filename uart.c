#include <stdio.h>

const unsigned long REG_PORT_G = 0x02000120; // Adresse des Registers vom GPIO-Port G

void uart_init(void);
// void gpio_init(void); // for LED
void uart_transmit(void);

static inline __attribute__((__always_inline__)) u_int32_t read32(unsigned long addr) {
    return (*((volatile unsigned int *)(addr))); // return the value stored at the address held in addr
}

static inline __attribute__((__always_inline__)) void write32(unsigned long addr, u_int32_t val) {
    *(volatile unsigned int *)(addr) = val; // write the "new" value into the address
}

int main(void) {
    // gpio_init();
    uart_init();
}

void uart_init(void) {
    // Configure UART1/PG6,7 to Tx and Rx

    unsigned long addr = REG_PORT_G;
    u_int32_t val = read32(addr);
    u_int32_t mask = (0xf << 24); // set the 4bit Value of PG6 to 1111
    mask = ~mask;                 // change all 1s to 0s
    val = val & mask;             // use a AND-Gate to bring the mask over the existing 4bit config in PG6
    val |= (2 << 24);             // short-form, but does the same. left-shift the 2 by 24 (6 times 4bit) and then use a OR to write 0010
    write32(addr, val);           // actually WRITE the new config Exact Same thing for PG7
    val = read32(addr);
    val &= ~(0xf << 28); // 28 instead of 24 bc of +4bit
    val |= (2 << 28);
    write32(addr, val);
}
