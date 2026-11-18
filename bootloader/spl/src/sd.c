/*
 * sd.c - SD card layer: which commands to send, in which order, to set up the card and read blocks
 *
 * It knows the SD standard, but touches no registers. All hardware access goes through smhc.c:
 *   smhc_init()        controller on
 *   smhc_send_cmd()    send one command, get the answer
 *   smhc_read_block()  read one 512-byte block
 *
 * sd_init(): brings the card from power-on into "transfer" state (ready to read)
 *   1. smhc_init(): controller on, 400 kHz card clock
 *   2. CMD0: reset the card (idle state)
 *   3. CMD8: "v2 card, 3.3 V?", the card must echo 0x1AA back
 *   4. CMD55 + ACMD41, repeat until the card says ready (max ~1 s)
 *   5. CCS bit: only SDHC/SDXC are supported, SDSC is refused
 *   6. CMD2: card ID
 *   7. CMD3: card gives itself an address (RCA)
 *   8. CMD7 with the RCA: select the card (transfer state)
 *
 * sd_read_block(): reads one 512-byte sector; only SDHC/SDXC, so the sector number is the address, passed straight to smhc_read_block()
 *
 * Both return 0 on success, 1 on error, errors are printed over UART
 * Still missing: switch to 25 MHz after sd_init
 * Not in the Makefile yet, not tested on hardware
 *
 * Source: SD Physical Layer Simplified Specification (sdcard.org), command table in docs/bootloader_plan.md section 5
 */

#include "../include/sd.h"
#include "../include/smhc.h"
#include "../include/timer.h"
#include "../include/uart.h"
#include <stdint.h>

/* OCR = Operation Conditions Register of the card, 32 bits
 * ACMD41: I send the argument with HCS + voltage window, the card answers with its OCR in response[0]
 * Source: SD Physical Layer Simplified Specification, OCR register + ACMD41 argument */

/* card types, by size; they differ in how CMD17 wants the address of a block:
 *   SDSC = SD Standard Capacity,  up to 2 GB:     address = byte offset (sector * 512)
 *   SDHC = SD High Capacity,      2-32 GB:        address = sector number
 *   SDXC = SD eXtended Capacity,  32 GB - 2 TB:   same as SDHC
 * the card says which one it is with the CCS bit in its ACMD41 answer (see SD_OCR_CCS) */

/* in the ACMD41 argument (what I send) */
#define SD_OCR_HCS (1u << 30)                /* Host Capacity Support: "I can handle SDHC/SDXC" */
#define SD_OCR_VOLTAGE_WINDOW (0x1ffu << 15) /* bits 23:15 = 2.7-3.6 V, "this is the voltage I supply" (= 0x00FF8000) */

/* in the answer (the card's OCR) */
#define SD_OCR_READY (1u << 31) /* 1 = card finished powering up (the spec calls it "busy", but 1 means ready) */
/* CCS = Card Capacity Status: 1 = SDHC/SDXC (block number), 0 = SDSC (byte offset)
 * only valid once SD_OCR_READY is 1, same bit as HCS but read, not sent */
#define SD_OCR_CCS (1u << 30)

int sd_init(void) {
    uint32_t response[4];
    uint32_t count;

    if (smhc_init() != 0) {
        uart_puts("SD init failed: SMHC0 controller setup failed\r\n");
        return 1;
    }

    // go Idle State
    if (smhc_send_cmd(0, 0, SMHC_CMD_INIT_SEQUENCE, response) != 0) {
        uart_puts("SD init failed: CMD0 (reset card) was not sent\r\n");
        return 1;
    }

    // Send on the conditions that voltage == 3V3 and it's a V2-card
    if (smhc_send_cmd(8, 0x1aa, SMHC_CMD_RESPONSE | SMHC_CMD_CHECK_CRC, response) != 0) {
        uart_puts("SD init failed: no answer to CMD8 (old v1 card or no card?)\r\n");
        return 1;
    }

    if ((response[0] & 0xfff) != 0x1aa) {
        uart_puts("SD init failed: CMD8 echo wrong (expected 0x1AA in bits 11:0), response0 0x");
        uart_puthex32(response[0]);
        uart_puts("\r\n");
        return 1;
    }

    // ask the card to power up until it says ready, max 1000 rounds x 1 ms = ~1 s
    count = 0;
    while (count < 1000) {
        // argument is the card's adress (RCA) which the card only later gets from CMD3 - that's why rn it's 0
        if (smhc_send_cmd(55, 0, SMHC_CMD_RESPONSE | SMHC_CMD_CHECK_CRC, response)) {
            uart_puts("SD init failed: no answer to CMD55 (app command)\r\n");
            return 1;
        }
        // no CRC check: the R3 answer has no real checksum (all 1s)
        if (smhc_send_cmd(41, SD_OCR_HCS | SD_OCR_VOLTAGE_WINDOW, SMHC_CMD_RESPONSE, response)) {
            uart_puts("SD init failed: no answer to ACMD41 (power up)\r\n"); // ACMD = ApplicationCommand
            return 1;
        }
        if (response[0] & SD_OCR_READY) {
            break;
        }
        timer_udelay(1000);
        count++;
    }

    //
    if (count == 1000) {
        uart_puts("SD init failed: Card never got ready (ACMD41 timeout)\r\n");
        return 1;
    }

    // Check CCS
    if ((SD_OCR_CCS & response[0]) == 0) {
        uart_puts("SD init failed: SD card too small - not supported\r\n");
        return 1;
    }

    // Get Card-ID (of every Card on the bus - for me 1)
    if (smhc_send_cmd(2, 0, SMHC_CMD_RESPONSE | SMHC_CMD_LONG_RESPONSE | SMHC_CMD_CHECK_CRC, response) != 0) {
        uart_puts("SD init failed: no answer to CMD2 (card ID)\r\n");
        return 1;
    }

    // get 16bit RCA (Relative Card Address)
    if (smhc_send_cmd(3, 0, SMHC_CMD_RESPONSE | SMHC_CMD_CHECK_CRC, response) != 0) {
        uart_puts("SD init failed: no awnser to CMD3 (RCA)\r\n");
        return 1;
    }
    uint32_t rca = response[0] >> 16;
    if (smhc_send_cmd(7, rca << 16, SMHC_CMD_RESPONSE | SMHC_CMD_CHECK_CRC, response) != 0) {
        uart_puts("SD init failed: no answer to CMD7 (select card)\r\n");
        return 1;
    }

    return 0;
}

int sd_read_block(uint32_t sector, uint32_t *buf) { return smhc_read_block(sector, buf); }
