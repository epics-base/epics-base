
/* devVX.c - Device Support Routines for vxWorks statistics */
/*
 *      Author: Jim Kowalkowski
 *      Date:  2/1/96
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 *
 * 	$Log$
 * 	Revision 1.2  1997/04/30 18:57:33  mrk
 * 	Fixed most compiler warning messages
 *
 * 	Revision 1.1  1996/10/21 15:30:37  jbk
 * 	Added ai/ao device support for vxWorks statics (memory/load/TCP con)
 *
 */

/*
	add the following to devSup.ascii:

	"ai"  VME_IO    "devAiVXStats"   "VX stats"
	"ao"  VME_IO    "devAoVXStats"   "VX stats"

	--------------------------------------------------------------------
	Add TCP and CA connection information before release.
	Add ability to adjust the rate of everything through ao records

	sample db file to use all data available:
	Notive that the valid values for the parm field of the link
	information are:

	ai:
		free_bytes       - number of bytes in IOC not allocated
		allocated_bytes  - number of bytes allocated
		load             - estimated percent CPU usage by tasks
		tcp_connections  - number of TCP connection currently in use
	ao:
		memoryScanRate   - max rate at which new memory stats can be read
		tcpScanRate      - max rate at which TCP connections can be calculated
		loadScanRate     - max rate at which load can be calulated

		loadSampleRate   - vxWorks spy facility samples-per-second for load
		loadSampleLength - length of time in second that samples for load
						   calculation are taken.

	* scan rates are all in seconds

	default rates:
		10 - memory scan rate
		60 - load scan rate
		20 - tcp scan rate
		15 - sample length
		80 - sample rate

	# sample database using all the different types of statistics
	record(ai,"$(PRE):freeBytes")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"#C0 S0 @free_bytes")
	}
	record(ai,"$(PRE):allocatedBytes")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"#C0 S0 @allocated_bytes")
	}
	record(ai,"$(PRE):load")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"#C0 S0 @load")
		field(PREC,"3")
	}
	record(ai,"$(PRE):tcpConnections")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"#C0 S0 @tcp_connections")
	}
	record(ao,"$(PRE):memoryScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"#C0 S0 @memory_scan_rate")
	}
	record(ao,"$(PRE):tcpScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"#C0 S0 @tcp_scan_rate")
	}
	record(ao,"$(PRE):loadScanRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"#C0 S0 @load_scan_rate")
	}
	record(ao,"$(PRE):loadSampleRate")
	{
		field(DTYP,"VX stats")
		field(OUT,"#C0 S0 @load_sample_rate")
	}
	record(ao,"$(PRE):loadSampleLength")
	{
		field(DTYP,"VX stats")
		field(OUT,"#C0 S0 @load_length")
	}
	
*/

#include <vxWorks.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <memLib.h>
#include <taskLib.h>
#include <wdLib.h>
#include <sysLib.h>
#include <spyLib.h>
#include <time.h>
#include <netinet/tcp.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_var.h>
#ifndef VX52
#include <private/memPartLibP.h>
#include <private/iosLibP.h>
#endif

#include <alarm.h>
#include <cvtTable.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <callback.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <dbCommon.h>

/* length in seconds */
#define HARDCODED_LOAD_LENGTH 15
/* rate in times per second */
#define SPY_SAMPLING_RATE 80

#define MAX_TASK 70
#define WORD_SIZE 2

#define MEMORY_TYPE	0
#define LOAD_TYPE	1
#define TCP_TYPE	2
#define TOTAL_TYPES	3

#define START_ME 0
#define STOP_ME 1

typedef void (*statGetFunc)(unsigned long*,double*);
typedef void (*statPutFunc)(int,unsigned long);
typedef struct vmeio vmeio;

#ifdef VX52
typedef MEM_PART_STATS MEM_PART_STATS_P;
#else
typedef struct
{
	unsigned long numBytesFree, /* Number of Free Bytes in Partition  */
	numBlocksFree,   /* Number of Free Blocks in Partition      */
	maxBlockSizeFree,/* Maximum block size that is free.        */
	numBytesAlloc,   /* Number of Allocated Bytes in Partition  */
	numBlocksAlloc;  /* Number of Allocated Blocks in Partition */
} MEM_PART_STATS_P;
#endif

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

