/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devVXStats.c - Device Support Routines for vxWorks statistics */
/*
 *      Original Author: Jim Kowalkowski
 *      Date:  2/1/96
 */

#include <vxWorks.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <taskLib.h>
#include <wdLib.h>
#include <tickLib.h>
#include <sysLib.h>
#include <memLib.h>          /* Memory Statistics */
#include <private/iosLibP.h> /* fd usage          */

#include <alarm.h>
#include <dbDefs.h>
#include <dbScan.h>
#include <dbAccess.h>
#include <callback.h>
#include <recSup.h>
#include <devSup.h>
#include <link.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <longinRecord.h>

static MEM_PART_STATS meminfo;

/*
	The following device definitions must exist

	device(ai,INST_IO,devAiVXStats,"VX stats")
	device(ao,INST_IO,devAoVXStats,"VX stats")
	device(longin,INST_IO,devLiVXStats,"VX stats")

	--------------------------------------------------------------------
	sample db file to use all data available:
	Notice that the valid values for the parm field of the link
	information are:

	ai:
		memory       	- % memory used
		cpu             - % CPU usage by tasks
		fd	 	- % of file descripters used
		memoryTotal     - Total memory available in Mbytes
		memoryUsed      - Memory used in Mbytes
		memoryFree      - Memory free in Mbytes

	longin:
		fdTotal         - Total number of file descriptors in the system
		fdUsed          - Number of used file descriptors
		fdFree          - Number of free file descriptors

	ao:
		memoryScanPeriod      - Set % memory scan period
		cpuScanPeriod         - Set % cpu scan period
		fdScanPeriod          - Set % fd scan period
		memoryTotalScanPeriod - Set memory total scan period
		memoryUsedScanPeriod  - Set memory used  scan period
		memoryFreeScanPeriod  - Set memory free  scan period
		fdTotalScanPeriod     - Set fd total scan period
		fdUsedScanPeriod      - Set fd used scan period
		fdFreeScanPeriod      - Set fd free scan period


	# Sample database, containing 18 records, demonstrating the use of all the
        # different types of statistics
        # Note that HOPR, LOPR and PREC are set automatically within the device support

	record(ai,"$(IOC):memoryUsedPercent") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@memory")
	    field(HIHI,"90.0")
	    field(HIGH,"60.0")
	    field(HHSV,"MAJOR")
	    field(HSV,"MINOR")
	}
	record(ai,"$(IOC):memoryTotal") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@memoryTotal")
	}
	record(ai,"$(IOC):memoryUsed") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@memoryUsed")
	}
	record(ai,"$(IOC):memoryFree") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@memoryFree")
	}
	record(ai,"$(IOC):fdUsedPercent") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@fd")
	    field(HIHI,"90.0")
	    field(HIGH,"60.0")
	    field(HHSV,"MAJOR")
	    field(HSV,"MINOR")
	}
	record(longin,"$(IOC):fdTotal") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@fdTotal")
	}
	record(longin,"$(IOC):fdUsed") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@fdUsed")
	}
	record(longin,"$(IOC):fdFree") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@fdFree")
	}
	record(ai,"$(IOC):cpuUsedPercent") {
	    field(SCAN,"I/O Intr")
	    field(DTYP,"VX stats")
	    field(INP,"@cpu")
	    field(HIHI,"90.0")
	    field(HIGH,"60.0")
	    field(HHSV,"MAJOR")
	    field(HSV,"MINOR")
	}
	record(ao,"$(IOC):memoryScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@memoryScanPeriod")
	}
	record(ao,"$(IOC):memoryTotalScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@memoryTotalScanPeriod")
	}
	record(ao,"$(IOC):memoryUsedScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@memoryUsedScanPeriod")
	}
	record(ao,"$(IOC):memoryFreeScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@memoryFreeScanPeriod")
	}
	record(ao,"$(IOC):fdScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@fdScanPeriod")
	}
	record(ao,"$(IOC):fdTotalScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@fdTotalScanPeriod")
	}
	record(ao,"$(IOC):fdUsedScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@fdUsedScanPeriod")
	}
	record(ao,"$(IOC):fdFreeScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@fdFreeScanPeriod")
	}
	record(ao,"$(IOC):cpuScanPeriod") {
	    field(DTYP,"VX stats")
	    field(OUT,"@cpuScanPeriod")
	}
*/

typedef struct aiaodset
{
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
}aiaodset;

typedef struct lilodset
{
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_write;
	DEVSUPFUN	special_linconv;
}lilodset;

typedef struct devPvt
{
	int type;
}devPvt;

static long ai_init(int pass);
static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt);
static long ai_init_record(aiRecord*);
static long ao_init_record(aoRecord* pr);
static long ai_read(aiRecord*);
static long ao_write(aoRecord*);

