/* devMz8310.c.c */
/* share/src/dev $Id$ */

/* Device Support Routines for MZ8310 */
/*
 *      Original Author: Marty Kraimer and Bob Daly
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
 * .01  10-22-91	mrk	Initial Implementation
 * .02  11-26-91	jba	Initialized clockDiv to 0
 * .03  12-11-91        jba     Moved set of alarm stat and sevr to macros
 * .04  01-14-92        mrk	Added interrupt support
 * .05	03-13-92	jba	ANSI C changes
 * .05  04-10-92        jba     pact now used to test for asyn processing, not return value
 * .05  04-13-92        jba     Removed filename fp from report
 *      ...
 */


/* In order to understand the code in this file you must first	*/
/* understand the MIZAR 8310 Users Manual.			*/
/* You must also understand Chapter 1 of the 			*/
/* the Advanced Micro Devices Am9513 Technical Manual		*/

#include	<vxWorks.h>
#include	<vme.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>
#include	<intLib.h>

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
#include	<pulseCounterRecord.h>
#include	<choicePulseCounter.h>
#include	<pulseDelayRecord.h>
#include	<choicePulseDelay.h>
#include	<pulseTrainRecord.h>
#include	<choicePulseTrain.h>
/* Create the dsets for devMz8310 */
long report();
long init();
long init_pc();
long init_pd();
long init_pt();
long get_ioint_info();
long cmd_pc();
long write_pd();
long write_pt();
long cmd_pc();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;} MZDSET;
/*It doesnt matter who honors report or init thus let 1st devSup do it*/
MZDSET devEventMz8310=	{ 5, report, init,    NULL, get_ioint_info, NULL};
MZDSET devPcMz8310=	{ 5,   NULL, NULL, init_pc,            NULL, cmd_pc};
MZDSET devPdMz8310=	{ 5,   NULL, NULL, init_pd,            NULL, write_pd};
MZDSET devPtMz8310=	{ 5,   NULL, NULL, init_pt,            NULL, write_pt};

/*forward references			*/
void mz8310_int_service(IOSCANPVT);

volatile int mz8310Debug=0;

static unsigned short *shortaddr;
/* definitions related to fields of records*/
/* defs for gtyp and ctyp fields */

#define INTERNAL 0
#define HARDWARE 0
#define SOFTWARE 1
#define EXTERNAL 1
#define INACTIVE 0
#define ACTIVE   1
#define DISABLE  0
#define ENABLE   1
#define RISING_EDGE   0
#define FALLING_EDGE  1
#define DELAY_LOW_EDGE   0 /* valid hts values */
#define DELAY_HIGH_EDGE  1 /* valid hts values */
#define NORMAL_LOW   0
#define NORMAL_HIGH  1

#define DLY_FIELD       0x0001
#define WIDE_FIELD      0x0002
#define STV_FIELD       0x0004
#define GATE_FIELD      0x0008
#define HTS_FIELD	0x0010

/* defs for counter commands */
#define CTR_READ  REC_PULSECOUNTER_CMD_READ
#define CTR_CLEAR REC_PULSECOUNTER_CMD_CLEAR
#define CTR_START REC_PULSECOUNTER_CMD_START
#define CTR_STOP  REC_PULSECOUNTER_CMD_STOP
#define CTR_SETUP REC_PULSECOUNTER_CMD_SETUP

#define SIZE_16 REC_PULSECOUNTER_CSIZ_16
#define SIZE_32 REC_PULSECOUNTER_CSIZ_32

/* defines specific to mz8310*/

static unsigned short BASE;
#define MAXCARDS	4
#define CARDSIZE	0x00000100
#define NUMINTVEC	4
#define CHIPSIZE	0x20
#define CHANONCHIP	5
#define NCHIP		2

static struct {
    unsigned char irq_lev;	/*interrupt request level  */
    unsigned char vec_addr;	/*offset from base register*/
}mz8310_strap_info[NUMINTVEC] = {
    {1,0x41},{3,0x61},{5,0x81},{6,0xA1}};

