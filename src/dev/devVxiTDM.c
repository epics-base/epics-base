/* devVxiTDM.c */
/* share/src/dev $Id$ */

/* Device Support Routines for Custom Vxi Time Delay Module */
/*
 *      Author: Jim Kowalkowski
 *      Date:            8/20/92
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
 * .01  08-20-92	jbk	Initial Implementation
 * .02	02-16-92	joh	vxi incl name change & cpu 
 *				independent int vector include
 *      ...
 */


/* Since this is a custom module, there is no user manual.  The
   information supplied here summorizing the usage.

   Simplified diagram of board:

   input 1 ---->|
                |              (delay module section)
   input 2 ---->|
                |---->4:1Mux---->8-bit digital delay line--->gate--+--->
   input 3 ---->|                0-1.275us in 5ns steps            |
                |                                                  V
   input 4 ---->|                                                 last
                                                                 value

   Ten delay module sections exist for the four inputs.  Each one has an
   associated vxi register.  So altogether there are ten of the
   following registers.

       _________________________________________________________________
  bits |15 |14 |13 |12 |11 |10 | 9 | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
       |---------------+---+---+-------+-------------------------------|
 write |   unused      |   |TE |  TS   |     delay setting in ns       |
       |---------------+---+---+-------+-------------------------------|
 read  |   unused      |TD |TE |  TS   |     delay setting in ns       |
       +---------------+---+---+-------+-------------------------------+

       where TE = trigger enable/disable
	     TS = trigger select (input 1-4)
	     TD = trigger detect (trigger detected/not detected)

   */

#include	<vxWorks.h>
#include	<vme.h>
#include	<types.h>
#include	<ctype.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<iv.h>
#include	<drvEpvxi.h>

#include	<alarm.h>
#include	<dbRecType.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<fast_lock.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<dbScan.h>
#include	<special.h>
#include	<module_types.h>
#include	<eventRecord.h>
#include	<pulseDelayRecord.h>

/* Create the dsets for devVxiTDM */

long report();
long init();
long init_pd();
long get_ioint_info();
long write_pd();

typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;
} TDM_DSET;

/*It doesnt matter who honors report or init thus let 1st devSup do it*/

TDM_DSET devPdVxiTDM = { 5, report, init, init_pd, NULL, write_pd };

volatile int VxiTDMDebug=0;

#define Debug(f,v) { if(VxiTDMDebug) printf(f,v); }

/* definitions related to fields of records*/
/* defs for gtyp and ctyp fields */

#define INTERNAL 0
#define EXTERNAL 1
#define SOFTWARE 1
#define ACTIVE 1
#define INACTIVE 0

/* Trigger Source Selection - all macros require short for reg */
#define TRGS_CLEAR(reg)		(reg&0xfcff)
#define TRGS_SET(trgs,reg)	(TRGS_CLEAR(reg)|trgs<<8)
#define TRGS_VALUE(reg)		((reg&0x0300)>>8)

#define TRGS1			0x0000
#define TRGS2			0x0100
#define TRGS3			0x0200
#define TRGS4			0x0300

/* vxi register offsets */
#define ID_REG_OFFSET		0x0000
#define DEV_TYPE_REG_OFFSET	0x0002
#define STAT_CTRL_REG_OFFSET	0x0004
#define OFFSET_REG_OFFSET	0x0006

#define TRIG_ENABLE(reg)	(reg|0x0400)
#define TRIG_DISABLE(reg)	(reg&(~0x400))
#define TRIG_VALUE(reg)		(reg&0x0400)

#define DELAY_CLEAR(reg)	(reg&0xff00)
#define DELAY_SET(delay,reg)	(DELAY_CLEAR(reg)|delay)
#define DELAY_VALUE(reg)	(reg&0x00ff)

#define TRIG_DETECT(reg)	(0x0800&reg)

#define VXI_BASE		0xc000
#define CARD_SIZE		0x0040
#define REG_OFFSET		0x0020
#define MAXCARDS		31
#define MAXSIG			9
#define MAX_NANOSEC		1275e1
#define MAX_DELAY		255e1
#define TIME_FACT		MAX_DELAY/MAX_NANOSEC
#define TIME_FACT_TO		MAX_NANOSEC/MAX_DELAY

#define CARD(la)		(char *)shortaddr+CARD_SIZE*la
#define SIGNAL(la,signal)	CARD(la)+REG_OFFSET+signal*2

#define VXI_MAKE_TDM		0xf00
#define VXI_MODEL_TDM		0xf00

static unsigned long tdmDriverID;

/* for stat-> bit per channel 1=used/valid, 0=unused/invalid, bit-0=channel-0 */

