/*
 * clock.c - everything that touches the CCU (Clock Controller Unit)
 *
 * clock_init_uart():
 *   1. APB1 clock -> 24 MHz source, no dividers (UART1 hangs on APB1, the baud divisor in uart.c relies on this)
 *   2. open UART1 clock gate + take it out of reset (UART_BGR_REG, p.118-119)
 *
 * clock_init_smhc():
 *   1. take SMHC0 out of reset + open its bus gate (SMHC_BGR_REG)
 *   2. SMHC0 clock: 24 MHz / 2 / 15 = 800 kHz, the controller halves it again -> 400 kHz card clock (SMHC0_CLK_REG)
 *
 * clock_enable_pll() and clock_set_pll_ddr(): see the comments above them
 *
 * NOT here: dram_clock_init() in dram.c also touches the CCU (MBUS, DRAM_BGR, DRAM_CLK)
 *   its CCU steps are interlinked with DRAM controller steps (block bus masters, then resets, then gates),
 *   so it stays in dram.c for now. Maybe moved here later, not decided yet
 *
 * NOT here: smhc_set_clock() in smhc.c also touches the CCU (SMHC0_CLK_REG), to switch the card clock to 25 MHz
 *   same reason: its CCU step sits between two SMHC controller steps (card clock off, change divider, card clock on),
 *   so it stays in smhc.c
 */

#include "../include/clock.h"
#include "../include/io.h"
#include "../include/timer.h"
#include "../include/uart.h"
#include <stdint.h>

void clock_init_uart(void) {
    // Set APB1 (Advanced Peripheral Bus 1) to 24MHz for sure

    uintptr_t addr = CCU_BASE + CCU_APB1_CLK;
    io_clearbits_le32(addr, (0x3u << 24) | (0x3u << 8) | 0x1fu);

    // release UART1 reset (first!)

    // UART_BGR_REG (CCU + 0x90C), bit 17 = UART1_RST, 1 = de-assert (manual p.119)
    addr = CCU_BASE + CCU_UART_BGR;
    io_setbits_le32(addr, 1u << 17);

    // Open Clock Gate for UART1

    // UART_BGR_REG (CCU + 0x90C), bit 1 = UART1_GATING, 1 = pass (manual p.120)
    io_setbits_le32(addr, 1u << 1);
}

void clock_init_smhc(void) {
    // release reset
    uintptr_t addr = CCU_BASE + CCU_SMHC_BGR;
    io_setbits_le32(addr, 1u << 16);

    // open glock gate
    io_setbits_le32(addr, 1u << 0);

    addr = CCU_BASE + CCU_SMHC0_CLK;

    // close clock gate  for the following clk-step-down
    io_clearbits_le32(addr, 1u << 31);

    /* 
     * step the Clock down from 24MHz to 400KHz
     * Internally there's another :2 that's why the code goes for 800kHz
     * The math is:
     * 24MHz/2/15
     * */
    io_clearsetbits_le32(addr,
                         (0x7u << 24) | (0x3u << 8) | (0xfu << 0), // clear: source, N, M
                         (0x0u << 24) | (0x1u << 8) | (14u << 0)); // set:   24 MHz, ÷2, ÷15

    io_setbits_le32(addr, 1u << 31);
}

/*
 * PLL = Phase-Locked Loop: a clock multiplier
 * the board only has a 24 MHz crystal, a PLL turns that into a much faster clock (e.g. PLL_DDR -> DRAM clock)
 * inside it an oscillator is compared to the 24 MHz over and over and corrected until both stay in step ("locked")
 * LOCK bit = 1 means the output is stable and safe to use
 */
int clock_enable_pll(uint32_t offset) {

    uintptr_t addr = CCU_BASE + offset;
    uint32_t val = io_read32(addr);

    // if the BROM already enabled it and something runs from it, closing the gate below would glitch that clock
    // BOTH bits must be set, so compare against both: (val & (A | B)) alone would be true if just one is set
    if ((val & (PLL_EN | PLL_LOCK)) == (PLL_EN | PLL_LOCK))
        return 0;

    // PLL + its power on, output gate closed, so no clock leaves the PLL while it settles
    io_clearsetbits_le32(addr, PLL_OUTPUT_GATE, PLL_EN | PLL_LDO_EN);

    // start lock detection
    io_setbits_le32(addr, PLL_LOCK_EN);

    // wait for LOCK, max 2 ms (PLL_CPU needs up to 1.5 ms, the others 500 us, p.60)
    uint32_t timeout = 2000;
    while ((io_read32(addr) & PLL_LOCK) == 0) {
        if (timeout == 0) {
            uart_puts("PLL lock timeout, CCU offset 0x");
            uart_puthex32(offset);
            uart_puts("\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    timer_udelay(20); // manual: wait 20 us after LOCK

    // open the output gate, now the PLL is usable
    io_setbits_le32(addr, PLL_OUTPUT_GATE);

    return 0;
}

/*
 * clock_set_pll_ddr: sets up the clock the DRAM runs on (used by dram_clock_init in dram.c)
 *   PLL_DDR    = 24 MHz * N, with N chosen so PLL_DDR = 2 * dram_clk (e.g. dram_clk 792 MHz -> N = 66 -> 1584 MHz)
 *   lock       LOCK_EN is cleared and set again, so the lock detection restarts with the new N
 *   DRAM_CLK   source = PLL_DDR, dividers N = M = 1, gate on
 * returns the real PLL_DDR frequency in MHz (24 * N, can be a bit lower than asked because N is rounded down), 0 = no lock
 * Source: awboot dram.c (called ccu_set_pll_ddr_clk there, lived in dram.c)
 */
uint32_t clock_set_pll_ddr(uint32_t clk) {
    uint32_t val, clk_div;

    clk_div = (clk * 2) / 24;
    val = io_read32(CCU_BASE + CCU_PLL_DDR);
    val &= 0xfff800fc;         // clear dividers
    val |= (clk_div - 1) << 8; // Set PLL division
    val |= 0xc0000000;         // enable PLL and LDO
    val &= 0xdfffffff;
    io_write32((CCU_BASE + CCU_PLL_DDR), val | 0x20000000);

    // wait for PLL to lock, max 2 ms (like clock_enable_pll), 0 = failed
    uint32_t timeout = 2000;
    while ((io_read32((CCU_BASE + CCU_PLL_DDR)) & 0x10000000) == 0) {
        if (timeout == 0)
            return 0;
        timer_udelay(1);
        timeout--;
    }
    timer_udelay(20);

    io_setbits_le32((CCU_BASE + CCU_PLL_DDR), PLL_OUTPUT_GATE); // open the PLL output gate

    // turn clock gate on
    val = io_read32((CCU_BASE + CCU_DRAM_CLK));
    val &= 0xfcfffcfc; // select DDR clk source, n=1, m=1
    val |= 0x80000000; // turn clock on
    io_write32((CCU_BASE + CCU_DRAM_CLK), val);

    return clk_div * 24;
}