/* time unit conversion constants - want in seconds
   seconds,milliseconds,microseconds,nanoseconds,picoseconds */
 
static double cons[] = { 1,1e-3,1e-6,1e-9,1,1e-12 };

/*keep information needed for reporting*/
static int ncards=0;
static struct mz8310_info {
    short present;
    FAST_LOCK lock;
    struct {
	short connected;
	short nrec_using;
	IOSCANPVT ioscanpvt;
    } int_info[NUMINTVEC];
    struct {
	short nrec_using;
    } chan_info[NCHIP][CHANONCHIP];
}mz8310_info[MAXCARDS];


#define PCMDREG(CARD,CHIP) \
( (unsigned char *)\
 ((unsigned char*)shortaddr + BASE + (CARD)*CARDSIZE + (CHIP)*CHIPSIZE + 3))
#define PDATAREG(CARD,CHIP) \
( (unsigned short *)\
 ((unsigned char*)shortaddr + BASE + (CARD)*CARDSIZE + (CHIP)*CHIPSIZE))
#define MZ8310INTVEC(CARD,INTVEC) \
( (unsigned char) \
 (MZ8310_INT_VEC_BASE + (CARD)*NUMINTVEC + (INTVEC)))
#define PVECREG(CARD,INTVEC) \
( (unsigned char *)\
 ((unsigned char*)shortaddr + BASE + (CARD)*CARDSIZE + mz8310_strap_info[(INTVEC)].vec_addr))

/*definitions for am9513 stc chip */

/* Internal clock rate and rate divisor */
#define INT_CLOCK_RATE	4e6
#define CLOCK_RATE_DIV	16e0
#define MAX_CLOCK_RATE 7e6

/* desired master mode 					*/
/* Binary Division, Enable Inc, 16-bit bus, FOUT on,	*/
/* FOUT divide by 1, Source F1, TOD Disabled		*/
#define MASTERMODE	0x2100

/* define command codes */
#define LDPCOUNTER	0x00
#define LDPMASTER	0x17
#define LDPSTATUS	0x1f
#define ARM		0x20
#define LOAD		0x40
#define LOADARM		0x60
#define DISARMSAVE	0x80
#define SAVE		0xa0
#define DISARM		0xc0
#define SETTOGOUT	0xe8
#define CLRTOGOUT	0xe0
#define STEP		0xf0
#define DISDPS		0xe8
#define GATEOFFFOUT	0xee
#define BUS16		0xef
#define ENADPS		0xe0
#define GATEONFOUT	0xe6
#define BUS8		0xe7
#define ENAPRE		0xf8
#define DISPRE		0xf9
#define RESET		0xff

/* Count Source Selection */
#define SRC1	0x0100
#define SRC2	0x0200
#define SRC3	0x0300
#define SRC4	0x0400
#define SRC5	0x0500
static unsigned short externalCountSource[] = {SRC1,SRC2,SRC3,SRC4,SRC5};
#define F1	0x0b00
#define F2	0x0c00
#define F3	0x0d00
#define F4	0x0e00
#define F5	0x0f00
static unsigned short internalCountSource[] = {F1,F2,F3,F4,F5};

/*The following are used to communicate with the mz8310.		*/
/*The mz8310 can not keep up when commands are sent as rapidly as possible.*/

putCmd(preg,cmd)
    unsigned char	*preg;
    unsigned char	cmd;
{
    *preg = cmd;
    if(mz8310Debug) printf("mz8310:putCmd:  pcmdreg=%x cmd=%x %d\n",preg,cmd,cmd);
}

putData(preg,data)
    unsigned short	*preg;
    unsigned short	data;
{
    *preg = data;
    if(mz8310Debug) printf("mz8310:putData: preg=%x data=%x %d\n",preg,data,data);
}

getCmd(preg,pcmd)
    unsigned char	*preg;
    unsigned char	*pcmd;
{
    *pcmd = *preg;
    if(mz8310Debug) printf("mz8310:getCmd:  pcmdreg=%x cmd=%x %d\n",preg,*pcmd,*pcmd);
}

