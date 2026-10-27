/*
 * uart.h - UART1 serial output (115200 8N1, polling), implemented in uart.c
 */

#pragma once

#include <stdint.h>

void uart_init();
void uart_putc(char c);
void uart_puts(const char *s) ;
void uart_puthex32(uint32_t val);
