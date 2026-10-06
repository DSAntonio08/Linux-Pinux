@ Assembly that establishes the CPU state and stack, initializes the C environment, and calls the C entry function.

.syntax unified
@ CPU used by the T113-S3
.cpu cortex-a7
@ Generate ARM Instructions
.arm

@ a --> allocate
@ x --> executable following
.section .text.entry, "ax"

#define CPU_MODE_MASK 0x1f /* Bottom 5 Bits 1*/
#define CPU_MODE_SUPERVISOR 0x13 /* Bottom 5 Bits: 10011 */
#define ARMV7_IRQ_MASK 0x80   /* Bit 7: block IRQ */
#define ARMV7_FIQ_MASK 0x40   /* Bit 6: block FIQ */

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

    @ Boot Image size: calculated in linker
    .long spl_size

    @ Public boot header size: 48 bytes
    .long 0x30
    
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
    @ Copy CPSR (the MetaData abt the CPU's current operating Mode) to the register r0, we then change some values in the copy and later move it back to CPSR
    mrs r0, cpsr /* msr --> move to Status Register */

    @ we put the mask into r0, and that way clear r0's bottom 5 bits (src, destination, mask )
    bic r0,r0, #CPU_MODE_MASK

    @ same here, but now we don't clear but put the right bits to select Supervisor
    orr r0,r0, #CPU_MODE_SUPERVISOR /* We use # to geet the value */
    orr r0, r0, #(ARMV7_IRQ_MASK | ARMV7_FIQ_MASK) /* block both Interrupt Request & Fast Interrupt Request */
    msr cpsr_c, r0 /* save modified back in CPSR_control */
    setend le /* set little endian - means the most left byte is the least significant */
    
    @ load the vector table adress into r0
    ldr r0, =vector_handler /* = means Adress Value*/
    @ Write r0 into VBAR
    mcr p15, 0, r0, c12, c0, 0     /* mcr --> move to Coprocessor from ARM Register */
    @ System Control Register into r0
    mrc p15, 0, r0, c1, c0, 0
    bic r0, #(1<<13) /* clear bit 13 of the SCTLR Copy*/
    @ write the modified version back into the SCTLR
    mcr p15, 0, r0, c1, c0, 0

    @ Read SCTLR into r0
    mrc p15, 0, r0, c1, c0, 0
    @ Clear C, A and M: data caching, strict alignment checks and MMU (Memory Management Unit)
    bic r0, r0, #0x07
    @ Clear I to disable instruction caching
    bic r0, r0, #(1 << 12)
    @ Write the modified settings back to SCTLR
    mcr p15, 0, r0, c1, c0, 0
    
    @ jump with return, CoProcessor 15
    bl cpu_init_cp15
    @ Stay here until the next startup block is implemented
    b .

.balign 32
vector_handler:
    b .    @ +0x00: Reset slot
    b .    @ +0x04: Undefined instruction
    b .    @ +0x08: Supervisor call
    b .    @ +0x0C: Prefetch abort
    b .    @ +0x10: Data abort
    b .    @ +0x14: Reserved
    b .    @ +0x18: IRQ
    b .    @ +0x1C: FIQ

cpu_init_cp15:
    @ the same as before but not with SCTLR; ACTLR (Auxiliary Control Register)
    mrc p15, 0, r0, c1, c0, 1
    orr r0, r0, #(1 << 6)   /* set SMP Bit; this configures how the 2nd Core participates in the mem. System */
    mcr p15, 0, r0, c1, c0, 1

    dsb /* Data Synchronisation Barrier */
    isb /* Instruction Synchronisation Barrier, sync the CPU */
    @ return
    bx lr

@ generate MetaData abt the size of the 'start' - Program
.size start, . - start
