/*
 * gpio.h - GPIO pin driver: select a pin's function, set its pull, read it, write it
 *
 * This is the whole GPIO driver, so the functions are defined here as static inline.
 * Every file that includes gpio.h gets these functions, and there is no gpio.c to compile.
 *
 * A port has a base address, and its registers are at fixed offsets from that base:
 *   PORT_BASE(port) = GPIO_BASE + port * 0x30
 *   CFG0            = PORT_BASE + 0x00
 *   DAT             = PORT_BASE + 0x10
 *   PULL            = PORT_BASE + 0x24
 * For example, PORT_G is 6, so PORT_BASE(PORT_G) = 0x02000000 + 6 * 0x30 = 0x02000120
 *
 * gpio_set_func(): each pin has a 4-bit function field, 8 pins fit in one CFG register
 *   register offset = CFG0 + (pin / 8) * 4; pin / 8 selects the register, * 4 counts bytes
 *   field start bit = (pin % 8) * 4; pin % 8 selects the pin inside that register
 *   mask            = 0xf << field start bit; 0xf is binary 1111, the 4 bits to replace
 *   clear those 4 bits, then write function into the same position; other pins stay unchanged
 *
 * gpio_set_pull(): each pin has a 2-bit pull field, 16 pins fit in one PULL register
 *   register offset = PULL + (pin / 16) * 4; pin / 16 selects the register, * 4 counts bytes
 *   field start bit = (pin % 16) * 2; pin % 16 selects the pin inside that register
 *   mask            = 0x3 << field start bit; 0x3 is binary 11, the 2 bits to replace
 *   pull: 0 = no pull, 1 = internal pull-up, 2 = internal pull-down
 *
 * gpio_read(): reads DAT, shifts the selected pin's bit down to bit 0, then & 1 keeps only it
 * gpio_write(): clears only the selected DAT bit, then writes val's lowest bit in its place
 *   (val & 1) means 0 writes low and 1 writes high; all other pins' bits stay unchanged
 *
 * The register helpers in io.h read the old 32-bit value, change only the selected bits, and
 * write it back. The registers are volatile hardware, so every read and write must happen.
 */

#pragma once

#include <stdint.h>
#include "io.h"
#include "t113_regs.h"

/*
 * gpio_set_func - select what a Port G pin does
 *
 * Every pin has a 4-bit function field in PG_CFGx (8 pins per register):
 *   register = PG_CFG0 + (pin / 8) * 4
 *   bits     = (pin % 8) * 4 ... +3
 *
 * Function codes:
 *    0  (0000)  input
 *    1  (0001)  output
 *    2-13       peripheral functions, DIFFERENT for every pin,
 *               e.g. PG6 func 2 = UART1_TX, PG7 func 2 = UART1_RX
 *               -> look up the pin multiplexing table in the manual
 *   14  (1110)  external interrupt (PG-EINTx)
 *   15  (1111)  IO disable, the reset default (pin does nothing)
 */

#define PORT_BASE(port) (GPIO_BASE + (port) * 0x30u)
#define CFG0 0x00u
#define DAT 0x10u
#define GPIO_PULL_UP_OFFSET 0x24u

enum { PULL_NONE = 0, INTERNAL_PULL_UP = 1, INTERNAL_PULL_DOWN = 2 };
enum { PORT_B = 1, PORT_C, PORT_D, PORT_E, PORT_F, PORT_G };

static inline void gpio_set_func(uint32_t port, uint32_t pin, uint32_t function) {
    uintptr_t address = PORT_BASE(port) + CFG0 + (pin / 8) * 4;
    io_clearsetbits_le32(address, 0xfu << ((pin % 8) * 4), function << ((pin % 8) * 4));
}

static inline void gpio_set_pull(uint32_t port, uint32_t pin, uint32_t pull) {
    uintptr_t address = PORT_BASE(port) + GPIO_PULL_UP_OFFSET + (pin / 16) * 4;
    io_clearsetbits_le32(address, 0x3u << ((pin % 16) * 2), pull << ((pin % 16) * 2));
}

static inline uint32_t gpio_read(uint32_t port, uint32_t pin) {
    return (io_read32(PORT_BASE(port) + DAT) >> pin) & 1;
}

static inline void gpio_write(uint32_t port, uint32_t pin, uint32_t val) {
    io_clearsetbits_le32(PORT_BASE(port) + DAT, 1u << pin, (val & 1u) << pin);
}
