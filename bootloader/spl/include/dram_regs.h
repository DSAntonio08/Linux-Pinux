#pragma once

/* ---- DRAM ---- */
#define DRAM_START 0x40000000u /* start of the DRAM address range */

/* ---- DRAM controller: COM block (MCTL_COM) ---- */
/* general controller setup: work mode, master access enables, address remap */
#define MCTL_COM_BASE 0x03102000u

/* MAER (Master Access Enable): which bus masters may use the DRAM
 *   master   hardware that reads/writes DRAM on its own (CPU, DMA, display, USB, ...)
 *   bit      one per master, 1 = may access, 0 = blocked
 * blocked during init, all allowed again at the end
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_COM_MAER0 0x20u /* master access allow, 1 bit per bus master */
#define MCTL_COM_MAER1 0x24u
#define MCTL_COM_MAER2 0x28u

/* DBGCR ("debug control"): undocumented, despite the name part of the normal init
 * bits 8-13 = controller wait time (awboot writes 0x20) */
#define MCTL_COM_DBGCR 0x08u

/* TIMER (awboot: TMR): meaning not documented, probably a timer register
 * same offset in U-Boot's COM block struct for older Allwinner chips (H3/A64), no explanation there either
 * not the same as MCTL_PHY_CLKEN (also 0x0c, but in the PHY block)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_COM_TIMER 0x0cu

/* CCCR (awboot: CCCR, name not explained anywhere)
 *   bit 31   set once at the end of mctl_channel_init, meaning unknown
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_COM_CCCR 0x14u

/* WORK_MODE0: what DRAM is attaced (type, bus width, ranks, banks, rows, page size) */
#define MCTL_COM_WORK_MODE0 0x00u
#define MCTL_COM_WORK_MODE1 0x04u

/* REMAP0..3 (Address/Command remap)
 *   AC       address/command lines from the controller to the DRAM (address bits, bank bits, RAS, CAS, WE, ...)
 *   remap    which pin each AC signal goes out on, so it fits how the DRAM is wired inside the package
 *   entry    5 bit pin number, 22 entries spread over the 4 registers
 * REMAP0: bit 0 = remap on, entries start at bit 5; REMAP1..3: entries start at bit 0
 * Source: awboot dram.c (raw addresses 0x3102500..0x310250c), not in the manual */
#define MCTL_COM_REMAP0 0x500u
#define MCTL_COM_REMAP1 0x504u
#define MCTL_COM_REMAP2 0x508u
#define MCTL_COM_REMAP3 0x50cu

/* ---- DRAM controller: PHY block (MCTL_PHY) ---- */
/* the DRAM's "physical" side: how fast, how long to wait, how the signals are tuned
 *   PIR      start button for the init steps (ZQ calibration, DRAM init, training)
 *   PGSR     status: are the init steps done, did one fail?
 *   MR0..3   settings that get written INTO the DRAM chip at init
 *   DRAMTMG  minimum waiting times between DRAM commands, in clock cycles
 *   ZQ       tunes the chip's drivers against R30 (240 ohm on DZQ)
 *   DXn      one register set per data lane (8 data lines each), n = lane number
 * Source: U-Boot sunxi DRAM driver, offsets not checked against the manual yet */
#define MCTL_PHY_BASE 0x03103000u
#define MCTL_PHY_CLKEN 0x0c
#define MCTL_PHY_IOVREF_CR0 0x110u
#define MCTL_PHY_IOVREF_CR1 0x114u

/* INIT (awboot: PIR, PHY Initialization Register): the start button for the init steps
 *   bit 0   GO, starts all steps selected in the other bits (write them first, then set bit 0)
 *   bit 1   ZQ calibration (driver strength against the 240 ohm resistor)
 *   bit 4   PLL init (the PHY's own clock)
 *   bit 5   delay line calibration
 *   bit 6   PHY reset
 *   bit 7   DRAM reset (DDR3 only, RESET pin)
 *   bit 8   DRAM init (power-on routine with PTR3/4, sends MR0-3)
 *   bit 10  DQS gate training (only dqs_gating_mode 1)
 * mine (DDR3, mode 0, cold boot): 0x1f2 = bits 1, 4, 5, 6, 7, 8, no training
 * Source: awboot reg-dram.h + its comments, bit order like the Synopsys PHY, not in the manual */
#define MCTL_PHY_INIT 0x00u