getData(preg,pdata)
    unsigned short	*preg;
    unsigned short	*pdata;
{
    *pdata = *preg;
    if(mz8310Debug) printf("mz8310:getData: preg=%x data=%x %d\n",preg,*pdata,*pdata);
}

static void Mz8310_shutdown()
{
    int card,chip;
    volatile unsigned char  *pcmd;

    for(card=0;card<MAXCARDS;card++)
    {
	if(mz8310_info[card].present==TRUE)
	{
	    FASTLOCK(&mz8310_info[card].lock);

	    for(chip=0;chip<NCHIP;chip++)
	    {
		pcmd=PCMDREG(card,chip);
		putCmd(pcmd,RESET);
	    }
	    FASTUNLOCK(&mz8310_info[card].lock);
	}
    }
}


static long init(after)
    int after;
{
    int card,chip,channel,intvec;
    volatile unsigned char  *pcmd;
    volatile unsigned short *pdata;
    unsigned short data;
    int dummy;

    if(after)return(0);
    if(sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO, 0, &shortaddr)) {
	logMsg("devMz8310: sysBusToLocalAdrs failed\n");
	exit(1);
    }
    memset((char *)&mz8310_info[0],0,MAXCARDS*sizeof(struct mz8310_info));
    BASE = tm_addrs[MZ8310];
    for(card=0; card<MAXCARDS; card++) {
	mz8310_info[card].present = FALSE;
	FASTLOCKINIT(&mz8310_info[card].lock);
	FASTLOCK(&mz8310_info[card].lock);
	for(chip=0; chip<NCHIP; chip ++) {
	    pcmd = PCMDREG(card,chip); pdata = PDATAREG(card,chip);
	    if(vxMemProbe(pcmd, READ, sizeof(*pcmd), &dummy) != OK)continue;
	    if(vxMemProbe(pdata, READ, sizeof(*pdata), &dummy) != OK)continue;
	    mz8310_info[card].present = TRUE;
	    if(mz8310Debug) printf("mz8310: Found card %d chip %d\n",card,chip);
	    putCmd(pcmd,BUS16);
	    putCmd(pcmd,LDPMASTER);
	    getData(pdata,&data);
	    if(data != MASTERMODE) { /* need to reinitialize */
	        putCmd(pcmd,RESET);
	        putCmd(pcmd,BUS16);
	        putCmd(pcmd,LDPMASTER);
	        putData(pdata,MASTERMODE);
	        putCmd(pcmd,(LOAD | 0x001F)); /* issue load command for all channels*/
		if(mz8310Debug) {
		    printf("reading back master mode register\n");
		    putCmd(pcmd,LDPMASTER);
		    getData(pdata,&data);
		}
	    }
	}
	FASTUNLOCK(&mz8310_info[card].lock);
	/*Initialize I/O scanning for each interrupt vector		*/
	/*Note that interrupt vectors are not initialized until the	*/
	/*first record is attached via get_ioint_info.			*/
	if(mz8310_info[card].present) for(intvec=0; intvec<NUMINTVEC; intvec++) {
	    scanIoInit(&mz8310_info[card].int_info[intvec].ioscanpvt);
	}
    }

    if(rebootHookAdd(Mz8310_shutdown)<0)
	logMsg("%s: reboot hook add failed\n", __FILE__);

    return(0);
}

