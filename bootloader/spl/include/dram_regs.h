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

/* ODTMAP (On-Die Termination map)
 *   ODT    a resistor INSIDE the DRAM chip, switched on so the data signals
 *          don't bounce back at the end of the wire (like an echo) and mess up the next bits
 *   MAP    a table: for each rank, which ODT pin gets switched on
 *   rank   a set of chips the controller can switch between (I only have 1)
 * value from the rank field in WORK_MODE0 bit 0: 1 rank -> 0x201, 2 ranks -> 0x303
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_ODTMAP 0x120u

/* DXnGCR0 (Data lane n, General Configuration Register 0)
 *   DX     data lane = 8 data lines (DQ) + 1 data mask (DM) + 1 strobe pair (DQS)
 *   n      lane number, 16 bit bus -> DX0 = bits 0-7, DX1 = bits 8-15
 *   GCR0   general settings of that lane (on/off, termination, timing)
 * every lane has the same register block, 0x80 apart: DX0 = 0x344, DX1 = 0x3c4
 * writing 0 turns the lane off (half DQ = 8 bit)
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_PHY_DXnGCR0(x) (0x344u + 0x80u * (x))

/* ---- SYS_CFG (System Configuration) ---- */
/* LDO and ZQ (resistor) calibration, needed for the DRAM PHY, manual p.168 */
#define SYS_CFG_BASE 0x03000000u
#define SYS_LDO_CTRL 0x150u
#define ZQ_CAL_CTRL 0x160u   /* RESCAL_CTRL_REG, start of the resistor calibration */
#define ZQ_RES_CTRL 0x168u   /* RES240_CTRL_REG, manual control of the 240 ohm resistor */
#define ZQ_RES_STATUS 0x16Cu /* RESCAL_STATUS_REG, calibration result */

/* ---- SID (Security ID / eFuse) ---- */
/* read-only chip data; U-Boot reads it during DRAM init
 * eFuse = tiny fuses inside the chip, "burned" once at the factory, can never change again
 * Allwinner stores a unique chip ID and per-chip calibration values there */
#define SID_BASE 0x03006000u
#define SID_CHIPID 0x200u /* bits 0-15: chip variant ID (0x6800 = T113-S4); Source: sunxi tools (xfel), not in the manual */
#define SID_EFUSE 0x228u /* chip variant info, used to pick the AC remap table */
#define SID_LDOB 0x21Cu  /* bits 8-15: LDOB calibration value for this chip */

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