/* POWER_CONTROL (awboot: PWRCTL, Power Control Register)
 *   bit 0   auto self-refresh on/off (1 = the DRAM goes to sleep by itself when nobody uses it)
 * off for me (tpr13 bit 30 = 0), cleared in dram_finish_init
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_POWER_CONTROL 0x04u

/* GENERAL_STATUS0 (awboot: PGSR0, PHY General Status Register 0): result of the PIR steps
 *   bit 0    IDONE, 1 = all PIR steps finished (poll this after GO)
 *   bit 20   ZQ calibration error (check the external 240 ohm resistor)
 *   bit 22   DQS gate training error (only used by awboot's auto-scan)
 * Source: awboot reg-dram.h + dram.c, not in the manual */
#define MCTL_PHY_GENERAL_STATUS0 0x10u

/* STATUS (awboot: STATR): what mode the controller is in
 *   bits 0-2   1 = normal, 3 = self-refresh (like the Synopsys controller's STAT)
 * after the init: wait until bit 0 = 1 (normal)
 * Source: awboot reg-dram.h + dram.c, not in the manual */
#define MCTL_PHY_STATUS 0x18u

/* DRAM_MR0..3: values for the DRAM's mode registers, the PHY sends them INTO the DRAM chip at init
 *   register   the outbox in the PHY, which one decides where it arrives (PHY MR0 -> DRAM chip MR0, ...)
 *   value      the letter, its bits go 1:1 into the DRAM chip's mode register
 * written now, sent later when the init is started (PIR, mctl_channel_init)
 * Source: U-Boot dram_sun20i_d1.c (raw addresses 0x3103030..0x310303c), not in the manual */
#define MCTL_PHY_DRAM_MR0 0x30u
#define MCTL_PHY_DRAM_MR1 0x34u
#define MCTL_PHY_DRAM_MR2 0x38u
#define MCTL_PHY_DRAM_MR3 0x3cu

/* LP3MR11: value for mode register 11, only LPDDR3 has it (its ODT setting)
 * DDR3 has no MR11 -> always 0 for me, written anyway so it's not left at some unknown value
 * awboot writes (dram_odt_en >> 4) & 0x3: in Allwinner's original code dram_odt_en probably had
 * more bits, in awboot it's only 0 or 1 -> the shift always gives 0 (awboot has a TODO about it)
 * Source: U-Boot dram_sun20i_d1.c (raw address 0x310302c), not in the manual */
#define MCTL_PHY_LP3MR11 0x2cu

/* PTR3, PTR4 (PHY Timing Register 3/4): wait times at power-on, the PHY waits them by itself
 *   PTR3   bits 0-19 = tDINIT0 (CKE off after power-on), bits 20+ = tDINIT1 (CKE on -> first command)
 *   PTR4   bits 0-19 = tDINIT2 (reset held low), bits 20+ = tDINIT3 (first ZQ calibration)
 * Source: U-Boot dram_sun20i_d1.c (raw addresses 0x3103050, 0x3103054), not in the manual */
#define MCTL_PHY_PTR3 0x50u
#define MCTL_PHY_PTR4 0x54u

/* DRAM_TIMING0..5 (DRAM Timing 0..5): waiting times between commands, in controller cycles
 * each register holds up to 4 timings, mostly one per byte (bit 0, 8, 16, 24):
 *   DRAM_TIMING0   tRAS, tRAS(max), tFAW, tWTP
 *   DRAM_TIMING1   tRC, tRTP, tXP
 *   DRAM_TIMING2   write->read, read->write, tCL, tCWL
 *   DRAM_TIMING3   tMOD (bit 0), tMRD (bit 12), tMRW (bit 16)
 *   DRAM_TIMING4   tRP, tRRD, tCCD, tRCD
 *   DRAM_TIMING5   tCKE, tCKESR, tCKSRX, tCKSRX again (bit 24, awboot and U-Boot both write it twice)
 * Source: U-Boot dram_sun20i_d1.c (raw addresses 0x3103058..0x310306c), not in the manual */
#define MCTL_PHY_DRAM_TIMING0 0x58u
#define MCTL_PHY_DRAM_TIMING1 0x5cu
#define MCTL_PHY_DRAM_TIMING2 0x60u
#define MCTL_PHY_DRAM_TIMING3 0x64u
#define MCTL_PHY_DRAM_TIMING4 0x68u
#define MCTL_PHY_DRAM_TIMING5 0x6cu

/* DRAM_TIMING8: awboot calls it "dual rank timing" (timing for switching between 2 ranks)
 * fixed value 0xf0006610 when dram_clk < 800, no explanation of the bits anywhere
 * Source: U-Boot dram_sun20i_d1.c (raw address 0x3103078), not in the manual */
#define MCTL_PHY_DRAM_TIMING8 0x78u

