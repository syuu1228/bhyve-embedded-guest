# $FreeBSD$

PROG=	load
SRCS=	load.c
MAN=

DPADD+=	${LIBVMMAPI} ${LIBUTIL}
LDADD+=	-lvmmapi -lutil
CFLAGS=-g

WARNS?=	3

clean:
	rm -f guest.o guest.bin load load.o

guest.o:
	as -o guest.o guest.s

guest.bin: guest.o
	objcopy -I elf64-x86-64 -O binary guest.o guest.bin

run: load
	sudo bhyvectl --vm=test1 --destroy
	sudo ./load
	sudo bhyve -c 1 -m 128 -s 0:0,hostbridge -S 31,uart,stdio test1

.include <bsd.prog.mk>
