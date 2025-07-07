/*	$NecBSD: ct_isa.c,v 1.6 1999/07/26 06:32:01 honda Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: releng/11.4/sys/dev/ct/ct_isa.c 296137 2016-02-27 03:38:01Z jhibbits $");
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	SCSIBUS_RESCAN

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/md_var.h>

#include <pc98/cbus/cbus.h>
#include <isa/isavar.h>

#include <compat/netbsd/dvcfg.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ct/ctvar.h>
#include <dev/ct/bshwvar.h>

#define	BSHW_IOSZ	0x08
#define	BSHW_IOBASE 	0xcc0
#define	BSHW_MEMSZ	(PAGE_SIZE * 2)

static int ct_isa_match(device_t);
static int ct_isa_attach(device_t);
static int ct_space_map(device_t, struct bshw *,
			struct resource **, struct resource **);
static void ct_space_unmap(device_t, struct ct_softc *);
static struct bshw *ct_find_hw(device_t);
static void ct_dmamap(void *, bus_dma_segment_t *, int, int);
static void ct_isa_bus_access_weight(struct ct_bus_access_handle *);
//static void ct_isa_dmasync_before(struct ct_softc *);
//static void ct_isa_dmasync_after(struct ct_softc *);
//static long ct_find_hw2(struct ct_softc *);

struct ct_isa_softc {
	struct ct_softc sc_ct;
	struct bshw_softc sc_bshw;
};

static struct isa_pnp_id ct_pnp_ids[] = {
	{ 0x0100e7b1,	"Logitec LHA-301" },
	{ 0x110154dc,	"I-O DATA SC-98III" },
	{ 0x4120acb4,	"MELCO IFC-NN" },
	{ 0,		NULL }
};

static device_method_t ct_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ct_isa_match),
	DEVMETHOD(device_attach,	ct_isa_attach),
	{ 0, 0 }
};

static driver_t ct_isa_driver = {
	"ct", ct_isa_methods, sizeof(struct ct_isa_softc),
};

static devclass_t ct_devclass;

DRIVER_MODULE(ct, isa, ct_isa_driver, ct_devclass, 0, 0);

static int
ct_isa_match(device_t dev)
{
	struct bshw *hw;
	struct resource *port_res, *mem_res;
	struct ct_bus_access_handle ch;
	int rv;

	if (ISA_PNP_PROBE(device_get_parent(dev), dev, ct_pnp_ids) == ENXIO)
		return ENXIO;

	switch (isa_get_logicalid(dev)) {
	case 0x0100e7b1:	/* LHA-301 */
	case 0x110154dc:	/* SC-98III */
	case 0x4120acb4:	/* IFC-NN */
		/* XXX - force to SMIT mode */
		device_set_flags(dev, device_get_flags(dev) | 0x40000);
		break;
	}

	if (isa_get_port(dev) == -1)
		bus_set_resource(dev, SYS_RES_IOPORT, 0,
				 BSHW_IOBASE, BSHW_IOSZ);

	if ((hw = ct_find_hw(dev)) == NULL)
		return ENXIO;
	if (ct_space_map(dev, hw, &port_res, &mem_res) != 0)
		return ENXIO;

	bzero(&ch, sizeof(ch));
	ch.ch_io = port_res;
	ch.ch_bus_weight = ct_isa_bus_access_weight;

	rv = ctprobesubr(&ch, 0, BSHW_DEFAULT_HOSTID,
			 BSHW_DEFAULT_CHIPCLK, NULL);
	if (rv != 0)
	{
		struct bshw_softc bshw_tab;
		struct bshw_softc *bs = &bshw_tab;

		memset(bs, 0, sizeof(*bs));
		bshw_read_settings(&ch, bs);
if((inb(0x7ea) != 98) || (inb(0x7eb) != 21)){
		bus_set_resource(dev, SYS_RES_IRQ, 0, bs->sc_irq, 1);
		bus_set_resource(dev, SYS_RES_DRQ, 0, bs->sc_drq, 1);
}
	}


	bus_release_resource(dev, SYS_RES_IOPORT, 0, port_res);
	if (mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, mem_res);

	if (rv != 0)
		return (BUS_PROBE_DEFAULT);
	return ENXIO;
}

