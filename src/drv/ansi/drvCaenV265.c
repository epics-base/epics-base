/* share/src/drv @(#)drvCaenV265.c	1.1 9/2/94 */
/* drvCaenV265.c -  Driver/Device Support Routines for CAEN V265 
 *
 *      Author:     	Jeff Hill (johill@lanl.gov)
 *      Date:        	8-11-94	
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
 *		MIT Bates Lab
 *
 * Modification Log:
 * -----------------
 */

/*
 * ANSI C Includes
 */
#include <stdio.h>
#include <stddef.h>
#include <types.h>
#include <assert.h>

/*
 * vxWorks includes
 */
#include <vme.h>
#include <iv.h>
#include <sysLib.h>
#include <intLib.h>
#include <logLib.h>
#include <vxLib.h>
#include <rebootLib.h>
#include <taskLib.h>
#include <tickLib.h>
#include <wdLib.h>

/*
 * EPICS include
 */
#include <dbDefs.h>
#include <dbScan.h>
#include <drvSup.h>
#include <devSup.h>
#include <recSup.h>
#include <devLib.h>
#include <aiRecord.h>
#include <errMdef.h>
#include <epicsPrint.h>


/*
 * base address, base interrupt vector,
 * number of cards, & interrupt level
 */
#define CAIN_V265_A24_BASE 		(0x000000)
#define CAIN_V265_INTVEC_BASE 		(0xA0)
#define CAIN_V265_MAX_CARD_COUNT 	(8)
#define CAIN_V265_INT_LEVEL 		(6)




/*
 * all device registers declared 
 * ANSI C volatile so we dont need to
 * use the -fvolatile flag (and dont
 * limit the optimizer)
 */
typedef volatile int16_t	devReg;

struct caenV265 {
	devReg		csr;
	devReg		clear;
	devReg		DAC;
	devReg		gate;
	const devReg	data;
	const devReg	pad1[(0xf8-0x8)/2];
	const devReg	fixed;
	const devReg	identifier;
	const devReg	version;
};
#define CAENV265ID 0x0812

/*
 * Insert or extract a bit field using the standard
 * masks and shifts defined below
 */
