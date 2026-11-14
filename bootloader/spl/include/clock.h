/*
 * clock.h - CCU (Clock Controller Unit) setup, implemented in clock.c
 */

#pragma once

#include "../include/t113_regs.h"
#include <stdint.h>

void clock_init_uart(void);
void clock_init_smhc(void);
int clock_enable_pll(uint32_t offset);
uint32_t clock_set_pll_ddr(uint32_t clk);

