/*
 * t113_regs.h - all T113-S3 register addresses I use, in ONE place
 *
 * Every address exists only here, so if one is wrong I fix it once
 * Source: Allwinner T113-S3 User Manual
 */

#pragma once

/* Source: Allwinner T113-S3 User Manual */

/* ---- CCU (Clock Controller Unit) ---- */
#define CCU_BASE 0x02001000u
#define CCU_UART_BGR 0x90Cu /* UART bus gate + reset, p.118-119 */
#define CCU_PLL_CPU 0x0000 /* p. 67-68 */
#define CCU_PLL_DDR 0x0010
#define CCU_PLL_PERI 0x0020
#define CCU_CPU_AXI_CFG 0x0500
#define CCU_PSI_CLK 0x0510
#define CCU_APB0_CLK 0x0520
#define CCU_APB1_CLK 0x524u /* APB1 clock source/dividers */

/* ---- GPIO ---- */
#define GPIO_BASE 0x02000000u
#define GPIO_PG_CFG0 0x120u /* PG0..PG7, 4 bits per pin */
#define GPIO_PG_CFG1 0x124u /* PG8..PG15, 4 bits per pin */
#define GPIO_PG_DAT 0x130u  /* PG data, 1 bit per pin */

/* ---- UART ---- */
#define UART1_BASE 0x02500400u