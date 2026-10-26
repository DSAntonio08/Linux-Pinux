/*
 * timer.h - busy-wait delay for the SPL (needed later by DRAM + SD init)
 */

#pragma once

#include <stdint.h>

void udelay(uint32_t us);