static long longin_init(int pass);
static long longin_ioint_info(int cmd,longinRecord *pr,IOSCANPVT *iopvt);
static long longin_init_record(longinRecord*);
static long longin_read(longinRecord*);

aiaodset devAiVXStats={ 6,NULL,ai_init,ai_init_record,ai_ioint_info,ai_read,NULL };
aiaodset devAoVXStats={ 6,NULL,NULL,ao_init_record,NULL,ao_write,NULL };
lilodset devLiVXStats={ 6,NULL,longin_init,longin_init_record,longin_ioint_info,longin_read,NULL };

#define MEMORY_TYPE	0
#define CPU_TYPE	1
#define FD_TYPE		2
#define MEMORY_USED     3
#define MEMORY_FREE     4
#define MEMORY_TOTAL    5

#define TOTAL_AI_TYPES  6
#define LI_START        6

#define FD_USED         6
#define FD_FREE         7
#define FD_TOTAL        8
#define TOTAL_TYPES	9

/* default rate in seconds */
/* (memory,cpu,fd,memoryUsed,memoryFree,memoryTotal,fdUsed,fdFree,fdTotal) */
static int default_scan_rate[] = { 10,50,10,10,10,10,10,10,10 };

static char *parmValue[TOTAL_TYPES] = {
        "memory",     "cpu",        "fd",
        "memoryUsed", "memoryFree", "memoryTotal",
        "fdUsed",     "fdFree",     "fdTotal"};
static char *aoparmValue[TOTAL_TYPES] = {
	"memoryScanPeriod",     "cpuScanPeriod",        "fdScanPeriod",
	"memoryUsedScanPeriod", "memoryFreeScanPeriod", "memoryTotalScanPeriod",
	"fdUsedScanPeriod",     "fdFreeScanPeriod",     "fdTotalScanPeriod"};

typedef struct scanInfo
{
	IOSCANPVT ioscanpvt;
	WDOG_ID wd;
	int 	rate_tick;	/* ticks */
}scanInfo;
static scanInfo scan[TOTAL_TYPES];

static void wdCallback(int type);
static double getMemory(void);
static double getFd(void);

#define SECONDS_TO_STARVATION 60
#define SECONDS_TO_BURN 5
#define SECONDS_TO_WAIT 15

#define TASK_PRIO_NOCONTENTION 5

typedef struct cpuUsage {
    SEM_ID        lock;
    unsigned long ticksToDeclareStarvation;
    unsigned long ticksToBurn;
    unsigned long ticksToWait;
    unsigned long nBurnNoContention;
    unsigned long ticksLastUpdate;
    double        usage;
} cpuUsage;
static cpuUsage usage;

static void wdCpu(void);
static double cpuBurn();
static void cpuUsageTask();
static double getCpu(void);
static void cpuUsageInit(void);

static long ai_init(int pass)
{
    int type;

    if(pass==1) {
        for(type=0;type<TOTAL_AI_TYPES;type++) {
          if(type==CPU_TYPE) {
	      wdStart(scan[type].wd,scan[type].rate_tick,
                  (FUNCPTR)wdCpu,0);
          } else {
	      wdStart(scan[type].wd,scan[type].rate_tick,
                  (FUNCPTR)wdCallback,type);
          }
        }
	return(0);
    }

    for(type=0;type<TOTAL_AI_TYPES;type++) {
	scanIoInit(&scan[type].ioscanpvt);
	scan[type].wd=wdCreate();
	scan[type].rate_tick=default_scan_rate[type]*sysClkRateGet();
    }
    cpuUsageInit();
    return 0;
}

static long longin_init(int pass)
{
    int type;

    if(pass==1) {
        for(type=LI_START;type<TOTAL_TYPES;type++) {
	      wdStart(scan[type].wd,scan[type].rate_tick,
                  (FUNCPTR)wdCallback,type);
        }
	return(0);
    }

    for(type=LI_START;type<TOTAL_TYPES;type++) {
	scanIoInit(&scan[type].ioscanpvt);
	scan[type].wd=wdCreate();
	scan[type].rate_tick=default_scan_rate[type]*sysClkRateGet();
    }
    return 0;
}

static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt)
{
    devPvt* pvt=(devPvt*)pr->dpvt;

    *iopvt=scan[pvt->type].ioscanpvt;
    return 0;
}

static long longin_ioint_info(int cmd, longinRecord *pr, IOSCANPVT *iopvt)
{
    devPvt* pvt=(devPvt*)pr->dpvt;

    *iopvt=scan[pvt->type].ioscanpvt;
    return 0;
}

