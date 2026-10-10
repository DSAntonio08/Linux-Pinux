/* Defines where code, globals, and stack belong in SRAM */
/* describes the layout */
/* Is working "hand in hand" with ASM */

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