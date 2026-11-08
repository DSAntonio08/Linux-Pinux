#include "../include/dram.h"
#include "../include/dram_regs.h"
#include "../include/io.h"
#include "../include/t113_regs.h"
#include "../include/timer.h"
#include "../include/uart.h"
#include <stdint.h>

#define DIV_ROUND_UP(a, b) (((a) + (b) - 1) / (b))

struct dram_config {
    uint32_t dram_clk;
    uint32_t dram_type;   // dram_type DDR2: 2    DDR3: 3    LPDDR2: 6    LPDDR3: 7   DDR3L: 31
    uint32_t dram_zq;     // val for the lower 24bit of ZQCR
    uint32_t dram_odt_en; // controls the termination on the controller side

    /*
     * Control config:
     * 128 MB DDR3, 16 bit wide:
     * 8 banks * 8192 rows * 1024 columns * 2 bytes = 134217728 B = 128 MB
     */
    uint32_t dram_geometry;   // para1
    uint32_t dram_rank_width; // para2

    /*
     * Timing config:
     * MR = Mode Register, settings INSIDE the DDR3 chip (not the T113)
     * written into the RAM once at init, bit meanings from the JEDEC DDR3 standard
     *   MR0  burst length, CAS latency (read delay), write recovery
     *   MR1  output drive strength, termination (ODT)
     *   MR2  CAS write latency (write delay)
     *   MR3  special modes, normally 0
     *
     * TPR = Allwinner's "timing parameters", but really a mix of PHY tuning values and flags
     * (only the ones my DDR3 setup reads, tpr0-4 and tpr7-9 are unused for it)1
     *   TPR5   Vref (reference voltage) for the data lines -> IOVCR0
     *   TPR6   Vref for the address/command lines -> IOVCR1
     *   TPR10  extra delays for clock/address lines (0 = none)
     *   TPR11  delay fine-tuning per data lane, read side
     *   TPR12  delay fine-tuning per data lane, write side
     *   TPR13  option flags, each bit switches one feature on/off
     */
    uint32_t dram_mr0;
    uint32_t dram_mr1;
    uint32_t dram_mr2;
    uint32_t dram_mr3;
    uint32_t dram_tpr5;
    uint32_t dram_tpr6;
    uint32_t dram_tpr10;
    uint32_t dram_tpr11;
    uint32_t dram_tpr12;
    uint32_t dram_tpr13;
};

static uint32_t dram_ns_to_cycles(const struct dram_config *cfg, uint32_t nanosecs) {
    const uint32_t ctrl_freq = cfg->dram_clk / 2;
    return DIV_ROUND_UP(ctrl_freq * nanosecs, 1000);
}

static void dram_allow_all_master(void) {
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER0), ~0);
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER1), 0xff);
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER2), 0xffff);
    timer_udelay(10);
}

static void dram_deny_all_master(void) {
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER0), 1);
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER1), 0);
    io_write32((MCTL_COM_BASE + MCTL_COM_MAER2), 0);
    timer_udelay(10);
}

