/* drvKscV215.c*/
/* share/src/drv @(#) $Id$ */
/*
 *      KscV215_driver.c
 *
 *      driver for KscV215 VXI module
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
 *	.04 012893 joh	include file name change	
 *	.05 080493 mgb	Removed V5/V4 and EPICS_V2 conditionals
 *
 */


#include <vxWorks.h>
#include <dbDefs.h>
#include <iv.h>
#include <types.h>
#include <stdioLib.h>

#include <module_types.h>
#include <task_params.h>
#include <fast_lock.h>
#include <drvEpvxi.h>
#include <drvSup.h>
#include <dbScan.h>
#include <devLib.h>

#include <drvKscV215.h>


#define MAXTRIES 100

#define KSCV215_PCONFIG(LA, PC) \
epvxiFetchPConfig((LA), KscV215DriverId, PC)

#define ChannelEnable(PCSR) ((PCSR)->dir.w.dd.reg.ddx08)
#define ModuleStatus(PCSR) ((PCSR)->dir.r.status)

#define ALL_SWITCHES_OPEN	0

struct KscV215_config{
	FAST_LOCK	lock;		/* mutual exclusion */
        IOSCANPVT ioscanpvt;
};

#define KSCV215_INT_LEVEL	1	
#define KscV215Handshake	(0x0040)
#define KscV215csrInit		(0x9000)

LOCAL int KscV215DriverId;


struct KscV215_A24{
	unsigned short	diag;
	unsigned short	isr;
	unsigned short	pad1[7];
	unsigned short	channels[64];	/* odd access causes a bus error ? */
	unsigned short	controlMemoryAddr;
	unsigned short	pad2;
	unsigned short	controlMemoryDataWrite;
	unsigned short	pad3;
	unsigned short	controlMemoryDataRead;
	unsigned short	pad4;
	unsigned short	lastChannel;
	unsigned short	pad5;
	unsigned short	singleScan;
	unsigned short	pad6;
	unsigned short	stopScan;
	unsigned short	pad7;
	unsigned short	clearControlMemoryAddr;
	unsigned short	pad8;
	unsigned short	enableContinuousScanning;
	unsigned short	pad9;
	unsigned short	disableContinuousScanning;
	unsigned short	pad10;
	unsigned short	enableDoneInt;
	unsigned short	pad11;
	unsigned short	disbaleDoneInt;
	unsigned short	pad12;
	unsigned short	clearDoneInt;
	unsigned short	pad13;
	unsigned short	testDoneInt;
	unsigned short 	pad14;
};

#ifdef INTERRUPTS
LOCAL void KscV215_int_service(unsigned la);
#endif
LOCAL void KscV215_init_card(unsigned la);

LOCAL void KscV215_stat(
unsigned 	la,
int		level
);

LOCAL kscV215Stat KscV215WriteSync(
struct KscV215_A24	*pA24,
unsigned short		*preg,
unsigned 		val
);


