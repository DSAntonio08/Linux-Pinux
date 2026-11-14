/*
 * smhc.c - SMHC0 driver for the microSD card (read only, polling, no DMA)
 *
 * smhc_init():
 *   1. pins PF0-PF5 -> function 2 (SMHC0), pull-ups on (CLK, CMD, D0-D3)
 *   2. clock_init_smhc(): reset/gate + 400 kHz card clock (in clock.c)
 *   3. version register != 0 -> controller answers
 *   4. controller reset (global_ctrl), wait until done, clear old status flags
 *   5. card clock on + "update clock" command, wait until the controller took it
 *
 * Returns 0 on success, 1 if a step fails (every wait loop has a timeout), errors are printed over UART
 * Not called from main yet, not tested on hardware
 */

#include "../include/smhc.h"
#include "../include/clock.h"
#include "../include/gpio.h"
#include "../include/timer.h"
#include "../include/uart.h"
#include <stdint.h>

// this struct is needed for the next struct
// every field is one register, in address order, so field N sits at SMHC0_BASE + its offset
// names: mine; manual name + offset in the comment; reserved = gaps without registers
struct smhc_reg {
    volatile uint32_t global_ctrl;           /* (0x000) SMHC_CTRL: resets, global enables */
    volatile uint32_t clock_ctrl;            /* (0x004) SMHC_CLKDIV: card clock on/off + divider */
    volatile uint32_t timeout_ctrl;          /* (0x008) SMHC_TMOUT: response/data timeouts */
    volatile uint32_t bus_width;             /* (0x00C) SMHC_CTYPE: 1/4/8 bit bus */
    volatile uint32_t block_size;            /* (0x010) SMHC_BLKSIZ */
    volatile uint32_t byte_count;            /* (0x014) SMHC_BYTCNT: bytes per data transfer */
    volatile uint32_t command;               /* (0x018) SMHC_CMD: command number + flags, bit 31 = start */
    volatile uint32_t command_arg;           /* (0x01C) SMHC_CMDARG: argument of the command */
    volatile uint32_t response0;             /* (0x020) SMHC_RESP0 */
    volatile uint32_t response1;             /* (0x024) SMHC_RESP1 */
    volatile uint32_t response2;             /* (0x028) SMHC_RESP2 */
    volatile uint32_t response3;             /* (0x02C) SMHC_RESP3 */
    volatile uint32_t int_mask;              /* (0x030) SMHC_INTMASK: which events may raise an interrupt */
    volatile uint32_t masked_int_status;     /* (0x034) SMHC_MINTSTS: raw_int_status AND int_mask */
    volatile uint32_t raw_int_status;        /* (0x038) SMHC_RINTSTS: what happened (cmd done, errors, ...), W1C */
    volatile uint32_t status;                /* (0x03C) SMHC_STATUS: FIFO level, card busy, ... */
    volatile uint32_t fifo_threshold;        /* (0x040) SMHC_FIFOTH: FIFO watermarks (for DMA) */
    volatile uint32_t function_select;       /* (0x044) SMHC_FUNS */
    volatile uint32_t card_byte_count;       /* (0x048) SMHC_TCBCNT: bytes moved between card and controller */
    volatile uint32_t bus_byte_count;        /* (0x04C) SMHC_TBBCNT: bytes moved between controller and RAM */
    volatile uint32_t debug_ctrl;            /* (0x050) SMHC_DBGC */
    volatile uint32_t crc_status_detect;     /* (0x054) SMHC_CSDC */
    volatile uint32_t auto_cmd12_arg;        /* (0x058) SMHC_A12A: argument for the automatic CMD12 (stop) */
    volatile uint32_t new_timing_set;        /* (0x05C) SMHC_NTSR: bit 31 = new timing mode */
    volatile uint32_t reserved1[6];          /* (0x060 - 0x074) */
    volatile uint32_t hardware_reset;        /* (0x078) SMHC_HWRST: eMMC reset pin */
    volatile uint32_t reserved2;             /* (0x07C) */
    volatile uint32_t dma_ctrl;              /* (0x080) SMHC_IDMAC: internal DMA on/off */
    volatile uint32_t dma_desc_list_addr;    /* (0x084) SMHC_DLBA: where the DMA descriptor list starts */
    volatile uint32_t dma_status;            /* (0x088) SMHC_IDST */
    volatile uint32_t dma_int_enable;        /* (0x08C) SMHC_IDIE */
    volatile uint32_t dma_current_desc_addr; /* (0x090) SMHC_CHDA */
    volatile uint32_t dma_current_buf_addr;  /* (0x094) SMHC_CBDA */
    volatile uint32_t reserved3[26];         /* (0x098 - 0x0FC) */
    volatile uint32_t card_threshold_ctrl;   /* (0x100) SMHC_THLD */
    volatile uint32_t sample_fifo_ctrl;      /* (0x104) SMHC_SFC */
    volatile uint32_t reserved4;             /* (0x108) */
    volatile uint32_t ddr_start_bit_detect;  /* (0x10C) EMMC_DDR_SBIT_DET: eMMC DDR only */
    volatile uint32_t reserved5[12];         /* (0x110 - 0x13C) */
    volatile uint32_t drive_delay;           /* (0x140) SMHC_DRV_DL: output phase */
    volatile uint32_t sample_delay;          /* (0x144) SMHC_SMAP_DL: input sample point */
    volatile uint32_t data_strobe_delay;     /* (0x148) SMHC_DS_DL: eMMC HS400 only */
    volatile uint32_t hs400_delay;           /* (0x14C) SMHC_HS400_DL: eMMC HS400 only */
    volatile uint32_t reserved6[4];          /* (0x150 - 0x15C) */
    volatile uint32_t deskew_data0_delay;    /* (0x160) */
    volatile uint32_t deskew_data1_delay;    /* (0x164) */
    volatile uint32_t deskew_data2_delay;    /* (0x168) */
    volatile uint32_t deskew_data3_delay;    /* (0x16C) */
    volatile uint32_t deskew_data4_delay;    /* (0x170) */
    volatile uint32_t deskew_data5_delay;    /* (0x174) */
    volatile uint32_t deskew_data6_delay;    /* (0x178) */
    volatile uint32_t deskew_data7_delay;    /* (0x17C) */
    volatile uint32_t deskew_ds_delay;       /* (0x180) */
    volatile uint32_t deskew_ctrl;           /* (0x184) */
    volatile uint32_t reserved7[30];         /* (0x188 - 0x1FC) */
    volatile uint32_t fifo;                  /* (0x200) SMHC_FIFO: read card data here, one 32-bit word at a time */
    volatile uint32_t reserved8[63];         /* (0x204 - 0x2FC) */
    volatile uint32_t version;               /* (0x300) SMHC_VERS: fixed value, != 0 if the block answers */
};

