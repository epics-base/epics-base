/*
 *
 * 	HP E1404A VXI bus slot zero translator
 * 	device dependent routines
 *
 *	share/src/drv/$Id$
 *
 * 	Author Jeffrey O. Hill
 * 	Date		030692
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
 * Modification Log:
 * -----------------
 * .01 joh 073092	Added msg device support & interrupt shutdown for
 *			soft reboots
 * .02 joh 082792	converted to ANSI C
 * .03 mgb 080493	Removed V5/V4 and EPICS_V2 conditionals
 *
 *
 *
 */

static char	*sccsId = "$Id$\t$Date$";

#include <vxWorks.h>
#include <iv.h>
#include <intLib.h>
#include <rebootLib.h>

#include <devLib.h>
#include <drvEpvxi.h>
#include <drvHp1404a.h>

LOCAL unsigned long	hpE1404DriverID;

struct hpE1404_config{
	void	(*pSignalCallback)(int16_t signal);
};

#define		TLTRIG(N) (1<<(N))
#define		ECLTRIG(N) (1<<((N)+8))

/*
 * enable int when signal register is written
 */
#define HP1404A_INT_ENABLE 	0x0008
#define HP1404A_INT_DISABLE	0x0000

/*
 *
 * tag the device dependent registers
 */
#define IRQ_enable	dir.w.dd.reg.ddx1a
#define MSG_status	dir.w.dd.reg.ddx1e
#define fp_trig_drive	dir.w.dd.reg.ddx2a	
#define bp_trig_drive	dir.w.dd.reg.ddx22
#define signal_read	dir.r.dd.reg.ddx10

#define hpE1404PConfig(LA, PC) \
	epvxiFetchPConfig((LA), hpE1404DriverID, (PC))

LOCAL void 	hpE1404InitLA(
	unsigned	la
);

LOCAL int	hpE1404ShutDown(
	void
);

LOCAL void 	hpE1404ShutDownLA(
	unsigned 		la
);

LOCAL void      hpE1404IOReport(
unsigned                la,
unsigned                level
);

LOCAL void      hpE1404Int(
unsigned        la
);



/*
 *
 *	hpE1404Init
 *
 */
hpE1404Stat hpE1404Init(void)
{
	hpE1404Stat	status;

	status = rebootHookAdd(hpE1404ShutDown);
	if(status<0){
		status = S_dev_internal;
		errMessage(status, "rebootHookAdd() failed");
		return status;
	}

	hpE1404DriverID = epvxiUniqueDriverID();
	
	status = epvxiRegisterMakeName(
			VXI_MAKE_HP,
			"Hewlett-Packard");
	if(status){
		errMessage(status, NULL);
	}
	status = epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_REG_SLOT0,
			"Slot Zero Translator (reg)");
	if(status){
		errMessage(status, NULL);
	}
	status = epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_REG,
			"Translator (reg)");
	if(status){
		errMessage(status, NULL);
	}
	status = epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_MSG,
			"Translator (msg)");
	if(status){
		errMessage(status, NULL);
	}

	{
		epvxiDeviceSearchPattern  dsp;

		dsp.flags = VXI_DSP_make | VXI_DSP_model;
		dsp.make = VXI_MAKE_HP;
		dsp.model = VXI_HP_MODEL_E1404_REG_SLOT0;
		status = epvxiLookupLA(&dsp, hpE1404InitLA, (void *)NULL);
		if(status){
			errMessage(status, NULL);
			return status;
		}

		dsp.model = VXI_HP_MODEL_E1404_REG;
		status = epvxiLookupLA(&dsp, hpE1404InitLA, (void *)NULL);
		if(status){
			errMessage(status, NULL);
			return status;
		}
	}

	return VXI_SUCCESS;
}


/*
 *
 * hpE1404ShutDown()
 *
 *
 */
