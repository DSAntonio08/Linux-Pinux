/*
 * clock.h - CCU (Clock Controller Unit) setup, implemented in clock.c
 */

#pragma once

#include "../include/t113_regs.h"
#include <stdint.h>
#include <stdio.h>

void cl_init_uart();
void enPLL();