struct {
        long    number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvKscV215={
        2,
        NULL,	/* VXI report takes care of this */
        KscV215Init};

/*
 * KscV215_init
 *
 * initialize all KscV215 cards
 *
 */
kscV215Stat	KscV215Init(void)
{
	kscV215Stat	r0;

        /*
         * do nothing on crates without VXI
         */
        if(!epvxiResourceMangerOK){
                return VXI_SUCCESS;
        }

        KscV215DriverId = epvxiUniqueDriverID();

        {
                epvxiDeviceSearchPattern  dsp;

                dsp.flags = VXI_DSP_make | VXI_DSP_model;
                dsp.make = VXI_MAKE_KSC;
                dsp.model = VXI_MODEL_KSCV215;
                r0 = epvxiLookupLA(&dsp, KscV215_init_card, (void *)NULL);
                if(r0){
                        return r0;
                }
        }

        return VXI_SUCCESS;
}



/*
 * KSCV215_INIT_CARD
 *
 * initialize single at5vxi card
 *
 */
LOCAL void KscV215_init_card(unsigned la)
{
        kscV215Stat		status;
	int			i;
        struct KscV215_config	*pc;
	struct KscV215_A24	*pA24;
	struct vxi_csr		*pcsr;
	int			model;

        status = epvxiOpen(
                la,
                KscV215DriverId,
                (unsigned long) sizeof(*pc),
                KscV215_stat);
        if(status){
		errPrintf(
			status,
			__FILE__,
			__LINE__,
			"AT5VXI: device open failed %d\n", 
			la);

                return;
        }

        status = KSCV215_PCONFIG(la, pc);
        if(status){
		errMessage(status,NULL);
		epvxiClose(la, KscV215DriverId);
                return;
        }

	pA24 = epvxiA24Base(la);
	pcsr = VXIBASE(la);

	pcsr->dir.w.control = KscV215csrInit;

	status = KscV215WriteSync(pA24, &pA24->controlMemoryAddr, 0);
	if(status){
		epvxiClose(la, KscV215DriverId);
		errMessage(status, "KscV215 init failed\n");
		return;
	}
	for(i=0; i<(NELEMENTS(pA24->channels)/2); i++){
		status = KscV215WriteSync(
				pA24, 
				&pA24->controlMemoryDataWrite, 
				0);	
		if(status){
			epvxiClose(la, KscV215DriverId);
			errMessage(status, "KscV215 init failed\n");
			return;
		}
	}

	/*
 	 * turn on continuous scan mode
	 */
	status = KscV215WriteSync(
			pA24, 
			&pA24->enableContinuousScanning, 
			0);	
	if(status){
		epvxiClose(la, KscV215DriverId);
		errMessage(status, "KscV215 init failed- device left open\n");
		return;
	}

	FASTLOCKINIT(&pc->lock);
        scanIoInit(&pc->ioscanpvt);

#ifdef INTERRUPTS
        status = intConnect(
		(unsigned char) INUM_TO_IVEC(la),
		KscV215_int_service,
		(void *) la);
	if(status == ERROR){
		epvxiClose(la, KscV215DriverId);
		errMessage(S_dev_vxWorksVecInstlFail, 
			"KscV215 init failed- device left open");
		return;
	}
	sysIntEnable(KSCV215_INT_LEVEL);
#endif

	status = epvxiRegisterMakeName(VXI_MAKE_KSC, "Kinetic Systems");
	if(status){
		errMessage(status, NULL);
	}

        model = VXIMODEL(pcsr);
        status = epvxiRegisterModelName(
                        VXIMAKE(pcsr),
                        model,
                        "V215 16 bit 32 channel ADC\n");
        if(status){
		errMessage(status, NULL);
        }

}


/*
 *
 * KscV215WriteSync
 *
 *
 */
LOCAL kscV215Stat KscV215WriteSync(
struct KscV215_A24	*pA24,
unsigned short		*preg,
unsigned 		val
)
{
        kscV215Stat	status;
	int 		i;

	for(i=0; i<MAXTRIES; i++){
		*preg = val;
		if(pA24->diag & KscV215Handshake){
			return VXI_SUCCESS;
		}
		taskDelay(1);
	}

	status = S_dev_deviceTMO;
	errMessage(status, NULL);
	return status;
}


/*
 *
 * KscV215_int_service()
 *
 *
 * This device interrupts once the 
 * switches have settled
 *
 */
#ifdef INTERRUPTS
LOCAL void KscV215_int_service(unsigned la)
{
	kscV215Stat		s;
        struct KscV215_config	*pc;

        s = KSCV215_PCONFIG(la, pc);
	if(s){
		logMsg(	"Int to ukn device %s line=%d\n", 
			__FILE__, 
			__LINE__, 
			NULL,
			NULL,
			NULL,
			NULL);
		return;
	}

	/*
	 * tell them that the switches have settled
	 */
        scanIoRequest(pc->ioscanpvt);
}
#endif


/*
 * KSCV215_STAT
 *
 * initialize single at5vxi card
 *
 */
LOCAL void KscV215_stat(
unsigned 	la,
int		level
)
{
	kscV215Stat		s;
        struct KscV215_config	*pc;
	struct vxi_csr		*pcsr;
	struct KscV215_A24	*pA24;
	int i;

        s = KSCV215_PCONFIG(la, pc);
        if(s){
		errMessage(s, NULL);
                return;
        }
	pcsr = VXIBASE(la);

	pA24 = (struct KscV215_A24 *) epvxiA24Base(la);

	if(level>0){
		printf ("KSC V215 32 CHANNEL 16 BIT ADC.\n");
	}
	if (level > 1) {
		for (i = 0; i < 32; i++)
			printf ("Channel %d Value %d\n", 
				i,
				pA24->channels[i*2]);
	}
	if (level > 2) {
		printf ("\nGain Setting (Control Memory Data Register\n");
		pA24->controlMemoryAddr = 0;
		for (i = 0; i < 32; i++) {
			switch (pA24->controlMemoryAddr) {
				case 0:
					printf ("+- 10V");
					break;
				case 1:
					printf ("+- 5V");
					break;
				case 3:
					printf ("+- 2.5V");
					break;
				case 5:
					printf ("+- 1.25V");
					break;
				case 6:
					printf ("+- 625mV");
					break;
				default:
					printf ("Unknown Gain Setting.");
				}
			}
		printf ("\n");
	}

		

}



/*
 *
 *
 *	AT5VXI_AI_DRIVER
 *
 *	analog input driver
 */
kscV215Stat KscV215_ai_driver(
unsigned 	la,
unsigned 	chan,
unsigned short 	*prval 
)
{
        struct KscV215_config	*pc;
	struct vxi_csr		*pcsr;
	struct KscV215_A24	*pA24;
	long			tmp;
	int			i;
	kscV215Stat		s;

        s = KSCV215_PCONFIG(la, pc);
        if(s){
                return s;
        }
	pcsr = VXIBASE(la);

	pA24 = epvxiA24Base(la);

	if(chan >= NELEMENTS(pA24->channels)/2){
		return S_dev_badSignalNumber;
	}

	for(i=0; i<MAXTRIES; i++){
		tmp = pA24->channels[chan<<1];
		if(pA24->diag & KscV215Handshake){
			tmp = tmp + 0xffff;
			tmp = tmp >> 4;
			tmp &= 0xfff;
			*prval = tmp;
			return VXI_SUCCESS;
		}
		taskDelay(1);
	}
	
	s = S_dev_deviceTMO;
	return s;
}


/*
 * KscV215_getioscanpvt()
 */
kscV215Stat KscV215_getioscanpvt(
unsigned 	la,
IOSCANPVT 	*scanpvt
)
{
	kscV215Stat             s;
        struct KscV215_config	*pc;

        s = KSCV215_PCONFIG(la, pc);
	if(s){
		errMessage(s, NULL);
		return s;
	}
        *scanpvt = pc->ioscanpvt;
	return	VXI_SUCCESS;
}
