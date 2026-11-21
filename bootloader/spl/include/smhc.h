/*
 * smhc.h - SMHC0 controller driver for the microSD card (read only, polling), implemented in smhc.c
 *
 * Controller layer: init, send one command, read one block. Used by sd.c (card layer), not by main.c
 * Also holds the names for the register bits (SMHC_..._ defines)
 *
 * SMHC = Secure Digital / MultiMediaCard Host Controller (manual 7.2)
 *   the hardware block in the T113 that talks to the SD card: sends commands, gets responses, moves data
 * awboot calls the same driver sdhci (sunxi_sdhci.c), but SDHCI is a standard register layout the T113 does NOT follow
 *
 * DMA = Direct Memory Access: hardware that copies card data into RAM by itself, without the CPU
 *   not used here, my code reads every word from the FIFO itself (= polling), slower but simpler
 */

#pragma once

#include <stdint.h>

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

#define SMHC_GCTRL_DMA_ENABLE (1u << 5) /* 1 = DMA moves the data, must be 0 for polling */
#define SMHC_GCTRL_FIFO_AHB (1u << 31)  /* 1 = my code may read the FIFO, 0 = only the DMA */

/* clock_ctrl (manual: SMHC_CLKDIV, offset 0x04), p.613
 *   bits 7:0 = extra divider, 0 = no extra dividing */
#define SMHC_CLOCK_CARD_ON (1u << 16) /* 1 = clock reaches the CLK pin */

/* command (manual: SMHC_CMD, offset 0x18), p.616: command number in bits 5:0 + these flags
 *   check: CMD17 = START | WAIT_DATA_DONE | DATA | CHECK_CRC | RESPONSE | 17 = 0x80002351 (manual p.607) */
#define SMHC_CMD_RESPONSE (1u << 6)        /* the card answers this command */
#define SMHC_CMD_LONG_RESPONSE (1u << 7)   /* answer is 136 bits (R2), not 48 */
#define SMHC_CMD_CHECK_CRC (1u << 8)       /* check the checksum of the answer, NOT for ACMD41 (R3) */
#define SMHC_CMD_DATA (1u << 9)            /* data comes with this command (CMD17, CMD18) */
#define SMHC_CMD_AUTO_STOP (1u << 12)      /* send CMD12 (stop) by itself after the data (CMD18) */
#define SMHC_CMD_WAIT_DATA_DONE (1u << 13) /* wait until the last data transfer is done */
#define SMHC_CMD_INIT_SEQUENCE (1u << 15)  /* send the wake-up clock ticks before the command */
#define SMHC_CMD_UPDATE_CLOCK (1u << 21)   /* no command is sent, the controller only takes the new clock setting */
#define SMHC_CMD_START (1u << 31)          /* "do it now", the hardware sets it back to 0 when the command is sent */

/* raw_int_status (manual: SMHC_RINTSTS, offset 0x38), p.620: what happened, write 1 to clear a bit */
#define SMHC_INT_RESPONSE_ERROR (1u << 1)
#define SMHC_INT_CMD_DONE (1u << 2)           /* command sent, answer is in response0..3 */
#define SMHC_INT_DATA_DONE (1u << 3)          /* all data arrived */
#define SMHC_INT_RESPONSE_CRC_ERROR (1u << 6)
#define SMHC_INT_DATA_CRC_ERROR (1u << 7)
#define SMHC_INT_RESPONSE_TIMEOUT (1u << 8)   /* card did not answer */
#define SMHC_INT_DATA_TIMEOUT (1u << 9)       /* card did not send data */
#define SMHC_INT_FIFO_ERROR (1u << 11)        /* FIFO overflow: data arrived faster than I read it */
#define SMHC_INT_CMD_BUSY (1u << 12)          /* command written while the controller was busy */
#define SMHC_INT_DATA_START_ERROR (1u << 13)
#define SMHC_INT_AUTO_STOP_DONE (1u << 14)    /* the automatic CMD12 is done */
#define SMHC_INT_DATA_END_ERROR (1u << 15)

/* all error bits together (= 0xBBC2) */
#define SMHC_INT_ERRORS                                                                                              \
    (SMHC_INT_RESPONSE_ERROR | SMHC_INT_RESPONSE_CRC_ERROR | SMHC_INT_DATA_CRC_ERROR | SMHC_INT_RESPONSE_TIMEOUT |   \
     SMHC_INT_DATA_TIMEOUT | SMHC_INT_FIFO_ERROR | SMHC_INT_CMD_BUSY | SMHC_INT_DATA_START_ERROR |                   \
     SMHC_INT_DATA_END_ERROR)

/* status (manual: SMHC_STATUS, offset 0x3C), p.622 */
#define SMHC_STATUS_FIFO_EMPTY (1u << 2) /* 1 = nothing to read in the FIFO */
#define SMHC_STATUS_CARD_BUSY (1u << 9)  /* 1 = card still working (holds D0 low), wait before the next command */

int smhc_init(void);
int smhc_send_cmd(uint32_t index, uint32_t arg, uint32_t flags, uint32_t *response);
int smhc_read_block(uint32_t sector, uint32_t *buf);
int smhc_set_clock(void);
