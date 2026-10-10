#pragma once

/* Source: Allwinner T113-S3 User Manual */

/* ---- CCU (Clock Controller Unit) ---- */
#define CCU_BASE        0x02001000u
#define CCU_APB1_CLK    0x524u      /* APB1 clock source/dividers */
#define CCU_UART_BGR    0x90Cu      /* UART bus gate + reset, p.118-119 */

/* ---- GPIO ---- */
#define GPIO_BASE       0x02000000u
#define GPIO_PG_CFG0    0x120u      /* PG0..PG7, 4 bits per pin */

/* ---- UART ---- */
#define UART1_BASE      0x02500400u