/*
 *
 * HP E1404A VXI bus slot zero translator
 * device dependent routines
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
 *
 *
 *
 */

static char	*sccsId = "$Id$\t$Date$";

#include <vxWorks.h>
#ifdef V5_vxWorks
#	include <iv.h>
#else
#	include <iv68k.h>
#endif
#include <epvxiLib.h>

#define		TLTRIG(N) (1<<(N))
#define		ECLTRIG(N) (1<<((N)+8))

#define LOCAL static

#define NULL 0
#define TRUE 1
#define FALSE 0

#define VXI_HP_MODEL_E1404_REG_SLOT0    0x10
#define VXI_HP_MODEL_E1404_REG          0x110
#define VXI_HP_MODEL_E1404_MSG          0x111

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

#define hpE1404PConfig(LA) \
	epvxiPConfig((LA), hpE1404DriverID, struct hpE1404_config *)

void	hpE1404Int();
void	hpE1404InitLA();
void	hpE1404ShutDown();
void	hpE1404ShutDownLA();
int	hpE1404IOReport();

struct hpE1404_config{
	void	(*pSignalCallback)();
};

LOCAL unsigned long	hpE1404DriverID;


/*
 *
 *	hpE1404Init
 *
 */
int
hpE1404Init()
{
	int		status;


	status = rebootHookAdd(hpE1404ShutDown);
	if(status<0){
		return ERROR;
	}

	hpE1404DriverID = epvxiUniqueDriverID();
	
	epvxiRegisterMakeName(
			VXI_MAKE_HP,
			"Hewlett-Packard");
	epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_REG_SLOT0,
			"Slot Zero Translator (reg)");
	epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_REG,
			"Translator (reg)");
	epvxiRegisterModelName(
			VXI_MAKE_HP,
			VXI_HP_MODEL_E1404_MSG,
			"Translator (msg)");

	{
		epvxiDeviceSearchPattern  dsp;

		dsp.flags = VXI_DSP_make | VXI_DSP_model;
		dsp.make = VXI_MAKE_HP;
		dsp.model = VXI_HP_MODEL_E1404_REG_SLOT0;
		status = epvxiLookupLA(&dsp, hpE1404InitLA, (void *)NULL);
		if(status<0){
			return ERROR;
		}

		dsp.model = VXI_HP_MODEL_E1404_REG;
		status = epvxiLookupLA(&dsp, hpE1404InitLA, (void *)NULL);
		if(status<0){
			return ERROR;
		}
	}

	return OK;
}


/*
 *
 * hpE1404ShutDown()
 *
 *
 */
LOCAL
void hpE1404ShutDown()
{
	int				status;
	epvxiDeviceSearchPattern  	dsp;

	dsp.flags = VXI_DSP_make | VXI_DSP_model;
	dsp.make = VXI_MAKE_HP;
	dsp.model = VXI_HP_MODEL_E1404_REG_SLOT0;
	status = epvxiLookupLA(&dsp, hpE1404ShutDownLA, (void *)NULL);
	if(status<0){
		return;
	}

	dsp.model = VXI_HP_MODEL_E1404_REG;
	status = epvxiLookupLA(&dsp, hpE1404ShutDownLA, (void *)NULL);
	if(status<0){
		return;
	}

}


/*
 *
 * hpE1404ShutDownLA()
 *
 *
 */
void
hpE1404ShutDownLA(la)
unsigned la;
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
LOCAL void
hpE1404InitLA(la)
{
	struct hpE1404_config	*pc;
        struct vxi_csr  	*pcsr;
	int			status;

	status = epvxiOpen(
			la,
			hpE1404DriverID,
			sizeof(*pc),
			hpE1404IOReport);
	if(status<0){
		logMsg( "%s: device open failed (stat=%d) (LA=0X%02X)\n",
			__FILE__,
			status,
			la);
		return;
	}

	pcsr = VXIBASE(la);

	pc = hpE1404PConfig(la);
	if(!pc){
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
                (void *) la);

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
int
hpE1404SignalConnect(la, pSignalCallback)
unsigned 	la;
void		(*pSignalCallback)();
{
	struct hpE1404_config	*pc;

	pc = hpE1404PConfig(la);
	if(!pc){
		return ERROR;
	}

	pc->pSignalCallback = pSignalCallback;

	return OK;
}


/*
 *
 *	hpE1404Int()	
 *
 */
LOCAL void 
hpE1404Int(la)
unsigned la;
{
	struct vxi_csr  	*pcsr;
	unsigned short		signal;
	struct hpE1404_config	*pc;

	pc = hpE1404PConfig(la);
	if(!pc){
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
hpE1404RouteTriggerECL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned        enable_map;     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map;         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
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
hpE1404RouteTriggerTTL(la, enable_map, io_map)
unsigned        la;             /* slot zero device logical address     */
unsigned        enable_map;     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map;         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
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
int hpE1404IOReport(la, level)
unsigned la;
unsigned level;
{




}
