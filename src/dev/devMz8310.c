/* devPtMZ8310.c.c */
/* share/src/dev @(#)devPtMZ8310.c	1.6     6/7/91 */

/* Device Support Routines for MZ8310 Pulse Generator output*/
/*
 *      Original Author: Bob Daly
 *      Date:            6-19-91
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
 * .01  mm-dd-yy        iii     Comment
 * .02  mm-dd-yy        iii     Comment
 *      ...
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<dbCommon.h>
#include	<devSup.h>
#include	<link.h>
#include	<special.h>
#include	<module_types.h>
#include	<pulseTrainRecord.h>

/* defines specific to mz8310 used as pulse generator */
/* NOTE: Pulse Generator refers to use of channels 0 thru 4 */
/*       These channels are referred to as 1o1,1o2,1o3,1o4, */
/*       and 1o5 on the I/O connector and they occupy the   */
/*       lower address space in the module's short addr space */
#define SHORTADDR		0xff0000
#define MZ8310IOAddr		0xfa00
#define MZ8310CHIPSIZE		0x20
#define MZ8310SIZE		0x00000100
#define MZ8310BASE		(SHORTADDR+MZ8310IOAddr)

#define MZ8310DATA		0
#define MZ8310CMD		3
#define MZ8310CHANONCHIP	5
#define MZ8310CHIPCOUNT		2
#define MZ8310CHANCNT		(MZ8310CHANONCHIP*MZ8310CHIPCOUNT)

#define MZ8310_CMD_ADDR		((unsigned char *)  MZ8310BASE + MZ8310CMD)
#define MZ8310_DATA_ADDR 	((unsigned short *) MZ8310BASE + MZ8310DATA)
#define MZ8310VECBASE		((unsigned char *)  MZ8310BASE + 0x41)
#define MZ8310VECSIZE		(0x20)
#define MZ8310INTCNT		4
#define TCON 0
#define TCOFF 1
#define TCPULSE 2
#define F1	0x0b00
#define F2	0x0c00
#define F3	0x0d00
#define F4	0x0e00
#define F5	0x0f00


/* The following must match the definition in choiceGbl.ascii */
#define LINEAR 1

/* local device variables */
static struct	mzCard_S{
  char				init;
  FAST_LOCK			lock;
}mzCard;

/* Create the dset for devPtMZ8310 */
long init_record();
long write_pt();
long mz8310PtReport();
short mz8310CardInit();
short stc_init();
short convert_pulseTrain();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_pt;
}devPtMZ8310={
	6,
	mz8310PtReport,
	NULL,
	init_record,
	NULL,
	write_pt};



static long mz8310PtReport(ppt)
    struct pulseTrainRecord	*ppt;
{
register unsigned char 	*pcmd = MZ8310_CMD_ADDR;
register unsigned short *pdata =MZ8310_DATA_ADDR;
short	mode,load,hold,channel,dummy;

  if( (vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK) ||
   (vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK) )
   {
    logMsg("\n***\nMZ8310 Pulse Generator Card not found at %x address\n***\n",*pcmd);
    return ERROR;
   }
   logMsg("\n***");
    logMsg("\nMZ8310 Pulse Generator Card found at %x address\n",*pcmd);
/* read the mode load and hold values from chip registers */
  for(channel=0; channel<MZ8310CHANONCHIP; channel++)
	{
	*pcmd = channel+1;  /* xfer regs to mode load hold vars for channel */
	mode = *pdata;
	load = *pdata;
	hold = *pdata;
	logMsg("CHANNEL:%d....\tMODE: %x\tLOAD: %x\tHOLD: %x\n",
	channel,mode,load,hold);
        }
   logMsg("***\n");
   return(OK);
}

static long init_record(ppt)
    struct pulseTrainRecord	*ppt;
{
    struct vmeio *pvmeio = (struct vmeio *)(&ppt->out.value);
    struct dbCommon *pdbCommon = (struct dbCommon *)ppt;
    struct ptDpvt *dpvt;

    char message[100];
    static firstTime = TRUE;
    static aOK = TRUE;