struct pvtArea
{
	int index;
	int type;
};
typedef struct pvtArea pvtArea;

struct mem_info
{
	MEM_PART_STATS_P m_stat;
};
typedef struct mem_info mem_info;

struct spy_info
{
	CALLBACK cb;
	WDOG_ID wd;				/* watch dog to stop spy clock */
	volatile double load_average;	/* last calculated load average */
	volatile int type;		/* start/stop indicator */
	volatile unsigned long length_ticks; /* length of sampling in ticks */
	volatile unsigned long sample_rate;	 /* sampling rate for spy (per sec) */
};
typedef struct spy_info spy_info;

struct validGetParms
{
	char* name;
	statGetFunc func;
	int type;
};
typedef struct validGetParms validGetParms;

struct validPutParms
{
	char* name;
	statPutFunc func;
	int type;
};
typedef struct validPutParms validPutParms;

struct glob_vars
{
	IOSCANPVT ioscan;
	WDOG_ID wd;
	volatile int total;					/* total users connected */
	volatile int on;					/* watch dog on? */
	volatile time_t last_read_sec;		/* last time seconds */
	volatile unsigned long rate_sec;	/* seconds */
	volatile unsigned long rate_tick;	/* ticks */
};
typedef struct glob_vars glob_vars;

static long ai_init(int pass);
static long ai_init_record(aiRecord*);
static long ai_read(aiRecord*);
static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt);

static long ao_init_record(aoRecord* pr);
static long ao_write(aoRecord*);

static void load_time_callback(CALLBACK*);
static void read_mem_stats(void);
static void read_tcp_stats(void);
static void read_load_stats(void);
static void scan_time(int);

static void statsFreeBytes(unsigned long*,double*);
static void statsFreeBlocks(unsigned long*,double*);
static void statsAllocBytes(unsigned long*,double*);
static void statsAllocBlocks(unsigned long*,double*);
static void statsProcessLoad(unsigned long*,double*);
static void statsTcpConnects(unsigned long*,double*);
static void statsRate(int type,unsigned long);
static void statsLoadSampleRate(int type,unsigned long);
static void statsLoadLength(int type,unsigned long);

/* rate in seconds (memory,load,tcp) */
static int scan_rate_sec[] = { 10,60,30 };

static validGetParms statsGetParms[]={
	{ "free_bytes",			statsFreeBytes,		MEMORY_TYPE },
	{ "free_blocks",		statsFreeBlocks,	MEMORY_TYPE },
	{ "allocated_bytes",	statsAllocBytes,	MEMORY_TYPE },
	{ "allocated_blocks",	statsAllocBlocks,	MEMORY_TYPE },
	{ "load",				statsProcessLoad,	LOAD_TYPE },
	{ "tcp_connections",	statsTcpConnects,	TCP_TYPE },
	{ NULL,NULL,0 }
};

static validPutParms statsPutParms[]={
	{ "memory_scan_rate",	statsRate,				MEMORY_TYPE },
	{ "tcp_scan_rate",		statsRate,				TCP_TYPE },
	{ "load_scan_rate",		statsRate,				LOAD_TYPE },
	{ "load_sample_rate",	statsLoadSampleRate,	LOAD_TYPE },
	{ "load_length",		statsLoadLength, 		LOAD_TYPE },
	{ NULL,NULL,0 }
};

aStats devAiVXStats={ 6,NULL,ai_init,ai_init_record,ai_ioint_info,ai_read,NULL };
aStats devAoVXStats={ 6,NULL,NULL,ao_init_record,NULL,ao_write,NULL };

static glob_vars globs[3];
static spy_info spyinfo;
static mem_info meminfo;
static int tcpinfo;

/* ---------------------------------------------------------------------- */

