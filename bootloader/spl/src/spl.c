// Secondary Program Loader (running in SRAM/ Static RAM)

/*
SPLs tasks:
- establish a Stack and Initialize C globals
- setup essential clocks and pins
- enable Serial-Output for debugging
- initialize DRAM/Dynamic RAM
- read following 'actual' Bootloader into DRAM
- jump to actual Bootloader
*/

int a = 1;       // should be .data
int b;           // should be .bss
const int c = 2; // should be .rodata

int main(void) { return a + b + c; } // should be .text
