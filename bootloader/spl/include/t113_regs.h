/*
 * t113_regs.h - all T113-S3 register addresses I use, in ONE place
 *
 * Every address is defined in exactly oen header, so if one is wrong I only fix it once
 * DRAM controller registers live in dram_regs.h (only dram.c needs them) 
 * Source: Allwinner T113-S3 User Manual
 */

#pragma once

/* Source: Allwinner T113-S3 User Manual */

/* ---- CCU (Clock Controller Unit) ---- */
#define CCU_BASE 0x02001000u
#define CCU_UART_BGR 0x90Cu /* UART bus gate + reset, p.118-119 */
#define CCU_PLL_CPU 0x0000  /* p. 67-68 */
#define CCU_PLL_DDR 0x0010
#define CCU_PLL_PERI 0x0020
#define CCU_CPU_AXI_CFG 0x0500
#define CCU_PSI_CLK 0x0510
#define CCU_APB0_CLK 0x0520
#define CCU_APB1_CLK 0x524u /* APB1 clock source/dividers */

#define CCU_DRAM_CLK 0x800u  /* DRAM clock source/divider + gate, p.xx */
#define CCU_DRAM_BGR 0x80Cu  /* DRAM bus gate + reset */
#define CCU_MBUS_CLK 0x540u  /* MBUS clock + reset */

#define CCU_SMHC0_CLK 0x830u /* SMHC0 clock: gate (bit 31), source (bits 26:24), N (bits 9:8), M (bits 3:0), around p.114 */
#define CCU_SMHC_BGR 0x84Cu  /* SMHC0-2 bus gate (bits 0-2) + reset (bits 16-18), around p.117 */

/* bits that are the same in every PLL_*_CTRL_REG, p.70 */
#define PLL_EN (1u << 31)          /* main switch */
#define PLL_LDO_EN (1u << 30)      /* PLL power supply, default 1 */
#define PLL_LOCK_EN (1u << 29)     /* start lock detection */
#define PLL_LOCK (1u << 28)        /* read-only, 1 = output is stable */
#define PLL_OUTPUT_GATE (1u << 27) /* 1 = clock goes out to the chip */

/* ---- GPIO ---- */
#define GPIO_BASE 0x02000000u
#define GPIO_PG_CFG0 0x120u /* PG0..PG7, 4 bits per pin */
#define GPIO_PG_CFG1 0x124u /* PG8..PG15, 4 bits per pin */
#define GPIO_PG_DAT 0x130u  /* PG data, 1 bit per pin */

/* ---- UART ---- */
#define UART1_BASE 0x02500400u

/* ---- SMHC (SD/MMC Host Controller) ---- */
#define SMHC0_BASE 0x04020000u /* SMHC0 = microSD card slot, manual 7.2 */