LOCAL int	hpE1404ShutDown(void)
{
	hpE1404Stat			status;
	epvxiDeviceSearchPattern  	dsp;

	dsp.flags = VXI_DSP_make | VXI_DSP_model;
	dsp.make = VXI_MAKE_HP;
	dsp.model = VXI_HP_MODEL_E1404_REG_SLOT0;
	status = epvxiLookupLA(&dsp, hpE1404ShutDownLA, (void *)NULL);
	if(status){
		errMessage(status, NULL);
		return ERROR;
	}

	dsp.model = VXI_HP_MODEL_E1404_REG;
	status = epvxiLookupLA(&dsp, hpE1404ShutDownLA, (void *)NULL);
	if(status){
		errMessage(status, NULL);
		return	ERROR;
	}
	return OK;
}


/*
 *
 * hpE1404ShutDownLA()
 *
 *
 */
LOCAL
void 	hpE1404ShutDownLA(
	unsigned 		la
)
{
        struct vxi_csr  	*pcsr;

	pcsr = VXIBASE(la);

	pcsr->IRQ_enable = HP1404A_INT_DISABLE;
}


/*
 *
 * hpE1404InitLA()
 *
 */
LOCAL 
void 	hpE1404InitLA(
	unsigned	la
)
{
	struct hpE1404_config	*pc;
        struct vxi_csr  	*pcsr;
	hpE1404Stat		status;

	status = epvxiOpen(
			la,
			hpE1404DriverID,
			sizeof(*pc),
			hpE1404IOReport);
	if(status){
		errMessage(status, NULL);
		return;
	}

	pcsr = VXIBASE(la);

	status  = hpE1404PConfig(la, pc);
	if(status){
		errMessage(status, NULL);
		epvxiClose(la, hpE1404DriverID);
		return;
	}

	/*
	 * 	set the self test status to passed for 
	 *	the message based device
	 */
	pcsr->MSG_status = VXIPASS<<2;

        intConnect(
                INUM_TO_IVEC(la),
		hpE1404Int,
                la);

	/*
	 * enable int when signal register is written
 	 */
	pcsr->IRQ_enable = HP1404A_INT_ENABLE;

	return;
}


/*
 *
 *	hpE1404SignalConnect()	
 *
 */
hpE1404Stat hpE1404SignalConnect(
unsigned 	la,
void		(*pSignalCallback)(int16_t signal)
)
{
	hpE1404Stat		s;
	struct hpE1404_config	*pc;

	s = hpE1404PConfig(la, pc);
	if(s){
		return s;
	}

	pc->pSignalCallback = pSignalCallback;

	return VXI_SUCCESS;
}


/*
 *
 *	hpE1404Int()	
 *
 */
LOCAL 
void 	hpE1404Int(
	unsigned 	la
)
{
	hpE1404Stat		s;
	struct vxi_csr  	*pcsr;
	unsigned short		signal;
	struct hpE1404_config	*pc;

	s = hpE1404PConfig(la, pc);
	if(s){
		errMessage(s, NULL);
		return;
	}

	/*
	 * vector is only D8 so we cant check the cause of the int
	 * (signal cause is assumed since that was all that was enabled)
	 */

	pcsr = VXIBASE(la);

	signal = pcsr->signal_read;

	if(pc->pSignalCallback){
		(*pc->pSignalCallback)(signal);	
	}
}


/*
 *
 *	hpE1404RouteTriggerECL
 *
 */
hpE1404Stat hpE1404RouteTriggerECL(
unsigned        la,             /* slot zero device logical address     */
unsigned        enable_map,     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
)
{
	struct vxi_csr	*pcsr;

	pcsr = VXIBASE(la);

	pcsr->fp_trig_drive = (io_map&enable_map)<<8;
	pcsr->bp_trig_drive = ((~io_map)&enable_map)<<8;

	return VXI_SUCCESS;
}


/*
 *
 *
 *	hpE1404RouteTriggerTTL
 *
 *
 */
hpE1404Stat hpE1404RouteTriggerTTL(
unsigned        la,             /* slot zero device logical address     */
unsigned        enable_map,     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
)
{
        struct vxi_csr  *pcsr;

        pcsr = VXIBASE(la);

        pcsr->fp_trig_drive = io_map&enable_map;
        pcsr->bp_trig_drive = (~io_map)&enable_map;

	return VXI_SUCCESS;
}


/*
 *
 *	hpE1404IOReport()
 *
 *
 */
LOCAL
void	hpE1404IOReport(
	unsigned 		la,
	unsigned 		level
)
{




}
