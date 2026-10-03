@ Assembly that establishes the CPU state and stack, initializes the C environment, and calls your C entry function.

.syntax unified
@ CPU used by the T113-S3
.cpu cortex-a7
@ Generate ARM Instructions
.arm

@ a --> allocate
@ x --> executable following
.section .text.entry, "ax"

@ global so linker can 'see' it
.global start

@ Mark "start" as a function symbol; this does not generate a CPU instruction
.type start, %function

start: 

    @ jump to spl_entry, we do this to skip the header which only contains Information for the Boot-ROM which is already over atp 
    b spl_entry

    @ Signature so Boot ROM can recognize the image format
    .byte 'e', 'G', 'O', 'N', '.', 'B', 'T', '0'
    
    @ Checksum placeholder, replace by the packaging tool for Boot ROM to check 
    .long 0x12345678 

    @ Public boot header size: 48 bytes
    .long 0x30

    @ Boot Image size: calculated in linker
    .long spl_size
    
    @ defines the format/protocol (that every1 understands the information the same)
    @ Header layout: 4 bytes branch, 8 bytes signature, 4 bytes checksum, 4 bytes image size
    .byte '3', '0', '0', '0'
    
    @ I cannot figure what the Boot-ROM uses these adresses for, but AWBoot writes em so here they r
    .long 0x00020000                                 /* return value */
    .long 0x00028000                                /* run address */
	.long 0x0                                       /* eGON version */
	.byte 0x00, 0x00, 0x00, 0x00                    /* platform information - 8byte */
	.byte 0x34, 0x2e, 0x30, 0x00

spl_entry:
    @ Temporary loop until I implement CPU setup
    b spl_entry

@ generate MetaData abt the size of the 'start' - Program
.size start, . - start
