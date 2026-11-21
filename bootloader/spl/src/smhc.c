/*
 * smhc.c - SMHC0 driver for the microSD card (read only, polling, no DMA)
 *
 * This is the controller layer: it knows the T113 registers, but not what the commands mean
 * The card layer (which commands, in which order) is sd.c, it calls the functions here
 *
 * smhc_init():
 *   1. pins PF0-PF5 -> function 2 (SMHC0), pull-ups on (CLK, CMD, D0-D3)
 *   2. clock_init_smhc(): reset/gate + 400 kHz card clock (in clock.c)
 *   3. version register != 0 -> controller answers
 *   4. controller reset (global_ctrl), wait until done
 *   5. 1-bit bus, interrupts off, FIFO readable by my code (not the DMA), clear old status flags
 *   6. card clock on + "update clock" command, wait until the controller took it
 *   7. wait 1 ms, the card needs 74 clock ticks before the first command
 *
 * smhc_send_cmd(): sends one command to the card and gets the answer
 *   1. wait until the last command is finished
 *   2. clear old status flags
 *   3. write the argument, then the command number + flags + start bit
 *   4. wait for "command done" or an error flag
 *   5. copy the answer (response0..3) to the caller
 *   6. only CMD7: wait until the card is not busy any more
 *
 * smhc_read_block(): reads one 512-byte block into a buffer of 128 words
 *   1. block size + byte count = 512
 *   2. send CMD17 (read single block) with smhc_send_cmd()
 *   3. take the 128 words out of the FIFO one by one, watch the error flags
 *   4. wait for "data done", check the error flags once more
 *
 * smhc_set_clock(): switches the card clock 400 kHz -> 25 MHz, called by sd_init after the card is selected
 *   1. clock_enable_pll(): make sure PLL_PERI runs (returns at once if the Boot ROM already turned it on)
 *   2. card clock off + "update clock" command
 *   3. CCU gate off, SMHC0 clock = PLL_PERI(1X) 600 MHz / 1 / 12 = 50 MHz, CCU gate on (the controller halves it -> 25 MHz)
 *   4. card clock on + "update clock" command
 *
 * update_clock() (static): sends the "update clock" command and waits until the controller took it,
 *   used by smhc_init and twice by smhc_set_clock
 *
 * All four return 0 on success, 1 if a step fails (every wait loop has a timeout), errors are printed over UART
 * Works on the board at 400 kHz (2026-11-18): init, send_cmd and read_block ran in the sector 16 test (see sd.c), UART messages not seen yet
 * smhc_set_clock (25 MHz) NOT TESTED ON THE BOARD YET
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

static struct smhc_reg *const reg = (struct smhc_reg *)SMHC0_BASE;

static int update_clock(void) {

    // go update command - not done automatically
    reg->command = SMHC_CMD_START | SMHC_CMD_UPDATE_CLOCK | SMHC_CMD_WAIT_DATA_DONE;

    uint32_t timeout = 2000;
    while ((reg->command & SMHC_CMD_START) != 0) {
        if (timeout == 0) {
            uart_puts("SMHC0 clock update timeout (command start bit did not clear)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }
    return 0;
}

int smhc_init(void) {
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

    if (reg->version == 0) {
        uart_puts("SMHC0 does not answer (version register = 0), check reset/gate in clock_init_smhc\r\n");
        return 1;
    }

    /* Reset Controller, FIFO and DMA - clears whtv state the BOOT ROM left */
    reg->global_ctrl = SMHC_GCTRL_HARDWARE_RESET;

    // wait for reset-Bits to clear, max 2 ms
    uint32_t timeout = 2000;
    while (reg->global_ctrl & SMHC_GCTRL_HARDWARE_RESET) {
        if (timeout == 0) {
            uart_puts("SMHC0 reset timeout (global_ctrl reset bits did not clear)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    /*
     * Basic setup:
     * set bus-width; interrupts off
     */
    reg->bus_width = 0; // 1bit-mode (D0 only)
    reg->int_mask = 0;

    // Enable FIFO access - otherwise only DMA allowed
    io_setbits_le32((uintptr_t)&reg->global_ctrl, SMHC_GCTRL_FIFO_AHB);

    /* Clears all old Status-flags*/
    reg->raw_int_status = 0xffffffffu;

    // card clock on, the clock now reaches the CLK pin
    // "=" writes the whole register, so the extra divider (bits 7:0) is 0 = no extra dividing
    reg->clock_ctrl = SMHC_CLOCK_CARD_ON;

    if (update_clock() != 0) {
        return 1;
    }

    // card needs 74 clock ticks (~185 us at 400 kHz) before the first command
    timer_udelay(1000);

    return 0;
}

int smhc_send_cmd(uint32_t index, uint32_t arg, uint32_t flags, uint32_t *response) {
    /*
     * Steps:
     * 1. Wait until command bit 31 is 0 (the last command is finished), with a timeout. -- done
     * 2. Clear the status flags: raw_int_status = 0xffffffff. -- done
     * 3. Write command_arg, then write command = index + flags + bit 31 (start). -- done
     * 4. Poll raw_int_status until CC (bit 2, command complete) or an error bit is set, with a timeout. -- done
     * 5. Read response0..3. -- done
     * 6. For R1b responses only (CMD7): wait until status bit 9 (CARD_BUSY) is 0.
     */

    uint32_t timeout = 2000;
    while ((reg->command & SMHC_CMD_START) != 0) {
        if (timeout == 0) {
            uart_puts("SMHC0 busy: last command never finished (start bit still 1)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    // clear the status-flag
    reg->raw_int_status = 0xffffffff;

    // argument first: writing command with the start bit sends the message right away
    reg->command_arg = arg;
    // command number sits in bits 5:0, the flags and the start bit in the bits above
    reg->command = index | flags | SMHC_CMD_START;

    timeout = 2000;
    while ((reg->raw_int_status & (SMHC_INT_CMD_DONE | SMHC_INT_ERRORS)) == 0) {
        if (timeout == 0) {
            uart_puts("SMHC0 command timeout (no done and no error flag), CMD 0x");
            uart_puthex32(index);
            uart_puts("\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    // the loop ends on "done" AND on errors, so check which one it was
    if (reg->raw_int_status & SMHC_INT_ERRORS) {
        uart_puts("SMHC0 command failed, CMD 0x");
        uart_puthex32(index);
        uart_puts(", raw_int_status 0x");
        uart_puthex32(reg->raw_int_status);
        uart_puts("\r\n");
        return 1;
    }

    response[0] = reg->response0;
    response[1] = reg->response1;
    response[2] = reg->response2;
    response[3] = reg->response3;

    // wait if CMD7 is busy
    if (index == 7) {
        timeout = 100000; // 100ms - card is much slower then controller
        while ((reg->status & SMHC_STATUS_CARD_BUSY) != 0) {
            if (timeout == 0) {
                uart_puts("SMHC0 card busy timeout after CMD7 (card still holds D0 low)\r\n");
                return 1;
            }
            timer_udelay(1);
            timeout--;
        }
    }

    return 0;
}

// reads one 512-byte block (sector) into buf (128 ARM-words)
int smhc_read_block(uint32_t sector, uint32_t *buf) {
    uint32_t flags = SMHC_CMD_RESPONSE | SMHC_CMD_CHECK_CRC | SMHC_CMD_DATA | SMHC_CMD_WAIT_DATA_DONE;
    uint32_t response[4];

    reg->block_size = 512;
    reg->byte_count = 512;

    if (smhc_send_cmd(17, sector, flags, response) != 0) {
        return 1;
    }

    // collect the 128 words: the card fills the FIFO, I take the words out one by one
    uint32_t count = 0;
    uint32_t timeout = 1000000; // 1 s, the card may need a while before the first data comes
    while (count < 128) {
        if (reg->raw_int_status & SMHC_INT_ERRORS) {
            uart_puts("SMHC0 read failed, raw_int_status 0x");
            uart_puthex32(reg->raw_int_status);
            uart_puts("\r\n");
            return 1;
        }

        if ((reg->status & SMHC_STATUS_FIFO_EMPTY) == 0) {
            // a word is waiting: take it, no delay so reading stays fast
            buf[count] = reg->fifo;
            count++;
        } else {
            // nothing there yet: wait a bit, give up after the timeout
            if (timeout == 0) {
                uart_puts("SMHC0 read timeout (FIFO stays empty), words read 0x");
                uart_puthex32(count);
                uart_puts("\r\n");
                return 1;
            }
            timer_udelay(1);
            timeout--;
        }
    }

    // all words are here, now wait until the controller says the transfer is finished, max 100 ms
    timeout = 100000;
    while ((reg->raw_int_status & (SMHC_INT_DATA_DONE | SMHC_INT_ERRORS)) == 0) {
        if (timeout == 0) {
            uart_puts("SMHC0 read timeout (no data done flag)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    // the loop ends on "done" AND on errors (e.g. wrong data checksum), so check which one it was
    if (reg->raw_int_status & SMHC_INT_ERRORS) {
        uart_puts("SMHC0 read failed at the end, raw_int_status 0x");
        uart_puthex32(reg->raw_int_status);
        uart_puts("\r\n");
        return 1;
    }

    return 0;
}

int smhc_set_clock(void) {
    if (clock_enable_pll(CCU_PLL_PERI) != 0)
        return 1;

    io_clearbits_le32((uintptr_t)&reg->clock_ctrl, SMHC_CLOCK_CARD_ON);
    if (update_clock() != 0) {
        return 1;
    }
    uintptr_t addr = CCU_BASE + CCU_SMHC0_CLK;

    // close clock gate for the following clk-step-up
    io_clearbits_le32(addr, 1u << 31);

    /*
     * step the Clock up from 400KHz to 25MHz
     * this obviously needs PLL because we only have 24MHz supplied externally
     * the math is:
     * 600 MHz ÷ 1 ÷ 12 = 50 MHz (plus the :2 internally)
     * */
    io_clearsetbits_le32(addr,
                         (0x7u << 24) | (0x3u << 8) | (0xfu << 0), // clear: source, N, M
                         (0x1u << 24) | (0x0u << 8) | (11u << 0)); // set:   PLL_PERI(1X) 600 MHz, ÷1, ÷12

    io_setbits_le32(addr, 1u << 31);
    io_setbits_le32((uintptr_t)&reg->clock_ctrl, SMHC_CLOCK_CARD_ON);
    if (update_clock() != 0) {
        return 1;
    }

    return 0;
}
