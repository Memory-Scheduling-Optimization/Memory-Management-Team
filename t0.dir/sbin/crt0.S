	.extern main

	.global start
start:
	.extern heap_init
	call heap_init
	.extern printf_init
	call printf_init
	call main

	push %eax
loop:
	call exit
	jmp loop