static long ai_init_record(aiRecord* pr)
{
    int     type;
    devPvt* pvt = NULL;
    char   *parm;

    if(pr->inp.type!=INST_IO) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devAiStats (init_record) Illegal INP field");
	return S_db_badField;
    }
    parm = pr->inp.value.instio.string;
    for(type=0; type < TOTAL_AI_TYPES; type++) {
	if(strcmp(parm,parmValue[type])==0) {
		pvt=(devPvt*)malloc(sizeof(devPvt));
		pvt->type=type;
                if( type <= FD_TYPE )
                  pr->hopr = 100.0;
                else
                {
                  memPartInfoGet(memSysPartId,&(meminfo));
                  pr->hopr = (meminfo.numBytesFree + meminfo.numBytesAlloc)/(double)(1024*1024);
                }
	}
    }
    if(pvt==NULL) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devAiStats (init_record) Illegal INP parm field");
	return S_db_badField;
    }
    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    pr->lopr = 0.0;
    pr->prec = 2;
    return 0;
}

static long longin_init_record( longinRecord *pr )
{
    int     type;
    devPvt* pvt = NULL;
    char   *parm;

    if(pr->inp.type!=INST_IO) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devLonginStats (init_record) Illegal INP field");
	return S_db_badField;
    }
    parm = pr->inp.value.instio.string;
    for(type=LI_START; type < TOTAL_TYPES; type++) {
	if(strcmp(parm,parmValue[type])==0) {
		pvt=(devPvt*)malloc(sizeof(devPvt));
		pvt->type=type;
	}
    }
    if(pvt==NULL) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devLonginStats (init_record) Illegal INP parm field");
	return S_db_badField;
    }
    /* Make sure record processing routine does not perform any conversion*/
    pr->dpvt = pvt;
    pr->lopr = 0.0;
    pr->hopr = maxFiles;
    return 0;
}

static long ao_init_record(aoRecord* pr)
{
    int type;
    devPvt* pvt = NULL;
    char   *parm;

    if(pr->out.type!=INST_IO) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devAoStats (init_record) Illegal OUT field");
	return S_db_badField;
    }
    parm = pr->out.value.instio.string;
    for(type=0; type < TOTAL_TYPES; type++) {
	if(strcmp(parm,aoparmValue[type])==0) {
		pvt=(devPvt*)malloc(sizeof(devPvt));
		pvt->type=type;
	}
	pr->val = default_scan_rate[type];
    }
    if(pvt==NULL) {
	recGblRecordError(S_db_badField,(void*)pr,
		"devAoStats (init_record) Illegal OUT parm field");
	return S_db_badField;
    }
    /* Make sure record processing routine does not perform any conversion*/
    pr->linr=0;
    pr->dpvt=pvt;
    return 2;
}

static long ao_write(aoRecord* pr)
{
    devPvt* pvt=(devPvt*)pr->dpvt;

    if(!pvt) return(0);
    if(pr->val<=1.0) pr->val=1.0;
    scan[pvt->type].rate_tick = (int)pr->val * sysClkRateGet();
    return 0;
}


static long ai_read(aiRecord* pr)
{
    double value = 0.0;
    devPvt* pvt=(devPvt*)pr->dpvt;

    if(!pvt) return(0);
    switch(pvt->type) {

        case MEMORY_TYPE: 
          value = getMemory();
          break;

        case CPU_TYPE:
          value = getCpu();
          break;

        case FD_TYPE:
          value = getFd();
          break;

        case MEMORY_USED:
          memPartInfoGet(memSysPartId,&(meminfo));
          value = meminfo.numBytesAlloc/(double)(1024*1024);
          break;

        case MEMORY_FREE:
          memPartInfoGet(memSysPartId,&(meminfo));
          value = meminfo.numBytesFree/(double)(1024*1024);
          break;

        case MEMORY_TOTAL:
          memPartInfoGet(memSysPartId,&(meminfo));
          value = (meminfo.numBytesFree + meminfo.numBytesAlloc)/(double)(1024*1024);
          break;

        default: 
          recGblRecordError(S_db_badField,(void*)pr,"Illegal type");
          break;
    }
    pr->val=value;
    pr->udf=0;
    return 2; /* don't convert */
}

static long longin_read( longinRecord *pr )
{
    devPvt *pvt;
    int    i;
    long   value;
    long   tot;

    pvt   = (devPvt*)pr->dpvt;
    value = 0;

    if(!pvt) return(0);

    switch(pvt->type) {

        case FD_USED:
          for(tot=0,i=0;i<maxFiles;i++) 
          {
            if(fdTable[i].inuse)
              tot++;
          }
          value = tot;
          break;

        case FD_FREE:
          for(tot=0,i=0;i<maxFiles;i++) 
          {
            if( !(fdTable[i].inuse) )
              tot++;
          }
          value = tot;
          break;

        case FD_TOTAL:
          value = maxFiles;
          break;

        default:
          recGblRecordError(S_db_badField,(void*)pr,"Illegal type");
          break;
    }
    pr->val=value;
    pr->udf=0;
    return 2; /* don't convert */
}

