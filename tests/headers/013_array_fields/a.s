	.text
	.p2align 4
	.globl sum
	.type sum, @function
	.cfi_startproc
sum:
	.cfi_def_cfa %rsp, 8
.LS1:
	movzbl 0(%rdi), %r10d
	movzbl 1(%rdi), %r11d
	addb %r11b, %r10b
	movzbl 2(%rdi), %r11d
	addb %r10b, %r11b
	movzbl %r11b, %r10d
	movslq 4(%rdi), %r11
	addl %r11d, %r10d
	movslq 8(%rdi), %r11
	movq %r10, %rax
	addl %r11d, %eax
	movslq %eax, %rax
	ret
	.cfi_endproc
	.size sum, .-sum
	.section .note.GNU-stack,"",@progbits
