/* devXxBugRac.c  */

/* Device Support Routines for BUF RAC commands */
/*
 *      Original Author: Ned Arnold (based on work by Jim Kowalkowski)
 *      Date: 02/01/93
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
 * .01  02-01093        nda     initialized
 * .02  06-03-94        nda     removed automatic retry on a TIMEOUT
 * .03  06-03-94        nda     removed drvBitBus.qReq at init time 
 *      ...
 */



#include	<vxWorks.h>
#include        <stdlib.h>
#include        <stdio.h>
#include        <string.h>
#include	<vme.h>
#include	<math.h>
#include	<iv.h>

#include	<alarm.h>
#include        <callback.h>
#include	<dbRecType.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbCommon.h>
#include	<fast_lock.h>
#include    <recSup.h>
#include	<devSup.h>
#include	<drvSup.h>
#include	<dbScan.h>
#include	<special.h>
#include	<module_types.h>
#include	<eventRecord.h>
#include	<drvBitBusInterface.h>
#include	<aiRecord.h>
#include	<aoRecord.h>
#include	<choiceAo.h>
#include	<biRecord.h>
#include	<boRecord.h>
#include	<longinRecord.h>
#include	<longoutRecord.h>
#include	<mbboRecord.h>
#include	<mbbiRecord.h>

/* types */
#define AI		0x01
#define AO		0x02
#define BI		0x03
#define BO		0x04
#define LI		0x05
#define LO		0x06
#define MBBI	0x07
#define MBBO	0x08


/* Define forward references     */
long report();
long init();
static long init_ai();
static long init_ao();
static long init_bo();
static long init_bi();
static long init_li();
long init_lo();
static long init_mbbi();
static long init_mbbo();
long get_ioint_info();
static long bug_rac();

/* Create the dsets for devXxBugRac */
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
} IODSET;

/* DEFINE SUPPORTED RAC COMMANDS
#define RESET_STATION	0x00	/* BO record */
#define GET_FUNCTION_ID	0x03	/* LI record */

/* xxDSET devXxxx={ 5,report,init,init_rec,get_ioint_info,bug_rac}; */

IODSET devBoBugRac = { 5, NULL, NULL, init_bo, NULL, bug_rac };
IODSET devLiBugRac = { 5, NULL, NULL, init_li, NULL, bug_rac };

/* forward references			*/

static void get_data();
static void send_cntl_trans();
extern struct drvBitBusEt drvBitBus;

volatile int bugRacDebug=0;

#define OFF		0
#define ON		1

#ifdef NODEBUG
#define Debug(FMT,V) ;
#else
#define Debug(FMT,V) {	if(bugRacDebug) \
			{\
				printf("%s(%d):",__FILE__,__LINE__); \
				printf(FMT,V); \
			} \
		     }
#endif



struct dprivate {
	struct dpvtBitBusHead bitbus;
	char type;
	unsigned char cmd;
	struct dbCommon	*precord; /* at end for callback to get it */
};
 
static int io_callback(struct dprivate *pcallback)
{
	struct dbCommon *precord;
	struct rset     *prset;

	precord=pcallback->precord;
	prset=(struct rset *)(precord->rset);

	dbScanLock(precord);
	(*prset->process)(precord);
	dbScanUnlock(precord);
}

static struct dprivate * set_bitbus(int prio,unsigned char cmd,struct bitbusio *pbitbusio)
{
	struct dprivate *my_dpvt;

        my_dpvt=(struct dprivate *)(malloc(sizeof(struct dprivate)));

	my_dpvt->bitbus.finishProc=io_callback;
	my_dpvt->bitbus.psyncSem=(SEM_ID *)NULL;
	my_dpvt->bitbus.priority=prio;
	my_dpvt->bitbus.link=pbitbusio->link;
	my_dpvt->bitbus.rxMaxLen=9;
	my_dpvt->bitbus.ageLimit=240;

	my_dpvt->bitbus.txMsg.length=9;
	my_dpvt->bitbus.txMsg.route=0x40;
	my_dpvt->bitbus.txMsg.node=pbitbusio->node;
	my_dpvt->bitbus.txMsg.tasks=0;
	my_dpvt->bitbus.txMsg.cmd=cmd;
	my_dpvt->bitbus.txMsg.data=(unsigned char *)malloc(2);
	my_dpvt->bitbus.rxMsg.data=(unsigned char *)malloc(BB_MAX_DAT_LEN);

	my_dpvt->bitbus.txMsg.data[0]='\0';
	my_dpvt->bitbus.txMsg.data[1]='\0';

	Debug("command sent:(%02.2x)\n", my_dpvt->bitbus.txMsg.cmd);

