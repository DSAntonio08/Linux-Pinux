/*
 * dram.h - DRAM init (built-in 128 MB DDR3), implemented in dram.c
 *
 * dram_init() returns 0 on success, 1 on failure. Call it once, after clocks and UART are up
 */

#pragma once

#include <stdint.h>

int dram_init(void);
