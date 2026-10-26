/*
 * io.h - read/write a 32-bit hardware register, shared by every driver
 */

#pragma once

#include <stdint.h>

static inline uint32_t read32(uintptr_t addr) {
    // volatile: this memory can change (or reacts to accesses) outside of this code, so the compiler must perform every read and write exactly as written, in order
    return *(volatile uint32_t *)addr; // return the value stored at the address held in addr
}

static inline void write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val; // write the "new" value into the address
}
