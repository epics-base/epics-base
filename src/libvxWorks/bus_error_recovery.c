
/*
	Special case bus error recovery code.

	The code in this file will attempt to retry instructions which caused
	bus errors up to 10 times before invoking the standard vxWorks bus
	error handler.  The code also records the last 20 bus errors so they
	can be reported using a utility run from the vxWorks console.  Database
	records can be used to monitor when bus errors occur - See support
	below.

	The bus error handler only processes bus errors outside the processor's
	local memory which are normal user/supervisor data accesses.

	------------------
	epicsBusErrorPrint - Print the last 20 bus errors that occurred.  The
	 report includes:
		1) The bus error number
		2) FA - The fault address
		3) PC - The program counter when the fault occurred
		4) SR - Status register
		5) SSW - Special status word

	-------------
	devAiBusError - Simple EPICS ai record device support.  Create an
	 ai record with DTYP="Bus Error" and SCAN="I/O Intr" to get
	 informed when a bus error occurs.  You may not get informed if the
	 bus error is handled by vxWorks and an important EPICS task is
	 suspended as a result.  You will always be notified of a bus
	 error that has been corrected by retries.  The ai record will
	 count up every time if is processed.  The bus error handler triggers
	 processing of the record if the SCAN type is "I/O Intr".
	 
	If a bus error is handled by vxWorks and you do not get notified via
	the ai record, you can sign on the console and run epicsBusErrorPrint
	and see the last 20 bus errors that occurred.

	 Add the following line to the cat_ascii/devSup.ascii file:
	        "ai"    VME_IO "devAiBusError" "Bus Error"

	----------------------
	epicsBusErrorInit68040
	 Initialize the bus error handling system.  ONLY needs to be run if
	 a bus error monitoring record is NOT present in the database.  The
	 ai record device support above runs with function automatically.  You
	 need to run this function before iocInit in your vxWorks startup
	 script if you do not use the ai record support described above and
	 have a record for it in the database.

*/

#include <stdio.h>
#include <types.h>
#include <vme.h>
#include <vxWorks.h>
#include <sysLib.h>
#include <taskLib.h>
#include <semLib.h>
#include <intLib.h>

#include <dbScan.h>
#include <devSup.h>
#include <callback.h>
#include <aiRecord.h>

struct accessFault
{
	unsigned short sr;		/* status reg */
	unsigned short* pc;		/* program counter */
	unsigned short vo;		/* vector offset */
	unsigned long ea;		/* effective address */
	unsigned short ssw;		/* special status word */
	unsigned short wb3s;	/* write-back 3 status */
	unsigned short wb2s;	/* write-back 2 status */
	unsigned short wb1s;	/* write-back 1 status */
	unsigned char * fa;		/* fault address */
	union {
		unsigned long* l; unsigned short* s; unsigned char * c;
	} wb3a;			/* write-back 3 address */
	union {
		unsigned long l; unsigned short s; unsigned char  c;
	} wb3d;			/* write-back 3 */
	union {
		unsigned long* l; unsigned short* s; unsigned char * c;
	} wb2a;			/* write-back 2 address */
	union {
		unsigned long l; unsigned short s; unsigned char  c;
	} wb2d;			/* write-back 2 */
	union {
		unsigned long* l; unsigned short* s; unsigned char * c;
	} wb1a;			/* write-back 1 address */
	union {
		unsigned long l; unsigned short s; unsigned char  c;
	} wb1d;			/* write-back 1 / push data LW0 */
	unsigned long pd1;		/* push data LW1 */
	unsigned long pd2;		/* push data LW2 */
	unsigned long pd3;		/* push data LW3 */
};
typedef struct accessFault accessFault;

struct faultData
{
	unsigned short* pc;
	unsigned char * fa;
	unsigned short sr;
	unsigned short ssw;
	unsigned char  flags; /* 0x01:write back incompleted, >0: valid bus error */
	long cnt;
};
typedef struct faultData faultData;

#define TOTAL_FAULTS 20
#define ACCESS_FAULT 2
#define BS_PRI 199
#define NUM_RETRIES 10