static long ai_init(int pass)
{
	int i;

	if(pass) return 0;

	for(i=0;i<TOTAL_TYPES;i++)
	{
		scanIoInit(&globs[i].ioscan);
		globs[i].wd=wdCreate();
		globs[i].total=0;
		globs[i].on=0;
		globs[i].rate_sec=scan_rate_sec[i];
		globs[i].rate_tick=scan_rate_sec[i]*sysClkRateGet();
		globs[i].last_read_sec=1000000;
	}

	spyinfo.sample_rate=SPY_SAMPLING_RATE;
	spyinfo.length_ticks=HARDCODED_LOAD_LENGTH*sysClkRateGet();
	spyinfo.type=START_ME;
	spyinfo.wd=wdCreate();

	callbackSetCallback(load_time_callback,&(spyinfo.cb));
	callbackSetPriority(priorityMedium,&(spyinfo.cb));
	/* callbackSetUser(&type_thing_a_ma_bob,&(spyinfo.cb)); */

	return 0;
}

static long ai_init_record(aiRecord* pr)
{
	int i;
    vmeio* io = (vmeio*)&(pr->inp.value);
    pvtArea* pvt = NULL;

	if(pr->inp.type!=VME_IO)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP field");
		return S_db_badField;
	}

	for(i=0;statsGetParms[i].name && pvt==NULL;i++)
	{
		if(strcmp(io->parm,statsGetParms[i].name)==0)
		{
			pvt=(pvtArea*)malloc(sizeof(pvtArea));
			pvt->index=i;
			pvt->type=statsGetParms[i].type;
		}
	}

	if(pvt==NULL)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP parm field");
		return S_db_badField;
	}

    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    return 0;
}

static long ao_init_record(aoRecord* pr)
{
	int i;
    vmeio* io = (vmeio*)&(pr->out.value);
    pvtArea* pvt = NULL;

	if(pr->out.type!=VME_IO)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal OUT field");
		return S_db_badField;
	}

	for(i=0;statsPutParms[i].name && pvt==NULL;i++)
	{
		if(strcmp(io->parm,statsPutParms[i].name)==0)
		{
			pvt=(pvtArea*)malloc(sizeof(pvtArea));
			pvt->index=i;
			pvt->type=statsPutParms[i].type;
		}
	}

	if(pvt==NULL)
	{
		recGblRecordError(S_db_badField,(void*)pr,
			"devAiStats (init_record) Illegal INP parm field");
		return S_db_badField;
	}

    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    return 0;
}

static void load_time_callback(CALLBACK* cb)
{
	if(spyinfo.type==START_ME)
	{
		spyClkStart(spyinfo.sample_rate);
		spyinfo.type=STOP_ME;
		wdStart(spyinfo.wd,spyinfo.length_ticks,
			(FUNCPTR)callbackRequest,(int)cb);
	}
	else
	{
		spyClkStop();
		spyinfo.type=START_ME;
		read_load_stats();
		scanIoRequest(globs[LOAD_TYPE].ioscan);
	}
}

static void scan_time(int type)
{
	scanIoRequest(globs[type].ioscan);

	if(type==LOAD_TYPE)
		callbackRequest(&(spyinfo.cb));
	else
		scanIoRequest(globs[type].ioscan);

	if(globs[type].on)
		wdStart(globs[type].wd,globs[type].rate_tick,(FUNCPTR)scan_time,type);
}

static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt)
{
	pvtArea* pvt=(pvtArea*)pr->dpvt;

	if(cmd==0) /* added */
	{
		if(globs[pvt->type].total++ == 0)
		{
			/* start a watchdog */
			wdStart(globs[pvt->type].wd,globs[pvt->type].rate_tick,
				(FUNCPTR)scan_time,pvt->type);
			globs[pvt->type].on=1;
		}
	}
	else /* deleted */
	{
		if(--globs[pvt->type].total == 0)
			globs[pvt->type].on=0; /* stop the watchdog */
	}

	*iopvt=globs[pvt->type].ioscan;
	return 0;
}

static long ao_write(aoRecord* pr)
{
    pvtArea* pvt=(pvtArea*)pr->dpvt;
	statsPutParms[pvt->index].func(pvt->type,(unsigned long)pr->val);
	return 0;
}

static long ai_read(aiRecord* pr)
{
	unsigned long val;
	double aver;
    pvtArea* pvt=(pvtArea*)pr->dpvt;

	statsGetParms[pvt->index].func(&val,&aver);

	if(pvt->type==LOAD_TYPE)
		pr->val=aver;
	else
		pr->val=(double)val;

    pr->udf=0;
    return 2; /* don't convert */
}

/* -------------------------------------------------------------------- */