static long report(interest)
    int  interest;
{
    int card,chip,channel,intvec;

    for(card=0; card<MAXCARDS; card++) {
	if(!mz8310_info[card].present) continue;
	printf("mz8310: card %d\n",card);
	if(interest==0)continue;
	for(chip=0; chip<NCHIP; chip++) {
	    for(channel=0; channel<CHANONCHIP; channel++) {
		short nrec_using;

		nrec_using = mz8310_info[card].chan_info[chip][channel].nrec_using;
		if(nrec_using==0) continue;
		printf("   chip: %d channel: %d nrec_using: %d\n",
			chip,channel,nrec_using);
	    }
	}
	for(intvec=0; intvec<NUMINTVEC; intvec++) {
	    short nrec_using,connected;

	    nrec_using = mz8310_info[card].int_info[intvec].nrec_using;
	    connected = mz8310_info[card].int_info[intvec].connected;
	    if(nrec_using==0 && !connected) continue;
	    printf("  intvec: %d connected: %d nrec_using: %d  eventRecord\n",
			intvec,connected,nrec_using);
	}
    }
}

static long init_common(pdbCommon,pout,nchan)
    struct dbCommon	*pdbCommon;
    struct link		*pout;
    int			nchan;
{
    struct vmeio *pvmeio = (struct vmeio *)(&pout->value);
    unsigned int card,signal,chip,channel;

    /* out must be an VME_IO */
    switch (pout->type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_dev_badBus,(void *)pdbCommon,
		"devMz8310 (init_record) Illegal OUT Bus Type");
	return(S_dev_badBus);
    }
    card = pvmeio->card;
    signal = pvmeio->signal;
    if(card>=MAXCARDS){
	recGblRecordError(S_dev_badCard,(void *)pdbCommon,
                "devMz8310 (init_record) exceeded maximum supported cards");
	return(S_dev_badCard);
    }
    if(!mz8310_info[card].present){
	recGblRecordError(S_dev_badCard,(void *)pdbCommon,
		"devMz8310 (init_record) VME card not found");
	return(S_dev_badCard);
    }
    if(signal >= NCHIP*CHANONCHIP
    || (nchan==2 && (signal==4 || signal==9))) {
	recGblRecordError(S_dev_badSignal,(void *)pdbCommon,
		"devMz8310 (init_record) Illegal SIGNAL field");
	return(S_dev_badSignal);
    }
    chip = (signal>=CHANONCHIP ? 1 : 0);
    channel = signal - chip*CHANONCHIP;
    if((mz8310_info[card].chan_info[chip][channel].nrec_using +=1)>1) 
	recGblRecordError(S_dev_Conflict,(void *)pdbCommon,
		"devMz8310 (init_record) signal already used");
    if(nchan==2) {
	if((mz8310_info[card].chan_info[chip][channel+1].nrec_using +=1)>1)
	    recGblRecordError(S_dev_Conflict,(void *)pdbCommon,
		"devMz8310 (init_record) signal already used");
    }
    return(0);
 }

static long init_pc(pr)
    struct pulseCounterRecord	*pr;
{
    long status;
    int num_chan;

    if(pr->csiz==SIZE_16)
        num_chan=1;
    else
        num_chan=2;

    status = init_common((struct dbCommon *)pr,&pr->out,num_chan);

    if(status) return(status);

    /*just use dpvt as flag that initialization succeeded*/
    pr->dpvt = (void *)pr;

    return(0);
}

static long init_pd(pr)
    struct pulseDelayRecord	*pr;
{
    long status;
    status = init_common((struct dbCommon *)pr,&pr->out,1);
    if(status) return(status);
    /*just use dpvt as flag that initialization succeeded*/
    pr->dpvt = (void *)pr;
    return(0);
}
static long init_pt(pr)
    struct pulseTrainRecord	*pr;
{
    long status;
    status = init_common((struct dbCommon *)pr,&pr->out,1);
    if(status) return(status);
    /*just use dpvt as flag that initialization succeeded*/
    pr->dpvt = (void *)pr;
    return(0);
}

static void mz8310_int_service(IOSCANPVT ioscanpvt)
{
	scanIoRequest(ioscanpvt);
}