static uint32_t ccu_set_pll_ddr_clk(const struct dram_config *cfg) {
    uint32_t val, clk, clk_div;

    clk = cfg->dram_clk;

    clk_div = (clk * 2) / 24;
    val = io_read32(CCU_BASE + CCU_PLL_DDR);
    val &= 0xfff800fc;         // clear dividers
    val |= (clk_div - 1) << 8; // Set PLL division
    val |= 0xc0000000;         // enable PLL and LDO
    val &= 0xdfffffff;
    io_write32((CCU_BASE + CCU_PLL_DDR), val | 0x20000000);

    // wait for PLL to lock, max 2 ms (like clock_enable_pll), 0 = failed
    int timeout = 2000;
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

static int dram_clock_init(struct dram_config *cfg) {
    uint32_t clk;

    // hold MBUS in reset
    io_clearbits_le32((CCU_BASE + CCU_MBUS_CLK), (1 << 30));

    io_clearbits_le32((CCU_BASE + CCU_DRAM_BGR), 0x10001);
    // DRAM_CLK_REG: bit 31 = clock gate off, bit 30 = module reset asserted (0 = held in reset), bit 27 = update (apply the change)
    // bit 30 may be "reserved" in the T113 manual, name from U-Boot's H6 CCU (DRAM_MOD_RESET), Allwinner's DRAM code uses it on the T113 too
    io_clearsetbits_le32((CCU_BASE + CCU_DRAM_CLK), (1 << 31) | (1 << 30), (1 << 27));
    timer_udelay(10);

    // Set ddr pll clock
    clk = ccu_set_pll_ddr_clk(cfg);
    if (clk == 0) {
        uart_puts("PLL_DDR lock timeout\r\n");
        return 1;
    }
    cfg->dram_clk = clk / 2;
    timer_udelay(100);
    dram_deny_all_master();

    // release sdram reset
    io_setbits_le32((CCU_BASE + CCU_DRAM_BGR), 1 << 16);

    // release MBUS reset
    io_setbits_le32((CCU_BASE + CCU_MBUS_CLK), 1 << 30);
    io_setbits_le32((CCU_BASE + CCU_DRAM_CLK), 1 << 30);

    timer_udelay(5);

    // turn on sdram clock gate
    io_setbits_le32((CCU_BASE + CCU_DRAM_BGR), 1);

    // turn dram clock gate on, trigger sdram clock gate update
    io_setbits_le32((CCU_BASE + CCU_DRAM_CLK), (1 << 31) | (1 << 27));
    timer_udelay(5);

    // memory Controller clock enable
    io_write32((MCTL_PHY_BASE + MCTL_PHY_CLKEN), 0x8000);
    timer_udelay(10);

    return 0;
}

/*
 * ZQ calibration: tunes the strength of the chip's signal drivers
 * every chip comes out of the factory a little different, and its resistance drifts with temperature and voltage
 * so the T113 compares its drivers against an exact reference: R30, 240 ohm on the ZQ pin
 * it adjusts them until they match, so the signals to the RAM are clean (no overshoot, no ringing)
 */
static void dram_zq_init(const struct dram_config *cfg) {
    // Test ZQ Status
    if (cfg->dram_tpr13 & (1 << 16)) {
        uart_puts("DRAM only has internal ZQ\r\n");
        io_setbits_le32((SYS_CFG_BASE + ZQ_CAL_CTRL), 1 << 8);
        io_write32((SYS_CFG_BASE + ZQ_RES_CTRL), 0);
        timer_udelay(10);
    } else {
        io_clearbits_le32((SYS_CFG_BASE + ZQ_CAL_CTRL), 0x3);
        io_write32((AON_PRCM_BASE + ANALOG_PWROFF_GATING), cfg->dram_tpr13 & (1 << 16));
        timer_udelay(10);
        io_clearsetbits_le32((SYS_CFG_BASE + ZQ_CAL_CTRL), 0x108, 2);
        timer_udelay(10);
        io_setbits_le32((SYS_CFG_BASE + ZQ_CAL_CTRL), 1);
        timer_udelay(20);
        uart_puts("ZQ value = 0x");
        uart_puthex32(io_read32(SYS_CFG_BASE + ZQ_RES_STATUS));
        uart_puts("\r\n");
    }
}

/*
 * Vref = reference voltage: the PHY's receivers compare every incoming signal against it (above = 1, below = 0)
 * should sit in the middle of the signal swing (~VCC-DRAM / 2), values from tpr5 (data lines) and tpr6 (address/command)
 */
static void dram_vref_init(const struct dram_config *cfg) {
    if (cfg->dram_tpr13 & (1 << 17))
        return;

    io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_IOVREF_CR0), 0x7f7f7f7f, cfg->dram_tpr5);

    if (!(cfg->dram_tpr13 & (1 << 16)))
        io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_IOVREF_CR1), 0x7f, cfg->dram_tpr6 & 0x7f);
}

/*
 * tells the controller WHAT RAM is attached: type (DDR3), width (16 bit), ranks, banks, rows, columns
 * needed so it can split a CPU address into bank / row / column, uses dram_geometry and dram_rank_width
 */
