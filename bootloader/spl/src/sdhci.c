#include "../include/sdhci.h"
#include "../include/gpio.h"

// this struct is needed for the next struct
struct sdhci_reg {
    volatile uint32_t gctrl;        /* (0x00) SMC Global Control Register */
    volatile uint32_t clkcr;        /* (0x04) SMC Clock Control Register */
    volatile uint32_t timeout;      /* (0x08) SMC Time Out Register */
    volatile uint32_t width;        /* (0x0C) SMC Bus Width Register */
    volatile uint32_t blksz;        /* (0x10) SMC Block Size Register */
    volatile uint32_t bytecnt;      /* (0x14) SMC Byte Count Register */
    volatile uint32_t cmd;          /* (0x18) SMC Command Register */
    volatile uint32_t arg;          /* (0x1C) SMC Argument Register */
    volatile uint32_t resp0;        /* (0x20) SMC Response Register 0 */
    volatile uint32_t resp1;        /* (0x24) SMC Response Register 1 */
    volatile uint32_t resp2;        /* (0x28) SMC Response Register 2 */
    volatile uint32_t resp3;        /* (0x2C) SMC Response Register 3 */
    volatile uint32_t imask;        /* (0x30) SMC Interrupt Mask Register */
    volatile uint32_t mint;         /* (0x34) SMC Masked Interrupt Status Register */
    volatile uint32_t rint;         /* (0x38) SMC Raw Interrupt Status Register */
    volatile uint32_t status;       /* (0x3C) SMC Status Register */
    volatile uint32_t ftrglevel;    /* (0x40) SMC FIFO Threshold Watermark Register */
    volatile uint32_t funcsel;      /* (0x44) SMC Function Select Register */
    volatile uint32_t cbcr;         /* (0x48) SMC CIU Byte Count Register */
    volatile uint32_t bbcr;         /* (0x4C) SMC BIU Byte Count Register */
    volatile uint32_t dbgc;         /* (0x50) SMC Debug Enable Register */
    volatile uint32_t csdc;         /* (0x54) CRC status detect control register*/
    volatile uint32_t a12a;         /* (0x58)Auto command 12 argument*/
    volatile uint32_t ntsr;         /* (0x5c)SMC2 Newtiming Set Register */
    volatile uint32_t res1[6];      /* (0x60~0x74) */
    volatile uint32_t hwrst;        /* (0x78) SMC eMMC Hardware Reset Register */
    volatile uint32_t res2;         /*  (0x7c) */
    volatile uint32_t dmac;         /*  (0x80) SMC IDMAC Control Register */
    volatile uint32_t dlba;         /*  (0x84) SMC IDMAC Descriptor List Base Address Register */
    volatile uint32_t idst;         /*  (0x88) SMC IDMAC Status Register */
    volatile uint32_t idie;         /*  (0x8C) SMC IDMAC Interrupt Enable Register */
    volatile uint32_t chda;         /*  (0x90) */
    volatile uint32_t cbda;         /*  (0x94) */
    volatile uint32_t res3[26];     /*  (0x98~0xff) */
    volatile uint32_t thldc;        /*  (0x100) Card Threshold Control Register */
    volatile uint32_t sfc;          /* (0x104) Sample Fifo Control Register */
    volatile uint32_t res4[1];      /*  (0x10b) */
    volatile uint32_t dsbd;         /* (0x10c) eMMC4.5 DDR Start Bit Detection Control */
    volatile uint32_t res5[12];     /* (0x110~0x13c) */
    volatile uint32_t drv_dl;       /* (0x140) Drive Delay Control register*/
    volatile uint32_t samp_dl;      /* (0x144) Sample Delay Control register*/
    volatile uint32_t ds_dl;        /* (0x148) Data Strobe Delay Control Register */
    volatile uint32_t ntdc;         /* (0x14C) HS400 New Timing Delay Control Register */
    volatile uint32_t res6[4];      /* (0x150~0x15f) */
    volatile uint32_t skew_dat0_dl; /*(0x160) deskew data0 delay control register*/
    volatile uint32_t skew_dat1_dl; /*(0x164) deskew data1 delay control register*/
    volatile uint32_t skew_dat2_dl; /*(0x168) deskew data2 delay control register*/
    volatile uint32_t skew_dat3_dl; /*(0x16c) deskew data3 delay control register*/
    volatile uint32_t skew_dat4_dl; /*(0x170) deskew data4 delay control register*/
    volatile uint32_t skew_dat5_dl; /*(0x174) deskew data5 delay control register*/
    volatile uint32_t skew_dat6_dl; /*(0x178) deskew data6 delay control register*/
    volatile uint32_t skew_dat7_dl; /*(0x17c) deskew data7 delay control register*/
    volatile uint32_t skew_ds_dl;   /*(0x180) deskew ds delay control register*/
    volatile uint32_t skew_ctrl;    /*(0x184) deskew control control register*/
    volatile uint32_t res8[30];     /* (0x188~0x1ff) */
    volatile uint32_t fifo;         /* (0x200) SMC FIFO Access Address */
    volatile uint32_t res7[63];     /* (0x201~0x2FF)*/
    volatile uint32_t vers;         /* (0x300) SMHC Version Register */
};

// I need to go over the struct again and get everything in detail
struct sdhci {
    char *name; // idk name for what
    struct sdhci_reg_t *reg;
    uint8_t id;
    uint32_t reset;

    uint32_t voltage;
    uint32_t width;

    uint32_t pclk;
    uint32_t dma_trglvl;

    bool removable;
    bool isspi;
};

int sdhci_init(struct sdhci *sdhci) {
  gpio_set_func(PORT_F, 0, 2);
  gpio_set_pull(PORT_F,0,INTERNAL_PULL_UP);

  return 0;
}