static long get_ioint_info(
	int			cmd,
	struct eventRecord	*pr,
	IOSCANPVT		*ppvt)
{
	struct vmeio *pvmeio = (struct vmeio *)(&pr->inp.value);
	unsigned int	card,intvec;

	*ppvt = NULL;	/*initialize pvt to null*/
	card = pvmeio->card;
	intvec = pvmeio->signal;
	if(card>=MAXCARDS){
	    recGblRecordError(S_dev_badCard,(void *)pr,
		"devMz8310 (get_ioint_info) exceeded maximum supported cards");
	    return(0);
	}
	if(intvec>=NUMINTVEC) {
	    recGblRecordError(S_dev_badSignal,(void *)pr,
		"devMz8310 (get_ioint_info) Illegal SIGNAL field");
	    return(0);
	}
	if(!mz8310_info[card].present){
	    recGblRecordError(S_dev_badCard,(void *)pr,
		"devMz8310 (get_ioint_info) card not found");
	     return(0);
	}
	*ppvt = mz8310_info[card].int_info[intvec].ioscanpvt;
	if(cmd!=0) {
	    if(cmd==1) mz8310_info[card].int_info[intvec].nrec_using -= 1;
	    return(0);
	}
	mz8310_info[card].int_info[intvec].nrec_using +=1;
	if(!mz8310_info[card].int_info[intvec].connected) {
	    unsigned char vector;
	    unsigned char *pvecreg;

	    vector=MZ8310INTVEC(card,intvec);
	    if(intConnect(INUM_TO_IVEC(vector),(FUNCPTR)mz8310_int_service,
	    (int)mz8310_info[card].int_info[intvec].ioscanpvt)!=OK) {
		recGblRecordError(0,(void *)pr,"devMz8310 (get_ioint_info) intConnect failed");
		return(0);
	    }
	    pvecreg = PVECREG(card,intvec);
	    if(vxMemProbe(pvecreg,WRITE,sizeof(char),&vector)!=OK) {
		recGblRecordError(0,(void *)pr,"devMz8310 (get_ioint_info) vxMemProbe failed");
		return(0);
	    }
	    sysIntEnable(mz8310_strap_info[intvec].irq_lev);
	    mz8310_info[card].int_info[intvec].connected = TRUE;
	}
	return(0);
}

static long cmd_pc(pr)
    struct pulseCounterRecord	*pr;
{
    /*volatile*/
     unsigned char  *pcmd;
    /*volatile*/
    unsigned short *pdata;
    struct vmeio 	*pvmeio;
    int card,chip,channel,signal,set_chan_bits;
    unsigned short  load,hold,mode;
	
    if(!pr->dpvt) return(S_dev_NoInit);

    pvmeio = (struct vmeio *)&(pr->out.value);

    card = pvmeio->card;
    signal = pvmeio->signal;

    chip = (signal>=CHANONCHIP ? 1 : 0);
    channel = signal - chip*CHANONCHIP;

    pcmd = PCMDREG(card,chip);
    pdata = PDATAREG(card,chip);

    if(pr->csiz==SIZE_32)
	set_chan_bits=3;
    else /* SIZE_16 */
	set_chan_bits=1;

    FASTLOCK(&mz8310_info[card].lock);

