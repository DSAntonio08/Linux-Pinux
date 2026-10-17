/*
 * linker.ld - tells the linker WHERE everything goes in SRAM A1 (0x20000, 32K)
 *
 * Memory layout:
 *   0x20000  .text.entry  header + start.S (must be first, BROM jumps to byte 0)
 *            .text        C code
 *            .rodata      const + strings
 *            .data        globals with a value
 *            ------------ spl_end, everything above here is in the .bin
 *            .bss         globals that start at 0 (NOLOAD, start.S zeroes it)
 *            free
 *   0x27000  .stack       4K, grows down (NOLOAD)
 *   0x28000  stack_top
 *
 * Symbols for start.S: spl_size, bss_start, bss_end, stack_top
 * ASSERTs stop the build if .bss isn't 4-byte aligned or runs into the stack
 *
 * SRAM A1 from manual 3.1, p.34
 */

/* found in 3.1 Memory Mapping - Page 34 */
START_ADDRESS =0x00020000 ;
SIZE_BYTES = 32K;
STACK_SIZE =4K; 

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
    .text : {
        /* Record the address before the boot header */
        spl_start = .; /*0x20000*/
        /* Place the entry code and header here
        basically the asm and the linker talk to 1 another over this "input asm here" call */
        KEEP(*(.text.entry))
        *(.text*)
    } > SRAM

    .rodata : { 
        *(.rodata*) 
    } > SRAM
    .data : {  
        *(.data*) 
    } > SRAM

    /* Record the address just after these contents */
    spl_end = .;
    
    .bss (NOLOAD) : {
        /* round up to the next multiple of 4*/ 
        . = ALIGN(4); /* We do this, because the clear loop in start.S writes 4 bytes per step */
        bss_start = .;
        *(.bss*) /* from every file, put every .bss type var here */
        *(COMMON) 
        . = ALIGN(4);
        bss_end = .;
    } > SRAM    

    /* that the STACK starts at this Address */
    .stack ORIGIN(SRAM) + LENGTH(SRAM) - STACK_SIZE (NOLOAD) : {
        stack_bottom = .;
        . = . + STACK_SIZE;
        stack_top = .;
    } > SRAM

    /* Calculate the image length in bytes */
    spl_size = spl_end - spl_start;

    /* if ! - give error msg "bss not aligned" */
    ASSERT(bss_start % 4 == 0 && bss_end % 4 == 0, "bss not 4-byte aligned")
    ASSERT(bss_end <= stack_bottom, "bss overlaps stack") 
}