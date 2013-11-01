/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*-
 * Copyright (c) 2011 Google, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/stat.h>
#include <machine/specialreg.h>
#include <machine/vmm.h>
#include <x86/segments.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vmmapi.h>

#define VM_NAME 		"test1"
#define VM_MEM_SIZE 		128 * 1024 * 1024UL

#define MSR_EFER        	0xc0000080
#define CR4_PAE         	0x00000020
#define CR4_PSE         	0x00000010
#define CR0_PG          	0x80000000
#define	CR0_PE			0x00000001
#define	CR0_NE			0x00000020

#define PG_V			0x001
#define PG_RW			0x002
#define PG_U			0x004
#define PG_PS			0x080

#define ADDR_PT4 		0x2000
#define ADDR_PT3 		0x3000
#define ADDR_PT2 		0x4000
#define ADDR_GDT 		0x5000
#define ADDR_STACK 		0x6000
#define ADDR_ENTRY 		0x10000

#define	DESC_UNUSABLE		0x00010000

#define	GUEST_NULL_SEL		0
#define	GUEST_CODE_SEL		1
#define	GUEST_DATA_SEL		2
#define	GUEST_GDTR_LIMIT	(3 * 8 - 1)

int
main(int argc, char** argv)
{
	struct vmctx *ctx;
	uint64_t *gdt, *pt4, *pt3, *pt2;
	int i;
	unsigned char *entry;
	unsigned char program[] = {
		0xb0, 0x2a, 			/* mov    $0x2a,%al */
		0xba, 0xf8, 0x03, 0x00, 0x00,	/* mov    $0x3f8,%edx */
		0xee,				/* out    %al,(%dx) */
		0xeb, 0xfd			/* jmp    7 <loop> */
	};

	vm_create(VM_NAME);
	ctx = vm_open(VM_NAME);
	vm_setup_memory(ctx, VM_MEM_SIZE, VM_MMAP_ALL);

	pt4 = vm_map_gpa(ctx, ADDR_PT4, sizeof(uint64_t) * 512);
	pt3 = vm_map_gpa(ctx, ADDR_PT3, sizeof(uint64_t) * 512);
	pt2 = vm_map_gpa(ctx, ADDR_PT2, sizeof(uint64_t) * 512);
	gdt = vm_map_gpa(ctx, ADDR_GDT, sizeof(uint64_t) * 3);
	entry = vm_map_gpa(ctx, ADDR_ENTRY, sizeof(program));

	bzero(pt4, PAGE_SIZE);
	bzero(pt3, PAGE_SIZE);
	bzero(pt2, PAGE_SIZE);
	for (i = 0; i < 512; i++) {
		pt4[i] = (uint64_t)ADDR_PT3;
		pt4[i] |= PG_V | PG_RW | PG_U;
		pt3[i] = (uint64_t)ADDR_PT2;
		pt3[i] |= PG_V | PG_RW | PG_U;
		pt2[i] = i * (2 * 1024 * 1024);
		pt2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}

	gdt[GUEST_NULL_SEL] = 0;
	gdt[GUEST_CODE_SEL] = 0x0020980000000000;
	gdt[GUEST_DATA_SEL] = 0x0000900000000000;

	memcpy(entry, program, sizeof(program));

	vm_set_desc(ctx, 0, VM_REG_GUEST_CS, 0, 0, 0x0000209B);
	vm_set_desc(ctx, 0, VM_REG_GUEST_DS, 0, 0, 0x00000093);
	vm_set_desc(ctx, 0, VM_REG_GUEST_ES, 0, 0, 0x00000093);
	vm_set_desc(ctx, 0, VM_REG_GUEST_FS, 0, 0, 0x00000093);
	vm_set_desc(ctx, 0, VM_REG_GUEST_GS, 0, 0, 0x00000093);
	vm_set_desc(ctx, 0, VM_REG_GUEST_SS, 0, 0, 0x00000093);
	vm_set_desc(ctx, 0, VM_REG_GUEST_TR, 0, 0, 0x0000008b);
	vm_set_desc(ctx, 0, VM_REG_GUEST_LDTR, 0, 0, DESC_UNUSABLE);

	vm_set_register(ctx, 0, VM_REG_GUEST_CS, GSEL(GUEST_CODE_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_DS, GSEL(GUEST_DATA_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_ES, GSEL(GUEST_DATA_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_FS, GSEL(GUEST_DATA_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_GS, GSEL(GUEST_DATA_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_SS, GSEL(GUEST_DATA_SEL, SEL_KPL));
	vm_set_register(ctx, 0, VM_REG_GUEST_TR, 0);
	vm_set_register(ctx, 0, VM_REG_GUEST_LDTR, 0);

	vm_set_register(ctx, 0, VM_REG_GUEST_CR0, CR0_PG | CR0_PE | CR0_NE);
	vm_set_register(ctx, 0, VM_REG_GUEST_CR3, 0x2000);
	vm_set_register(ctx, 0, VM_REG_GUEST_CR4, CR4_PAE | CR4_VMXE);
	vm_set_register(ctx, 0, VM_REG_GUEST_EFER, EFER_LMA | EFER_LME);
	vm_set_desc(ctx, 0, VM_REG_GUEST_GDTR, 0x5000, GUEST_GDTR_LIMIT, 0);
	vm_set_register(ctx, 0, VM_REG_GUEST_RFLAGS, 0x2);
	vm_set_register(ctx, 0, VM_REG_GUEST_RSP, ADDR_STACK);
	vm_set_register(ctx, 0, VM_REG_GUEST_RIP, ADDR_ENTRY);

	return (0);
}