    switch (pr->cmd) {
	case CTR_READ:
	    putCmd(pcmd,(SAVE | (set_chan_bits<<channel)));
	    {
		unsigned short low,high;

		putCmd(pcmd,(LDPCOUNTER | 0x10 | (channel+1)));
		getData(pdata,&low);

                if(pr->csiz==SIZE_32)
		{
		   putCmd(pcmd,(LDPCOUNTER | 0x10 | (channel+2)));
		   getData(pdata,&high);
		}
		else /* SIZE_16 */
		   high=0;

		pr->val = high<<16 | low;
	    }
	    break;

	case CTR_CLEAR:
	    putCmd(pcmd,(DISARM | (set_chan_bits<<channel)));
	    putCmd(pcmd,(LOADARM | (set_chan_bits<<channel)));
	    break;

	case CTR_START:
	    putCmd(pcmd,(ARM | (set_chan_bits<<channel)));
	    break;

	case CTR_STOP:
	    putCmd(pcmd,(DISARM | (set_chan_bits<<channel)));
	    break;

	case CTR_SETUP:
	    /* setup counter i */
	    mode = 0x0028; /*Count Repeatedly, Count Up*/

	    if(pr->cnte==FALLING_EDGE)
		mode |= 0x1000; /*count on falling edge*/

	    /* If necessary set hgv control Active High Level Gate N */
	    if(pr->gtyp==INTERNAL && pr->hgv!=0)
	    {
		unsigned short hgv = (unsigned short)pr->hgv;

		if(hgv>5) recGblRecordError(S_db_badField,(void *)pr,
			"devMz8310 : illegal hgv value");
		else mode |= hgv<<13;
	    }

	    /*set count source selection*/
	    if(pr->cnts<0 || pr->cnts>15)
	    {
                recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
		recGblRecordError(S_db_badField,(void *)pr,
		    "devMz8310 : illegal cnts value");
		pr->pact=TRUE;
		break;
	    }

	    mode |= (pr->cnts << 8);

	    putCmd(pcmd,(DISARM | (set_chan_bits<<channel)));
	    putCmd(pcmd,(LDPCOUNTER | (channel+1)));
	    putData(pdata,mode);
	    putData(pdata,0x0000);

	    /* now setup high order portion of counter*/
            if(pr->csiz==SIZE_32)
	    {
	       putCmd(pcmd,(LDPCOUNTER | (channel+2)));
	       putData(pdata,0x0008);
	       putData(pdata,0x0000);
	    }

	    /* Load counter(s) */
	    putCmd(pcmd,(LOAD | (set_chan_bits<<channel)));
	    pr->udf = FALSE;
	    break;

	default:
            recGblSetSevr(pr,WRITE_ALARM,MAJOR_ALARM);
	    recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : illegal command");
	    break;
    }

    FASTUNLOCK(&mz8310_info[card].lock);
    return(0);
}  

static long write_pd(pr)
    struct pulseDelayRecord	*pr;
{
    /*volatile*/ 
    unsigned char  *pcmd;
    /*volatile*/
    unsigned short *pdata;
    struct vmeio 	*pvmeio;
    int card,chip,channel,signal;
    unsigned short  load,hold,mode;
    double clockRate,holdCount,loadCount;
    int clockDiv=0;
	
    if(!pr->dpvt) return(S_dev_NoInit);

    pvmeio = (struct vmeio *)&(pr->out.value);

    card = pvmeio->card;
    signal = pvmeio->signal;

    chip = (signal>=CHANONCHIP ? 1 : 0);
    channel = signal - chip*CHANONCHIP;

    pcmd = PCMDREG(card,chip);
    pdata = PDATAREG(card,chip);

    if(mz8310Debug)
    {
	printf("pfld=0x%4.4X, odly=%f, dly=%f, gate=%d, stv=%d\n",
	    pr->pfld,pr->odly,pr->dly,pr->gate,pr->stv);
    }

    if(pr->pfld==GATE_FIELD||pr->pfld==STV_FIELD)
    {
	if(pr->odly==pr->dly)
	{
	    if(pr->gate==DISABLE)
	    {
    		putCmd(pcmd,(DISARM | (1<<channel)));
		return(0);
	    }

	    if(pr->stv==ENABLE)
	    {
    		putCmd(pcmd,(ARM | (1<<channel)));
		return(0);
	    }
	}
    }

    /* compute hold count and load count */
    clockRate = (pr->ctyp==INTERNAL ? INT_CLOCK_RATE : pr->ecr);

    if(clockRate<=0 || clockRate>MAX_CLOCK_RATE) 
    {
        recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : computed illegal clock rate");
	pr->pact=TRUE;
	return(0);
    }

    holdCount = clockRate*(cons[pr->unit]*pr->wide);

    if(holdCount<1e0) holdCount = 1e0;

    loadCount = clockRate*(cons[pr->unit]*pr->dly);

