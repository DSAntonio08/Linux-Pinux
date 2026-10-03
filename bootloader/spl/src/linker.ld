/* Defines where code, globals, and stack belong in SRAM */
/* describes the layout */
/* Is working "hand in hand" with ASM */

/* found in 3.1 Memory Mapping - Page 34 */
START_ADDRESS =0x00020000 ;
SIZE_BYTES = 32K;

/* StartPoint basically */
ENTRY(start)

/* Define avail. Memory */
MEMORY 
{
    /*rwx = read,write,executable */
    SRAM (rwx) : ORIGIN = START_ADDRESS, LENGTH = SIZE_BYTES
}

SECTIONS
{
    .text :
    {
        /* Record the address before the boot header */
        spl_start = .;

        /* Place the entry code and header here
        basically the asm and the linker talk to 1 another over this "input asm here" call */
        KEEP(*(.text.entry))

        /* Record the address just after these contents */
        spl_end = .;
    } > SRAM

    /* Calculate the image length in bytes */
    spl_size = spl_end - spl_start;
}