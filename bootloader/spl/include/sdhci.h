#pragma once

#include <stdbool.h>
#include <stdint.h>

/* forward declaration - "actual" declaration lives in shdci.c */
struct sdhci;

// add comment that sdhci stands for Secure Digital Host Controller Interface

int sdhci_init(struct sdhci *sdhci);