	return(my_dpvt);
}


static long init_ai(struct aiRecord *ai)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&ai->inp.value);
	struct dprivate *my_dpvt;
	int ret;

	Debug("init ai invoked\n",0);

	/* type must be an BITBUS_IO */
	if(ai->inp.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)ai,
		    "devXxBugRac (init_record) Illegal IN Bus Type");
		return(S_dev_badBus);
	}


	my_dpvt=set_bitbus(ai->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)ai;
        my_dpvt->type = AI;

        ai->dpvt=(void *)my_dpvt; 
	return(0);
}


static long init_ao(struct aoRecord *ao)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&ao->out.value);
	struct dprivate *my_dpvt;

	Debug("init ao invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(ao->out.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)ao,
		    "devXxBugRac (init_record) Illegal OUT Bus Type");
		return(S_dev_badBus);
	}

	my_dpvt=set_bitbus(ao->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)ao;
        my_dpvt->type = AO;

	ao->dpvt=(void *)my_dpvt;

	return(0);
}


static long init_bi(struct biRecord *bi)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&bi->inp.value);
	struct dprivate *my_dpvt;
	unsigned char cmd = 0;

	Debug("init bi invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(bi->inp.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)bi,
		    "devXxBugRac (init_record) Illegal IN Bus Type");
		return(S_dev_badBus);
	}

 
 	my_dpvt=set_bitbus(bi->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)bi;
        my_dpvt->type = BI;
        my_dpvt->cmd = cmd;
	bi->dpvt=(void *)my_dpvt;

	return(0);
}


static long init_bo(struct boRecord *bo)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&bo->out.value);
	struct dprivate *my_dpvt;

	Debug("init bo invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(bo->out.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)bo,
		    "devXxBugRac (init_record) Illegal IN Bus Type");
		return(S_dev_badBus);
	}

	switch(pbitbusio->signal)
	{
	default:
		recGblRecordError(S_dev_badBus,(void *)bo,
		    "devXxBugRac (init_record) Illegal IN signal number");
		return(S_dev_badBus);
	case 0:  /* signal 0 is a bo record */
 		break;
	}
 
	my_dpvt=set_bitbus(bo->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)bo;
        my_dpvt->type = BO;
        my_dpvt->cmd = pbitbusio->signal;

	bo->dpvt=(void *)my_dpvt;

	return(0);
}


static long init_li(struct longinRecord *li)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&li->inp.value);
	struct dprivate *my_dpvt;
	unsigned char cmd = 0;

	Debug("init li invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(li->inp.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)li,
		    "devXxBugRac (init_record) Illegal IN Bus Type");
		return(S_dev_badBus);
	}

	switch(pbitbusio->signal)
	{
	default:
		recGblRecordError(S_dev_badBus,(void *)li,
		    "devXxBugRac (init_record) Illegal IN signal number");
		return(S_dev_badBus);
	case 3:  /* signal 3 is a li record */
 		break;
	}

 	my_dpvt=set_bitbus(li->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)li;
        my_dpvt->type = LI;
        my_dpvt->cmd = pbitbusio->signal;
	li->dpvt=(void *)my_dpvt;

	return(0);
}


static long init_mbbi(struct mbbiRecord *mbbi)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&mbbi->inp.value);
	struct dprivate *my_dpvt;
	unsigned char cmd = 0;

	Debug("init mbbi invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(mbbi->inp.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)mbbi,
		    "devXxBugRac (init_record) Illegal IN Bus Type");
		return(S_dev_badBus);
	}

 	my_dpvt=set_bitbus(mbbi->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)mbbi;
        my_dpvt->type = MBBI;
        my_dpvt->cmd = cmd;
	mbbi->dpvt=(void *)my_dpvt;

	return(0);
}


static long init_mbbo(struct mbboRecord *mbbo)
{
	struct bitbusio *pbitbusio = (struct bitbusio *)(&mbbo->out.value);
	struct dprivate *my_dpvt;
	unsigned char cmd = 0;

	Debug("init mbbo invoked\n",0);
 
	/* out must be an BITBUS_IO */
	if(mbbo->out.type!=BITBUS_IO)
	{
		recGblRecordError(S_dev_badBus,(void *)mbbo,
		    "devXxBugRac (init_record) Illegal OUT Bus Type");
		return(S_dev_badBus);
	}

	if(pbitbusio->signal >= 8 )
	{
		recGblRecordError(S_dev_badBus,(void *)mbbo,
	    	"devXxBugRac (init_record) Illegal OUT signal number");
		return(S_dev_badBus);
	}
 
 	my_dpvt=set_bitbus(mbbo->prio,pbitbusio->signal,pbitbusio);
        my_dpvt->precord = (struct dbCommon *)mbbo;
        my_dpvt->type = MBBO;
        my_dpvt->cmd = cmd;

	mbbo->dpvt=(void *)my_dpvt;

	return(0);
}


