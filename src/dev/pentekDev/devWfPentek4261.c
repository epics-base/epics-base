
/*
*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).

*/

#include    <vxWorks.h>
#include    <vme.h>
#include	<iv.h>
#include	<vxLib.h>
#include    <stdio.h>
#include    <string.h>
#include    <stdlib.h>
#include    <math.h>
#include	<intLib.h>
#include	<sysLib.h>
#include    <module_types.h>

#include    <alarm.h>
#include    <dbDefs.h>
#include    <recSup.h>
#include    <devSup.h>
#include    <dbScan.h>
#include    <callback.h>
#include	<waveformRecord.h>

/*
	This is waveform record support for the Pentek 4261A 10MHz 12 bit ADC.
	All scan types are supported, including I/O interrupt scan.  The ADC
	currently supports the edge trigger mode for starting the sampling
	operation.  The ADC is programmed to record an array of samples the
	same size as the waveform record.  The internal or an external clock
	may be used as the sampling clock.  The sampling can be triggered by
	an external signal, or automatically by record support.

	Using I/O interrupt scanning, the record is processed and the waveform
	record array is updated each time an external trigger signal is present.
	With the other scan types, the waveform array is updated each time the
	record is processed.

	I could not get the continuous bank swapping feature to work with the
	trigger, I got a glitch in the data approximately 1 us into the sampling.
	After each round of sampling is done, the buffers are reset.

	The function ConfigurePentekADC() must be run in the 
	vxWorks startup.cmd file for every ADC board present.  This function
	makes it easy to configure jumpers on the ADC and inform EPICS of them.

	parameters:

	ConfigurePentekADC(
		int card, - the ADC card number in the crate (0=first)
		unsigned long a16_base, - where the card exists in A16
		unsigned long a32_base, - where the card exists in A32 (not used)
		int irq_vector, - interrupt request vector number
		int irq_level, - interrupt request level number
		int word_clock, - the ADC word clock (see 4261 documentation)
		unsigned long clock_freq - clock frequency (see notes below)
	)

	Good values to use for configuring the module:
		A16 base address = 0x0200
		A32 base address = 0xc0200000 (A32 not used in the support)
		IRQ vector = 121
		IRQ level = 3
		word clock = 32

	The number of samples programed (size of waveform record) must be
	divisible by the word clock.

	The operating mode the ADC is selected using the input field parm area.
	The user must supply two option, the clock type and the trigger type,
	in that order.  Here are all the valid parm field entry and what they
	represent (all are entered as strings without the quotes):

	"x_clock,x_trigger" = external clock and external trigger inputs.
	"i_clock,x_trigger" = internal clock and external trigger input.
	"x_clock,i_trigger" = external clock and internal trigger input.
	"i_clock,i_trigger" = internal clock and internal trigger input.

	If external trigger is selected and scan type is not I/O interrupt scan,
	then sampling will start on the next external trigger input after device
	support completes.

	If internal trigger is selected, the sampling will be started by software
	at the end of device support.  Internal trigger only applied to scan
	types that are not I/O interrupt scanned.

	Configuration function clock frequency note:
		The clock frequency specified in the configuration function is only
		applicable to the internal clock.  When the external clock is used,
		the clock frequency parameter is meaningless, the sampling clock
		will be the external clock frequency.  When the internal clock is
		used (10MHz), the clock frequency parameter will be the sampling
		frequency.  The 10Mhz internal clock must be divisible by the clock
		frequency value.

	-----------------------------------------------------------------------
	Modify devSup.ascii and do a makesdr.  Add devPentekADC
	device support to the waveform record.
	 
	Running ReportPentekADC() will print a report of configuration for
	the ADC.
	 
	Waveform data types of long, short, unsigned short, double, and float are
	supported.
*/

long devWfPentek4261Debug=0;

#define CMD_ADDED 0
#define CMD_DELETED 1

#define A16_SIZE 256
#define A32_SIZE 256
#define MAX_CARDS 4
#define MAX_FILES_ALLOWED 20

