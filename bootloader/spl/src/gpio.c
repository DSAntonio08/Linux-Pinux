/*
 * gpio.c - Port G pin driver: select a pin's function, read it, write it
 *
 * Only Port G for now (UART1 on PG6/PG7, LED on PG13), addresses in t113_regs.h
 *
 * gpio_set_func(): writes the pin's 4-bit function field in PG_CFGx (8 pins per register)
 * gpio_read():     returns the pin's bit from PG_DAT
 * gpio_write():    sets/clears only the pin's bit in PG_DAT, the other pins stay as they are
 */

#include "../include/gpio.h"
#include "../include/io.h"
#include "../include/t113_regs.h"
#include <stdint.h>

void gpio_set_func(uint32_t pin, uint32_t function) {
    uint32_t reg = GPIO_PG_CFG0 + (pin / 8) * 4;
    uintptr_t address = GPIO_BASE + reg;
    io_clearsetbits_le32(address, 0xfu << ((pin % 8) * 4), function << ((pin % 8) * 4));
}

uint32_t gpio_read(uint32_t pin) {
    uintptr_t addr = GPIO_BASE + GPIO_PG_DAT;
    uint32_t val = io_read32(addr) >> pin;
    return val & 1;
}

void gpio_write(uint32_t pin, uint32_t val) {
    uintptr_t addr = GPIO_BASE + GPIO_PG_DAT;
    io_clearsetbits_le32(addr, 1u << pin, (val & 1u) << pin);
}