struct tdm_config {
	FAST_LOCK	lock;
	unsigned short	stat;
	};

/* time unit conversion constants - want in nanoseconds 
   seconds,milliseconds,microseconds,nanoseconds,picoseconds */

static double cons[] = { 
	1e9,1e6,1e3,1,1e-3 };

void tdmInitLA();
void tdm_stat();
void tdm_report();



static long report(int interest)
{
unsigned short	la,signal;
struct	vxi_csr	*pcsr;
struct	tdm_config *tc;
epvxiDeviceSearchPattern	dsp;

	printf("Report for Vxi Time Delay Module\n");

	dsp.flags=VXI_DSP_make|VXI_DSP_model;
	dsp.make=VXI_MAKE_TDM;
	dsp.model=VXI_MODEL_TDM;

	if( epvxiLookupLA(&dsp,tdm_report,(void *)&interest)<0 )
		return ERROR;

}

static void tdm_report(unsigned la,void *interest)
{
	tdm_stat(la,(int)*(int *)interest);
}

static long init(int after)
{
epvxiDeviceSearchPattern	dsp;

	if(after) return(0);

	tdmDriverID=epvxiUniqueDriverID();

	dsp.flags=VXI_DSP_make|VXI_DSP_model;
	dsp.make=VXI_MAKE_TDM;
	dsp.model=VXI_MODEL_TDM;

	if( epvxiLookupLA(&dsp,tdmInitLA,(void *)NULL) <0)
		return ERROR;

	if(epvxiRegisterMakeName(VXI_MAKE_TDM,"ANL")<0)
		logMsg("%s: unable to register MAKE\n",__FILE__);

	if(epvxiRegisterModelName(VXI_MAKE_TDM,VXI_MODEL_TDM,
	   "Time Delay Module")<0)
		logMsg("%s: unable to register MODEL\n",__FILE__);

	return OK;

}

static void tdmInitLA(unsigned la)
{
int	status;
struct	vxi_csr	*pcsr;
struct	tdm_config	*tc;

	status=epvxiOpen(la,tdmDriverID,sizeof(struct tdm_config),tdm_stat);

	if(status<0)
	{
		logMsg("%s:Device Open Failed (stat=%d)(LA=0x%02X)\n",
			__FILE__,status,la);
		return;
	}

	pcsr=VXIBASE(la);
	tc=epvxiPConfig(la,tdmDriverID,struct tdm_config *);

	if(!tc)
	{
		epvxiClose(la,tdmDriverID);
		return;
	}

	FASTLOCKINIT(&tc->lock);

	tc->stat=0;

	return;
}


static long init_pd(struct pulseDelayRecord *pd)
{
struct vxiio *pvxiio = (struct vxiio *)&(pd->out.value);
int	dummy;
short	la,signal;
unsigned short	*channel_reg;
struct vxi_csr	*pcsr;
struct tdm_config *tc;

	/* out must be an VXI_IO */

	switch (pd->out.type)
	{
	case (VXI_IO):
		break;

	default :
		recGblRecordError(S_dev_badBus,pd,
		    "devVxiTDM (init_record) Illegal OUT Bus Type");
		return(S_dev_badBus);
	}

	/* initialize to null indicating no address found */

	la = pvxiio->la;

	if(isdigit(pvxiio->parm[0]))
		signal=pvxiio->parm[0]-'0';
	else
	{
		recGblRecordError(S_dev_badSignal,pd,
			"devVxiTDM (init_record) Illegal SIGNAL field");
		return(S_dev_badSignal);
	}

	Debug("frame = %d \n",pvxiio->frame);
	Debug("slot = %d \n",pvxiio->slot);
	Debug("la = %d \n",pvxiio->la);
	Debug("flag = %d \n",pvxiio->flag);
	Debug("signal = %d \n",signal);
	Debug("parm = (%s) \n",pvxiio->parm);

	if(la>MAXCARDS)
	{
		recGblRecordError(S_dev_badCard,pd,
		    "devVxiTDM (init_record) exceeded maximum supported cards");
		return(S_dev_badCard);
	}

	if(signal>MAXSIG)
	{
		recGblRecordError(S_dev_badSignal,pd,
			"devVxiTDM (init_record) Illegal SIGNAL field");
		return(S_dev_badSignal);
	}

	/* get the address of the register for this la and signal */

	tc=epvxiPConfig(la,tdmDriverID,struct tdm_config *);
	pcsr=VXIBASE(la);

	(char *)channel_reg=(char *)pcsr+REG_OFFSET;

	Debug("channel_reg = %08.8X \n",channel_reg);

	if( tc->stat&(1<<signal) )
	{
		recGblRecordError(S_dev_badSignal,pd,
		   "devVxiTDM (init_record) signal/la already in use");
		return(S_dev_badSignal);
	}

	FASTLOCK(&tc->lock);

	/* check if register readable */
	if(vxMemProbe(channel_reg[signal],READ,
	   sizeof(unsigned short),&dummy)!=OK)
	{
		recGblRecordError(S_dev_badSignal,pd,
		   "devVxiTDM (init_record) vxMemProbe failed for signal/la");
		return(S_dev_badSignal);
	}

	/* enable/disable gate */
	switch(pd->gate)
	{
	default:
	case ACTIVE:
		channel_reg[signal]=TRIG_ENABLE(channel_reg[signal]);
		break;
	case INACTIVE:
		channel_reg[signal]=TRIG_DISABLE(channel_reg[signal]);
		break;
	}

	FASTUNLOCK(&tc->lock);

	tc->stat|=(1<<signal);

	/* 
	This field is used to indicate the address of the field
	which caused processing of the record which is none now.
	*/

	pd->dpvt=(void *)pd;

	return(0);
}