/*-----------------------------------------------------------------*/
/* IMPORTANT, LOOK HERE: test areas - good ones to use for jumpers */
/*	They are good setting for the jumpers for the first ADC card,
	none of these defines are actually used, are they parameters come
	from the ConfigurePentekADC().
*/

#define A16_BASE 0xffff0200
#define A32_BASE 0xc0200000 /* not really used currently */
#define IRQ_VECTOR ((unsigned char)121)
#define IRQ_LEVEL 3
#define WORD_CLOCK 32 /* my default */
/*-----------------------------------------------------------------*/

#define INTERNAL_CLOCK 10000000 /* 10MHz */

#define SAMPLE_CLOCK_SELECT 0x08
#define EXT_TRIGGER_ENABLE 0x04
#define RESET_RUN_FF 0x02
#define SET_RUN_FF 0x01

#define START_MODE	0x20

#define EXTERNAL 0
#define INTERNAL 1

struct card {
	int in_use;
	unsigned long a16_base;
	unsigned long a32_base;
	int irq_vector;
	int irq_level;
	int word_clock;
	unsigned long clock_freq;
	int soc;
	IOSCANPVT ioscanpvt;
};
typedef struct card CARD;

/* careful here: horrid looking structure -offset and access (rw=read/write) */
struct adc {
	unsigned short ctc0_bs_a;		/* 0x00 rw */
	unsigned short ctc1_bs_b;		/* 0x02 rw */
	unsigned short ctc2_clock_div;	/* 0x04 rw */
	unsigned short ctc_control;		/*c0x06 rw */ /*char a;*/ short junk1[0x04];
	unsigned short int_id_status;	/*c0x10 rw */ /*char b;*/
	unsigned short int_mask;			/*c0x12 rw */ /*char c;*/
	unsigned short command;			/*c0x14 rw */ /*char d;*/
	unsigned short trigger;			/*c0x16 rw */ /*char e;*/
	unsigned short reserved;		/* 0x18 -- */
	unsigned short tag_cmd;			/*c0x1a rw */ /*char f;*/
	unsigned short tag_data;		/* 0x1c rw */
	unsigned short int_status;		/*c0x1e r- */ /*char g;*/ char junk2[0x10];
	unsigned short vme_fifo;		/* 0x30 rw */
};
typedef struct adc ADC;

struct pvt_area {
	CALLBACK callback;
	int clock_mode;
	int trigger_mode;
	int last_int_status;
	int last_status;
	volatile ADC* adc_regs;
	unsigned short div;
	int card;
	int file_count;
	unsigned long total_count;
};
typedef struct pvt_area PVT_AREA;

static long dev_report(int level);
static long dev_init_rec(struct waveformRecord* pr);
static long dev_ioint_info(int cmd,struct waveformRecord* pr,IOSCANPVT* iopvt);
static long dev_read(struct waveformRecord* pr);
static long dev_complete_read(struct waveformRecord* pr);
static long dev_other_read(struct waveformRecord* pr);
static void irq_func(void*);
static int full_reset(PVT_AREA* pvt);
static int buffer_reset(PVT_AREA* pvt);
static long setup(PVT_AREA* pvt);

void ConfigurePentekADC(int,unsigned long,unsigned long,int,int,int,unsigned long);

/* generic structure for device support */
typedef struct {
    long        number;
    DEVSUPFUN   report;
    DEVSUPFUN   init;
    DEVSUPFUN   init_record;
    DEVSUPFUN   get_ioint_info;
    DEVSUPFUN   read_write;
    DEVSUPFUN   conv;
} ADC_DSET;

ADC_DSET devWfPentek4261=
	{6,dev_report,NULL,dev_init_rec,dev_ioint_info,dev_read,NULL};

/* this is a bug! there should be one per card! */
static CARD** cards=0;

static void callback(CALLBACK* cback)
{
    struct dbCommon* pc;
    struct rset *prset;

    callbackGetUser(pc,cback);
    prset=(struct rset *)(pc->rset);

    /* process the record */
    dbScanLock(pc);
    (*prset->process)(pc);
    dbScanUnlock(pc);
}

