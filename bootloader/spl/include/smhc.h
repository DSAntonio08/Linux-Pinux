/*
 * smhc.h - microSD card driver (SMHC0, read only, polling), implemented in smhc.c
 *
 * SMHC = Secure Digital / MultiMediaCard Host Controller (manual 7.2)
 *   the hardware block in the T113 that talks to the SD card: sends commands, gets responses, moves data
 * awboot calls the same driver sdhci (sunxi_sdhci.c), but SDHCI is a standard register layout the T113 does NOT follow
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/* forward declaration - "actual" declaration lives in smhc.c */
struct smhc;

/* global_ctrl (manual: SMHC_CTRL, offset 0x00): switches controller features on/off and resets parts of it
 *   reset bit   write 1 to start the reset, the hardware sets it back to 0 when the reset is done
 *   FIFO        small buffer inside the controller, card data passes through it on the way to the CPU
 * Source: awboot sunxi_sdhci.c:45-57 */
#define SMHC_GCTRL_SOFT_RESET (1u << 0) /* resets the controller logic (state machines) */
#define SMHC_GCTRL_FIFO_RESET (1u << 1) /* empties the FIFO */
#define SMHC_GCTRL_DMA_RESET (1u << 2)  /* resets the Direct Memory Access interface */

/* HARDWARE_RESET: all three resets at once (= 0x7)
 * name taken from awboot, but it's a software reset, nothing happens on a pin */
#define SMHC_GCTRL_HARDWARE_RESET (SMHC_GCTRL_SOFT_RESET | SMHC_GCTRL_FIFO_RESET | SMHC_GCTRL_DMA_RESET)

int smhc_init(struct smhc *smhc);