static void mctl_com_init(const struct dram_config *cfg) {
    uint32_t val;

    // Set controller wait time
    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_DBGCR), 0x3f00, 0x2000);

    // Set SDRAM type and data-bus width
    val = ((cfg->dram_type & 0x7) << 16) |        // DRAM type
          ((~cfg->dram_rank_width & 0x1) << 12) | // DQ width
          (1 << 22);                              // burst length 8 (DDR3 always uses BL8)

    if (cfg->dram_tpr13 & (1 << 5))
        val |= (1 << 19); // 1T instead of 2T

    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_WORK_MODE0), 0x00fff000, val);

    // Set rank, banks, rows and page size
    // awboot loops twice when 2 ranks have a different layout, I only have 1 rank -> once is enough
    val = (cfg->dram_rank_width >> 12) & 0x3;             // rank
    val |= ((cfg->dram_geometry >> 12) << 2) & 0x4;       // bank - 2
    val |= (((cfg->dram_geometry >> 4) - 1) << 4) & 0xff; // row - 1

    // page size (KB) -> column address width - 3
    switch (cfg->dram_geometry & 0xf) {
    case 8:
        val |= 0xa00;
        break;
    case 4:
        val |= 0x900;
        break;
    case 2:
        val |= 0x800;
        break;
    case 1:
        val |= 0x700;
        break;
    default:
        val |= 0x600;
        break;
    }

    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_WORK_MODE0), 0xfff, val);

    val = (io_read32(MCTL_COM_BASE + MCTL_COM_WORK_MODE0) & 0x1) ? 0x303 : 0x201;
    io_write32((MCTL_PHY_BASE + MCTL_PHY_ODTMAP), val);

    if (cfg->dram_rank_width & 1)
        io_write32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(1)), 0);
}

/*
 * AC remap tables: which physical pin each address/command signal goes out on
 * one table per chip variant, the eFuse value picks which one (see mctl_phy_addrcomm_remapping)
 *   22 entries   one per AC signal, which entry is which signal is not documented
 *   value        pin number (5 bit), packed into REMAP0..3
 *   [0]          all 0 = no remap, straight wiring
 * DDR3 only uses [0]..[5] and [7], [6] [8] [9] are for DDR2 / H133 chips
 * Source: awboot dram.c, numbers copied 1:1
 */
static const uint8_t ac_remapping_tables[][22] = {
    [0] = {0},
    /* FPGA Verify DDR REMAP */
    [1] = {0x1, 0x9, 0x3, 0x7, 0x8, 0x12, 0x4, 0xD, 0x5, 0x6, 0xA, 0x2, 0xE, 0xC, 0x0, 0x0, 0x15, 0x11, 0x14, 0x13, 0xB, 0x16}, // Generic DDR3 Type1
    [2] = {0x4, 0x9, 0x3, 0x7, 0x8, 0x12, 0x1, 0xD, 0x2, 0x6, 0xA, 0x5, 0xE, 0xC, 0x0, 0x0, 0x15, 0x11, 0x14, 0x13, 0xB, 0x16}, // Generic DDR3 Type C
    [3] = {0x1, 0x7, 0x8, 0xC, 0xA, 0x12, 0x4, 0xD, 0x5, 0x6, 0x3, 0x2, 0x9, 0x0, 0x0, 0x0, 0x15, 0x11, 0x14, 0x13, 0xB, 0x16}, // Generic DDR3 Type 8
    [4] = {0x4, 0xC, 0xA, 0x7, 0x8, 0x12, 0x1, 0xD, 0x2, 0x6, 0x3, 0x5, 0x9, 0x0, 0x0, 0x0, 0x15, 0x11, 0x14, 0x13, 0xB, 0x16}, // Generic DDR3 Type 9
    [5] = {0xD, 0x2, 0x7, 0x9, 0xC, 0x13, 0x5, 0x1, 0x6, 0x3, 0x4, 0x8, 0xA, 0x0, 0x0, 0x0, 0x15, 0x16, 0x12, 0x11, 0xB, 0x14}, // Generic DDR3 Type bf
    /* ASIC Chip */
    [6] = {0x3, 0xA, 0x7, 0xD, 0x9, 0xB, 0x1, 0x2, 0x4, 0x6, 0x8, 0x5, 0xC, 0x0, 0x0, 0x0, 0x14, 0x12, 0x0, 0x15, 0x16, 0x11},   // DDR2
    [7] = {0x3, 0x2, 0x4, 0x7, 0x9, 0x1, 0x11, 0xC, 0x12, 0xE, 0xD, 0x8, 0xF, 0x6, 0xA, 0x5, 0x13, 0x16, 0x10, 0x15, 0x14, 0xB}, // DDR3 D1-H
    [8] = {0x2, 0x13, 0x8, 0x6, 0xE, 0x5, 0x14, 0xA, 0x3, 0x12, 0xD, 0xB, 0x7, 0xF, 0x9, 0x1, 0x16, 0x15, 0x11, 0xC, 0x4, 0x10}, // DDR3 H133
    [9] = {0x1, 0x2, 0xD, 0x8, 0xF, 0xC, 0x13, 0xA, 0x3, 0x15, 0x6, 0x11, 0x9, 0xE, 0x5, 0x10, 0x14, 0x16, 0xB, 0x7, 0x4, 0x12}, // DDR2 H133
};