static long dev_report(int level)
{
	int i;
	volatile ADC* adc_regs;

	if(cards==0)
		printf("No Pentek ADC cards configured in system\n");
	else
	{
		for(i=0;i<MAX_CARDS;i++)
		{
			if(cards[i]!=0)
			{
				adc_regs=(ADC*)(i*A16_SIZE+cards[i]->a16_base);

				printf("Pentek 4261A ADC card %d information:\n",i);
				printf(" a16 base=%8.8x",cards[i]->a16_base);
				printf(" a32 base=%8.8x\n",cards[i]->a32_base);
				printf(" irq vector=%d",cards[i]->irq_vector);
				printf(" irq level=%d\n",cards[i]->irq_level);
				printf(" word clock=%d",cards[i]->word_clock);
				printf(" clock frequency=%ld\n",cards[i]->clock_freq);

				printf("  status=%4.4x\n", adc_regs->int_id_status);
				printf("  int mask=%4.4x\n", adc_regs->int_mask);
				printf("  command=%4.4x\n", adc_regs->command);
				printf("  trigger=%4.4x\n", adc_regs->trigger);
				printf("  int status=%4.4x\n", adc_regs->int_status);
			}
			else
				printf("Pentek 4261A ADC card %d not installed\n",i);
		}
	}
	return 0;
}

static long dev_init_rec(struct waveformRecord* pr)
{
	volatile ADC* adc_regs;
	PVT_AREA* pvt;
	struct vmeio *pvmeio = (struct vmeio *)&(pr->inp.value);
	char *clock_type,*trigger_type;
	char parm[40];
	char** save_area=NULL;

	if(pr->inp.type != VME_IO)
	{
		recGblRecordError(S_db_badField,(void *)pr,
		"devPentekADC (init_record) Illegal INP field");
		return(S_db_badField);
	}
	if(cards[pvmeio->card]==0)
	{
		recGblRecordError(S_dev_badCard,(void *)pr,
		"devPentekADC (init_record) Card not Configured!");
		return(S_dev_badCard);
	}
	if(cards[pvmeio->card]->in_use==1)
	{
		recGblRecordError(S_dev_badCard,(void *)pr,
		"devPentekADC (init_record) Card already in use");
		return(S_dev_badCard);
	}
	if(pr->nelm%cards[pvmeio->card]->word_clock!=0)
	{
		recGblRecordError(S_db_badField, (void *)pr,
		"devPentekADC (init_record) num of elements must be divisible by the word clock");
		return(S_db_badField);
	}

	pvt=(PVT_AREA*)malloc(sizeof(PVT_AREA));
	cards[pvmeio->card]->in_use=1;
	strcpy(parm,pvmeio->parm);

	if((clock_type=strtok_r(parm,",",save_area))==NULL)
	{
		printf("Clock type parameter missing from parm field\n");
		printf(" Defaulting to i_clock,i_trigger (internal clock/trigger)\n");
		clock_type="i_clock";
		trigger_type="i_trigger";
	}
	else if((trigger_type=strtok_r(NULL,",",save_area))==NULL)
	{
		printf("Triggert type parameter missing from parm field\n");
		printf(" Defaulting to i_trigger (internal trigger)\n");
		trigger_type="i_trigger";
	}

	if( strcmp(clock_type,"i_clock")==0 )
		pvt->clock_mode=INTERNAL;
	else if( strcmp(clock_type,"x_clock")==0 )
		pvt->clock_mode=EXTERNAL;
	else
	{
		printf("Invalid parm for clock type, must be i_clock or x_clock\n");
		pvt->clock_mode=INTERNAL;
	}

	if( strcmp(trigger_type,"i_trigger")==0 )
		pvt->trigger_mode=INTERNAL;
	else if( strcmp(trigger_type,"x_trigger")==0 )
		pvt->trigger_mode=EXTERNAL;
	else
	{
		printf("Invalid parm for clock type, must be i_trigger or x_trigger\n");
		pvt->trigger_mode=INTERNAL;
	}

	adc_regs=(ADC*)(pvmeio->card*A16_SIZE+cards[pvmeio->card]->a16_base);

	callbackSetCallback(callback,&pvt->callback);
	callbackSetPriority(pr->prio,&pvt->callback);
	callbackSetUser(pr,&pvt->callback);

	pvt->adc_regs=adc_regs;
	pvt->card=pvmeio->card;
	pvt->file_count=0;
	pr->dpvt=(void*)pvt;

	/* program number of samples and sample clock */
	pvt->div=pr->nelm/cards[pvmeio->card]->word_clock;

	/* install the interrupt handler */
    if(intConnect(INUM_TO_IVEC(cards[pvmeio->card]->irq_vector),
		(VOIDFUNCPTR)irq_func, (int)pr)!=OK)
       		{ printf("intConnect failed\n"); return -1; }
 
    sysIntEnable(cards[pvmeio->card]->irq_level);
	full_reset(pvt);
	return 0;
}

