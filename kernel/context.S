
	// contextSwitch(long* saveArea, long* restoreArea, void (*action)(long), long arg)
	.global gheith_contextSwitch
gheith_contextSwitch:
	mov 4(%esp),%eax
	mov 8(%esp),%ecx
	mov 12(%esp),%edx

	pushf

	mov %ebx,0(%eax)
	mov %esp,4(%eax)
	mov %ebp,8(%eax)
	mov %esi,12(%eax)
	mov %edi,16(%eax)

	mov %cr2,%ebx
	mov %ebx,20(%eax)

	// What follows is very delicate code. Let's remember where things are:
	//     - %eax points to the save area for the source thread
	//     - %ecx points to the save area for the target thread
	//     - %edx points to the function we need to call after we achieve the switch
	//     - %esp points to the source stack but everything has been shifted by 4 beacuse
	//       we pushed the flags above
	//     - the argument we need to pass to this function is still sitting on the source
	//       stack
	//     - both save areas have the no_preempt flag set
	//
	// Here is what we want to achieve:
	//     - restore the target context (this includes the interrupt state)
	//     - clear the no_preempt flags but only when we are in a safe state
	//     - switch stacks
	//     - call the switch handler on the new stack allowing it to manipuate the
	//       source TCB and stack in any way it sees fit (delete, schedule, ...)
	//
	// We will be walking on egg shells for a few instructions

	cli     # disable interrupts until we get our target context established
        	# why?
        	#    - we might be switching back into apitHander and need to be conservative
        	#    - give us a chance to quickly do any cleanup work we need to do

	// clear the no_preempt flag for both, interrupts are disabled so there is no risk (yet)
	movl $0,24(%eax)
	movl $0,24(%ecx)
    
	// This is the argument we need to pass to the handler.
	// We can get it now beacuse %eax is avialable and %esp still points to the old stack
	mov %esp,%eax        # remember the source ESP so we can get to the arguments
	//mov 20(%esp),%eax    # was 16 when we entered the function but we pushed the flags above

	// Now we can restore the target context, interrupts still disabled
	mov 20(%ecx),%ebx
	mov %ebx,%cr2
	
	mov 0(%ecx),%ebx
	mov 4(%ecx),%esp
	mov 8(%ecx),%ebp
	mov 12(%ecx),%esi
	mov 16(%ecx),%edi

	// Running on the target stack with interrupts disabled. Do any simple
	// the caller of block wanted to do
	//
	// The caller of contextSwitch gave a pointer to a function to call (the handler)
	// and an argument to pass to it (the argument). It also wants to get a pointer
	// to the source save area.
	//
	// void handler(SaveArea*, void*arg);
	//
	// But the handler needs to be careful and only do interrupt-safe things if
	// interrupts are disabled.
	//
	push 20(%eax)      # function argument
	push 8(%eax)       # source saveArea
	call *%edx         # call the function
	add $8,%esp

	// Now it is safe to restore the interrupt state
	popf

	ret