    /* pt.out must be an VME_IO */
    switch (ppt->out.type) {
    case (VME_IO) :
	break;
    default :
	strcpy(message,pdbCommon->name);
	strcat(message,": devPtMZ8310 (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }

   /* only one card supported i.e card 0 */
    if(pvmeio->card != 0)
	{
	strcpy(message,pdbCommon->name);
	strcat(message,": devPtMZ8310 (init_record) Illegal CARD field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
        }

   /* only 5 channels on card supported */ 
    if(pvmeio->signal >= 5)
	{
	strcpy(message,pdbCommon->name);
	strcat(message,": devPtMZ8310 (init_record) Illegal SIGNAL field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
        }
   /* make sure card was initialized OK */
    if(!aOK)
        {
	strcpy(message,pdbCommon->name);
	strcat(message,": devPtMZ8310 (init_record) CARD not OK");
	errMessage(S_db_badField,message);
	return(S_db_badField);
        }

   /* do first time things */
    if(firstTime)
    {
	firstTime = FALSE;
	FASTLOCKINIT(&mzCard.lock);
        if(mz8310CardInit(pvmeio->card) == ERROR)
	{
        aOK = FALSE;
	strcpy(message,pdbCommon->name);
	strcat(message,": devPtMZ8310 (init_record) CARD INIT error(missing?)");
	errMessage(S_db_badField,message);
	return(S_db_badField);
        }
     }

 }


static short mz8310CardInit(card)
	register int 	card;		/* 0 through ... 	*/
{
  /*
  BCD division
  data ptr seq enbl
  16 bit bus
  FOUT on 
  FOUT divide by one
  FOUT source (F1)
  Time of day disabled
  */
  register unsigned short master_mode = 0xa100;
  return(stc_init(MZ8310_CMD_ADDR, MZ8310_DATA_ADDR, master_mode)) ;
}


static short stc_init(pcmd, pdata, master_mode)
register unsigned char 	*pcmd;
register unsigned short *pdata;
unsigned short		master_mode;
{
  int	dummy;
  int 	channel;

  if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK)
    return ERROR;
  if(vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK)
    return ERROR;
 
  *pcmd = 0xef;  /* 16 BIT MODE */
  *pcmd = 0x17; /* next access will be to master mode register */
  if(master_mode != *pdata) /* reset master mode if not what is wanted */
    {
    *pcmd = 0xff;   /* RESET */
    *pcmd = 0xef;   /* return to 16 BIT MODE  after reset*/
    *pcmd = 0x17;   /* next data write is to master mode register */
    *pdata = master_mode; /* set up master mode register */
    for(channel=0; channel<MZ8310CHANONCHIP; channel++)
      *pcmd = 0x40 | 1<< channel; /* LOAD must be done after reset */
    }
  /* set the initial output states of TC to LOW */
  
    for(channel=0; channel<MZ8310CHANONCHIP; channel++)
	{
	*pcmd = channel+1;  /* xfer to mode load hold registers for channel */
	*pdata = 0x0b62;
	*pdata = 0;
	*pdata = 0;
	*pcmd = 0xe0 | channel+1; /* clear the TC toggle output*/
	}

  return OK;
}



static long write_pt(ppt)
    struct pulseTrainRecord	*ppt;
{
	struct vmeio 	*pvmeio;
	int	    	status;
	unsigned short  load,hold,mode;
	short  tcMode;		/*TCON = 0,TCOFF =1,TCPULSE =2*/
	register unsigned char 	*pcmd = MZ8310_CMD_ADDR;
	register unsigned short *pdata = MZ8310_DATA_ADDR;
	short channel;
	
	pvmeio = (struct vmeio *)&(ppt->out.value);
	if (ppt->out.type != VME_IO) {
		if(ppt->nsev<VALID_ALARM) {
			ppt->nsta = WRITE_ALARM;
			ppt->nsev = VALID_ALARM;
		}
		return(OK);
	}

	if(convert_pulseTrain(ppt,&load,&hold,&mode,&tcMode)==ERROR){
		if(ppt->nsev<VALID_ALARM) {
			ppt->nsta = WRITE_ALARM;
			ppt->nsev = VALID_ALARM;
		}
		return(OK);
	}
	ppt->udf=FALSE;
	channel = pvmeio->signal;
/* disarm the channel */
	*pcmd = 0xc0 | channel+1;
/* clear TC so starts out with same level each change */
	   *pcmd = 0xe0 | channel+1;
/* check if duty cycle = 0 ... if so clear tc i.e do nothing */
	if(tcMode == TCOFF){
	    return(OK);
	}
/* if GATE OFF ...then clear tc i.e. do nothing*/
	if(ppt->gate != 0){
	    return(OK);
	}
/* if duty cycle = 100%...then set tc*/
	if(tcMode == TCON){
	   *pcmd = 0xe8 | channel+1;
	    return(OK);
	}
/*got here so must be variable duty cycle with gate on*/
/*xfer the mode,hold, and load values to registers & arm*/
logMsg("write_pt...gate on...var duty cycle & freq");
	*pcmd = channel+1;
	*pdata = mode;
	*pdata = load;
	*pdata = hold;
	*pcmd = 0x20 | 1<< channel;
        return(OK);
}  


static short convert_pulseTrain(ppt,pload,phold,pmode,ptcMode)
struct pulseTrainRecord	*ppt;
unsigned short 	*phold,*pload,*pmode,*ptcMode;
{
	int	totalPulses;
	int	timeScale[] = {1,1000,1000000};/*micro,milli,sec in micro*/
	int	periodInMicroSec;

	if(ppt->dcy == 0)
	{
	*ptcMode = TCOFF;
	return(OK);
	}

	if(ppt->dcy == 100)
	{
	*ptcMode = TCON;
	return(OK);
	}
	/* check period to set Fx  */
	periodInMicroSec = ppt->per*timeScale[ppt->timu];
	if(periodInMicroSec <= 10) return(ERROR);        /* must be > 10 usec */
	if(periodInMicroSec > 100000000) return(ERROR);  /* must be < 100 sec */
	*ptcMode = TCPULSE;

	if(periodInMicroSec < 10000) { /*10 millisec*/
	   *pmode = 0x0062 | F1;       /* 4 MHz Count Source */
	    totalPulses = (periodInMicroSec*4); 
	   *phold = ((totalPulses*ppt->dcy)/100)+1; /*+1..-1 (on TC) = 0*/
 	   *pload = totalPulses-*phold+2;
 	   return(OK);
	}
	if(periodInMicroSec < 100000) { /*100 millisec*/
	   *pmode = 0x0062 | F2;        /* .4 MHz Count Source */
	    totalPulses = (periodInMicroSec/10)*4;
	   *phold = ((totalPulses*ppt->dcy)/100)+1; /*+1..-1 (on TC) = 0*/
 	   *pload = totalPulses-*phold+2;
	   return(OK);
	}
	   
	if(periodInMicroSec < 1000000) {/* 1 sec */
	   *pmode = 0x0062 | F3;        /*.04Mhz Count Source */     
	    totalPulses = (periodInMicroSec/100)*4;
	   *phold = ((totalPulses*ppt->dcy)/100)+1; /*+1..-1 (on TC) = 0*/
 	   *pload = totalPulses-*phold+2;
	   return(OK);
	}
	   
	if(periodInMicroSec < 10000000) {/* 10 sec */
	   *pmode = 0x0062 | F4;         /* .004MHz Count Source */
	    totalPulses = (periodInMicroSec/1000)*4;
	   *phold = ((totalPulses*ppt->dcy)/100)+1; /*+1..-1 (on TC) = 0*/
 	   *pload = totalPulses-*phold+2;
	   return(OK);
	}
	   
	if(periodInMicroSec < 100000000) {/* 100 sec */
	   *pmode = 0x0062 | F5;
	    totalPulses = (periodInMicroSec/10000)*4;
	   *phold = ((totalPulses*ppt->dcy)/100)+1; /*+1..-1 (on TC) = 0*/
 	   *pload = totalPulses-*phold+2;
	   return(OK);
	}
}
	   
	  
