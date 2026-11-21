/*
 * sd.h - SD card layer (set up the card, read blocks), implemented in sd.c
 *
 * This is what main.c uses to read from the microSD card. The controller underneath is smhc.h
 *   sd_init()         set up the card once, before any read (ends at 25 MHz card clock)
 *   sd_read_blocks()  read count 512-byte sectors into a buffer of count * 128 words
 * Only SDHC/SDXC cards (> 2 GB) are supported
 */

#pragma once

#include <stdint.h>

int sd_init(void);
int sd_read_blocks(uint32_t start_sector, uint32_t count, uint32_t *buf);
