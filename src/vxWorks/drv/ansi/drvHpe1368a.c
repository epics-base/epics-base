/* drvHpe1368a.c*/
/* base/src/drv $Id$ */

/*
 *      hpe1368a_driver.c
 *
 *      driver for hpe1368a and hpe1369a microwave switch VXI modules
 *
 *      Author:      Jeff Hill
 *      Date:        052192
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 *	.01 071792 joh	Added model name registration
 *	.02 081992 joh	vxiUniqueDriverID -> epvxiUniqueDriverID	
 *	.03 082692 mrk	Added support for new I/O event scanning and DRVET
 *	.04 080493 mgb	Removed V5/V4 and EPICS_V2 conditionals
 *
 */

static char *sccsId = "@(#)drvHpe1368a.c	1.14\t9/9/93";

#include <vxWorks.h>
#include <iv.h>
#include <types.h>
#include <intLib.h>
#include <sysLib.h>
#include <stdioLib.h>
#include <vxLib.h>

#include  "dbDefs.h"
#include  "errlog.h"
#include <module_types.h>
#include <task_params.h>
#include <fast_lock.h>
#include <drvEpvxi.h>
#include <dbDefs.h>
#include <drvSup.h>
#include <dbScan.h>
#include <devLib.h>

#include <drvHpe1368a.h>


#define HPE1368A_PCONFIG(LA, PC) \
epvxiFetchPConfig((LA), hpe1368aDriverId, (PC))

#define ChannelEnable(PCSR) ((PCSR)->dir.w.dd.reg.ddx08)
#define ModuleStatus(PCSR) ((PCSR)->dir.r.status)

#define ALL_SWITCHES_OPEN	0

struct hpe1368a_config{
	FAST_LOCK	lock;		/* mutual exclusion */
	uint16_t	pending;	/* switch position pending int */
	uint16_t	shadow;		/* shadow of actual switch pos */
	int		busy;		/* relays active */
        IOSCANPVT 	ioscanpvt;
};

#define HPE1368A_INT_LEVEL	1	

LOCAL int hpe1368aDriverId;

LOCAL void hpe1368a_int_service(unsigned la);
LOCAL void hpe1368a_init_card(unsigned la);
LOCAL void hpe1368a_stat(unsigned la, int level);

