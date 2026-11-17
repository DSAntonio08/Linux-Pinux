/*
 * sd.h - SD card layer (set up the card, read blocks), implemented in sd.c
 *
 * This is what main.c uses to read from the microSD card. The controller underneath is smhc.h
 * No functions yet, planned: sd_init(), sd_read_block()
 */

#pragma once

int sd_init(void);