static void mctl_phy_addrcomm_remapping(const struct dram_config *cfg) {
    const uint8_t *remap;
    uint32_t val;
    uint32_t fuse = (io_read32(SID_BASE + SID_EFUSE) & 0xf00) >> 8;
    uint32_t chip_id = io_read32(SID_BASE + SID_CHIPID) & 0xffff;

    uart_puts("DDR efuse: 0x");
    uart_puthex32(fuse);
    uart_puts("\r\n");

    uart_puts("chip id efuse: 0x");
    uart_puthex32(chip_id);
    uart_puts("\r\n");

    if (cfg->dram_tpr13 & 0xc0000) {
        remap = ac_remapping_tables[7];
    } else {
        /* Keep the switch for none
         * once I know the value I can exachnge it
         * with just some "precaution" if(!)
         * and the actual value for my T113-S3 */
        switch (fuse) {
        case 8:
            remap = ac_remapping_tables[2];
            break;
        case 9:
            remap = ac_remapping_tables[3];
            break;
        case 10:
            if (chip_id == 0x6800) { // 0x6800 is T113-S4 no remap
                remap = ac_remapping_tables[0];
            } else {
                remap = ac_remapping_tables[5];
            }
            break;
        case 11:
            remap = ac_remapping_tables[4];
            break;
        default:
        case 12:
            remap = ac_remapping_tables[1];
            break;
        case 13:
        case 14:
            remap = ac_remapping_tables[0];
            break;
        }
    }
    val = ((remap[4] << 25) | (remap[3] << 20) | (remap[2] << 15) | (remap[1] << 10) | (remap[0] << 5));
    io_write32(MCTL_COM_BASE + MCTL_COM_REMAP0, val);

    val = ((remap[10] << 25) | (remap[9] << 20) | (remap[8] << 15) | (remap[7] << 10) | (remap[6] << 5) | (remap[5] << 0));
    io_write32(MCTL_COM_BASE + MCTL_COM_REMAP1, val);

    val = ((remap[15] << 20) | (remap[14] << 15) | (remap[13] << 10) | (remap[12] << 5) | (remap[11] << 0));
    io_write32(MCTL_COM_BASE + MCTL_COM_REMAP2, val);

    val = (remap[21] << 25) | (remap[20] << 20) | (remap[19] << 15) | (remap[18] << 10) | (remap[17] << 5) | (remap[16] << 0);
    io_write32((MCTL_COM_BASE + MCTL_COM_REMAP3), val);

    io_setbits_le32((MCTL_COM_BASE + MCTL_COM_REMAP0), 1);
}

