/*
 * main.c - Secondary Program Loader, main() of the SPL (runs in SRAM / Static RAM)
 *
 * Runs on the board, freestanding (no libc!), started by start.S via main()
 *
 * SPLs tasks:
 *   1. establish a Stack and Initialize C globals (start.S)
 *   2. setup essential clocks and pins
 *   3. enable Serial-Output for debugging
 *   4. initialize DRAM / Dynamic RAM
 *   5. read following 'actual' Bootloader into DRAM
 *   6. jump to actual Bootloader
 *
 * Right now: 1-4, plus an SD test instead of 5 (sd_init, read sector 16 with sd_read_blocks, check "eGON.BT0")
 *   and a multi-sector test (test B, docs/phase_f.md): read 1024 sectors from 2048 into DRAM, check every word
 *   4 works on the board (2026-11-18): dram_init() incl. its write/read test returns 0, seen via the LED
 *   SD test passed on the board at 25 MHz (2026-11-24): "Hello World" comes after it, every fail path returns 1 before
 *   3 works on the board (2026-11-24): "Hello World" seen in the terminal
 *   multi-sector test passed on the board (2026-11-24), seen via the LED
 *   whole UART output seen (2026-11-25, logic analyzer on PG6 + sigrok-cli): every step prints its OK line,
 *     from dram_init's "ZQ value" to "multi-sector test OK: 1024 sectors". Fail messages not seen yet (nothing failed)
 *   LED: blinks every 100 ms = DRAM + SD init + SD test + multi-sector test OK, dark = something failed (UART will say what)
 */

#include "../include/clock.h"
#include "../include/dram.h"
#include "../include/gpio.h"
#include "../include/io.h"
#include "../include/sd.h"
#include "../include/t113_regs.h"
#include "../include/timer.h"
#include "../include/uart.h"

static void dump_clock_state(void);
static int multi_sector_test(void);

int main(void) {
    uint32_t buf[128]; // exactly one sector

    clock_init_uart();
    uart_init(); // everything UART except clock
    // dump_clock_state();

    gpio_set_func(PORT_G, 13, 1); // PG13 -> output

    if (dram_init() != 0) {
        uart_puts("DRAM init failed: stopping\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }
    uart_puts("DRAM init OK\r\n");

    if (sd_init() != 0) {
        uart_puts("SD init failed: stopping\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }
    uart_puts("SD init OK\r\n");

    /*
     * SD test: sector 16 * 512 bytes = 8192 = 8 KiB, exactly where the SPL (this program) starts on the card
     * bytes 4-11 of its eGON header must be the magic "eGON.BT0" (see start.S)
     *
     *   byte          4      5      6      7      8      9      10     11
     *   character     e      G      O      N      .      B      T      0
     *   ASCII (hex)   0x65   0x47   0x4F   0x4E   0x2E   0x42   0x54   0x30
     *                 '------- buf[1] --------'   '------- buf[2] --------'
     *
     * little endian: the first byte sits in the lowest bits of the word, so read each group backwards:
     *   buf[1] = 0x4E4F4765 ("NOGe"), buf[2] = 0x3054422E ("0TB.")
     */
    if (sd_read_blocks(16, 1, buf) != 0) {
        uart_puts("SD test failed: could not read sector 16\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }

    if (buf[1] != 0x4E4F4765) {
        uart_puts("SD test failed: sector 16, bytes 4-7 are not \"eGON\"\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }
    uart_puts("SD test: \"eGON\" found\r\n");

    if (buf[2] != 0x3054422E) {
        uart_puts("SD test failed: sector 16, bytes 8-11 are not \".BT0\"\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }
    uart_puts("SD test OK: sector 16 holds the SPL header \"eGON.BT0\"\r\n");

    if (multi_sector_test() != 0) {
        uart_puts("SD test failed: multi sector read wasn't succesful\r\n");
        return 1;
    }

    // Works, uart tested - could delete
    while (1) {
        // uart_puts("Hello World\r\n");
        gpio_write(PORT_G, 13, 1); // PG13 high (3V3), LED on
        timer_udelay(100000);
        gpio_write(PORT_G, 13, 0); // PG13 low (0V), LED off
        timer_udelay(100000);
    }

    return 0;
}

/*
read each clock register at startup and print it next to its manual reset value, so you can see what the BROM changed.
*/
static void dump_clock_state(void) {

    struct clock_reg {
        const char *name;
        uint32_t offset;
        uint32_t reset_value;
    };

    static const struct clock_reg clock_regs[] = {
        {"PLL_CPU_CTRL", CCU_PLL_CPU, 0x4A001000}, 
        {"PLL_DDR_CTRL", CCU_PLL_DDR, 0x48002301}, 
        {"PLL_PERI_CTRL", CCU_PLL_PERI, 0x48216300}, 
        {"CPU_AXI_CFG", CCU_CPU_AXI_CFG, 0x00000301},
        {"PSI_CLK", CCU_PSI_CLK, 0x00000000},      
        {"APB0_CLK", CCU_APB0_CLK, 0x00000000},    
        {"APB1_CLK", CCU_APB1_CLK, 0x00000000},
    };

    for (uint32_t i = 0; i < sizeof(clock_regs) / sizeof(clock_regs[0]); i++) {
        uint32_t val = io_read32(CCU_BASE + clock_regs[i].offset);
        if (val != clock_regs[i].reset_value) {
            uart_puts(clock_regs[i].name);
            uart_puts(": ");
            uart_puthex32(val);
            uart_puts(" (reset ");
            uart_puthex32(clock_regs[i].reset_value);
            uart_puts(")");
            if (val != clock_regs[i].reset_value)
                uart_puts(" CHANGED");
            uart_puts("\r\n");
        }
    }
}

/*
 * Multi-sector test (test B, docs/phase_f.md section 0), temporary
 * pattern.bin is on the card at 1 MiB = sector 2048: 1024 sectors, word w of sector s = (s << 16) | w
 * read all of it with ONE sd_read_blocks call into DRAM, then check every word
 */
static int multi_sector_test(void) {
    uint32_t *dram = (uint32_t *)0x40000000;

    for (uint32_t i = 0; i < 1024 * 128; i++) {
        dram[i] = 0xFFFFFFFF; // never in the pattern, so a word the read didn't write shows up
    }

    uart_puts("reading 1024 sectors...\r\n");
    if (sd_read_blocks(2048, 1024, dram) != 0) {
        uart_puts("multi-sector test failed: could not read\r\n");
        gpio_write(PORT_G, 13, 0);
        return 1;
    }

    for (uint32_t s = 0; s < 1024; s++) {
        for (uint32_t w = 0; w < 128; w++) {
            uint32_t expected = (s << 16) | w;
            uint32_t got = dram[s * 128 + w]; // sector s starts at word s * 128

            if (got != expected) {
                uart_puts("pattern mismatch: sector 0x");
                uart_puthex32(s);
                uart_puts(", word 0x");
                uart_puthex32(w);
                uart_puts(": got 0x");
                uart_puthex32(got);
                uart_puts(", expected 0x");
                uart_puthex32(expected);
                uart_puts("\r\n");
                gpio_write(PORT_G, 13, 0);
                return 1;
            }
        }
    }
    uart_puts("multi-sector test OK: 1024 sectors\r\n");
    return 0;
}
