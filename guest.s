	mov $0x2a, %al
	mov $0x3f8, %edx
loop:
	out %al,(%dx)
	jmp loop
