	.extern main

	.global start
start:
	.extern printf_init
	call printf_init
	.extern heap_init
	call heap_init
	call main

	push %eax
loop:
	call exit
	jmp loop