static void read_mem_stats(void)
{
	time_t nt;
	time(&nt);

	if((nt-globs[MEMORY_TYPE].last_read_sec)>=globs[MEMORY_TYPE].rate_sec)
	{
#ifdef VX52
		if(memPartInfoGet(memSysPartId,&(meminfo.m_stat))==OK)
			globs[MEMORY_TYPE].last_read_sec=nt;
#else
		meminfo.m_stat.numBytesFree=
			2*(memSysPartId->totalWords-memSysPartId->curWordsAllocated);
		meminfo.m_stat.numBytesAlloc=2*memSysPartId->curWordsAllocated;
		meminfo.m_stat.numBlocksFree=0;
		meminfo.m_stat.maxBlockSizeFree=0;
		meminfo.m_stat.numBlocksAlloc=0;
		globs[MEMORY_TYPE].last_read_sec=nt;
#endif
	}
}

static void read_load_stats(void)
{
	WIND_TCB* wt;
	int task_list[MAX_TASK];
	int tot_task,i;
	unsigned long tot_ticks=0;

	if((tot_task=taskIdListGet(task_list,MAX_TASK))==0) return;

	for(i=0;i<tot_task;i++)
	{
		if(wt=taskTcb(task_list[i]))
			tot_ticks+=wt->taskTicks+wt->taskIncTicks;
	}

	spyinfo.load_average=(double)(tot_ticks)/(double)(spyinfo.length_ticks);
}

static void read_tcp_stats(void)
{
	time_t nt;
	int i,tot;
	time(&nt);

	if((nt-globs[TCP_TYPE].last_read_sec)>=globs[TCP_TYPE].rate_sec)
	{
		/* this is bogus */
		/* tcpinfo=tcpstat.tcps_connects-
			(tcpstat.tcps_closed-tcpstat.tcps_connattempt); */

		for(tot=0,i=0;i<maxFiles;i++)
			if(fdTable[i].inuse)
				tot++;

		tcpinfo=tot;
		globs[TCP_TYPE].last_read_sec=nt;
	}
}

/* -------------------------------------------------------------------- */

static void statsFreeBytes(unsigned long* val,double* aver)
{
	read_mem_stats();
	*val=meminfo.m_stat.numBytesFree;
	*aver=0;
}

static void statsFreeBlocks(unsigned long* val,double* aver)
{
	read_mem_stats();
	*val=meminfo.m_stat.numBlocksFree;
	*aver=0;
}

static void statsAllocBytes(unsigned long* val,double* aver)
{
	read_mem_stats();
	*val=meminfo.m_stat.numBytesAlloc;
	*aver=0;
}

static void statsAllocBlocks(unsigned long* val,double* aver)
{
	read_mem_stats();
	*val=meminfo.m_stat.numBlocksAlloc;
	*aver=0;
}

static void statsProcessLoad(unsigned long* val,double* aver)
{
	*aver=spyinfo.load_average*100.0;
	*val=0;
	return;
}

static void statsTcpConnects(unsigned long* val,double* aver)
{
	read_tcp_stats();
	*val=tcpinfo;
	*aver=0;
	return;
}

/* ---------------------------Rate adjustments------------------------- */

static void statsRate(int type,unsigned long sec)
{
	globs[type].rate_sec=sec;
	globs[type].rate_tick=sec*sysClkRateGet();
}

static void statsLoadSampleRate(int type,unsigned long samples_per_sec)
{
	spyinfo.sample_rate=samples_per_sec*sysClkRateGet();
}

static void statsLoadLength(int type,unsigned long sample_length)
{
	spyinfo.length_ticks=sample_length*sysClkRateGet();

	if(globs[LOAD_TYPE].rate_tick<=spyinfo.length_ticks)
		spyinfo.length_ticks=globs[LOAD_TYPE].rate_tick-2;
}

/* ----------test routines----------------- */

int jbk_artificial_load(unsigned long iter,unsigned long tick_delay)
{
	volatile int i;

	if(iter==0)
	{
		printf("Usage: jbk_artificial_load(num_iterations,tick_delay)\n");
		return 0;
	}

	for(i=0;i<iter;i++)
	{
		if(i%5==0) taskDelay(tick_delay);
	}

	return 0;
}