    if(pr->ctyp==INTERNAL) 
    {
	clockDiv = 0;
	while(clockDiv<=5 && (loadCount>65536.0 || holdCount>65535.0)) 
	{
	    clockDiv++;
	    clockRate /= CLOCK_RATE_DIV;
    	    holdCount = clockRate*(cons[pr->unit]*pr->wide);

    	    if(holdCount<1e0) holdCount = 1e0;

    	    loadCount = clockRate*(cons[pr->unit]*pr->dly);

    	    if(loadCount<1e0) loadCount = 1e0;
        }
    }

    if(loadCount>65536.0 || holdCount>65535.0) 
    {
        recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : computed illegal clock rate");
	pr->pact=TRUE;
	return(0);
    }

    load = loadCount + .5;
    hold = holdCount + .5;

    /* compute mode */
    if(pr->ttyp==INTERNAL)
    {
    	mode = 0x0062; /* MODE L Waveform */

    	if(pr->hts==DELAY_HIGH_EDGE)
	   mode |=0xc000;	/*gate on high edge*/
    	else /* DELAY_LOW_EDGE */
	   mode |= 0xe000;	/*gate on low edge*/
    }
    else
    {
	mode = 0x0042; /* MODE G Waveform */
    }

    /* for a delay of zero */
    if(pr->dly==0)
    {
	load=1;
    }

    if(pr->cedg==FALLING_EDGE)
	mode |= 0x1000; /*count on falling edge*/

    /*set count source selection*/

    if(pr->ctyp==INTERNAL)
    {
	mode |= internalCountSource[clockDiv];
    }
    else
    {	/*external clock. Determine source*/
	if(pr->ecs<0 || pr->ecs>15)
	{
            recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	    recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : illegal ecs value");
	    pr->pact=TRUE;
	    return(0);
	}
	mode |= (pr->ecs << 8);
    }

    /* setup counter */

    FASTLOCK(&mz8310_info[card].lock);

    putCmd(pcmd,(DISARM | (1<<channel)));

    /* Set initial state of output */

    putCmd(pcmd,((pr->llow==NORMAL_LOW ? CLRTOGOUT : SETTOGOUT) | (channel+1)));
    putCmd(pcmd,(LDPCOUNTER | (channel+1)));
    putData(pdata,mode);
    putData(pdata,load);
    putData(pdata,hold);

    if(mz8310Debug)
    {
	putCmd(pcmd,(LDPCOUNTER | (channel+1)));
	printf("reading mode,load,hold\n");
	getData(pdata,&mode);
	getData(pdata,&load);
	getData(pdata,&hold);
    }

    /* Load, Step, and Arm Counter */
    putCmd(pcmd,(LOAD | (1<<channel)));
    putCmd(pcmd,(STEP | (channel+1)));

    if( (pr->ttyp==INTERNAL||pr->stv==ENABLE) && (pr->gate==ENABLE) )
    	putCmd(pcmd,(ARM | (1<<channel)));

    FASTUNLOCK(&mz8310_info[card].lock);

    pr->udf = FALSE;
    return(0);
}

static long write_pt(pr)
    struct pulseTrainRecord	*pr;
{
    /*volatile*/
    unsigned char  *pcmd;
    /*volatile*/
    unsigned short *pdata;
    struct vmeio 	*pvmeio;
    int card,chip,channel,signal;
    unsigned short  load,hold,mode;
    double clockRate,periodInClockUnits,holdCount,loadCount;
    int clockDiv=0;
	
    if(!pr->dpvt) return(S_dev_NoInit);
    pvmeio = (struct vmeio *)&(pr->out.value);
    card = pvmeio->card;
    signal = pvmeio->signal;
    chip = (signal>=CHANONCHIP ? 1 : 0);
    channel = signal - chip*CHANONCHIP;
    pcmd = PCMDREG(card,chip);
    pdata = PDATAREG(card,chip);