static void mctl_set_timing_params(const struct dram_config *cfg) {
    // row timing (open a row = activate, close a row = precharge)
    uint8_t t_act_window;    // tFAW: max 4 row opens in this time
    uint8_t t_row2row_delay; // tRRD: open row in bank A -> open row in bank B
    uint8_t t_ras_cas_delay; // tRCD: open row -> read/write (RAS = row address, CAS = column address)
    uint8_t t_row_cycle;     // tRC: open row -> open next row in the same bank
    uint8_t t_row_precharge; // tRP: how long closing a row takes
    uint8_t t_min_row_open;  // tRAS: min time a row has to stay open
    uint8_t t_max_row_open;  // tRAS(max): max time a row may stay open, in steps of 1024 cycles

    // refresh
    uint16_t t_refresh_cycle;    // tRFC: how long one refresh takes
    uint16_t t_refresh_interval; // tREFI: how often a refresh is needed, in steps of 32 cycles

    // read/write delays
    uint8_t t_write_read_delay;  // tWTR: write -> read
    uint8_t t_cas_latency;       // tCL: read command -> DRAM sends the data
    uint8_t t_cas_write_latency; // tCWL: write command -> DRAM wants the data
    uint8_t write_latency;       // write command -> controller hands the data to the PHY
    uint8_t t_rdata_en;          // read command -> controller listens to the PHY
    uint8_t t_col2col_delay;     // tCCD: read -> read or write -> write
    uint8_t t_read_precharge;    // tRTP: read -> close the row
    uint8_t t_write_precharge;   // tWTP: write -> close the row
    uint8_t t_write2read;        // write command -> read command, as the controller counts it
    uint8_t t_read2write;        // read command -> write command, as the controller counts it

    // power-down and self-refresh (DRAM sleeps, self-refresh = it refreshes itself while sleeping)
    uint8_t t_powerdown_exit;   // tXP: wake up from power-down -> next command
    uint8_t t_clk_enable;       // tCKE: min time the CKE pin (clock enable) stays on or off
    uint8_t t_selfref_exit_clk; // tCKSRX: clock has to run this long before leaving self-refresh
    uint8_t t_selfref_min;      // tCKESR: min time the DRAM stays in self-refresh

    // mode registers
    uint8_t t_mode_update;  // tMOD: write a mode register -> next normal command
    uint8_t t_mode_to_mode; // tMRD: write a mode register -> write the next one
    uint8_t t_mode_write;   // tMRW: LPDDR only, 0 for DDR3

    // wait times at power-on, done by the PHY, in DRAM clock cycles (dram_clk in MHz * us = cycles)
    uint32_t t_init_clk_en_low;  // tDINIT0: 500 us CLK_EN off after power-on
    uint32_t t_init_clk_en_high; // tDINIT1: 360 ns CLK_EN on -> first command
    uint32_t t_init_reset;       // tDINIT2: 200 us reset pin held low
    uint32_t t_init_zq;          // tDINIT3: 1 us for the first ZQ calibration

    t_refresh_cycle = dram_ns_to_cycles(cfg, 350);
    t_refresh_interval = dram_ns_to_cycles(cfg, 7800) / 32 + 1; // +1 from awboot, no reason given there (marked XXX)
    t_write_read_delay = dram_ns_to_cycles(cfg, 8) + 2;         // +2 from awboot, no reason given there (marked XXX)

    // awboot: max(..., 2), dropped: 10 ns is only below 2 cycles when dram_clk <= 200 MHz
    t_row2row_delay = dram_ns_to_cycles(cfg, 10);
    t_powerdown_exit = dram_ns_to_cycles(cfg, 10);

    // awboot's values for dram_clk <= 800 (mine: 792), the > 800 branch is dropped
    t_act_window = dram_ns_to_cycles(cfg, 50);
    t_ras_cas_delay = dram_ns_to_cycles(cfg, 15);
    t_row_precharge = dram_ns_to_cycles(cfg, 15);
    t_row_cycle = dram_ns_to_cycles(cfg, 53);
    t_min_row_open = dram_ns_to_cycles(cfg, 38);

    t_cas_latency = 6;
    write_latency = 2;
    t_cas_write_latency = 4;
    t_rdata_en = 4;

    t_max_row_open = cfg->dram_clk / 30;
    t_write_precharge = t_cas_write_latency + 2 + t_write_read_delay; // tCWL + BL/2 + tWTR
    t_write2read = t_cas_write_latency + t_write_read_delay;          // tCWL + tWTR

    // fixed numbers from awboot
    t_col2col_delay = 2;
    t_read_precharge = 4;
    t_read2write = 5; // awboot: 6 only if tpr13 bits 2-3 != 1 AND dram_clk >= 912, neither is true for me
    t_clk_enable = 3;
    t_selfref_exit_clk = 5;
    t_selfref_min = 4;
    t_mode_update = 12;
    t_mode_to_mode = 4;
    t_mode_write = 0;

    t_init_clk_en_low = 500 * cfg->dram_clk + 1;         // 500 us
    t_init_clk_en_high = 360 * cfg->dram_clk / 1000 + 1; // 360 ns
    t_init_reset = 200 * cfg->dram_clk + 1;              // 200 us
    t_init_zq = 1 * cfg->dram_clk + 1;                   // 1 us

    /* Set mode registers */
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_MR0), cfg->dram_mr0);
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_MR1), cfg->dram_mr1);
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_MR2), cfg->dram_mr2);
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_MR3), cfg->dram_mr3);
    io_write32((MCTL_PHY_BASE + MCTL_PHY_LP3MR11), 0);

    /* Set DRAM timings */
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING0), (t_write_precharge << 24) | (t_act_window << 16) | (t_max_row_open << 8) | (t_min_row_open << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING1), (t_powerdown_exit << 16) | (t_read_precharge << 8) | (t_row_cycle << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING2), (t_cas_write_latency << 24) | (t_cas_latency << 16) | (t_read2write << 8) | (t_write2read << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING3), (t_mode_write << 16) | (t_mode_to_mode << 12) | (t_mode_update << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING4), (t_ras_cas_delay << 24) | (t_col2col_delay << 16) | (t_row2row_delay << 8) | (t_row_precharge << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING5), (t_selfref_exit_clk << 24) | (t_selfref_exit_clk << 16) | (t_selfref_min << 8) | (t_clk_enable << 0));

    /* Set dual rank timing */
    io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DRAM_TIMING8), 0xf000ffff, (cfg->dram_clk < 800) ? 0xf0006610 : 0xf0007610);

    /* PI_TIMING0: timing between controller and PHY
   PTR3, PTR4: wait times at power-on (DRAM init) */
    io_write32((MCTL_PHY_BASE + MCTL_PHY_PI_TIMING0), (0x2 << 24) | (t_rdata_en << 16) | (1 << 8) | (write_latency << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_PTR3), (t_init_clk_en_low << 0) | (t_init_clk_en_high << 20));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_PTR4), (t_init_reset << 0) | (t_init_zq << 20));

    /* Set refresh timing & mode */
    io_write32((MCTL_PHY_BASE + MCTL_PHY_REFRESH_TIMING), (t_refresh_interval << 16) | (t_refresh_cycle << 0));
    io_write32((MCTL_PHY_BASE + MCTL_PHY_REFRESH_CONTROL1), (t_refresh_interval << 15) & 0x0fff0000);
}

