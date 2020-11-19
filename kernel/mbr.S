/*
    This is the bootstrap code.

    It will be stored in sector#0 of the boot drive but the BIOS will load
    it at address 0x7c00

    We will compile the MBR at 0x7C00 then extract the appropriate 512 byte
    block and copy it to the boot drive


*/

    // see script.ld to see how this code ends up at 0x7C00
    .section .mbr16,"awx",@progbits

    .code16
    .global start
start:
    cli            # disable interrupts

    mov $0,%ax        # initialize data and stack segments
    mov %ax,%ds
    mov %ax,%ss

    movb %dl,0x7000        # save boot drive id

    # print an 'x' to let the world know we're here
    call onex

    # read the drive parameters
    movw $30,0x6000
    movb $0x48,%ah        # function code (read drive parameters)
    movb 0x7000,%dl        # drive index
    movw $0x6000,%si    # result buffer
    int $0x13        # call the BIOS

    movw 0x6010,%cx        # low 16 bits of sector count
                # this limits hdd size to 64k * 512 = 32MB


    mov $0x7e0/* 0 */,%di # where to read next sector
    mov $1,%bp              # next sector number


    # read hdc into memory starting at loadKernelHere
1:
    call onex
    movw $0x6000,%si    # DAP pointer
    movb $16,(%si)        # size of buffer
    movb $0,1(%si)        # unused
    add $-1,%cx        # -1 sector

    cmp $0,%cx
    jz 1f

    movw $1,2(%si)        # number of sectors
    movw $0,4(%si)    # buffer offset
    movw %di,6(%si)        # buffer segment
    add $0x20/*0*/,%di
    movw %bp,8(%si)        # starting sector number
    inc %bp
    movw $0,10(%si)
    movw $0,12(%si)
    movw $0,14(%si)

    mov $0x42,%ah        # function code
    movb 0x7000,%dl        # drive index
    int $0x13        # read the sectors
    jmp 1b

1:
    xor %cx,%cx
    xor %bx,%bx
    xor %dx,%dx
    mov $0xe801,%ax
    int $0x15

    mov %ax,memInfoAX
    mov %bx,memInfoBX
    mov %cx,memInfoCX
    mov %dx,memInfoDX
    mov $0x8000,%ax
    jmp *%ax
onex:
    pusha
    movb $'x',%al
    movb $1,%ah
    mov $0,%dx
    int $0x14
    popa
    ret
end:
    .skip 510 - (end - start)
    .byte 0x55
    .byte 0xaa

// 0x7e00

loadKernelHere:

// 0x8000 - important because the SMP entry point needs to be a multiple of 4K

    // see script.ld to kow how this code ends up at 0x8000
    .section .entry16,"awx",@progbits
    .global resetEIP
resetEIP:
    cli
    mov $0, %ax
    mov %ax,%ds
    mov %ax,%ss

    # load the gdt
    lgdt gdtDesc

    # load the idt
    lidt idtDesc

    # enable protected mode, still running in 16 bit segment with
    # original segments
    mov %cr0,%eax
    or $1,%eax
    mov %eax,%cr0

    # do a far jump to the kernel entry point in 32 bit mode
    ljmp $8,$kk

/***********/
/* The TSS */
/***********/

// Needs to be in the low 64K because of the way
//   we're setting up the GDT
// 104 * 16 (processors)
// esp0(i) = 4 + 104*i
// ss0(i) = 8 * 104*i
    .global tss
tss:
    .skip 104 * 16

// Everyting below this point is 32 bit


    .section .entry32,"awx",@progbits
    .code32

kk:
    mov $16,%eax
    mov %eax,%ds
    mov %eax,%es
    mov %eax,%fs
    mov %eax,%gs
    mov $24,%eax
    mov %eax,%ss
    mov $40,%eax

    .extern pickKernelStack
    .extern kernelInit

    mov $tempStack,%esp
    call pickKernelStack
    mov %eax,%esp
    call kernelInit
    ud2


/***********/
/* MemInfo */
/***********/

    .global memInfo
memInfo:
memInfoAX:    .word 0
memInfoBX:    .word 0
memInfoCX:    .word 0
memInfoDX:    .word 0


/***********/
/* The GDT */
/***********/
gdt:
    .long 0            /* #0 Always 0 */
    .long 0
    .long 0x0000FFFF   /* #1 kernel CS */
    .long 0x00CF9a00
    .long 0x0000FFFF   /* #2 kernel/user DS */
    .long 0x00CFF200
    .long 0x0000FFFF   /* #3 kernel SS */
    .long 0x00CF9200
    .long 0x0000FFFF   /* #4 user CS */
    .long 0x00CFFa00
// TSS descriptors per CPU
    .word 104          /* #5 TSS for CPU#0 */
    .word tss + 0 * 104
    .long 0x00008900
    .word 104
    .word tss + 1 * 104
    .long 0x00008900
    .word 104
    .word tss + 2 * 104
    .long 0x00008900
    .word 104
    .word tss + 3 * 104
    .long 0x00008900
    .word 104
    .word tss + 4 * 104
    .long 0x00008900
    .word 104
    .word tss + 5 * 104
    .long 0x00008900
    .word 104
    .word tss + 6 * 104
    .long 0x00008900
    .word 104
    .word tss + 7 * 104
    .long 0x00008900
    .word 104
    .word tss + 8 * 104
    .long 0x00008900
    .word 104
    .word tss + 9 * 104
    .long 0x00008900
    .word 104
    .word tss + 10 * 104
    .long 0x00008900
    .word 104
    .word tss + 11 * 104
    .long 0x00008900
    .word 104
    .word tss + 12 * 104
    .long 0x00008900
    .word 104
    .word tss + 13 * 104
    .long 0x00008900
    .word 104
    .word tss + 14 * 104
    .long 0x00008900
    .word 104
    .word tss + 15 * 104
    .long 0x00008900
gdtEnd:

gdtDesc:
    .word (gdtEnd - gdt) - 1
    .long gdt

    .global kernelCS
kernelCS:
    .long 8

    .global kernelUserDS
kernelDS:
    .long 16

    .global kernelSS
kernelSS:
    .long 24

    .global userCS
userCS:
    .long 32 + 3

    .global userSS
userSS:
    .long 16 + 3

    .global tssDescriptorBase
tssDescriptorBase:
    .long 40

    .global idt
    .align 64
idt:
    .skip 256*8, 0
idtEnd:

idtDesc:
    .word (idtEnd - idt) - 1
    .long idt

    .data

    .align 16
    .skip 4096
tempStack:
    .word 0
