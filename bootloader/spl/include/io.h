/*
 * io.h - read/write a 32-bit hardware register, shared by every driver
 *
 * addr is an integer that holds a memory address; val, clear, and set are 32-bit values.
 * A hardware register is accessed like memory, but its value can control or report hardware.
 *
 * The syntax *(volatile uint32_t *)addr has two different * characters:
 *   uint32_t * means "pointer to a uint32_t"; this * is part of the pointer type
 *   (volatile uint32_t *)addr converts the integer address into a pointer to a volatile value
 *   the * before the parentheses' result means "follow this pointer and access the value there"
 * For example, *(volatile uint32_t *)addr = val writes val to the 32-bit value at addr.
 * The * in uint32_t * describes what kind of thing the pointer points to; the other * reads
 * from or writes to the address that the pointer holds. They do different jobs.
 *
 * volatile tells the compiler that hardware can change or react to this memory by itself.
 * The compiler must perform each read or write in the order the code requests it.
 *
 * io_read32(): reads the 32-bit value stored at addr, then returns that value
 * io_write32(): writes val to the 32-bit value stored at addr
 *
 * io_clearbits_le32(): clears the bits that are 1 in clear; every other bit stays unchanged
 *   math is: new value = old value AND NOT clear
 *   NOT clear flips every bit: selected 1s become 0s, so AND forces those old bits to 0
 *   the other bits in NOT clear are 1, so AND lets the old value pass through unchanged
 *
 * io_setbits_le32(): sets the bits that are 1 in set; every other bit stays unchanged
 *   math is: new value = old value OR set
 *   OR with 1 makes a bit 1; OR with 0 leaves that bit's old value unchanged
 *
 * io_clearsetbits_le32(): replaces selected bits and keeps all the other bits unchanged
 *   math is: new value = (old value AND NOT clear) OR set
 *   first clear removes the old setting, then OR inserts the new setting in those bit positions
 *   for a field, clear is its mask and set is the new field value shifted into position
 *
 * These clear/set helpers do read-modify-write: read the old register, calculate the new value,
 * then write it back. They preserve unrelated bits, but the full read/change/write is not atomic.
 */

#pragma once

#include <stdint.h>

static inline uint32_t io_read32(uintptr_t addr) {
    // volatile: this memory can change (or reacts to accesses) outside of this code, so the compiler must perform every read and write exactly as written, in order
    return *(volatile uint32_t *)addr; // return the value stored at the address held in addr
}

static inline void io_write32(uintptr_t addr, uint32_t val) {
    *(volatile uint32_t *)addr = val; // write the "new" value into the address
}

static inline void io_clearbits_le32(uintptr_t addr, uint32_t clear) {
    io_write32(addr, io_read32(addr) & ~(clear));
}

static inline void io_setbits_le32(uintptr_t addr, uint32_t set) {
    io_write32(addr, io_read32(addr) | set);
}

static inline void io_clearsetbits_le32(uintptr_t addr, uint32_t clear, uint32_t set) {
    io_write32(addr, (io_read32(addr) & ~(clear)) | (set));
}