static long bug_rac(struct dbCommon *io)
{
	struct dprivate *my_dpvt=(struct dprivate *)io->dpvt;
	struct dpvtBitBusHead *pbitbus;

	if(!io->dpvt) return(S_dev_NoInit);

	pbitbus=&(my_dpvt->bitbus);

	if(io->pact==TRUE)
	{
		Debug("Bitbus message completed \n",0);
 
		/* a transaction to bitbus has completed */
		switch(pbitbus->status)
		{
		case BB_OK:
			Debug("Getting data \n",0);
			get_data(io);
			return(0);
		default:
			recGblSetSevr(io,WRITE_ALARM,MAJOR_ALARM);
			recGblRecordError(S_dev_badBus,(void *)io,
	    			"devXxBugRac (iobug_rdwr) BitBus Error!");
			return(S_dev_badBus);
		}
	}
	else
	{
		/* data needs to be sent to bitbus */
		Debug("Sending transaction \n",0);
		send_cntl_trans(io);
	}

	io->pact=TRUE;

	/* queue the command */
	if((*drvBitBus.qReq)(pbitbus,BB_Q_LOW)<0)
	{
		recGblSetSevr(io,WRITE_ALARM,MAJOR_ALARM);
		recGblRecordError(S_dev_badBus,(void *)io,
	    	"devXxBugRac (init_record) Initial BitBus message failed");
		return(S_dev_badBus);
	}
	Debug("Queued transaction \n",0);

	return(0);
}

static void get_data(struct dbCommon *io)
{
        struct dprivate *my_dpvt=(struct dprivate *)io->dpvt;
    struct dpvtBitBusHead *pbitbus;
	struct boRecord *bo;
	struct biRecord *bi;
	struct aoRecord *ao;
	struct aiRecord *ai;
	struct longoutRecord *lo;
	struct longinRecord *li;
	struct mbboRecord *mbbo;
	struct mbbiRecord *mbbi;
	long value;
	unsigned long uvalue;
 
    pbitbus=&(my_dpvt->bitbus);

	Debug("Command return: 0x%04.4X\n",pbitbus->rxMsg.cmd);
	Debug("byte 1 of return: 0x%04.4X\n",pbitbus->rxMsg.data[0]);
	Debug("byte 2 of return: 0x%04.4X\n",pbitbus->rxMsg.data[1]);

	switch(my_dpvt->type)
	{
	case LI:
		li=(struct longinRecord *)io;
	    switch(my_dpvt->bitbus.txMsg.cmd)
		{
		case GET_FUNCTION_ID:
			/* extract the second ID code (task 1) */
 			li->val = pbitbus->rxMsg.data[1];
			li->udf = 0;
			if ((pbitbus->rxMsg.cmd) != 0) 
			    {
			    recGblSetSevr(li, READ_ALARM, INVALID_ALARM);
			    }
			break;
		default:
			break;
	    }
 	default:
		break;
	}

	return;
}

static void send_cntl_trans(struct dbCommon *io)
{
        struct dprivate *my_dpvt=(struct dprivate *)io->dpvt;
        struct dpvtBitBusHead *pbitbus;
	struct boRecord *bo;
	struct aoRecord *ao;
	struct mbboRecord *mbbo;
	short lsb,msb;
 
        pbitbus=&(my_dpvt->bitbus);

	pbitbus->finishProc=io_callback;
	pbitbus->psyncSem=(SEM_ID *)NULL;
	pbitbus->priority=io->prio;
	pbitbus->txMsg.cmd=my_dpvt->cmd;
	 	
	switch(my_dpvt->type)
	{
	case BO:
		bo=(struct boRecord *)io;
  		break;
	case MBBO:
		mbbo=(struct mbboRecord *)io;
 		break;
	case AO:
		ao=(struct aoRecord *)io;
		pbitbus->txMsg.data[0]=0;
		pbitbus->txMsg.data[1]=0;
 		break;
	default:
		break;
	}

	my_dpvt->precord=(struct dbCommon *)io;

	Debug("Command send: 0x%02.2X\n",pbitbus->txMsg.cmd);
	Debug("byte 1 sent: 0x%02.2X\n",(unsigned char)pbitbus->txMsg.data[0]);
	Debug("byte 2 sent: 0x%02.2X\n",(unsigned char)pbitbus->txMsg.data[1]);

	return;
}