    /* Should we just set on or off */
    if(pr->dcy<=0e0 || (pr->gtyp==SOFTWARE && pr->hgv!=0)) {
	pr->udf = FALSE;
	putCmd(pcmd,(DISARM | (1<<channel)));
	putCmd(pcmd,(LDPCOUNTER | (channel+1)));
	putData(pdata,(pr->llow==NORMAL_LOW ? 0x0000 : 0x0004));
	return(0);
    }
    if(pr->dcy>=100.0) {
	pr->udf = FALSE;
	putCmd(pcmd,(DISARM | (1<<channel)));
	putCmd(pcmd,(LDPCOUNTER | (channel+1)));
	putData(pdata,(pr->llow==NORMAL_LOW ? 0x0004 : 0x0000));
	return(0);
    }

    /* compute hold count and load count */
    clockRate = (pr->ctyp==INTERNAL ? INT_CLOCK_RATE : pr->ecr);
    periodInClockUnits = (cons[pr->unit]*pr->per) * clockRate;
    if(clockRate<=0 || clockRate>MAX_CLOCK_RATE || periodInClockUnits<1) {
        recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : computed illegal clock rate");
	pr->pact=TRUE;
	return(0);
    }
    holdCount = periodInClockUnits*pr->dcy/100.0;
    if(holdCount<1e0) holdCount = 1e0;
    loadCount = periodInClockUnits - holdCount;

    if(pr->ctyp==INTERNAL)
    {
	clockDiv = 0;

	while(clockDiv<5 && (loadCount>65536.0 || holdCount>65535.0)) 
	{
	    clockDiv++;
	    periodInClockUnits /= CLOCK_RATE_DIV;
	    holdCount = periodInClockUnits*pr->dcy/100.0;
	    if(holdCount<1e0) holdCount = 1e0;
	    loadCount = periodInClockUnits - holdCount;
        }
    }

    if(loadCount>65536.0 || holdCount>65535.0) 
    {
        recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : computed illegal clock rate");
	pr->pact=TRUE;
	return(0);
    }

    load = loadCount + .5;
    hold = holdCount + .5;

    /* compute mode */
    mode = 0x0062; /*MODE J: reload load or hold, count repeatedly, TC toggled*/

    if(pr->cedg==FALLING_EDGE)
	mode |= 0x1000; /*count on falling edge*/

    /* If necessary set hgv control MODE K: Active High Level Gate N */
    if(pr->gtyp==INTERNAL && pr->hgv!=0)
    {
	unsigned short hgv = (unsigned short)pr->hgv;

	if(hgv>5)
	   recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : illegal hgv value");
	else
	   mode |= hgv<<13;
    }

    /*set count source selection*/
    if(pr->ctyp==INTERNAL)
    {
	mode |= internalCountSource[clockDiv];
    }
    else
    {   /*external clock. Determine source*/
	if(pr->ecs<0 || pr->ecs>15) {
            recGblSetSevr(pr,WRITE_ALARM,INVALID_ALARM);
	    recGblRecordError(S_db_badField,(void *)pr,
		"devMz8310 : illegal ecs value");
	    pr->pact=TRUE;
	    return(0);
	}
	mode |= (pr->ecs << 8);
    }

    /* setup counter */
    FASTLOCK(&mz8310_info[card].lock);
    putCmd(pcmd,(DISARM | (1<<channel)));
    /* Set initial state of output */
    putCmd(pcmd,((pr->llow==NORMAL_LOW ? CLRTOGOUT : SETTOGOUT) | (channel+1)));
    putCmd(pcmd,(LDPCOUNTER | (channel+1)));
    putData(pdata,mode);
    putData(pdata,load);
    putData(pdata,hold);
    if(mz8310Debug){
	putCmd(pcmd,(LDPCOUNTER | (channel+1)));
	printf("reading mode,load,hold\n");
	getData(pdata,&mode);
	getData(pdata,&load);
	getData(pdata,&hold);
    }
    /* Load and arm counter*/
    putCmd(pcmd,(LOADARM | (1<<channel)));
    FASTUNLOCK(&mz8310_info[card].lock);
    pr->udf = FALSE;
    return(0);
}