struct {
        long   	 	number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvHpe1368a={
        2,
        NULL,	/*VXI io report takes care of this */
	hpe1368a_init};


/*
 * hpe1368a_init
 *
 * initialize all hpe1368a cards
 *
 */
hpe1368aStat hpe1368a_init(void)
{
        hpe1368aStat	r0;

        /*
         * do nothing on crates without VXI
         */
        if(!epvxiResourceMangerOK){
                return VXI_SUCCESS;
        }

        hpe1368aDriverId = epvxiUniqueDriverID();

        {
                epvxiDeviceSearchPattern  dsp;

                dsp.flags = VXI_DSP_make | VXI_DSP_model;
                dsp.make = VXI_MAKE_HP;
                dsp.model = VXI_MODEL_HPE1368A;
                r0 = epvxiLookupLA(&dsp, hpe1368a_init_card, (void *)NULL);
                if(r0){
			errMessage(r0, NULL);
                        return r0;
                }
        }

        return VXI_SUCCESS;
}



/*
 * HPE1368A_INIT_CARD
 *
 * initialize single at5vxi card
 *
 */
LOCAL void hpe1368a_init_card(unsigned la)
{
        hpe1368aStat		r0;
        struct hpe1368a_config	*pc;
	struct vxi_csr		*pcsr;
	int			model;
	
        r0 = epvxiOpen(
                la,
                hpe1368aDriverId,
                (unsigned long) sizeof(*pc),
                hpe1368a_stat);
        if(r0){
		errMessage(r0,NULL);
                return;
        }

        r0 = HPE1368A_PCONFIG(la, pc);
        if(r0){
		errMessage(r0, NULL);
                return;
        }

	pcsr = VXIBASE(la);

	/*
	 * we must reset the device to a known state since
	 * we cant read back the current state
	 */
	pc->pending = ALL_SWITCHES_OPEN;
	pc->shadow = ALL_SWITCHES_OPEN;
	ChannelEnable(pcsr) = ALL_SWITCHES_OPEN;

	FASTLOCKINIT(&pc->lock);
        scanIoInit(&pc->ioscanpvt);

        r0 = intConnect(
		INUM_TO_IVEC(la),
		hpe1368a_int_service,
		la);
	if(r0 == ERROR){
		errMessage(S_dev_vxWorksVecInstlFail, NULL);
		return;
	}

	sysIntEnable(HPE1368A_INT_LEVEL);

        model = VXIMODEL(pcsr);
        r0 = epvxiRegisterModelName(
                        VXIMAKE(pcsr),
                        model,
                        "E 1368A Microwave Switch\n");
	if(r0){
		errMessage(r0, NULL);
        }
	r0 = epvxiRegisterMakeName(VXIMAKE(pcsr), "Hewlett-Packard");
	if(r0){
		errMessage(r0,NULL);
	}
}


/*
 *
 * hpe1368a_int_service()
 *
 *
 * This device interrupts once the 
 * switches have settled
 *
 */
LOCAL void
hpe1368a_int_service(unsigned la)
{
	hpe1368aStat		s;
        struct hpe1368a_config	*pc;

	s = HPE1368A_PCONFIG(la,pc);
        if(s){
                return;
        }

	/*
	 * operation completed so we can update 
	 * the shadow value
	 */
	pc->shadow = pc->pending;
	pc->busy = FALSE;

	/*
	 * tell them that the switches have settled
	 */
        scanIoRequest(pc->ioscanpvt);
}


/*
 * HPE1368A_STAT
 *
 * initialize single at5vxi card
 *
 */
LOCAL void hpe1368a_stat(
unsigned 	la,
int		level
)
{
	hpe1368aStat		s;
        struct hpe1368a_config	*pc;
	struct vxi_csr		*pcsr;

        s = HPE1368A_PCONFIG(la, pc);
        if(s){
		errMessage(s,NULL);
                return;
        }
	pcsr = VXIBASE(la);

	if(level>0){
		printf("\tSwitch states %x\n", pc->shadow);
		printf("\tModule status %x\n", pcsr->dir.r.status);
		if(pc->busy){
			printf("\tModule is busy.\n");
		}
	}
}


/*
 * hpe1368a_getioscanpvt()
 */
hpe1368aStat hpe1368a_getioscanpvt(
unsigned 	la,
IOSCANPVT 	*scanpvt 
)
{
	hpe1368aStat		s;
        struct hpe1368a_config	*pc;

        s = HPE1368A_PCONFIG(la, pc);
        if(s){
		errMessage(s, NULL);
		return s;
	}
	*scanpvt = pc->ioscanpvt;
	return VXI_SUCCESS;
}


/*
 * HPE1368A_BO_DRIVER
 */
hpe1368aStat hpe1368a_bo_driver(
unsigned 	la,
unsigned 	val,
unsigned 	mask 
)
{
	hpe1368aStat		s;
        struct hpe1368a_config	*pc;
        struct vxi_csr		*pcsr;
	unsigned int		work;	
	
        s = HPE1368A_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return s;
	}
        
	pcsr = VXIBASE(la);

	FASTLOCK(&pc->lock);

	work = pc->pending;

	/* alter specified bits */
	work = (work & ~mask) | (val & mask);

	pc->pending = work;

	ChannelEnable(pcsr) = work;

	FASTUNLOCK(&pc->lock);

	return VXI_SUCCESS;
}



/*
 *
 * HPE1368A_BI_DRIVER
 *
 *
 *
 */
hpe1368aStat hpe1368a_bi_driver(
unsigned 	la,
unsigned 	mask,
unsigned 	*pval 
)
{
	hpe1368aStat		s;
        struct hpe1368a_config	*pc;
	
        s = HPE1368A_PCONFIG(la, pc);
        if(s){
		errMessage(s, NULL);
                return s;
        }

	FASTLOCK(&pc->lock);

	*pval = pc->shadow & mask;

	FASTUNLOCK(&pc->lock);

	return VXI_SUCCESS;
}
