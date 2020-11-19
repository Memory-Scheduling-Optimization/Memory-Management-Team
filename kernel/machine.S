	# outb(int port, int val)
	.global outb
outb:
	push %edx
	mov 8(%esp),%dx
	mov 12(%esp),%al
	outb %al,%dx
	pop %edx
	ret

	# outl(int port, int val)
        .global outl
outl:
        mov 4(%esp),%dx
        mov 8(%esp),%eax
        outl %eax,%dx
        ret

	# int inb(int port)
	.global inb
inb:
	push %edx
	mov 8(%esp),%dx
	inb %dx,%al
	pop %edx
	and $0xff,%eax
	ret

	# unsigned long inl(int port)
	.global inl
inl:
	push %edx
	mov 8(%esp),%dx
	inl %dx,%eax
	pop %edx
	ret

    .globl rdmsr
    # uint64_t rdmsr(uint32_t id)
rdmsr:
    mov 4(%esp), %ecx
    rdmsr
    ret

    .globl wrmsr
    # wrmsr (uint32_t id, uint64_t value)
wrmsr:
    mov 4(%esp), %ecx
    mov 8(%esp), %eax
    mov 12(%esp), %edx
    wrmsr
    ret

    .global spuriousHandler_
spuriousHandler_:
    iret

    .extern apitHandler
    .global apitHandler_
apitHandler_:
    // TODO: XMM, MMX, FP, ...
    pusha
    push %esp
    call apitHandler
    pop %esp
    popa
    iret

    .global sti
sti:
    sti
    ret

    .global cli
cli:
    cli
    ret

    .global getFlags
getFlags:
    pushf
    pop %eax
    ret

	/* memcpy(void* dest, void* src, size_t n) */
        .global memcpy
memcpy:
        mov 4(%esp),%eax       # dest
        mov 8(%esp),%edx       # src
        mov 12(%esp),%ecx      # n
        push %ebx
1:
        add $-1,%ecx
        jl 1f
        movb (%edx),%bl
        movb %bl,(%eax)
        add $1,%edx
        add $1,%eax
        jmp 1b
1:
        pop %ebx
        ret

     /* bzero(void* dest, size_t n) */
    .global bzero
bzero:
    mov 4(%esp),%eax       # dest
    mov 8(%esp),%ecx      # n
1:
    add $-1,%ecx
    jl 1f
    movb $0,(%eax)
    add $1,%eax
    jmp 1b
1:
    ret

	# ltr(uint32_t tr)
	.global ltr
ltr:
	mov 4(%esp),%eax
	ltr %ax
	ret

    # monitor(void* ptr)
    .global monitor
monitor:
    mov 4(%esp),%eax
    xor %ecx,%ecx
    xor %edx,%edx
    monitor
    ret

    # mwait()
    .global mwait
mwait:
    xor %eax,%eax
    xor %ecx,%ecx
    xor %edx,%edx
    mwait
    ret

    # cpuid(long eax, cpuid_out* out)
    #          12              16
    .global cpuid
cpuid:
    push %ebx          // moves everything by 4
    push %esi          // moves everything by 4+4=8
    mov 12(%esp),%eax
    cpuid
    mov 16(%esp),%esi
    mov %eax,0(%esi)
    mov %ebx,4(%esi)
    mov %ecx,8(%esi)
    mov %edx,12(%esi)
    pop %esi
    pop %ebx
    ret

/* invlpg(uint32_t va) */
    .global invlpg
invlpg:
    mov 4(%esp),%eax
    invlpg (%eax)
    ret

    .global pageFaultHandler_
pageFaultHandler_:
    pusha

    mov %esp,%eax
    push %eax       /* second argument */
    mov %cr2,%eax   /* address */
    push %eax       /* first argument (va) */

    .extern vmm_pageFault
    call vmm_pageFault   /* (va, sp) */

    add $8,%esp    /* pop arguments */

    popa

    add $4,%esp   /* pop error */
    iret

    /* vmm_on(uint32_t pd) */
    .global vmm_on
vmm_on:
    mov 4(%esp),%eax
    mov %eax,%cr3

    mov %cr0,%eax
    or $0x80000000,%eax
    mov %eax,%cr0
    ret

    /* uint32_t getCR3() */
    .global getCR3
getCR3:
    mov %cr3,%eax
    ret

	# switchToUser(pc,esp,eax)
        .global switchToUser
switchToUser:
        mov 4(%esp),%ecx        # pc
        mov 8(%esp),%ebx        # esp
        mov 12(%esp),%eax       # eax
        pushl userSS            # user SS
        pushl %ebx              # user ESP
        pushf                   # ring 3 flags
        orl $0x200,(%esp)       # enable interrupts
        pushl userCS            # user CS
        pushl %ecx              # user EIP
        iret


	.global sysHandler_
sysHandler_:
        push %esp
        push %eax
        .extern sysHandler
        call sysHandler
        add $8,%esp
        iret

	/* Added by me */

	.global setWP
setWP:
	mov %cr0,%eax
	or $0x10000,%eax
	mov %eax,%cr0
	ret
	
	.global setCR3
setCR3:
	mov 4(%esp),%eax
	mov %eax,%cr3
	ret

	.global setPGE
setPGE:
	mov %cr4,%eax
	or $0x80,%eax
	mov %eax,%cr4
	ret