/*
 * evens out the "skew": the bits of 1 byte arrive at slightly different times
 * (traces have different lengths, every pin driver is a bit faster or slower)
 * DQS (data strobe) is the clock that travels with the data, on each edge the bits are read
 * so every DQ line gets a small extra delay, until all line up and the DQS edge lands in the middle of the "eye":
 *
 *   ___________        ___________
 *  /           \      /
 * X     eye     X----X     eye
 *  \___________/      \___________
 *        ^
 *   sample here (middle = safest, at the X the signal is still switching)
 *
 * fixed values from the board maker, nothing is measured here:
 *   tpr11, tpr12   data lines (DQ, DM) and DQS, 4 bit per group
 *   tpr10          address/command lines
 * must be set before PIR in mctl_channel_init, in my mode (dqs_gating_mode 0) no training runs
 */
static void eye_delay_compensation(const struct dram_config *cfg) {
    uint32_t delay, i;

    // DQ0-7 + DM delay, lane 0
    delay = (cfg->dram_tpr11 & 0xf) << 9;
    delay |= (cfg->dram_tpr12 & 0xf) << 1;

    for (i = 0; i < 9; i++)
        io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(0, i)), delay);

    // same with lane 1
    delay = (cfg->dram_tpr11 & 0xf0) << 5;
    delay |= (cfg->dram_tpr12 & 0xf0) >> 3;
    for (i = 0; i < 9; i++)
        io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(1, i)), delay);

    // General Config 0: assert AC (address/command) loopback FIFO reset
    io_clearbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG0), 1 << 26);

    // DQS0 read and write delay
    delay = (cfg->dram_tpr11 & 0xf0000) >> 7;
    delay |= (cfg->dram_tpr12 & 0xf0000) >> 15;
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(0, 9)), delay);  // +
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(0, 10)), delay); // -

    // same with DQS1
    delay = (cfg->dram_tpr11 & 0xf00000) >> 11;
    delay |= (cfg->dram_tpr12 & 0xf00000) >> 19;
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(1, 9)), delay);  // +
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_IO_CONFIG(1, 10)), delay); // -

    // DQS0 enable delay (when to start listening to DQS)
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_DELAY_LINE6(0)), (cfg->dram_tpr11 & 0xf0000) << 9);

    // same with DQS1
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_DELAY_LINE6(1)), (cfg->dram_tpr11 & 0xf00000) << 5);

    // General Config 0: release FIFO reset
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG0), 1 << 26);

    timer_udelay(1);

    delay = (cfg->dram_tpr10 & 0xf0) << 4;

    // Set RAS, CAS and AC delay
    for (i = 6; i < 27; i++)
        io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_AC_IO_CONFIG(i)), delay);

    // Set CLK CS (chip select) delay
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_AC_IO_CONFIG(2)), (cfg->dram_tpr10 & 0x0f) << 8);
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_AC_IO_CONFIG(3)), (cfg->dram_tpr10 & 0x0f) << 8);
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_AC_IO_CONFIG(28)), (cfg->dram_tpr10 & 0xf00) >> 4);
}

