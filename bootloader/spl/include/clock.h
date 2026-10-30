/*
 * clock.h - CCU (Clock Controller Unit) setup, implemented in clock.c
 */

#pragma once

#include "../include/t113_regs.h"
#include <stdint.h>

void clock_init_uart();
int clock_enable_pll(uint32_t offset);