static int
ct_isa_attach(device_t dev)
{

//	if (inb(0x63f)!= 0xff){
//		if (inb(0x63f) & 0x80)//WriteBack machine??
//			need_post_dma_flush = 0;
//	}
	struct ct_isa_softc *pct = device_get_softc(dev);
	struct ct_softc *ct = &pct->sc_ct;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct bshw_softc *bs = &pct->sc_bshw;
	struct bshw *hw;
	int irq_rid, drq_rid, chiprev;

	hw = ct_find_hw(dev);
	if (ct_space_map(dev, hw, &ct->port_res, &ct->mem_res) != 0) {
		device_printf(dev, "bus io mem map failed\n");
		return ENXIO;
	}

	chp->ch_io = ct->port_res;
	chp->ch_mem = ct->mem_res;
	chp->ch_bus_weight = ct_isa_bus_access_weight;

if((inb(0x7ea) != 98) || (inb(0x7eb) != 21)){
	irq_rid = 0;
	ct->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &irq_rid,
					     RF_ACTIVE);
	drq_rid = 0;
	ct->drq_res = bus_alloc_resource_any(dev, SYS_RES_DRQ, &drq_rid,
					     RF_ACTIVE);
	if (ct->irq_res == NULL || ct->drq_res == NULL) {
		ct_space_unmap(dev, ct);
		return ENXIO;
	}
}
	if (ctprobesubr(chp, 0, BSHW_DEFAULT_HOSTID,
			BSHW_DEFAULT_CHIPCLK, &chiprev) == 0)
	{
		device_printf(dev, "hardware missing\n");
		ct_space_unmap(dev, ct);
		return ENXIO;
	}
	/* setup machdep softc */
	bs->sc_hw = hw;
	bs->sc_io_control = 0;

	bs->sc_bounce_phys = NULL;
	bs->sc_bounce_addr = NULL;
	bs->sc_bounce_size = 0;


	bs->sc_minphys = (1 << 24);
//	bs->sc_dmasync_before =  ct_isa_dmasync_before;
//	bs->sc_dmasync_after = ct_isa_dmasync_after;
	bshw_read_settings(chp, bs);

	/* setup ct driver softc */
	ct->ct_hw = bs;
	ct->ct_dma_xfer_start = bshw_dma_xfer_start;
	ct->ct_pio_xfer_start = bshw_smit_xfer_start;
	ct->ct_dma_xfer_stop = bshw_dma_xfer_stop;
	ct->ct_pio_xfer_stop = bshw_smit_xfer_stop;
	ct->ct_bus_reset = bshw_bus_reset;
	ct->ct_synch_setup = bshw_synch_setup;

	ct->sc_xmode = CT_XMODE_DMA;
	if (chp->ch_mem != NULL)
		ct->sc_xmode |= CT_XMODE_PIO;

	ct->sc_chiprev = chiprev;
	switch (chiprev)
	{
	case CT_WD33C93:
		/* s = "WD33C93"; */
		ct->sc_chipclk = 8;
		break;
	case CT_WD33C93_A:
		if (DVCFG_MAJOR(device_get_flags(dev)) > 0)
		{
			/* s = "AM33C93_A"; */
			ct->sc_chipclk = 20;
			ct->sc_chiprev = CT_AM33C93_A;
		}
		else
		{
			/* s = "WD33C93_A"; */
			ct->sc_chipclk = 8;
		}
		break;

	case CT_AM33C93_A:
		/* s = "AM33C93_A"; */
		ct->sc_chipclk = 20;
		break;

	default:
	case CT_WD33C93_B:
		/* s = "WD33C93_B"; */
		ct->sc_chipclk = 20;
		break;
	}

#if	1
	printf("chiprev chipclk %d MHz\n", 
		ct->sc_chipclk);
#endif

	slp->sl_dev = dev;
	slp->sl_hostid = bs->sc_hostid;
	slp->sl_cfgflags = device_get_flags(dev);