long epicsBusErrorInit68040(void);
long epicsBusErrorHandler(void* v);
void epicsBusErrorPrint(void);
void epicsBusError(void);

volatile long epicsBusErrorTotal=0;
volatile long epicsBusErrorLastRC=0;
static volatile faultData* fault_table=NULL;
static volatile int pos_handler=0;
static unsigned char * be_mem_top=NULL;
static unsigned char * be_mem_bottom=0;
static FUNCPTR* vbr;
static FUNCPTR vx_access_fault;
static IOSCANPVT ioscan;
static unsigned short* curr_pc=NULL;
static unsigned short* last_pc=NULL;
static unsigned short count=0;
static unsigned long regs[64];
static unsigned long* regs_addr=regs;
static int init_run=0;

FUNCPTR* epicsGetVBR()
{
	unsigned long x;
	asm ("movec vbr,%0" : "=g" (x) );
	return (FUNCPTR*)x;
}

/* ------------------- ai record to monitor bus errors -------------------- */
struct aStats
{
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
};
typedef struct aStats aStats;

static long aiInit(int pass)
{
	if(pass) return 0;
	if(init_run==0)
	{
		epicsBusErrorInit68040();
		init_run=1;
	}
	return 0;
}

static long aiInitRecord(aiRecord* pr)
{
	unsigned long* x = (unsigned long*)&pr->dpvt;
	x=0;
	pr->linr=0;
	return 0;
}

static long aiIointInfo(int cmd,aiRecord* pr,IOSCANPVT* iopvt)
{
	*iopvt=ioscan;
	return 0;
}

static long aiRead(aiRecord* pr)
{
	unsigned long* x = (unsigned long*)&pr->dpvt;
	pr->val=++(*x);
	pr->udf=0;
	return 2;
}

aStats devAiBusError=
	{ 6,NULL,aiInit,aiInitRecord,aiIointInfo,aiRead,NULL };

/* ------------------- ai record to monitor bus errors -------------------- */

long epicsBusErrorInit68040(void)
{
	long rc=0;
	int i;

	if(init_run)
		return 0;
	else
		init_run=1;

	vbr=epicsGetVBR(); /* vbr=intVecBaseGet(); */
	be_mem_top=(unsigned char *)sysMemTop();
	vx_access_fault=vbr[ACCESS_FAULT];
	scanIoInit(&ioscan);

/*
	printf("VBR=0x%8.8x\n",vbr);
	printf("MemTop=0x%8.8x\n",be_mem_top);
	printf("vxWorks Access Fault Handler=0x%8.8x\n",vx_access_fault);
*/

	/* up to TOTAL_FAULTS number of interrupts can occur before printing */
	fault_table=(volatile faultData*)malloc(sizeof(faultData)*TOTAL_FAULTS);
	for(i=0;i<TOTAL_FAULTS;i++) fault_table[i].flags=0x00;

	/* replace the bus error handler from vxWorks */
	vbr[ACCESS_FAULT]=(FUNCPTR)epicsBusError;

	return rc;
}

void epicsBusErrorPrint(void)
{
	volatile int pos;
	int i;

	for(i=0;i<TOTAL_FAULTS;i++)
	{
		if(fault_table[i].flags)
		{
			printf("%ld: FA=0x%8.8x PC=0x%8.8x SR=0x%4.4x, SSW=0x%4.4x ",
				fault_table[i].cnt,
				fault_table[i].fa,
				fault_table[i].pc,
				fault_table[i].sr,
				fault_table[i].ssw);
			switch(fault_table[i].flags)
			{
				case 0x01: /* write back failed */
					printf(" Write back incomplete\n");
					break;
				case 0x02: /* normal retry */
					printf(" Normal Retry\n");
					break;
				case 0x04: /* vxWorks process */
					printf(" vxWorks Handled\n");
					break;
			}
		}
	}
}

/* sp+60+sizeof(access fault stack frame) ---> 60+60 */

