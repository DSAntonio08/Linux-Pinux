#include "../include/dram.h"
#include "../include/clock.h"
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
    val |= (clk_div - 1) << 8; // set PLL division
    val |= 0xc0000000;         // enable PLL and LDO
    val &= 0xdfffffff;
    io_write32((CCU_BASE + CCU_PLL_DDR), val | 0x20000000);

    // wait for PLL to lock
    while ((io_read32((CCU_BASE + CCU_PLL_DDR)) & 0x10000000) == 0)
        ;
    timer_udelay(20);

    clock_enable_pll(0x08000000);

    // turn clock gate on
    val = io_read32((CCU_BASE + CCU_DRAM_CLK));
    val &= 0xfcfffcfc; // select DDR clk source, n=1, m=1
    val |= 0x80000000; // turn clock on
    io_write32((CCU_BASE + CCU_DRAM_CLK), val);

    return clk_div * 24;
}

static void dram_clock_init(struct dram_config *cfg) {
    // hold MBUS in reset
    io_clearbits_le32((CCU_BASE + CCU_MBUS_CLK), (1 << 30));

    io_clearbits_le32((CCU_BASE + CCU_DRAM_BGR), 0x10001);
    // DRAM_CLK_REG: bit 31 = clock gate off, bit 30 = module reset asserted (0 = held in reset), bit 27 = update (apply the change)
    // bit 30 may be "reserved" in the T113 manual, name from U-Boot's H6 CCU (DRAM_MOD_RESET), Allwinner's DRAM code uses it on the T113 too
    io_clearsetbits_le32((CCU_BASE + CCU_DRAM_CLK), (1 << 31) | (1 << 30), (1 << 27));
    timer_udelay(10);

    // set ddr pll clock
    cfg->dram_clk = ccu_set_pll_ddr_clk(cfg) / 2;
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
 * LDO = Low-DropOut regulator: a small voltage regulator built into the T113
 * it turns an existing supply into a lower, stable voltage
 * the T113 has two of them, both fed from LDO-IN (3.3 V):
 *   LDOA  1.8 V for IO and analog parts
 *   LDOB  VCC-DRAM, the supply of the DRAM IO and the DDR3 chip itself
 * after reset LDOB only gives 1.35 V, but DDR3 needs 1.5 V (datasheet: 1.425-1.575 V)
 * so it has to be raised before the DRAM is used ("VCC-DRAM needs be stable before SDRAM driver initialization")
 * SYS_LDO_CTRL bits 8-15 = LDOB_TRIM, 1.167 V + value * 13.3 mV:
 *   0x0E (14) = 1.353 V  reset default
 *   0x19 (25) = 1.5 V    DDR3   <- what I set
 *   0x2F (47) = 1.8 V    DDR2
 * every chip is a little different, so Allwinner burns a correction value for LDOB into the eFuse (SID) at the factory
 * if that value exists, it replaces the default (values above 0x20 are lowered by 0x16 = 22 steps = 0.3 V,
 * which turns a 1.8 V trim into 1.5 V: 0x2F - 0x16 = 0x19)
 */
static void dram_ldo_init(void) {
    uint32_t ldo_val = 25;

    io_clearsetbits_le32((SYS_CFG_BASE + SYS_LDO_CTRL), 0x20ff00, ldo_val << 8);
    timer_udelay(1);

    uint32_t cal = (io_read32(SID_BASE + SID_LDOB) & 0xff00) >> 8;

    if (cal == 0)
        return;

    if (cal > 0x20)
        cal -= 0x16;

    io_clearsetbits_le32((SYS_CFG_BASE + SYS_LDO_CTRL), 0xff00, cal << 8);
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

    // set controller wait time
    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_DBGCR), 0x3f00, 0x2000);

    // set SDRAM type and data-bus width
    val = ((cfg->dram_type & 0x7) << 16) |        // DRAM type
          ((~cfg->dram_rank_width & 0x1) << 12) | // DQ width
          (1 << 22);                              // burst length 8 (DDR3 always uses BL8)

    if (cfg->dram_tpr13 & (1 << 5))
        val |= (1 << 19); // 1T instead of 2T

    io_clearsetbits_le32((MCTL_COM_BASE + MCTL_COM_WORK_MODE0), 0x00fff000, val);

    // set rank, banks, rows and page size
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
    uint32_t t_init_cke_low;  // tDINIT0: 500 us CKE off after power-on
    uint32_t t_init_cke_high; // tDINIT1: 360 ns CKE on -> first command
    uint32_t t_init_reset;    // tDINIT2: 200 us reset pin held low
    uint32_t t_init_zq;       // tDINIT3: 1 us for the first ZQ calibration

    t_refresh_cycle = dram_ns_to_cycles(cfg, 350);
    t_refresh_interval = dram_ns_to_cycles(cfg, 7800) / 32 + 1; // +1 from awboot, no reason given there (marked XXX)
    t_write_read_delay = dram_ns_to_cycles(cfg, 8) + 2;         // +2 from awboot, no reason given there (marked XXX)

    // awboot: max(..., 2), dropped: 10 ns is only below 2 cycles when dram_clk <= 200 MHz
    t_row2row_delay = dram_ns_to_cycles(cfg, 10);
    t_powerdown_exit = dram_ns_to_cycles(cfg, 10);

    if (cfg->dram_clk <= 800) {
        t_act_window = dram_ns_to_cycles(cfg, 50);
        t_ras_cas_delay = dram_ns_to_cycles(cfg, 15);
        t_row_precharge = dram_ns_to_cycles(cfg, 15);
        t_row_cycle = dram_ns_to_cycles(cfg, 53);
        t_min_row_open = dram_ns_to_cycles(cfg, 38);

        t_cas_latency = 6;
        write_latency = 2;
        t_cas_write_latency = 4;
        t_rdata_en = 4;
    }

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

    t_init_cke_low = 500 * cfg->dram_clk + 1;         // 500 us
    t_init_cke_high = 360 * cfg->dram_clk / 1000 + 1; // 360 ns
    t_init_reset = 200 * cfg->dram_clk + 1;           // 200 us
    t_init_zq = 1 * cfg->dram_clk + 1;                // 1 us
}