static uint32_t mctl_channel_init(const struct dram_config *cfg) {
    uint32_t val;
    int timeout;

    // set DDR-clk to half of CPU-clk
    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_TIMER), 0xfff, (cfg->dram_clk / 2) - 1);

    // undocumented: I cannot tell you why we set this
    io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG2), 0xf00, 0x300);

    if (cfg->dram_odt_en)
        val = 0;
    else
        val = (1 << 5);

    // Data lane 0 + 1 Config Register 0
    if (cfg->dram_clk > 672) {
        io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(0)), 0xf63e, val);
        io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(0)), 0x400);
        io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(1)), 0xf63e, val);
    } else {
        io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(0)), 0xf03e, val);
        io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DXnGCR0(1)), 0xf03e, val);
    }

    // MCTL_PHY_BASE + MCTL_PHY_AC_IO_GENERAL_CONFIG undocumented: couldn't tell you either.
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_AC_IO_GENERAL_CONFIG), 1 << 1);

    /*
     * Fixed delays from the board maker, must be set BEFORE PIR,
     * because PIR is the first time the PHY talks to the DRAM (DRAM init sends MR0-3 over the AC lines)
     */
    eye_delay_compensation(cfg);

    /*
     * DQS gating: the gate is the window where the PHY listens to DQS on a read,
     * outside of it DQS floats and noise could be read as edges.
     * Mode 0 (mine): the PHY handles the gate by itself (GENERAL_CONFIG2 bits 6-7)
     */

    io_clearbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG2), 0x40);
    timer_udelay(10);
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG2), 0xc0);

    io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_DATA_TRAINING_CONFIG), 0x0fffffff, (cfg->dram_rank_width & (1 << 12) ? 0x03000001 : 0x01000007));
    io_clearsetbits_le32((MCTL_PHY_BASE + MCTL_PHY_ZQ_CONFIG), 0x3ffffff, (cfg->dram_zq & 0x00ffffff) | (1 << 25));

    io_write32((MCTL_PHY_BASE + MCTL_PHY_INIT), 0x1f2);    // select the steps
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_INIT), 0x1); // GO

    timer_udelay(10);

    // Wait until all PIR steps are done (IDONE), max 100 ms
    timeout = 100000;
    while ((io_read32(MCTL_PHY_BASE + MCTL_PHY_GENERAL_STATUS0) & 0x1) == 0) {
        if (timeout == 0) {
            uart_puts("DRAM init timeout (IDONE)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    // ZQ calibration error
    if (io_read32(MCTL_PHY_BASE + MCTL_PHY_GENERAL_STATUS0) & (1 << 20)) {
        uart_puts("ZQ calibration error, check external 240 ohm resistor\r\n");
        return 1;
    }

    // Wait until the controller is in normal mode, max 100 ms
    timeout = 100000;
    while ((io_read32(MCTL_PHY_BASE + MCTL_PHY_STATUS) & 0x1) == 0) {
        if (timeout == 0) {
            uart_puts("DRAM init timeout (normal mode)\r\n");
            return 1;
        }
        timer_udelay(1);
        timeout--;
    }

    // undocumented: refresh pulse, then CCCR bit 31
    io_setbits_le32((MCTL_PHY_BASE + MCTL_PHY_REFRESH_CONTROL0), (1u << 31));
    timer_udelay(10);
    io_clearbits_le32((MCTL_PHY_BASE + MCTL_PHY_REFRESH_CONTROL0), (1u << 31));
    timer_udelay(10);
    io_setbits_le32((MCTL_COM_BASE + MCTL_COM_CCCR), (1u << 31));
    timer_udelay(10);

    // undocumented: clear GENERAL_CONFIG3 bits 25-26
    io_clearbits_le32((MCTL_PHY_BASE + MCTL_PHY_GENERAL_CONFIG3), 0x06000000);

    return 0;
}
