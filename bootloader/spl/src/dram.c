#include "../include/dram.h"
#include "../include/t113_regs.h"

void dram_init() {
    /*
    The steps

    1. Clocks: set PLL_DDR to the desired frequency (clock_enable_pll comes in handy here), then configure DRAM_CLK_REG (0x800) and MBUS_CLK_REG (0x540), and open the gate and release reset in DRAM_BGR_REG
    (0x80C). This part is documented in the CCU chapter.
    2. ZQ calibration: the controller calibrates its output impedance against a reference resistor so the signals are clean.
    3. Configure the controller: RAM type (DDR3), bus width (16 bit), number of rows, columns and banks.
    4. Timing parameters: values from the DDR3 world like CAS latency, tRCD, tRP, tRAS, tRFC and the refresh interval, converted to clock cycles.
    5. DDR3 init sequence: reset → CKE → write the mode registers MR0–MR3 into the RAM chip.
    6. Training: find out the exact delays for the data lines (DQS gating, read/write leveling).
    7. Check: write patterns into DRAM, read them back, and detect the size. You should end up with 128 MB.


    Writing DRAM init purely from the manual is impossible. Writing it without documentation would be serious reverse-engineering work, and that's not realistic for a Matura project. The realistic
    approach:

    1. Do step 1 yourself. The clocks are documented, and it builds on what you already know.
    2. Read and understand the existing open-source code, then port it into your own structure with your style, your comments and your register defines.
    3. Document it honestly, something like: "DRAM init is based on the reverse-engineered implementation from X, because Allwinner doesn't publish the controller." That's completely legitimate and
    shows you understood the problem. Every other T113 bootloader does exactly the same.
    */
}