#ifdef __STDC__ 
#define INSERT(FIELD,VALUE)\
        (((VALUE)&(FD_ ## FIELD ## _M))<<(FD_ ## FIELD ## _S))
#define EXTRACT(FIELD,VALUE)\
        ( ((VALUE)>>(FD_ ## FIELD ## _S)) &(FD_ ## FIELD ## _M))
#else /*__STDC__*/
#define INSERT(FIELD,VALUE)\
        (((VALUE)&(FD_/* */FIELD/* */_M))<<(FD_/* */FIELD/* */_S))
#define EXTRACT(FIELD,VALUE)\
        ( ((VALUE)>>(FD_/* */FIELD/* */_S)) &(FD_/* */FIELD/* */_M))
#endif /*__STDC__*/

/*
 * in the constants below _M is a right justified mask
 * and _S is a shift required to right justify the field
 */

/*
 * csr register 
 */
#define FD_FULL_M		(0x1)
#define FD_FULL_S        	(14)
#define FD_READY_M		(0x1)
#define FD_READY_S        	(15)
#define FD_BUSY_FULL_M		(0x3)
#define FD_BUSY_FULL_S        	(14)
#define FD_IVEC_M		(0xff)
#define FD_IVEC_S        	(0)
#define FD_ILEVEL_M		(0x7)
#define FD_ILEVEL_S        	(8)

/*
 * series/version register
 */
#define FD_SERIES_M		(0xfff)
#define FD_SERIES_S        	(0)
#define FD_VERSION_M		(0xf)
#define FD_VERSION_S        	(12)

/*
 * data register
 */
#define FD_CHANNEL_M		(0x7)
#define FD_CHANNEL_S		(13)
#define FD_RANGE_M		(1)
#define FD_RANGE_S		(12)
#define FD_DATA_M		(0xfff)
#define FD_DATA_S		(0)

struct channel{
	int16_t	signal; 
	char	newData;
};

enum adc_range {adc_12, adc_15, NUMBER_OF_ADC_RANGES};
#define NUMBER_OF_SIGNALS 8
#define NUMBER_OF_FIFO_ENTRIES (16*NUMBER_OF_SIGNALS*NUMBER_OF_ADC_RANGES)
LOCAL struct caenV265Config{
	struct caenV265	*pCaenV265;	/* pointer to the card */
	struct channel	chan[NUMBER_OF_SIGNALS][NUMBER_OF_ADC_RANGES];
	IOSCANPVT	scanpvt;
	WDOG_ID		wdid;
}caenV265Info[CAIN_V265_MAX_CARD_COUNT];

#ifdef __STDC__
#define SHOW_OFFSET(STRUCT,FIELD) \
printf(	"%s.%s is at 0x%X\n", \
	#STRUCT, \
	#FIELD, \
	offsetof(struct STRUCT, FIELD))
#endif

LOCAL void 	caenV265ISR(unsigned card);
LOCAL int 	caenV265Shutdown(void);
LOCAL int 	caenV265IdTest(struct caenV265 *pCaenV265);
int 		caenV265Test(unsigned card);
LOCAL void 	caenV265ReadData(struct caenV265Config *pCaenV256Config);
LOCAL int 	caenV265TestVal(unsigned card, unsigned dacVal);
LOCAL int	caenV265IntEnable(unsigned card);

/*
 * device support entry table
 */
LOCAL long 	caenV265InitRecord(struct aiRecord *pai);
LOCAL long 	caenV265AiRead(struct aiRecord *pai);
LOCAL long 	caenV265SpecialLinconv(struct aiRecord *pai, int after);
LOCAL long 	caenV265GetIoIntInfo(int cmd, struct aiRecord *pai, IOSCANPVT *ppvt);
struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       read_ai;
        DEVSUPFUN       special_linconv;
} devCaenV265 ={
        6,
        NULL,
        NULL,
        caenV265InitRecord,
        caenV265GetIoIntInfo,
        caenV265AiRead,
        caenV265SpecialLinconv};

/*
 * driver support entry table
 */
LOCAL long 	caenV265Init(void);
LOCAL long 	caenV265IOReport(int level);
struct {
        long    	number;
        DRVSUPFUN       report;
        DRVSUPFUN       init;
} drvCaenV265 ={
        2,
        caenV265IOReport,      
        caenV265Init};



/*
 * verify that register
 * offsets match the doc.
 */
#ifdef DEBUG
void offsettest()
{
	SHOW_OFFSET(caenV265, version);
	SHOW_OFFSET(caenV265, identifier);
	SHOW_OFFSET(caenV265, fixed);
	SHOW_OFFSET(caenV265, data);
	SHOW_OFFSET(caenV265, gate);
	SHOW_OFFSET(caenV265, DAC);
	SHOW_OFFSET(caenV265, clear);
	SHOW_OFFSET(caenV265, csr);

	return;
}
#endif


/* 
 * caenV265InitRecord()
 */
LOCAL long caenV265InitRecord(struct aiRecord *pai)
{
    	struct vmeio 	*pvmeio;
 
    	/* ai.inp must be an VME_IO */
    	switch (pai->inp.type) {
    	case (VME_IO):
        	break;
    	default :
        	recGblRecordError(S_db_badField,(void *)pai,
                	"devAiXy566Se (init_record) Illegal INP field");
        	return(S_db_badField);
    	}
 
        pvmeio = (struct vmeio *)&(pai->inp.value);

	/*
	 * check for bad signal or card number
	 */
	if (	pvmeio->signal >= NUMBER_OF_SIGNALS ||
		pvmeio->card >= CAIN_V265_MAX_CARD_COUNT ) {

        	recGblRecordError(
			S_db_badField,
			(void *)pai,
                	"devCaenV265 bad card or signal number");
		return -1;
	}

	if(!caenV265Info[pvmeio->card].pCaenV265){
        	recGblRecordError(
			S_db_badField,
			(void *)pai,
                	"devCaenV265 card does not exist");
		return -1;
	}

    	/* set linear conversion slope*/
    	pai->eslo = (pai->eguf-pai->egul)/FD_DATA_M;
 
    	return(0);
}


/* 
 * caenV265AiRead()
 */
LOCAL long 	caenV265AiRead(struct aiRecord *pai)
{
        int16_t		value;
        struct vmeio 	*pvmeio;
 
        pvmeio = (struct vmeio *)&(pai->inp.value);
 
	/*
	 * check for bad signal or card number
	 */
	if (	pvmeio->signal >= NUMBER_OF_SIGNALS ||
		pvmeio->card >= CAIN_V265_MAX_CARD_COUNT ) {

                recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
		return -1;
	}

	/*
	 * uninitialized data?
	 */
	if (!caenV265Info[pvmeio->card].chan[pvmeio->signal][adc_12].newData) {
                recGblSetSevr(pai, READ_ALARM, INVALID_ALARM);
		return -1;
	}

	value = caenV265Info[pvmeio->card].chan[pvmeio->signal][adc_12].signal;
        pai->rval = value;

        return 0;
}


/*
 * caenV265SpecialLinconv()
 */
LOCAL long 	caenV265SpecialLinconv(struct aiRecord *pai, int after)
{
    	if(!after) { 
		return 0;
	}

    	/* set linear conversion slope*/
    	pai->eslo = (pai->eguf-pai->egul)/FD_DATA_M;
    	return 0;
}


/*
 * caenV265GetIoIntInfo()
 */
LOCAL long 	caenV265GetIoIntInfo(
int cmd, 
struct aiRecord *pai, 
IOSCANPVT *ppvt)
{
        struct vmeio 	*pvmeio;

        pvmeio = (struct vmeio *)&(pai->inp.value);
 
	/*
	 * check for bad card number
	 */
	if ( pvmeio->card >= CAIN_V265_MAX_CARD_COUNT ) {
		epicsPrintf (
			"%s.%d:devCaenV265 bad card number %d %s\n",
			(int)__FILE__,
			__LINE__,
			pvmeio->card,
			(int)pai->name);
        	recGblRecordError(
			S_db_badField,
			(void *)pai,
                	"devCaenV265 bad card number");
		return -1;
	}

	*ppvt = &caenV265Info[pvmeio->card].scanpvt;

	return 0;
}  


/*
 * caenV265Init()
 */
LOCAL long caenV265Init(void)
{
	unsigned	card;
	struct caenV265 *pCaenV265;
	int 		status;

	status = rebootHookAdd(caenV265Shutdown);
	if(status){
		errMessage(S_dev_internal,"reboot hook add failed");
		return ERROR;
	}

	status = sysBusToLocalAdrs(
			VME_AM_STD_SUP_DATA,
			CAIN_V265_A24_BASE,
			(char **)&pCaenV265);
	if(status!=OK){
		errPrintf(
			S_dev_badA24, 
			__FILE__, 	
			__LINE__, 
			"caenV265Init");
		return ERROR;
	}
			
	for(card=0; card<CAIN_V265_MAX_CARD_COUNT; card++, pCaenV265++){
		unsigned vec;

		if(!caenV265IdTest(pCaenV265)){
			continue;
		}

		caenV265Info[card].wdid = wdCreate();
		if(!caenV265Info[card].wdid){
			continue;	
		}

		/*
		 * flag that we have found a caen V265 
		 */
		caenV265Info[card].pCaenV265 = pCaenV265;

		/*
		 * init the EPICS db int event scan block
		 */
		scanIoInit(&caenV265Info[card].scanpvt);

		/*
 		 * reset the device
		 */
		pCaenV265->clear = 0;  /* any rw op resets the device */
		pCaenV265->DAC = 0;    /* set test-signal "offset" to zero */

		/*
 		 * attach ISR 
		 */
		vec = CAIN_V265_INTVEC_BASE+card;
		status = intConnect(
				INUM_TO_IVEC(vec), 
				caenV265ISR, 
				card);
		assert(status>=0);

		/*
 		 * Enable interrupts
		 */
		caenV265IntEnable(card);
	}
	status = sysIntEnable(CAIN_V265_INT_LEVEL);
	assert(status>=0);
	
	return OK;
}


/*
 * caenV265ISR()
 */
LOCAL void caenV265ISR(unsigned card)
{
	struct caenV265Config	*pCaenV256Config = &caenV265Info[card];
	struct caenV265 	*pCaenV265 = pCaenV256Config->pCaenV265;
	unsigned		signal;
	int16_t			csr;
	static unsigned		ticks;
	unsigned		newTicks;
	
	/*
 	 * If its full then its more efficient
	 * to read it out without checking 
	 * in between each read
	 */
	csr = pCaenV265->csr;	
	if (EXTRACT(FULL,csr)) {
		for(	signal=0; 
			signal<NUMBER_OF_FIFO_ENTRIES; 
			signal++){

			caenV265ReadData(pCaenV256Config);
		}
		return;
	}

	/*
	 * Not full so check to see if its ready before
	 * reading
	 */
	while (EXTRACT(READY, csr)) {
		caenV265ReadData(pCaenV256Config);
		csr = pCaenV265->csr;
	}

	/*
	 * limit the EPICS scan rate
	 */
	newTicks = tickGet();
	if(newTicks == ticks){
		/*
 		 * Disable Interrupts
		 */
		pCaenV265->csr = 0;

		/*
		 * start a watch dog after one tick
		 * so that we limit the int rate to
		 * the system tick rate.
		 */
		wdStart(pCaenV256Config->wdid,
			1,
			caenV265IntEnable,
			card);

		return;
	}
	else{
		ticks = newTicks;
	}

	/*
	 * tell EPICS to scan on int
	 */
	scanIoRequest(&caenV265Info[card].scanpvt);

	return;
}


/*
 * caenV265IntEnable
 */
LOCAL int caenV265IntEnable(unsigned card)
{
	struct caenV265Config	*pCaenV256Config = &caenV265Info[card];
	struct caenV265 	*pCaenV265 = pCaenV256Config->pCaenV265;
	unsigned 		vec;
	int16_t			newcsr;

	
	vec = CAIN_V265_INTVEC_BASE+card;
	newcsr = INSERT(IVEC, vec) | INSERT(ILEVEL, CAIN_V265_INT_LEVEL);
	pCaenV265->csr = newcsr;

	return OK;
}


/*
 * caenV265ReadData()
 */
LOCAL void caenV265ReadData(struct caenV265Config *pCaenV256Config)
{
	struct caenV265 *pCaenV265 = pCaenV256Config->pCaenV265;
	int16_t	val = pCaenV265->data;
	int16_t data = EXTRACT(DATA, val);
	unsigned range = EXTRACT(RANGE, val);
	unsigned signal = EXTRACT(CHANNEL, val);

	if(range>=NUMBER_OF_ADC_RANGES){
		epicsPrintf ("caenV265ReadData: bad range number\n");
		return;
	}
	if(signal>=NUMBER_OF_SIGNALS){
		epicsPrintf ("caenV265ReadData: bad signal number\n");
		return;
	}
	pCaenV256Config->chan[signal][range].signal=data;
	pCaenV256Config->chan[signal][range].newData=TRUE;

	return;
}


/*
 * caenV265IdTest()
 */
LOCAL int caenV265IdTest(struct caenV265 *pCaenV265)
{
	int 	status;
	int16_t	id;

	/*
	 * Is a card present
	 */
	status = vxMemProbe(
			(char *)&pCaenV265->identifier,
			READ,
			sizeof(id),
			(char *)&id);
	if(status!=OK){
		return FALSE;
	}

	/*
	 * Is the correct type of card present
	 */
	if(id!=CAENV265ID){
		errPrintf(
			S_dev_wrongDevice, 
			__FILE__, 	
			__LINE__, 
			"caenV265IdTest");
		return FALSE;
	}
	return TRUE;
}


/*
 * caenV265Shutdown()
 * turns off interrupts so that dont foul up the boot
 */
LOCAL int caenV265Shutdown(void)
{
	struct caenV265 *pCaenV265;
	unsigned 	card;

	for(card=0; card<CAIN_V265_MAX_CARD_COUNT; card++){
		pCaenV265 = caenV265Info[card].pCaenV265;
		if(!pCaenV265){
			continue;
		}

		if(caenV265IdTest(pCaenV265)){
			/*
			 * disable interrupts
			 */
			pCaenV265->csr=0;
		}
	}
	return OK;
}


/*
 * caenV265Test()
 */
int caenV265Test(unsigned card)
{
	unsigned		dacVal;
	struct caenV265Config 	cofigCpy;
	unsigned		range;
	unsigned		signal;


	dacVal=0;
	caenV265TestVal(card, dacVal);
	while(dacVal<FD_DATA_M){ 

		cofigCpy = caenV265Info[card];
		dacVal = dacVal+32;
		caenV265TestVal(card, dacVal);

		for(	range=0;
			range<NUMBER_OF_ADC_RANGES;
			range++){
			char *pRangeName[] = {	"12 bit signal",
						"15 bit signal"};

			printf(	"\t%s with DAC = 0x%X\n", 
				pRangeName[range],
				dacVal);

			for(	signal=0; 
				signal<NUMBER_OF_SIGNALS; 
				signal++){
				unsigned newdata;
				unsigned olddata;
		
				olddata = cofigCpy.chan[signal][range].signal;
				newdata = caenV265Info[card].
						chan[signal][range].signal;

				printf(	"\t\tchan=0x%1X diff = 0x%03X\n",
					signal,
					newdata-olddata);
			}
		}
	}	
	return OK;
}


/*
 * caenV265TestVal()
 */
LOCAL int caenV265TestVal(unsigned card, unsigned dacVal)
{
	struct caenV265Config	*pCaenV256Config = &caenV265Info[card];
	struct caenV265 	*pCaenV265 = pCaenV256Config->pCaenV265;
	unsigned		signal;

	if(!pCaenV265){
		return ERROR;
	}

	if(!caenV265IdTest(pCaenV265)){
		return ERROR;
	}

	/*
	 * clear the module
	 */
	pCaenV265->clear=0;

	/*
	 * generate a test signal 
	 */
	pCaenV265->DAC=dacVal;

	/*
	 * generate a test gate
	 */
	for(	signal=0; 
		signal<NUMBER_OF_SIGNALS; 
		signal++){
		caenV265Info[card].chan[signal][adc_12].newData=FALSE;
		caenV265Info[card].chan[signal][adc_15].newData=FALSE;
		while(!caenV265Info[card].chan[signal][adc_15].newData){
			pCaenV265->gate=0;
			taskDelay(1);
		}
		while(!caenV265Info[card].chan[signal][adc_12].newData){
			pCaenV265->gate=0;
			taskDelay(1);
		}
	}	

	/*
	 * turn off test signal 
	 */
	pCaenV265->clear=0;
	pCaenV265->DAC=0;

	return OK;
}


/*
 * caenV265IOReport()
 */
LOCAL long caenV265IOReport(int level)
{
	struct caenV265 *pCaenV265;
	unsigned 	card;
	unsigned	signal;
	unsigned	range;
	char		*pVersion[] = {"NIM","ECL"};
	char		*pState[] = {
				"FIFO empty",
				"FIFO full",
				"FIFO partially filled",
				"FIFO full"};

	for (card=0; card<CAIN_V265_MAX_CARD_COUNT; card++) {
		pCaenV265 = caenV265Info[card].pCaenV265;
		if (!pCaenV265) {
			continue;
		}
		if (!caenV265IdTest(pCaenV265)) {
			continue;
		}
		printf("AI: caen V265:\tcard %d\n", card);
		if (level == 0) {
			continue;
		}

		printf("\tversion = %s\n",
			pVersion[EXTRACT(VERSION, pCaenV265->version)]);
		printf("\tseries = %d\n",
			EXTRACT(SERIES, pCaenV265->version));
		printf("\tstate = %s\n",
			pState[EXTRACT(BUSY_FULL,pCaenV265->csr)]);
		printf("\tint level = %d\n",
			EXTRACT(ILEVEL,pCaenV265->csr));
		printf("\tint vec = 0x%02X\n",
			EXTRACT(IVEC,pCaenV265->csr));
		printf(	"\tbase addr= 0x%X on the %s\n", 
			(unsigned)caenV265Info[card].pCaenV265,
			sysModel());

		for(	range=0;
			range<NUMBER_OF_ADC_RANGES;
			range++){
			char *pRangeName[] = {	"12 bit signal",
						"15 bit signal"};

			printf("\t%s\n", pRangeName[range]);

			for(	signal=0; 
				signal<NUMBER_OF_SIGNALS; 
				signal++){
				int16_t	data;

				data = caenV265Info[card].
						chan[signal][range].signal;

				if(caenV265Info[card].chan[signal][range].newData){
					printf(	"\t\tchan=0x%1X val = 0x%03X\n",
					signal,
					data);
				}
				else{
					printf( "\t\tchan=0x%1X <NO GATE>\n",
					signal);
				}
			}

		}
	}
	return OK;
}
