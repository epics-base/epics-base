/* devVXStats.c - Device Support Routines for vxWorks statistics */
/*
 *      Original Author: Jim Kowalkowski
 *      Date:  2/1/96
 *	Current Author: Marty Kraimer
 *	Date: 18NOV1997
 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

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

/*
	The following device definitions must exist

	device(ai,INST_IO,devAiVXStats,"VX stats")
	device(ao,INST_IO,devAoVXStats,"VX stats")

	--------------------------------------------------------------------
	sample db file to use all data available:
	Notive that the valid values for the parm field of the link
	information are:

	ai:
		memory       	- % memory used
		cpu             - % CPU usage by tasks
		fd	 	- % of file descripters used
	ao:
		memoryScanPeriod   - Set memory scan period
		cpuScanPeriod      - Set cpu scan period
		fdScanPeriod       - Set fd scan period


	# sample database using all the different types of statistics
	record(ai,"$(IOC):memory")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@memory")
		field(HOPR,"100.0")
		field(HIHI,"90.0")
		field(HIGH,"60.0")
		field(HHSV, "MAJOR")
		field(HSV, "MINOR")
	}
	record(ai,"$(IOC):cpu")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@cpu")
		field(HOPR,"100.0")
		field(HIHI,"90.0")
		field(HIGH,"60.0")
		field(HHSV, "MAJOR")
		field(HSV, "MINOR")
	}
	record(ai,"$(IOC):fd")
	{
		field(DTYP,"VX stats")
		field(SCAN,"I/O Intr")
		field(INP,"@fd")
		field(HOPR,"100.0")
		field(HIHI,"90.0")
		field(HIGH,"60.0")
		field(HHSV, "MAJOR")
		field(HSV, "MINOR")
	}
	record(ao,"$(IOC):memoryScanPeriod")
	{
		field(DTYP,"VX stats")
		field(OUT,"@memoryScanPeriod")
	}
	record(ao,"$(IOC):fdScanPeriod")
	{
		field(DTYP,"VX stats")
		field(OUT,"@fdScanPeriod")
	}
	record(ao,"$(IOC):cpuScanPeriod")
	{
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

aiaodset devAiVXStats={ 6,NULL,ai_init,ai_init_record,ai_ioint_info,ai_read,NULL };
aiaodset devAoVXStats={ 6,NULL,NULL,ao_init_record,NULL,ao_write,NULL };

#define MEMORY_TYPE	0
#define CPU_TYPE	1
#define FD_TYPE		2
#define TOTAL_TYPES	3

/* default rate in seconds (memory,cpu,fd) */
static int default_scan_rate[] = { 10,50,10 };

static char *aiparmValue[TOTAL_TYPES] = {"memory","cpu","fd"};
static char *aoparmValue[TOTAL_TYPES] = {
	"memoryScanPeriod","cpuScanPeriod","fdScanPeriod"};

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

typedef struct cpuUsage {
    SEM_ID        lock;
    unsigned long ticksToDeclareStarvation;
    unsigned long ticksToBurn;
    unsigned long ticksToWait;
    double        nBurnNoContention;
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
        for(type=0;type<TOTAL_TYPES;type++) {
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
    for(type=0;type<TOTAL_TYPES;type++) {
	scanIoInit(&scan[type].ioscanpvt);
	scan[type].wd=wdCreate();
	scan[type].rate_tick=default_scan_rate[type]*sysClkRateGet();
    }
    cpuUsageInit();
    return 0;
}

static long ai_ioint_info(int cmd,aiRecord* pr,IOSCANPVT* iopvt)
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
    for(type=0; type < TOTAL_TYPES; type++) {
	if(strcmp(parm,aiparmValue[type])==0) {
		pvt=(devPvt*)malloc(sizeof(devPvt));
		pvt->type=type;
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
    pr->hopr = 100.0;
    pr->lopr = 0.0;
    pr->prec = 2;
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
        case MEMORY_TYPE: value = getMemory(); break;
        case CPU_TYPE: value = getCpu(); break;
        case FD_TYPE: value = getFd(); break;
        default: recGblRecordError(S_db_badField,(void*)pr,"Illegal type");
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

/*Memory statistics*/
#include <memLib.h>
static MEM_PART_STATS meminfo;

static double getMemory(void)
{
    double nfree,nalloc;

    memPartInfoGet(memSysPartId,&(meminfo));
    nfree = meminfo.numBytesFree;
    nalloc = meminfo.numBytesAlloc;
    return(100.0 * nalloc/(nalloc + nfree));
}


/* fd usage */
#include <private/iosLibP.h>
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
        int nBurnNow;
        double nBurnNoContention = (double)usage.nBurnNoContention;
	double nBurnContention,newusage;

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
        nBurnContention = (double)nBurnNow;
        newusage = 100.0*((nBurnNoContention - nBurnNow)/nBurnNoContention);
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
    int           nBurnNoContention=0;

    usage.lock =
        semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY);
    usage.usage = 0.0;
    usage.ticksToDeclareStarvation = SECONDS_TO_STARVATION * sysClkRateGet();
    usage.ticksToWait = SECONDS_TO_WAIT*sysClkRateGet();
    usage.ticksToBurn = SECONDS_TO_BURN*sysClkRateGet();
    tickStart = tickGet();
    /*wait for a tick*/
    while(tickStart==(tickNow = tickGet())) {;}
    tickStart = tickNow;
    while(TRUE) {
	cpuBurn();
	nBurnNoContention++;
        tickEnd = tickGet();
        if(tickEnd<tickStart) break; /*same test as cpuUsageTask*/
        if((tickEnd - tickStart) >= usage.ticksToBurn) break;
    }
    if(tickEnd<tickStart) printf("cpuUsageInit logic error\n");
    usage.nBurnNoContention = (double)nBurnNoContention;
    usage.ticksLastUpdate = tickGet();
    taskSpawn("cpuUsageTask",255,VX_FP_TASK,3000,(FUNCPTR)cpuUsageTask,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}
