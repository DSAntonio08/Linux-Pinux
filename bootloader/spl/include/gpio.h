/*
 * gpio.h - Port G pin config, read and write, implemented in gpio.c
 */

#pragma once

#include <stdint.h>

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
void gpio_set_func(uint8_t pin, uint32_t function);
uint8_t gpio_read(uint8_t pin);
void gpio_write(uint8_t pin, uint32_t val);