/* PI_TIMING0 (PHY Interface Timing 0): delays between the controller and the PHY
 *   bits 0-7     write_latency (write command -> data to the PHY)
 *   bits 16-23   t_rdata_en (read command -> listen to the PHY)
 *   bit 8 = 1, bits 24+ = 2   fixed in awboot, meaning unknown
 * Source: U-Boot dram_sun20i_d1.c (raw address 0x3103080), not in the manual */
#define MCTL_PHY_PI_TIMING0 0x80u

/* REFRESH_CONTROL0 (awboot: RFSHCTL0, Refresh Control Register 0)
 *   bit 31   set, wait 10 us, clear again at the end of mctl_channel_init, meaning unknown
 *            (looks like a restart pulse for the refresh logic, not confirmed)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_REFRESH_CONTROL0 0x8cu

/* REFRESH_TMIING: bits 0-15 = tRFC, bits 16+ = tREFI
 * REFRESH_CONTROL1: bits 16-27 = tREFI / 2
 *   probably the start value of the refresh timer for rank 1, so 2 ranks don't refresh at the
 *   same time (same layout as the Synopsys DDR controller); with 1 rank it doesn't matter
 * Source: U-Boot dram_sun20i_d1.c (raw addresses 0x3103090, 0x3103094), not in the manual */
#define MCTL_PHY_REFRESH_TIMING 0x90u
#define MCTL_PHY_REFRESH_CONTROL1 0x94u

/* AUTO_SELFREF_TIMER (awboot: ASRTC, Auto Self-Refresh Timer Control)
 *   bits 0-15   idle time before the DRAM goes into self-refresh by itself (probably)
 * auto self-refresh is off for me (tpr13 bit 30 = 0), only cleared in dram_finish_init
 * not the same as normal refresh (REFRESH_TIMING), that one always runs
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_AUTO_SELFREF_TIMER 0xa0u

/* VTF_CONFIG (awboot: VTFCR)
 *   bits 8-9   set when tpr13 bit 8 = 1 (mine: yes), meaning unknown
 *   "VTF" is not explained anywhere, maybe voltage/temperature compensation (not confirmed)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_VTF_CONFIG 0xb8u

/* DATA_TRAINING_CONFIG (awboot: DTCR, Data Training Configuration Register)
 *   settings for the PHY's data training (which ranks, how)
 *   bits 24-27   probably rank enable: 1 = rank 0 only, 3 = rank 0 + 1 (Synopsys layout)
 *   bits 0-3     unknown, awboot writes 7 for 1 rank, 1 for 2 ranks
 * 1 rank (para2 bit 12 = 0) -> 0x01000007
 * Source: U-Boot dram_sun20i_d1.c (raw address 0x31030c0), not in the manual */
#define MCTL_PHY_DATA_TRAINING_CONFIG 0xc0u

/* GENERAL_CONFIG0 (awboot: PGCR0, PHY General Configuration Register 0)
 *   bit 26   AC loopback FIFO reset, 0 = held in reset, 1 = running
 *            (a small buffer in the PHY, kept in reset while the delays get changed)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_GENERAL_CONFIG0 0x100u

/* GENERAL_CONFIG2 (awboot: PGCR2, PHY General Configuration Register 2)
 *   bits 8-11   set to 3 in awboot, meaning unknown
 *               (U-Boot's comment calls it "MRCTRL0", but the address is PGCR2)
 * Source: U-Boot dram_sun20i_d1.c (raw address 0x3103108), not in the manual */
#define MCTL_PHY_GENERAL_CONFIG2 0x108u