static int buffer_reset(PVT_AREA* pvt)
{
	pvt->adc_regs->command=0x00;	/* reset the card */
	pvt->adc_regs->command=0x90;	/* vme enable, no reset mode */
	return 0;
}

static int full_reset(PVT_AREA* pvt)
{
	unsigned long clock_div,clock_freq,sample_freq;
	unsigned short clock_div_short;
	unsigned char trig;

	buffer_reset(pvt);

	/* auto arm mode not enabled */
	trig=0x20; /* positive going edge */

	if(pvt->clock_mode==EXTERNAL) trig|=0x08; /* ext clock */

	pvt->adc_regs->trigger=trig;	/* set the trigger reg */
	pvt->adc_regs->int_id_status=cards[pvt->card]->irq_vector;

	/* program counter for bank A */
	pvt->adc_regs->ctc_control=0x00|0x30|0x04; /* CTC-0,LSB-MSB,mode */
	pvt->adc_regs->ctc0_bs_a=(unsigned short)(pvt->div&0x00ff);
	pvt->adc_regs->ctc0_bs_a=(unsigned short)(pvt->div>>8);
	/* program counter for bank B */
	pvt->adc_regs->ctc_control=0x40|0x30|0x04; /* CTC-1,LSB-MSB,mode */
	pvt->adc_regs->ctc1_bs_b=(unsigned short)(pvt->div&0x00ff);
	pvt->adc_regs->ctc1_bs_b=(unsigned short)(pvt->div>>8);

	/* printf("bank div=%4.4x\n",pvt->div); */

	/* -------------------------------------------------------------- */
	/*
	careful here, if changing code to use internal clock, use the
	code below to program the sample clock divisor, remember that if
	sample clock divisor is one, then the else portion of the code must
	be used.

	SampleClock = InputClock / N

	where:
		N is programmed divisor and 1<=N<=65535
		InputClock is 10MHz for internal clock or the external clock
		SampleClock if the effective sampling clock

	We are given SampleClock, and need N, so

	N = InputClock / SampleClock
	*/

	/* internal clock type uses clock_freq from config as sample clock,
	   external clock does not use clock divisor */

	if(pvt->clock_mode==INTERNAL) /* internal clock */
	{
		clock_freq=INTERNAL_CLOCK;
		sample_freq=cards[pvt->card]->clock_freq;
	}
	else
	{
		clock_freq = cards[pvt->card]->clock_freq;
		sample_freq = clock_freq;
	}

	if(clock_freq==sample_freq)
	{
		/* if clock divisor of one is to be used (10MHz) */
		pvt->adc_regs->ctc_control=0x80|0x10; /* CTC-2, LSB, mode 0 */
	}
	else
	{
		clock_div=clock_freq/sample_freq;
		/* printf("clock div=%8.8x\n",clock_div); */

		if(clock_div>=1 && clock_div<=65535)
		{
			clock_div_short = clock_div;
			/* if clock divisor used, sample rate */
			pvt->adc_regs->ctc_control=0x80|0x30|0x04; /* CTC-2,LSB-MSB,mode */
			pvt->adc_regs->ctc2_clock_div=(clock_div_short&0x00ff);
			pvt->adc_regs->ctc2_clock_div=(clock_div_short>>8);
		}
		else
		{
			pvt->adc_regs->ctc_control=0x80|0x10; /* CTC-2, LSB, mode 0 */
			printf("Invalid clock/sample frequency: %ld/%ld\n",
				clock_freq,sample_freq);
		}
	}
	/* -------------------------------------------------------------- */

	/* start mode = off at this point */
	return 0;
}