asm("	.text");
asm("	.even");
asm("	.globl _epicsBusError");
asm("_epicsBusError:");
asm("	moveml	#0xfffe,sp@-"); /* save the registers */
asm("	pea		sp@(60)");		/* get address of access error stack frame */
asm("	jbsr	_epicsBusErrorHandler"); /* call my bus error handler */
asm("	addql	#4,sp");
asm("	tstl	d0"); /* check return code from bus error handler */
asm("	jeq		L1"); /* exit OK if zero return code */
asm("	moveml	sp@+,#0x7fff"); /* restore regs */
asm("	movel	_vx_access_fault,sp@-"); /* put vxWorks handler addr on stack */
asm("	rts		");	/* load PC from stack (vxWorks handler) */
asm("L1:");
asm("	moveml	sp@+,#0x7fff"); /* restore regs */
asm("	rte");

/*
 see section 8.4.6 of M68040 user's manual, page 8-24
 see section 5.3, page 5-5 for Transfer attribute signals (TT/TM)
 SIZ: 00=long word, 01=byte, 10=word
 TT:  00=normal, 01=move16, 10=alternate, 11=ack cycle
 TM:  001=user data, 010=user code, 101=sup data, 110=sup code
 R/W: set indicates a read access
*/ 

long epicsBusErrorHandler(void* v)
{
	long rc;
	unsigned char f;
	volatile int pos,posp;
	volatile accessFault* af = (volatile accessFault*)v;
	unsigned short size,op_code;
	unsigned short* ppc;
	int len;

	curr_pc=af->pc;
	if(curr_pc==last_pc) ++count;

	if(	count<NUM_RETRIES &&
		((af->ssw & 0x0018) == 0) && ((af->ssw & 0xfc00) == 0) &&
		((af->ssw & 0x0001) || (af->ssw & 0x0005)) &&
		(af->fa >= be_mem_top) )
	{
		/* TT=normal TM=user/sup data access bus-errors */
		rc=0;
		f=0x02;

		/*
		Don't force PC to next instruction, just retry several times
		Note: the current instruction may not have caused the fault
		*/

		/* correct bus error here */
		if(af->wb1s & 0x0080)
		{
			/* this is where the fault occured if write */
			if(af->wb1a.c < be_mem_top)
			{
				size=(af->wb1s & 0060)>>5;
				if(size==0x0000) *(af->wb1a.l)=af->wb1d.l;
				else if(size==0x0001) *(af->wb1a.c)=af->wb1d.c;
				else if(size==0x0010) *(af->wb1a.s)=af->wb1d.s;
			}
		}
		if(af->wb2s & 0x0080)
		{
			if(af->wb2a.c < be_mem_top)
			{
				size=(af->wb2s & 0060)>>5;
				if(size==0x0000) *(af->wb2a.l)=af->wb2d.l;
				else if(size==0x0001) *(af->wb2a.c)=af->wb2d.c;
				else if(size==0x0010) *(af->wb2a.s)=af->wb2d.s;
			}
			else
				f|=0x01;
		}
		if(af->wb3s & 0x0080)
		{
			if(af->wb3a.c < be_mem_top)
			{
				size=(af->wb3s & 0060)>>5;
				if(size==0x0000) *(af->wb3a.l)=af->wb3d.l;
				else if(size==0x0001) *(af->wb3a.c)=af->wb3d.c;
				else if(size==0x0010) *(af->wb3a.s)=af->wb3d.s;
			}
			else
				f|=0x01;
		}
	}
	else
	{
		f=0x04;
		rc=-1; /* let vxWorks handle it */
	}

	if(count==0 || count>=NUM_RETRIES)
	{
		if((++pos_handler)>=TOTAL_FAULTS) pos_handler=0;
		fault_table[pos_handler].pc=af->pc;
		fault_table[pos_handler].fa=af->fa;
		fault_table[pos_handler].sr=af->sr;
		fault_table[pos_handler].ssw=af->ssw;
		fault_table[pos_handler].cnt=epicsBusErrorTotal;
		fault_table[pos_handler].flags=f;

		if(count>=NUM_RETRIES)
		{
			count=0;
			curr_pc=NULL;
		}
		else
			scanIoRequest(ioscan);

		++epicsBusErrorTotal;
	}

	epicsBusErrorLastRC=rc;
	last_pc=curr_pc;
	return rc;
}