/* GENERAL_CONFIG3 (awboot: PGCR3, PHY General Configuration Register 3)
 *   bits 25-26   cleared at the end of mctl_channel_init, meaning unknown
 *                (only set to something else in awboot's standby wake-up path)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_GENERAL_CONFIG3 0x10cu

/* ODTMAP (On-Die Termination map)
 *   ODT    a resistor INSIDE the DRAM chip, switched on so the data signals
 *          don't bounce back at the end of the wire (like an echo) and mess up the next bits
 *   MAP    a table: for each rank, which ODT pin gets switched on
 *   rank   a set of chips the controller can switch between (I only have 1)
 * value from the rank field in WORK_MODE0 bit 0: 1 rank -> 0x201, 2 ranks -> 0x303
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_ODTMAP 0x120u

/* ZQ_CONFIG (awboot: ZQCR, ZQ Configuration Register)
 *   ZQ           calibration of the pin drivers and ODT against the external 240 ohm resistor,
 *                so the signal strength stays right (temperature, voltage)
 *   bits 0-23    ZQ value from the board maker (dram_zq, 0x7b7bfb)
 *   bit 25       set in awboot, meaning unknown
 *   bit 31       set at the end of the init in awboot ("disable ZQ calibration")
 * PIR runs the ZQ calibration, error = PGSR0 bit 20
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_ZQ_CONFIG 0x140u

/* DXnGCR0 (Data lane n, General Configuration Register 0)
 *   DX     data lane = 8 data lines (DQ) + 1 data mask (DM) + 1 strobe pair (DQS)
 *   n      lane number, 16 bit bus -> DX0 = bits 0-7, DX1 = bits 8-15
 *   GCR0   general settings of that lane (on/off, termination, timing)
 * every lane has the same register block, 0x80 apart: DX0 = 0x344, DX1 = 0x3c4
 * writing 0 turns the lane off (half DQ = 8 bit)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_DXnGCR0(x) (0x344u + 0x80u * (x))

/* DATA_IO_CONFIG (awboot: DATX0IOCR / DATX1IOCR, Data lane I/O Configuration Register)
 *   one register per wire of a data lane, holds that wire's extra delay (eye delay compensation)
 *   lane   0 = DQ0-7, 1 = DQ8-15, 0x80 apart (like DXnGCR0)
 *   line   0-7 = DQ0-7 of the lane, 8 = DM, 9 = DQS P, 10 = DQS N (strobe pair)
 *   bits 1-4   delay from tpr12
 *   bits 9-12  delay from tpr11
 * awboot DATX0IOCR(i) = DATA_IO_CONFIG(0, i), DATX1IOCR(i) = DATA_IO_CONFIG(1, i)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_DATA_IO_CONFIG(lane, line) (0x310u + 0x80u * (lane) + 0x4u * (line))

/* DATA_DELAY_LINE6 (awboot: DXnSDLR6, Data lane n Slave Delay Line Register 6)
 *   bits 25-28   DQS enable delay: when the PHY starts listening to DQS on a read
 *                (from tpr11 bits 16-19 for lane 0, bits 20-23 for lane 1)
 * sits right after the lane's DATA_IO_CONFIG registers (DX0 = 0x33c, DX1 = 0x3bc)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_DATA_DELAY_LINE6(lane) (0x33cu + 0x80u * (lane))

/* AC_IO_GENERAL_CONFIG (awboot: ACIOCR0, Address/Command I/O Configuration Register 0)
 *   one register for ALL address/command wires (not one per wire like AC_IO_CONFIG)
 *   bit 1   set in mctl_channel_init, meaning unknown
 * careful: NOT the same as AC_IO_CONFIG(0) = 0x210, that's awboot's ACIOCR1(0)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_AC_IO_GENERAL_CONFIG 0x208u

/* AC_IO_CONFIG (awboot: ACIOCR1, Address/Command I/O Configuration Register 1)
 *   one register per address/command wire, holds that wire's extra delay (from tpr10)
 *   line 2, 3    CK, CS (clock, chip select), delay in bits 8-11
 *   line 6-26    RAS, CAS and the address lines, delay in bits 8-11
 *   line 28      delay in bits 4-7
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_AC_IO_CONFIG(line) (0x210u + 0x4u * (line))

/* ---- SYS_CFG (System Configuration) ---- */
/* LDO and ZQ (resistor) calibration, needed for the DRAM PHY, manual p.168 */
#define SYS_CFG_BASE 0x03000000u
#define ZQ_CAL_CTRL 0x160u   /* RESCAL_CTRL_REG, start of the resistor calibration */
#define ZQ_RES_CTRL 0x168u   /* RES240_CTRL_REG, manual control of the 240 ohm resistor */
#define ZQ_RES_STATUS 0x16Cu /* RESCAL_STATUS_REG, calibration result */

/* ---- SID (Security ID / eFuse) ---- */
/* read-only chip data; U-Boot reads it during DRAM init
 * eFuse = tiny fuses inside the chip, "burned" once at the factory, can never change again
 * Allwinner stores a unique chip ID and per-chip calibration values there */
#define SID_BASE 0x03006000u
#define SID_CHIPID 0x200u /* bits 0-15: chip variant ID (0x6800 = T113-S4); Source: sunxi tools (xfel), not in the manual */
#define SID_EFUSE 0x228u  /* chip variant info, used to pick the AC remap table */

/* ---- AON_CPUCFG (CPU config, always-on domain) ---- */
/* manual: R_CPUCFG */
#define AON_CPUCFG_BASE 0x07000400u
#define AON_CPUCFG_SUP_STAN_FLAG 0x1D4u

/* ---- AON_PRCM (Power Reset Clock Management, always-on domain) ---- */
/* power gating of the system and analog supplies */
/*manual: R_PRCM*/
#define AON_PRCM_BASE 0x07010000u
#define VDD_SYS_PWROFF_GATING 0x250u
#define ANALOG_PWROFF_GATING 0x254u