static long write_pd(struct pulseDelayRecord *pr)
{
unsigned short	*channel_reg;
unsigned short	la,signal;
unsigned short	delay_time,channel_value,channel_read;
struct vxiio	*pvxiio;
struct vxi_csr	*pcsr;
struct tdm_config *tc;

	Debug("Entering write_pd for pulseDelay\n",0);

	/* will be null if init failed */
	if(!pr->dpvt) return(S_dev_NoInit);

	pvxiio = (struct vxiio *)&(pr->out.value);

	la = pvxiio->la;

	if(isdigit(pvxiio->parm[0]))
		signal=pvxiio->parm[0]-'0';
	else
	{
		recGblRecordError(S_dev_badSignal,pr,
			"devVxiTDM (init_record) Illegal SIGNAL field");
		return(S_dev_badSignal);
	}

	/* get address of register for this card and signal */

	tc=epvxiPConfig(la,tdmDriverID,struct tdm_config *);
	pcsr=VXIBASE(la);
	(char *)channel_reg=(char *)pcsr+REG_OFFSET;

	Debug("Channel base address = 0x%08.8X \n",channel_reg);
	Debug("Channel register address = 0x%08.8X \n",&channel_reg[signal]);

	FASTLOCK(&tc->lock);

	/* set val to active if trigger detect on and a periodic scan
	caused processing, special processing will set pfld */

	if(!pr->pfld)
	{
		channel_read=channel_reg[signal];

		if(TRIG_DETECT(channel_read))
			pr->val=ACTIVE;
		else
			pr->val=INACTIVE;

		pr->dly=(double)DELAY_VALUE(channel_read)*TIME_FACT_TO;
		pr->hts=TRGS_VALUE(channel_read);
		pr->gate=TRIG_VALUE(channel_read);

		Debug("trigger detect read - value = %d\n",pr->val);
	}
	else
	{
		/* initialize new channel value */
		channel_value=0x0000;

		/* compute delay time */

		delay_time=(unsigned short)(cons[pr->unit]*pr->dly*TIME_FACT);

		Debug("delay time = %d\n",delay_time);
		Debug("units = %d \n",pr->unit);

		if(delay_time > 255) delay_time=255;

		channel_value=DELAY_SET(delay_time,channel_value);

		/* enable/disable gate */
		switch(pr->gate)
		{
		default:
		case ACTIVE:
			channel_value=TRIG_ENABLE(channel_value);
			break;
		case INACTIVE:
			channel_value=TRIG_DISABLE(channel_value);
			break;
		}

		/* trigger selection set */
		if(pr->hts > 3 || pr->hts < 0)
		{
			recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
			recGblRecordError(S_db_badField,pr,"invalid trigger value");
			return(0);
		}

		channel_value=TRGS_SET(pr->hts,channel_value);

		/* write to register */
		channel_reg[signal]=channel_value;
	}

	FASTUNLOCK(&tc->lock);

	Debug("Ending channel value = %04.4X\n",channel_value);

	return(0);

}

static void tdm_stat(unsigned la,int level)
{
struct tdm_config *tc;
struct vxi_csr	 *pcsr;
int	i;

	tc=epvxiPConfig(la,tdmDriverID,struct tdm_config *);
	pcsr=VXIBASE(la);

	Debug("Report level = %d\n",level);

	printf("Report for Vxi Time Delay module %d:\n",la);
	printf("  Signals in use:");

	for(i=0;i<=MAXSIG;i++) if(tc->stat&(1<<i)) printf("%d ",i);

	printf("\n");
}
