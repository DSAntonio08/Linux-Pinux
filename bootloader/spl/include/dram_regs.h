#pragma once

/* ---- DRAM ---- */
#define DRAM_START 0x40000000u /* start of the DRAM address range */

/* ---- DRAM controller: COM block (MCTL_COM) ---- */
/* general controller setup: work mode, master access enables, address remap */
#define MCTL_COM_BASE 0x03102000u
/* add the MCTL_COM_* offsets here when dram.c uses them */
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

/* ---- SYS_CFG (System Configuration) ---- */
/* LDO and ZQ (resistor) calibration, needed for the DRAM PHY, manual p.168 */
#define SYS_CFG_BASE 0x03000000u
#define SYS_LDO_CTRL 0x150u
#define ZQ_CAL_CTRL 0x160u   /* RESCAL_CTRL_REG, start of the resistor calibration */
#define ZQ_RES_CTRL 0x168u   /* RES240_CTRL_REG, manual control of the 240 ohm resistor */
#define ZQ_RES_STATUS 0x16Cu /* RESCAL_STATUS_REG, calibration result */

/* ---- SID (Security ID / eFuse) ---- */
/* read-only chip data; U-Boot reads it during DRAM init */
#define SID_BASE 0x03006000u
#define SID_EFUSE 0x228u
#define SID_LDOB 0x21Cu

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

/* MAER (Master Access Enable): which bus masters may use the DRAM
 *   master   hardware that reads/writes DRAM on its own (CPU, DMA, display, USB, ...)
 *   bit      one per master, 1 = may access, 0 = blocked
 * blocked during init, all allowed again at the end
 * Source: awboot reg-dram.h, not in the manual */
#define MCTL_COM_MAER0 0x20u /* master access allow, 1 bit per bus master */
#define MCTL_COM_MAER1 0x24u
#define MCTL_COM_MAER2 0x28u