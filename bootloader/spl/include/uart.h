/*
 * uart.h - UART1 serial output (115200 8N1, polling), implemented in uart.c
 */

#pragma once

#include <stdint.h>

void uart_init();
void uart_transmit(char DAT);