static long dev_ioint_info(int cmd,struct waveformRecord* pr,IOSCANPVT* iopvt)
{
	PVT_AREA* pvt = (PVT_AREA*)pr->dpvt;

	pr->pact=0;

	if(cmd==CMD_ADDED)
		setup(pvt);
	else /* CMD_DELETED */
		buffer_reset(pvt); /* ensure that we are in a good state */

	*iopvt=(cards[pvt->card])->ioscanpvt;
	return 0;
}

static long dev_read(struct waveformRecord* pr)
{
	long rc;
	PVT_AREA* pvt = (PVT_AREA*)pr->dpvt;

	if(pr->scan==SCAN_IO_EVENT)
	{
		pvt->adc_regs->command|=0x20;		/* start mode */
		rc=dev_complete_read(pr);
		setup(pvt);
	}
	else
		rc=dev_other_read(pr);

	return rc;
}

static long setup(PVT_AREA* pvt)
{
	unsigned char trig;
	volatile unsigned char istat;

	buffer_reset(pvt);
	trig=pvt->adc_regs->trigger&0xf8; /* clear run FF,trigger */
	trig|=0x44; /* external trigger on, auto arm mode */
	istat=pvt->adc_regs->int_status;	/* read int status */
	pvt->adc_regs->int_mask=0x10;		/* bank swap interrupt */
	pvt->adc_regs->trigger=trig|0x02;	/* trigger mode + reset run FF */
	pvt->adc_regs->command|=0x20;		/* start mode */
	pvt->adc_regs->trigger=trig;		/* trigger mode */

	return 0;
}

static long dev_other_read(struct waveformRecord* pr)
{
	PVT_AREA* pvt = (PVT_AREA*)pr->dpvt;
	long rc=0;

	if(pr->pact==TRUE)
	{
		/* i/o complete */

		/* interrupt handler shut down everything already */
		pvt->adc_regs->command|=0x20;	/* start mode on */
		rc=dev_complete_read(pr); 		/* process data in buffer */
		pvt->adc_regs->command&=~0x20;	/* start mode off */
	}
	else
	{
		/* start the i/o */

		/* this sucks, with internal mode trigger, the board glitch
		every so often, I cannot get it to work correctly, so do full reset */
		if(pvt->trigger_mode==INTERNAL) full_reset(pvt);

		callbackSetPriority(pr->prio,&pvt->callback);
		setup(pvt);
		pr->pact=TRUE;

		if(pvt->trigger_mode==INTERNAL)
			pvt->adc_regs->trigger|=0x01; /* set run FF */
	}
	return rc;
}

