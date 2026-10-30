#include "../include/gpio.h"
#include "../include/io.h"
#include "../include/t113_regs.h"
#include <stdint.h>

void gpio_set_func(uint8_t pin, uint32_t function) {
    uint32_t reg = GPIO_PG_CFG0 + (pin / 8) * 4;
    uintptr_t address = GPIO_BASE + reg;
    uint32_t val = io_read32(address);
    val &= ~(0xfu << ((pin % 8) * 4));  // clear
    val |= function << ((pin % 8) * 4); // put the function-value in
    io_write32(address, val);
}

uint8_t gpio_read(uint8_t pin) {
    uintptr_t addr = GPIO_BASE + GPIO_PG_DAT;
    uint32_t val = io_read32(addr) >> pin;
    return val & 1;
}

void gpio_write(uint8_t pin, uint32_t val) {
    uintptr_t addr = GPIO_BASE + GPIO_PG_DAT;
    uint32_t reg = io_read32(addr);
    reg &= ~(1u << pin);      // clear
    reg |= (val & 1u) << pin; // put the level in
    io_write32(addr, reg);
}