// I need to go over the struct again and get everything in detail
struct smhc {
    char *name;           // idk name for what
    struct smhc_reg *reg; // use the register struct inside this "main" struct
    uint8_t id;
    uint32_t reset;

    uint32_t voltage;
    uint32_t width;

    uint32_t pclk;
    uint32_t dma_trglvl;

    bool removable;
    bool isspi;
};

int smhc_init(struct smhc *smhc) {
    smhc->reg = (struct smhc_reg *)SMHC0_BASE;

    // D1
    gpio_set_func(PORT_F, 0, 2);
    gpio_set_pull(PORT_F, 0, INTERNAL_PULL_UP);
    // D0: Data-Line
    gpio_set_func(PORT_F, 1, 2);
    gpio_set_pull(PORT_F, 1, INTERNAL_PULL_UP);
    // CLK
    gpio_set_func(PORT_F, 2, 2);
    gpio_set_pull(PORT_F, 2, INTERNAL_PULL_UP);
    // Commands and Responses
    gpio_set_func(PORT_F, 3, 2);
    gpio_set_pull(PORT_F, 3, INTERNAL_PULL_UP);
    // D3: Card-Detect
    gpio_set_func(PORT_F, 4, 2);
    gpio_set_pull(PORT_F, 4, INTERNAL_PULL_UP);
    // D2
    gpio_set_func(PORT_F, 5, 2);
    gpio_set_pull(PORT_F, 5, INTERNAL_PULL_UP);

    // init_default_timing() from awboot - skipped for now

    clock_init_smhc();

    if (smhc->reg->version == 0) {
        uart_puts("SMHC0 does not answer (version register = 0), check reset/gate in clock_init_smhc\r\n");
        return 1;
    }

    /* Reset Controller, FIFO and DMA - clears whtv state the BOOT ROM left */
    smhc->reg->global_ctrl = SMHC_GCTRL_HARDWARE_RESET;
    // wait for reset-Bits to clear, max 2 ms
    uint32_t timeout = 2000;
    while (smhc->reg->global_ctrl & SMHC_GCTRL_HARDWARE_RESET) {
        if (timeout == 0) {
            uart_puts("SMHC0 reset timeout (global_ctrl reset bits did not clear)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }
    /* Clears all old Status-flags*/
    smhc->reg->raw_int_status = 0xffffffffu;

    // get the clock from controller to pin (last step)
    io_setbits_le32((uintptr_t)&smhc->reg->clock_ctrl, 1u << 16);
    // tell the controller to switch - otherwise ignored
    smhc->reg->command = (1u << 31) | (1u << 21) | (1u << 13);
    //               "do it now"   "only clock"  "wait if busy"

    // wait until does
    timeout = 2000;
    while ((smhc->reg->command & (1u << 31)) != 0) {
        if (timeout == 0) {
            uart_puts("SMHC0 clock update timeout (command start bit did not clear)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    return 0;
}
