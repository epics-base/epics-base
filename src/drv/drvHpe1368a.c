/*
 *      hpe1368a_driver.c
 *
 *      driver for hpe1368a and hpe1369a VXI modules
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
 *
 */


#include <vxWorks.h>
#ifdef V5_vxWorks
#       include <iv.h>
#else
#       include <iv68k.h>
#endif
#include <types.h>
#include <module_types.h>
#include <task_params.h>
#include <fast_lock.h>
#include <epvxiLib.h>


#define VXI_MODEL_HPE1368A 	(0xf28)

#define HPE1368A_PCONFIG(LA) \
epvxiPConfig((LA), hpe1368aDriverId, struct hpe1368a_config *)

#define ChannelEnable(PCSR) ((PCSR)->dir.w.dd.reg.ddx08)
#define ModuleStatus(PCSR) ((PCSR)->dir.r.status)

#define ALL_SWITCHES_OPEN	0

struct hpe1368a_config{
	FAST_LOCK	lock;		/* mutual exclusion */
	unsigned short	pending;	/* switch position pending int */
	unsigned short	shadow;		/* shadow of actual switch pos */
	int		busy;		/* relays active */
};

#define HPE1368A_INT_LEVEL	1	

LOCAL
int hpe1368aDriverId;

void hpe1368a_int_service();
void hpe1368a_init_card();
void hpe1368a_stat();


/*
 * hpe1368a_init
 *
 * initialize all hpe1368a cards
 *
 */
hpe1368a_init()
{
        int     r0;

        /*
         * do nothing on crates without VXI
         */
        if(!epvxiResourceMangerOK){
                return OK;
        }

        hpe1368aDriverId = epvxiUniqueDriverID();

        {
                epvxiDeviceSearchPattern  dsp;

                dsp.flags = VXI_DSP_make | VXI_DSP_model;
                dsp.make = VXI_MAKE_HP;
                dsp.model = VXI_MODEL_HPE1368A;
                r0 = epvxiLookupLA(&dsp, hpe1368a_init_card, (void *)NULL);
                if(r0<0){
                        return ERROR;
                }
        }

        return OK;
}



/*
 * HPE1368A_INIT_CARD
 *
 * initialize single at5vxi card
 *
 */
LOCAL void
hpe1368a_init_card(la)
unsigned la;
{
        int                     r0;
        struct hpe1368a_config	*pc;
	struct vxi_csr		*pcsr;
	int			model;
	
        r0 = epvxiOpen(
                la,
                hpe1368aDriverId,
                (unsigned long) sizeof(*pc),
                hpe1368a_stat);
        if(r0<0){
                logMsg("hpe1368a: device open failed %d\n", la);
                return;
        }

        pc = HPE1368A_PCONFIG(la);
        if(pc == NULL){
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

        r0 = intConnect(
		(unsigned char) INUM_TO_IVEC(la),
		hpe1368a_int_service,
		(void *) la);
	if(r0 == ERROR)
		return;

	sysIntEnable(HPE1368A_INT_LEVEL);

        model = VXIMODEL(pcsr);
        r0 = epvxiRegisterModelName(
                        VXIMAKE(pcsr),
                        model,
                        "E 1368A Microwave Switch\n");
        if(r0<0){
        	logMsg("%s: failed to register model at init: %x\n",
                       __FILE__,
                       model);
        }
	r0 = epvxiRegisterMakeName(VXIMAKE(pcsr), "Hewlett-Packard");
	if(r0<0){
		logMsg( "%s: failed to register make\n", __FILE__);
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
hpe1368a_int_service(la)
unsigned	la;
{
        struct hpe1368a_config	*pc;

        pc = HPE1368A_PCONFIG(la);
        if(pc == NULL){
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
	io_scanner_wakeup(IO_BI, HPE1368A_BI, la);
}


/*
 * HPE1368A_STAT
 *
 * initialize single at5vxi card
 *
 */
LOCAL void
hpe1368a_stat(la,level)
unsigned 	la;
int		level;
{
        struct hpe1368a_config	*pc;
	struct vxi_csr		*pcsr;

        pc = HPE1368A_PCONFIG(la);
        if(pc == NULL){
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
 *
 * HPE1368A_BO_DRIVER
 *
 *
 *
 */
int
hpe1368a_bo_driver(la,val,mask)
register unsigned short la;
register unsigned int   val;
unsigned int            mask;
{
        struct hpe1368a_config	*pc;
        struct vxi_csr		*pcsr;
	unsigned int		work;	
	
        pc = HPE1368A_PCONFIG(la);
        if(pc == NULL){
                return ERROR;
        }

        pcsr = VXIBASE(la);

	FASTLOCK(&pc->lock);

	work = pc->pending;

	/* alter specified bits */
	work = (work & ~mask) | (val & mask);

	pc->pending = work;

	ChannelEnable(pcsr) = work;

	FASTUNLOCK(&pc->lock);

	return OK;
}



/*
 *
 * HPE1368A_BI_DRIVER
 *
 *
 *
 */
hpe1368a_bi_driver(la,mask,pval)
register unsigned short la;
unsigned int            mask;
register unsigned int   *pval;
{
        struct hpe1368a_config	*pc;
	
        pc = HPE1368A_PCONFIG(la);
        if(pc == NULL){
                return ERROR;
        }

	FASTLOCK(&pc->lock);

	*pval = pc->shadow & mask;

	FASTUNLOCK(&pc->lock);

	return OK;
}