static void wdCallback(int type)
{
    scanIoRequest(scan[type].ioscanpvt);
    wdStart(scan[type].wd,scan[type].rate_tick, (FUNCPTR)wdCallback,type);
}

static double getMemory(void)
{
    double nfree,nalloc;

    memPartInfoGet(memSysPartId,&(meminfo));
    nfree = meminfo.numBytesFree;
    nalloc = meminfo.numBytesAlloc;
    return(100.0 * nalloc/(nalloc + nfree));
}

static double getFd()
{
    int i,tot;

    for(tot=0,i=0;i<maxFiles;i++)  {
		if(fdTable[i].inuse)
		tot++;
    }
    return(100.0*(double)tot/(double)maxFiles);
}

static void wdCpu(void)
{
    unsigned long ticksLastUpdate = usage.ticksLastUpdate;
    unsigned long ticksNow = tickGet();

    if(ticksNow>=ticksLastUpdate /*ignore tick overflow*/
    && (ticksNow-ticksLastUpdate) >= usage.ticksToDeclareStarvation) {
        usage.usage = 100.0;
        scanIoRequest(scan[CPU_TYPE].ioscanpvt);
    }
    wdStart(scan[CPU_TYPE].wd,scan[CPU_TYPE].rate_tick,(FUNCPTR)wdCpu,0);
}

static double cpuBurn()
{
    int i;
    double result = 0.0;

    for(i=0;i<5; i++) result += sqrt((double)i);
    return(result);
}

static void cpuUsageTask()
{

    while(TRUE) {
	unsigned long tickStart,tickEnd=0;
        unsigned long nBurnNow;
        unsigned long nBurnNoContention = usage.nBurnNoContention;
	double newusage;

	nBurnNow=0;
	tickStart = tickGet();
        while(1) {
	    cpuBurn();
	    ++nBurnNow;
            tickEnd = tickGet();
            if(tickEnd<tickStart) break; /*allow for tick overflow*/
            if((tickEnd - tickStart) >= usage.ticksToBurn) break;
	}
        if(tickEnd<tickStart) continue; /*allow for tick overflow*/
        if (nBurnNow > nBurnNoContention) nBurnNoContention = nBurnNow;
        newusage = (100.0*(nBurnNoContention - nBurnNow))/nBurnNoContention;
        semTake(usage.lock,WAIT_FOREVER);
	usage.usage = newusage;
        semGive(usage.lock);
        taskDelay(usage.ticksToWait);
        usage.ticksLastUpdate = tickEnd;
        scanIoRequest(scan[CPU_TYPE].ioscanpvt);
    }
}

static double getCpu()
{
    double value;
    semTake(usage.lock,WAIT_FOREVER);
    value = usage.usage;
    semGive(usage.lock);
    return(value);
}

static void cpuUsageInit(void)
{
    unsigned long tickStart,tickEnd=0,tickNow;
    unsigned long nBurnNoContention=0;
    int i, prio, tid=taskIdSelf();

    usage.lock =
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
    usage.usage = 0.0;
    usage.ticksToDeclareStarvation = SECONDS_TO_STARVATION * sysClkRateGet();
    usage.ticksToWait = SECONDS_TO_WAIT*sysClkRateGet();
    usage.ticksToBurn = 1*sysClkRateGet(); /* measure in 1 second spurts */
    taskPriorityGet(tid, &prio);
    taskPrioritySet(tid, TASK_PRIO_NOCONTENTION);
    for (i=0; i<SECONDS_TO_BURN; i++) {
        tickStart = tickGet();
        /*wait for a tick*/
        while (tickStart==(tickNow = tickGet())) {;}
        tickStart = tickNow;
        while (TRUE) {
	    cpuBurn();
	    nBurnNoContention++;
            tickEnd = tickGet();
            if (tickEnd<tickStart) break; /*same test as cpuUsageTask*/
            if ((tickEnd - tickStart) >= usage.ticksToBurn) break;
        }
        if (tickEnd<tickStart)
            epicsPrintf("cpuUsageInit: clock rollover during calibration\n");
        taskDelay(1); /* allow OS tasks a chance to do some work */
    }
    taskPrioritySet(tid, prio); /* original priority */
    usage.nBurnNoContention = nBurnNoContention;
    usage.ticksToBurn = SECONDS_TO_BURN*sysClkRateGet();
    usage.ticksLastUpdate = tickGet();
    taskSpawn("cpuUsageTask",255,VX_FP_TASK,3000,(FUNCPTR)cpuUsageTask,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
