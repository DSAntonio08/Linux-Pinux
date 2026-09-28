#include <stdio.h>
#include <stdint.h>

const unsigned long REG_PORT_G = 0x02000120; // Adress of the Register for GPIO-Port G
const unsigned long UART_SEND_REGISTER = 0x02500400;

void uart_init(void);
// void gpio_init(void); // for LED
void uart_transmit(char DAT);

static inline __attribute__((__always_inline__)) uint32_t read32(unsigned long addr) {
    return (*((volatile unsigned int *)(addr))); // return the value stored at the address held in addr
}

static inline __attribute__((__always_inline__)) void write32(unsigned long addr, uint32_t val) {
    *(volatile unsigned int *)(addr) = val; // write the "new" value into the address
}

int main(void) {
    // gpio_init();
    uart_init();
    // Merksatz: Bei addr + 0x0c geht es 12 Bytes weiter zu einem anderen Register. Bei 1 << 7 geht es um Bit 7 innerhalb eines Registerwerts.
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
    }
}

void uart_init(void) {
    // Configure UART1/PG6,7 to Tx and Rx

    unsigned long addr = REG_PORT_G;
    uint32_t val = read32(addr);
    uint32_t mask = (0xf << 24); // set the 4bit Value of PG6 to 1111
    mask = ~mask;                 // change all 1s to 0s
    val = val & mask;             // use a AND-Gate to bring the mask over the existing 4bit config in PG6
    val |= (2 << 24);             // short-form, but does the same. left-shift the 2 by 24 (6 times 4bit) and then use a OR to write 0010
    write32(addr, val);           // actually WRITE the new config Exact Same thing for PG7
    val = read32(addr);
    val &= ~(0xfu << 28); // 28 instead of 24 bc of +4bit, u js means unsigned
    val |= (2 << 28);
    write32(addr, val);

    // Open Clock Gate for UART1

    addr = 0x0200190C;
    val = read32(addr);
    val |= 1 << 1;
    write32(addr, val);

    // Leave Reset after Boot

    addr = 0x0200190C;
    val = read32(addr);
    val |= 1 << 17;
    write32(addr, val);

    // Configure:
    // - UART1 Baudrate too 115200
    // - UART Config 8N1 (8data, 1stop, no parity)
    // - FIFO

    addr = 0x02500400;
    write32(addr + 0x04, 0x0);  // disable Interrupt by writing 0 in 0x02500404; because we send via polling, not Interrupt
    write32(addr + 0x08, 0xf7); // enable FIFO Puffer for MSGs
    val = read32(addr + 0x0c);
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

void uart_transmit(char DATA) {
    // Put whatever you wanna send into the Send-Register if it's not occupied already

    unsigned long addr = UART_SEND_REGISTER;

    while ((read32(addr + 0x7c) & (0x1 << 1)) == 0)
        ;
    write32(addr + 0x00, DATA);
}