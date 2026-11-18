/*
 * sd.h - SD card layer (set up the card, read blocks), implemented in sd.c
 *
 * This is what main.c uses to read from the microSD card. The controller underneath is smhc.h
 *   sd_init()        set up the card once, before any read
 *   sd_read_block()  read one 512-byte sector into a buffer of 128 words
 * Only SDHC/SDXC cards (> 2 GB) are supported
 */

#pragma once

#include <stdint.h>

int sd_init(void);
int sd_read_block(uint32_t sector, uint32_t *buf);
