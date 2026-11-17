/*
 * sd.c - SD card layer: which commands to send, in which order, to set up the card and read blocks
 *
 * It knows the SD standard, but touches no registers. All hardware access goes through smhc.c:
 *   smhc_init()        controller on
 *   smhc_send_cmd()    send one command, get the answer
 *   smhc_read_block()  read one 512-byte block
 *
 * sd_init() (planned):
 *   1. smhc_init()
 *   2. CMD0                reset the card
 *   3. CMD8  (arg 0x1AA)   is it a v2 card, is 3.3 V ok? the card sends the pattern back
 *   4. CMD55 + ACMD41      power up, repeat until the card says ready (no CRC check for this one)
 *                          the answer also says: SDHC (address = block number) or SDSC (address = byte offset)
 *   5. CMD2                card ID
 *   6. CMD3                card gives itself an address (RCA)
 *   7. CMD7                select the card, now it can send data
 *
 * sd_read_block() (planned): turns a block number into the right address for the card type, then smhc_read_block()
 *
 * Source: SD Physical Layer Simplified Specification (sdcard.org), command table in docs/bootloader_plan.md section 5
 * Nothing written yet, not in the Makefile yet
 */

#include "../include/sd.h"
#include "../include/smhc.h"
#include "../include/timer.h"
#include "../include/uart.h"
#include <stdint.h>

/* OCR = Operation Conditions Register of the card, 32 bits
 * ACMD41: I send the argument with HCS + voltage window, the card answers with its OCR in response[0]
 * Source: SD Physical Layer Simplified Specification, OCR register + ACMD41 argument */

/* in the ACMD41 argument (what I send) */
#define SD_OCR_HCS (1u << 30)                /* Host Capacity Support: "I can handle SDHC/SDXC" */
#define SD_OCR_VOLTAGE_WINDOW (0x1ffu << 15) /* bits 23:15 = 2.7-3.6 V, "this is the voltage I supply" (= 0x00FF8000) */

/* in the answer (the card's OCR) */
#define SD_OCR_READY (1u << 31) /* 1 = card finished powering up (the spec calls it "busy", but 1 means ready) */
#define SD_OCR_CCS                                                                                                                                                                                     \
    (1u << 30) /* Card Capacity Status: 1 = SDHC/SDXC (block number), 0 = SDSC (byte offset)                                                                                                           \
                  only valid once SD_OCR_READY is 1, same bit as HCS but read, not sent */

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
            uart_puts("SD init failed: no answer to ACMD41 (power up)\r\n");
            return 1;
        }
        if (response[0] & SD_OCR_READY) {
            break;
        }
        timer_udelay(1000);
        count++;
    }
}