if(((*hw->hw_dma_init)(ct)) == 0x56){//IF-2771
//ct->sc_xmode &= ~(CT_XMODE_DMA|CT_XMODE_PIO);//data register mode veryyyy slow
//	hw->hw_sregaddr = 0x38;//??? iti humei
	printf("%x: chiprev IF-2771 chipclk %d MHz but 20.0MHz???\n", 
		 chiprev,ct->sc_chipclk);
	slp->sl_cfgflags = device_get_flags(dev);
	slp->sl_cfgflags |= CFG_ASYNC|CFG_NODISC;
}


if(slp->sl_cfgflags & 0x1000)//original parameter
	{
	ct->sc_xmode = CT_XMODE_FIFO;
//	ct->sc_xmode &= ~(CT_XMODE_DMA|CT_XMODE_PIO);//data register mode veryyyy slow
//	ct->sc_xmode = 0;//data register mode veryyyy slow
//	printf("data register mode veryyyy slow\n");
	hw->hw_dma_start = NULL;
	hw->hw_dma_stop = NULL;
	hw->hw_sregaddr = 0;

	ct->ct_pio_xfer_start = bshw_fifo_xfer_start;
	ct->ct_pio_xfer_stop = bshw_fifo_xfer_stop;
	}
else
{
	if(((*hw->hw_dma_init)(ct)) == 0){
		isa_dma_init16(bs->sc_drq ,0x10000 ,M_ZERO|0x8000);
		hw->hw_dma_start = NULL;
		hw->hw_dma_stop = NULL;
		hw->hw_sregaddr = 0;
	}else isa_dma_init16(bs->sc_drq ,0x10000 ,M_ZERO);
}

	if((inb(0x7ea) == 98) && (inb(0x7eb) == 21)){//NekoProjectII FAST SCSI Controller
		ct->sc_xmode = 0;//data register mode veryyyy slow
	}

	mtx_init(&slp->sl_lock, "ct", NULL, MTX_DEF);

	ctattachsubr(ct);

if((inb(0x7ea) != 98) || (inb(0x7eb) != 21)){
	if (bus_setup_intr(dev, ct->irq_res, INTR_TYPE_CAM | INTR_MPSAFE,
			   NULL, ctintr, ct, &ct->sc_ih)) {
		ct_space_unmap(dev, ct);
		return ENXIO;
	}
}
	return 0;
}

static struct bshw *
ct_find_hw(device_t dev)
{
	return DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(device_get_flags(dev)));
}


static int
ct_space_map(device_t dev, struct bshw *hw,
	     struct resource **iohp, struct resource **memhp)
{
	int port_rid, mem_rid;

	*memhp = NULL;

	port_rid = 0;
	*iohp = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT, &port_rid,
					    BSHW_IOSZ, RF_ACTIVE);
	if (*iohp == NULL)
		return ENXIO;

	if ((hw->hw_flags & BSHW_SMFIFO) == 0 || isa_get_maddr(dev) == -1)
		return 0;

	mem_rid = 0;
	*memhp = bus_alloc_resource_anywhere(dev, SYS_RES_MEMORY, &mem_rid,
					     BSHW_MEMSZ, RF_ACTIVE);
	if (*memhp == NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, port_rid, *iohp);
		return ENXIO;
	}

	return 0;
}

static void
ct_space_unmap(device_t dev, struct ct_softc *ct)
{
	if (ct->port_res != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, ct->port_res);
	if (ct->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, ct->mem_res);
	if (ct->irq_res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, ct->irq_res);
	if (ct->drq_res != NULL)
		bus_release_resource(dev, SYS_RES_DRQ, 0, ct->drq_res);
}

static void
ct_dmamap(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	bus_addr_t *addr = (bus_addr_t *)arg;

	*addr = seg->ds_addr;
}

static void
ct_isa_bus_access_weight(struct ct_bus_access_handle *chp)
{
	outb(0x5f, 0);
}

/*
static void
ct_isa_dmasync_before(struct ct_softc *ct)
{
//	if (need_pre_dma_flush){
//		wbinvd();
//		outb(0x43f, 0xa0);
//	}
}

static void
ct_isa_dmasync_after(struct ct_softc *ct)
{
//	if (need_post_dma_flush){
//		invd();
//		outb(0x43f, 0xa0);
//	}
}
*/
