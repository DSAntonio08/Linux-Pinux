#include <stdint.h>
#include "bootloader/spl/include/t113_regs.h"

#define REG_PORT_G 0x02000120 // Adress of the Register for GPIO-Port G

void uart_init(void);
// void gpio_init(void); // for LED
void uart_transmit(char DAT);

static uint32_t read32(uintptr_t addr) {
    // volatile: this memory can change (or reacts to accesses) outside of this code, so the compiler must perform every read and write exactly as written, in order
    return *(volatile uint32_t *)addr; // return the value stored at the address held in addr
}

static void write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val; // write the "new" value into the address
}

int main(void) {
    // gpio_init();
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

void uart_init(void) {
    // Set ADPB1 (Advanced Peripheral Bus 1) to 24MHz for sure

    uintptr_t addr =  CCU_BASE + CCU_APB1_CLK; 
    uint32_t val = read32(addr);
    val &= ~((0x3u << 24) | (0x3u << 8) | 0x1fu);
    write32(addr, val);

    // Configure UART1/PG6,7 to Tx and Rx

    addr = GPIO_BASE + GPIO_PG_CFG0;
    val = read32(addr);
    uint32_t mask = (0xf << 24); // set the 4bit Value of PG6 to 1111
    mask = ~mask;                // change all 1s to 0s
    val = val & mask;            // use a AND-Gate to bring the mask over the existing 4bit config in PG6
    val |= (2 << 24);            // short-form, but does the same. left-shift the 2 by 24 (6 times 4bit) and then use a OR to write 0010
    write32(addr, val);          // actually WRITE the new config Exact Same thing for PG7
    val = read32(addr);
    val &= ~(0xfu << 28); // 28 instead of 24 bc of +4bit, u js means unsigned
    val |= (2 << 28);
    write32(addr, val);

    // Open Clock Gate for UART1

    // UART_BGR_REG (CCU + 0x90C), bit 1 = UART1_GATING, 1 = pass (manual p.119)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 1;
    write32(addr, val);

    // Leave Reset after Boot

    // UART_BGR_REG (CCU + 0x90C), bit 17 = UART1_RST, 1 = de-assert (manual p.118)
    addr = CCU_BASE + CCU_UART_BGR;
    val = read32(addr);
    val |= 1 << 17;
    write32(addr, val);

    // Configure:
    // - UART1 Baudrate too 115200
    // - UART Config 8N1 (8data, 1stop, no parity)
    // - FIFO

    addr = UART1_BASE;
    write32(addr + 0x04, 0x0);  // disable Interrupt by writing 0 in 0x02500404; because we send via polling, not Interrupt
    write32(addr + 0x08, 0xf7); // enable FIFO Puffer for MSGs
    val = read32(addr + 0x0c);     // addr + 0x0c means go on 12bytes to another register; 1 << 7 means go 7Bit in 1 Register
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
    
    uintptr_t addr = UART1_BASE;
    while ((read32(addr + 0x7c) & (0x1 << 1)) == 0)
        ;
    write32(addr + 0x00, DATA);
}