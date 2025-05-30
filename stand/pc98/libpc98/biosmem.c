/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: releng/11.4/stand/pc98/libpc98/biosmem.c 298230 2016-04-18 23:09:22Z allanjude $");

/*
 * Obtain memory configuration information from the BIOS
 */
#include <stand.h>
#include "libi386.h"
#include "btxv86.h"
#include "bootstrap.h"

vm_offset_t	memtop, memtop_copyin, high_heap_base,memtop0;
uint32_t	bios_basemem, bios_extmem, high_heap_size;

/*
 * The minimum amount of memory to reserve in bios_extmem for the heap.
 */
#define	HEAP_MIN	(64 * 1024 * 1024)

static 	u_int32_t bios_extmem_under16;
static	u_int32_t bios_extmem_over16;
void
bios_getmem(void)
{
	bios_extmem_under16 = *(u_char *)PTOV(0xA1401) * 128 * 1024;
	bios_extmem_over16 = *(u_int16_t *)PTOV(0xA1594) * 1024 * 1024; 
    bios_basemem = ((*(u_char *)PTOV(0xA1501) & 0x07) + 1) * 128 * 1024;
    bios_extmem = *(u_char *)PTOV(0xA1401) * 128 * 1024 +
	*(u_int16_t *)PTOV(0xA1594) * 1024 * 1024
	 - 4096;//Sorry memory decrease 4096byte for SMP_Table

    /* Set memtop to actual top of memory */
	if((bios_extmem_over16 > 0)&&(bios_extmem_under16 < 0xe00000))
    memtop = memtop_copyin = 0x1000000 + bios_extmem_over16;
	else
    memtop = memtop_copyin = 0x100000 + bios_extmem;

    memtop0 = 0x100000 + bios_extmem_under16;
    /*
     * If we have extended memory, use the last 3MB of 'extended' memory
     * as a high heap candidate.
     */
/*
    if (bios_extmem >= HEAP_MIN) {
	high_heap_size = HEAP_MIN;
	high_heap_base = memtop - HEAP_MIN;
    }
*/
/*
    if(bios_extmem_over16 >= 16 * 1024 * 1024){
	high_heap_size = bios_extmem_over16 - 4096;
	high_heap_base = 0x1000000;
    }else{
	high_heap_size = bios_extmem;//with 4096byte no-SMP
	high_heap_base = 0x100000;
    }
*/
}

static int
command_biosmem(int argc, char *argv[])
{

	printf("bios_basemem: 0x%llx\n",(unsigned long long)bios_basemem);
	printf("bios_extmem: 0x%llx\n",(unsigned long long)bios_extmem);
	printf("memtop0: 0x%llx\n",(unsigned long long)memtop0);
	printf("memtop: 0x%llx\n",(unsigned long long)memtop);
	printf("high_heap_base: 0x%llx\n",(unsigned long long)high_heap_base);
	printf("high_heap_size: 0x%llx\n",(unsigned long long)high_heap_size);

	printf("bios_under16: 0x%llx\n",(unsigned long long)bios_extmem_under16);
	printf("bios_over16 : 0x%llx\n",(unsigned long long)bios_extmem_over16);

	return (CMD_OK);
}
COMMAND_SET(biosmem, "biosmem", "show BIOS memory setup", command_biosmem);