static long dev_complete_read(struct waveformRecord* pr)
{
	int i;
	PVT_AREA* pvt = (PVT_AREA*)pr->dpvt;
	volatile unsigned short* source;
	volatile unsigned short samples;

	source=&(pvt->adc_regs->vme_fifo);
	i=0;

	switch(pr->ftvl)
	{
	case DBF_FLOAT:
		{
			float* f_thing = (float*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				f_thing[i]=(float)(((short)samples)>>4);
			}
			pr->nord=i;
			break;
		}
	case DBF_DOUBLE:
		{
			double* d_thing = (double*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				d_thing[i]=(double)(((short)samples)>>4);
			}
			pr->nord=i;
			break;
		}
	case DBF_ULONG:
		{
			unsigned long* ul_thing = (unsigned long*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				ul_thing[i]=(unsigned long)((samples>>4)&0x0fff);
			}
			pr->nord=i;
			break;
		}
	case DBF_LONG:
		{
			long* l_thing = (long*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				l_thing[i]=(long)(((long)samples)>>4);
			}
			pr->nord=i;
			break;
		}
	case DBF_USHORT:
		{
			unsigned short* s_thing = (unsigned short*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				s_thing[i]=(unsigned short)((samples>>4)&0x0fff);
			}
			pr->nord=i;
			break;
		}
	default:
		printf("devPentek4261: Invalid data type\n");
	case DBF_SHORT:
		{
			short* ss_thing = (unsigned short*)pr->bptr;

			for(i=0;i<pr->nelm;i++)
			{
				samples=pvt->adc_regs->vme_fifo;
				ss_thing[i]=(short)(((short)(samples))>>4);
			}
			pr->nord=i;
			break;
		}
	}

	/* clear remaining samples if any */
	while(pvt->adc_regs->int_id_status&0x80)
		samples=pvt->adc_regs->vme_fifo;

	/* pr->nelm, pr->bptr, &(pr->nord) */ /* three important fields */
	pr->udf=0;
	return 0;
}

/* IRQ under vxWorks */
static void irq_func(void* v)
{
	struct waveformRecord* pr = (struct waveformRecord*)v;
	PVT_AREA* pvt = (PVT_AREA*)pr->dpvt;
	CALLBACK* cb = (CALLBACK*)pvt;
	unsigned char trig;

	pvt->last_int_status=pvt->adc_regs->int_status; /* read status */

	/* logMsg("in irq_func\n"); */
	/* if(pvt->last_int_status&0x02) logMsg("Overrun error\n"); */

	pvt->adc_regs->command&=~0x20;	/* start mode off - freeze all */
	pvt->adc_regs->int_mask=0x00;	/* interrupts off */
	trig=pvt->adc_regs->trigger&0xfc; /* clear run FF bits */
	pvt->adc_regs->trigger=trig; /* clear run FF bits */
	pvt->adc_regs->trigger=trig|0x02; /* force reset run FF */

	if(pr->scan==SCAN_IO_EVENT)
		scanIoRequest((cards[pvt->card])->ioscanpvt); /* scan EPICS records */
	else
		callbackRequest(cb);
}

void ReportPentekADC() { dev_report(0); }

void ConfigurePentekADC( int card, 
		unsigned long a16_base, unsigned long a32_base,
		int irq_vector, int irq_level,
		int word_clock, unsigned long clock_freq)
{
	unsigned short dummy;

	if(cards==0)
	{
		cards=(CARD**)malloc(sizeof(CARD*)*MAX_CARDS);
		memset((char*)cards,0,sizeof(CARD*)*MAX_CARDS);
	}

	if(cards[card]!=0) printf("Overriding previous configuration\n");
	else cards[card]=(CARD*)malloc(sizeof(CARD));

	cards[card]->in_use=0;
	if( sysBusToLocalAdrs(VME_AM_SUP_SHORT_IO,(char*)a16_base,(char**)&(cards[card]->a16_base))!=OK) 
	{
		printf(" a16 base could not be converted\n");
	}
	if( sysBusToLocalAdrs(VME_AM_EXT_SUP_DATA,(char*)a32_base,(char**)&(cards[card]->a32_base))!=OK) 
	{
		printf(" a32 base could not be converted\n");
	}
	cards[card]->irq_vector=irq_vector;
	cards[card]->irq_level=irq_level;
	cards[card]->word_clock=word_clock;
	cards[card]->clock_freq=clock_freq;

	if(vxMemProbe((char*)cards[card]->a16_base,READ,
				sizeof(unsigned short),(char*)&dummy)!=OK)
	{
		/* card not really present */
		cards[card]->in_use=1;
	}
	scanIoInit(&(cards[card]->ioscanpvt));
}

