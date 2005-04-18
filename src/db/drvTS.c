/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*drvTs.c*/

/**************************************************************************
 *
 *     Author:	Jim Kowalkowski
 *
 ***********************************************************************/

#define MAKE_DEBUG TSdriverDebug
#define TS_DRIVER 1

#include <vxWorks.h>
#include <vme.h>
#include <stdio.h>
#include <string.h>
#include <ioctl.h>
#include <math.h>
#include <unistd.h>
#include <sysSymTbl.h>
#include <semLib.h>
#include <sysLib.h>
#include <symLib.h>
#include <taskLib.h>
#include <intLib.h>
#include <bootLib.h>
#include <inetLib.h>
#include <wdLib.h>
#include <ioLib.h>
#include <semLib.h>
#include <sockLib.h>
#include <selectLib.h>
#include <logLib.h>
#include <timers.h>
#include <time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

#include "osiSock.h"
#include "envDefs.h"
#include "envLib.h"
#include "dbDefs.h"
#include "epicsPrint.h"
#include "errMdef.h"
#include "drvSup.h"
#include "drvTS.h"
#include "bsdSocketResource.h"
#include "epicsDynLink.h"

/* Use TSprintf() for anything that should be logged, else printf() */
#define TSprintf epicsPrintf

#define DEFAULT_TIME	0
#define NO_EVENT_SYSTEM	1

/* functions used by this driver */
static long TSgetUnixTime(struct timespec*);
static long TSinitClockFromUnix(void);
static long TSgetMasterTime(struct timespec*);
static void TSsyncServer();
static void TSsyncClient();
static long TSasyncClient();
static long TSsyncTheTime(struct timespec* cts, struct timespec* ts);
static void TSaddStamp(	struct timespec* result,
    struct timespec* op1, struct timespec* op2);
static void TSwdIncTime();
static void TSwdIncTimeSync();
static void TSstampServer();
static void TSeventHandler(int Card,int EventNum,unsigned long Ticks);
static void TSerrorHandler(int Card, int ErrorNum);
static long TSsetClockFromMaster();
static void TSstartSoftClock();
static long TScalcDiff(struct timespec* a, struct timespec* b,
    struct timespec* diff);

static long TSgetCurrentTime(struct timespec* ts);
static long TSforceSoftSync(int Card);
static long TSstartSyncServer();
static long TSstartSyncClient();
static long TSstartAsyncClient();
static long TSstartStampServer();
static long TSuserGetJunk(int event_number,struct timespec* sp);
/* network utilities*/
static long TSgetBroadcastAddr(int soc, struct sockaddr*);
static int TSgetNtpSocketAddr(struct sockaddr_in *sin);
static int  TSgetSocket(int port, struct sockaddr_in* sin);
static int  TSgetBroadcastSocket(int port, struct sockaddr_in* sin);
static long TSgetData(char* buf, int buf_size, int soc,
    struct sockaddr* to_sin, struct sockaddr* from_sin,
    struct timespec* round_trip);

/* event system and time clock functions */
static long (*TSregisterEventHandler)(int Card, void(*func)());
static long (*TSregisterErrorHandler)(int Card, void(*func)());
static long (*TSgetTicks)(int Card, unsigned long *Ticks);
static long (*TShaveReceiver)(int Card);
static long (*TSforceSync)(int Card);
static long (*TSgetTime)(struct timespec*);
static long (*TSsyncEvent)();
static long (*TSdirectTime)();
static long (*TSdriverInit)();
static long (*TSuserGet)(int event_number,struct timespec* sp);

static int TSinitialized = 0;

/* global functions */
#ifdef __cplusplus
extern "C" {
#endif
long TSinit(void);	/* called by iocInit currently */
long TSreport();	/* callable from vxWorks shell */

/* test functions */
void TSprintRealTime();
void TSprintUnixTime();
void TSprintMasterTime();
void TSprintTimeStamp(int event_number);
void TSprintCurrentTime();
#ifdef __cplusplus
}
#endif

/* data used by all */
TSinfo TSdata =
{ 	TS_master_dead, TS_async_slave, TS_async_none,
	0,NULL,
	TS_SYNC_RATE_SEC,TS_CLOCK_RATE_HZ,0,TS_TIME_OUT_MS,0,
	TS_MASTER_PORT,TS_SLAVE_PORT,1,0,0,0,0,
	NULL, {NULL}, {NULL},
        0,0,10
};

extern char* sysBootLine;

int TSdirectTimeVar = 0; /* aother way to indicate direct time */
int TSgoodTimeStamps = 0; /* a way to force use of accurate time stamps */

static WDOG_ID wd;
static long correction_factor = 0;
static long correction_count = 0;


/* static long TSreturnError() { return -1; } */
static long TSdirectTimeError() { return -1; }
static long TShaveReceiverError(int i) { return -1; }
static long TSgetTicksError(int i,unsigned long* t) { return -1; }

static long TSregisterErrorHandlerError(int i, void(*f)())
    { if(TSdata.has_direct_time==1) return 0; else return -1; }
static long TSregisterEventHandlerError(int i, void(*f)())
    { if(TSdata.has_direct_time==1) return 0; else return -1; }


long TSdriverInitError()
{
	struct timespec ts;
	time_t t;

	clock_gettime(CLOCK_REALTIME,&ts);
	time(&t);
	Debug(1,"vxWorks time = %s\n",ctime(&t));

	return 0;
}

unsigned long TSepochNtpToUnix(struct timespec* ts)
{
    unsigned long nfssecs = (unsigned long)ts->tv_sec;
    unsigned long secs = 0;

    if(!TSinitialized) TSinit();
    /*If high order bit is not set then nfssecs has overflowed */
    if(!(nfssecs & 0x80000000ul)) {
	/*secs = nfssecs - TS_1900_TO_VXWORKS_EPOCH + 2**32 */
	/* in order to prevent overflows rearrange as */
	/* secs = (2**32 -1) - TS_1900_TO_VXWORKS_EPOCH + nfssecs +1 */
	secs = 0xffffffffUL - TS_1900_TO_VXWORKS_EPOCH + nfssecs +1;
    } else {
	secs = nfssecs - TS_1900_TO_VXWORKS_EPOCH;
    }
    return(secs);
}

unsigned long TSfractionToNano(unsigned long fraction)
{
    double value;

    /*value = 1e9 * fraction / 2**32 */
    value = (1e9 * (double)fraction)/4294967296.0;
    return((unsigned long)value);
}

unsigned long TSepochNtpToEpics(struct timespec* ts)
{
    unsigned long nfssecs = (unsigned long)ts->tv_sec;
    unsigned long secs = 0;

    /*If high order bit is not set then nfssecs has overflowed */
    if(!(nfssecs & 0x80000000ul)) {
	/*secs = nfssecs - TS_1900_TO_EPICS_EPOCH + 2**32 */
	/* in order to prevent overflows rearrange as */
	/* secs = (2**32 -1) - TS_1900_TO_EPICS_EPOCH + nfssecs +1 */
	secs = 0xffffffffUL - TS_1900_TO_EPICS_EPOCH + nfssecs +1;
    } else {
	secs = nfssecs - TS_1900_TO_EPICS_EPOCH;
    }
    return(secs);
}

unsigned long TSepochUnixToEpics(struct timespec* ts)
{
    unsigned long unixsecs = (unsigned long)ts->tv_sec;
    unsigned long secs;

    secs = unixsecs - TS_VXWORKS_TO_EPICS_EPOCH;
    return(secs);
}

unsigned long TSepochEpicsToUnix(struct timespec* ts)
{
    unsigned long epicssecs = (unsigned long)ts->tv_sec;
    unsigned long secs;

    secs = epicssecs + TS_VXWORKS_TO_EPICS_EPOCH;
    return(secs);
}

/*-----------------------------------------------------------------------*/
/*	
	TSreport() - report information about the state of the time stamp
	support software
*/
long TSreport()
{
	char buf[64];

	switch(TSdata.type)
	{
	case TS_direct_master:  printf("Direct timing master\n"); break;
	case TS_sync_master:	printf("Event timing master\n"); break;
	case TS_async_master:	printf("Soft timing master\n"); break;
	case TS_direct_slave:   printf("Direct timing slave\n"); break;
	case TS_sync_slave:	printf("Event timing slave\n"); break;
	case TS_async_slave:	printf("Soft timing slave\n"); break;
	default: break;
	}
	switch(TSdata.state)
	{
	case TS_master_alive:	printf("Master timing IOC alive\n"); break;
	case TS_master_dead:	printf("Master timing IOC dead\n"); break;
	default: break;
	}
	switch(TSdata.async_type)
	{
	case TS_async_none:	printf("No clock synchronization\n"); break;
	case TS_async_private:	printf("Sync protocol with master\n"); break;
	case TS_async_ntp:	printf("NTP sync with unix server\n"); break;
	case TS_async_time:	printf("Time protocol sync with unix\n"); break;
	default: break;
	}
	printf("Clock Rate in Hertz = %lu\n",TSdata.clock_hz);
	printf("Sync Rate in Seconds = %lu\n",TSdata.sync_rate);
	printf("Master communications port = %d\n",TSdata.master_port);
	printf("Slave communications port = %d\n",TSdata.slave_port);
	printf("Total events supported = %d\n",TSdata.total_events);
	printf("Request Time Out = %lu milliseconds\n",TSdata.time_out);

	ipAddrToA ((struct sockaddr_in*)&TSdata.hunt, buf, sizeof(buf));
	printf("Broadcast address: %s\n", buf);
	ipAddrToA ((struct sockaddr_in*)&TSdata.master, buf, sizeof(buf));
	printf("Master address: %s\n", buf);

	if(TSdata.UserRequestedType)
		printf("\nForced to not use the event system\n");

	if(TSdata.has_direct_time)
		printf("Event system has time directly available\n");
        printf("syncReportThreshold %lu milliseconds\n",
            TSdata.syncReportThreshold);
        printf("syncUnixReportThreshold %lu milliseconds\n",
            TSdata.syncUnixReportThreshold);
	return 0;
}

/*-----------------------------------------------------------------------*/
/*	
	TSconfigure() - This is the configuration routine which is meant to 
	be called from the vxWorks startup.cmd script before iocInit.
	It's job is to set operating parameters for the time stamp support code.

    JRW -- if type = 0, then try to config using event system
           if type = 1, then permanantly inhibit use of the event system
*/
void TSconfigure(int master, int sync_rate_sec, int clock_rate_hz,
	int master_port, int slave_port, unsigned long time_out, int type)
{
	if(master) TSdata.master_timing_IOC=1;
	else TSdata.master_timing_IOC=0;

	if(sync_rate_sec) TSdata.sync_rate=sync_rate_sec;
	else TSdata.sync_rate=TS_SYNC_RATE_SEC;

	if(clock_rate_hz) TSdata.clock_hz=clock_rate_hz;
	else TSdata.clock_hz=TS_CLOCK_RATE_HZ;

	TSdata.clock_conv= TS_BILLION / TSdata.clock_hz;

	if(master_port) TSdata.master_port=master_port;
	else TSdata.master_port=TS_MASTER_PORT;

	if(slave_port) TSdata.slave_port=slave_port;
	else TSdata.slave_port=TS_SLAVE_PORT;

	if(time_out) TSdata.time_out=time_out;
	else TSdata.time_out=TS_TIME_OUT_MS;

	switch(type)
	{
	case DEFAULT_TIME:
	case NO_EVENT_SYSTEM:
		TSdata.UserRequestedType = type;
		break;
	default:
		TSprintf("Invalid type parameter <%d> must be:\n",type);
		TSprintf("  0 = default time system\n");
		TSprintf("  1 = force no event system\n");
		TSdata.UserRequestedType = 0;
		break;
	}

	return;
}

/*
   this sucks, but who cares, user should not be using event number if not
   prepared to handle them by defining an ErUserGetTimeStamp() routine.
*/
static long TSuserGetJunk(int event_number,struct timespec* sp)
{
	return TSgetTimeStamp(0,sp);
}

/*-----------------------------------------------------------------------*/
/*	
	TSgetTimeStamp() - This routine returns the time stamp which represents
	the time when an event event_number occurred.  Soft timing will always
	return the current time.  Event zero will always return the current time
	also.
*/
long TSgetTimeStamp(int event_number,struct timespec* sp)
{
	/* this is questionable, a sync slave with a dead master will have
	an invalid time stamp, so use the vxworks clock. Also remember that
	the IOC vxWorks clock may not be in sync with time stamps if 
	default TSgetTime() in use and master failure is detected */
        if(!TSinitialized) TSinit();

	switch(TSdata.type)
	{
	case TS_async_master:
	case TS_async_slave:
	    if(event_number<=0)
		*sp = TSdata.event_table[TSdata.sync_event];
	    else
		return TSuserGet(event_number,sp);
	    break;
	case TS_direct_slave:
	case TS_direct_master:
	    TSgetTime(sp);
	    break;
	case TS_sync_slave:
	case TS_sync_master:
	    switch(event_number)
	    {
	    case 0:
		if(TSgoodTimeStamps==0)
		{
		    /* one tick watch dog maintains */
		    *sp = TSdata.event_table[0];
		    break;
		}
	    case -1:
		{
		    struct timespec ts;
		    unsigned long ticks;

		    TSgetTicks(0,&ticks); /* add in the board time */
		    *sp = TSdata.event_table[TSdata.sync_event];

		    /* calculate a time stamp from the tick count */
		    ts.tv_sec = ticks / TSdata.clock_hz;
		    ts.tv_nsec=(ticks-(ts.tv_sec*TSdata.clock_hz))*
			TSdata.clock_conv;

		    sp->tv_sec += ts.tv_sec;
		    sp->tv_nsec += ts.tv_nsec;

		    /* adjust seconds if needed */
		    if(sp->tv_nsec >= TS_BILLION)
		    {
			sp->tv_sec++;
			sp->tv_nsec -= TS_BILLION;
		    }
		}
	    break;
	    default:
		if(TSdata.state==TS_master_dead)
		    TSgetTime(sp);
		else
		    *sp = TSdata.event_table[event_number];
		break;
	    }
	    break;
	default:
	    if(event_number==0)
		*sp = TSdata.event_table[TSdata.sync_event];
	    else
		*sp = TSdata.event_table[event_number];
	}
	return 0;
}

/*-----------------------------------------------------------------------*/
/*	
	TSinit() - initialize the driver, determine mode.
*/
long TSinit(void)
{
    SYM_TYPE stype;
    char tz[100],min_west[20];

    Debug0(5,"In TSinit()\n");

    if(TSinitialized) return(0);
    TSinitialized=1;
    /* 0=default, 1=none, 2=direct */

    if(	TSdata.UserRequestedType==DEFAULT_TIME)
    {
	/* default configuration probe */
	/* ------------------------------------------------------------- */
	/* find the lower level event system functions */
	if(symFindByNameEPICS(sysSymTbl,"_ErHaveReceiver",
	    (char**)&TShaveReceiver,&stype)==ERROR)
	    TShaveReceiver = TShaveReceiverError;
	if(symFindByNameEPICS(sysSymTbl,"_ErGetTicks",
	    (char**)&TSgetTicks,&stype)==ERROR)
	    TSgetTicks = TSgetTicksError;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErRegisterEventHandler",
	    (char**)&TSregisterEventHandler,&stype)==ERROR)
	    TSregisterEventHandler = TSregisterEventHandlerError;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErRegisterErrorHandler",
	    (char**)&TSregisterErrorHandler,&stype)==ERROR)
	    TSregisterErrorHandler = TSregisterErrorHandlerError;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErForceSync",
	    (char**)&TSforceSync,&stype)==ERROR)
	    TSforceSync = TSforceSoftSync;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErDirectTime",
	    (char**)&TSdirectTime,&stype)==ERROR)
	    TSdirectTime = TSdirectTimeError;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErDriverInit",
	    (char**)&TSdriverInit,&stype)==ERROR)
	    TSdriverInit = TSdriverInitError;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErGetTime",
	    (char**)&TSgetTime,&stype)==ERROR)
	    TSgetTime = TSgetCurrentTime;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErUserGetTimeStamp",
	    (char**)&TSuserGet,&stype)==ERROR)
	    TSuserGet = TSuserGetJunk;
	
	if(symFindByNameEPICS(sysSymTbl,"_ErSyncEvent",
	    (char**)&TSsyncEvent,&stype)==ERROR)
	    TSdata.sync_event=ER_EVENT_RESET_TICK;
	else
	    TSdata.sync_event=TSsyncEvent();
	
	/* ------------------------------------------------------------- */
    }
    else
    {
	/* inhibit probe and use of the event system */
	TSprintf("WARNING: drvTS event hardware probe inhibited by user\n");
	TShaveReceiver = TShaveReceiverError;
	TSgetTicks = TSgetTicksError;
	TSregisterEventHandler = TSregisterEventHandlerError;
	TSregisterErrorHandler = TSregisterErrorHandlerError;
	TSforceSync = TSforceSoftSync;
	TSgetTime = TSgetCurrentTime;
	TSdriverInit = TSdriverInitError;
	TSdirectTime = TSdirectTimeError;
        TSuserGet = TSuserGetJunk;
	TSdata.sync_event=ER_EVENT_RESET_TICK;
    }

    /* set all the known information about the system */
    TSdata.event_table=NULL;
    TSdata.ts_sync_valid=0; /* the sync time stamp invalid */
    TSdata.state=TS_master_dead;
    TSdata.sync_occurred = semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);
    TSdata.has_event_system = 0;
    TSdata.has_direct_time = 0;
    TSdata.async_type=TS_async_none;

    if( (TSdata.total_events=TShaveReceiver(0))<=0)
    {
	Debug0(5,"TSinit() - no event receiver\n");
	TSdata.total_events=1;
	TSdata.sync_event=0;
	TSdata.clock_hz=sysClkRateGet();
	TSdata.clock_conv=TS_BILLION / TSdata.clock_hz;
	TSdata.has_event_system = 0;
    }
    else
	TSdata.has_event_system = 1;

    if(TSdirectTime()>0 || TSdirectTimeVar>0) TSdata.has_direct_time=1;

    /* allocate the event table */
    TSdata.event_table=(struct timespec*)malloc(
	TSdata.total_events*sizeof(struct timespec));

    if(TSdata.master_timing_IOC)
    {
	/* master */
	Debug0(5,"TSinit() - I am master\n");
	if(TSdata.has_direct_time)
	{
	    TSdata.type=TS_direct_master;
	}
	else
	{
	    if(TSdata.has_event_system)
		TSdata.type=TS_sync_master;
	    else
		TSdata.type=TS_async_master;
	}
    }
    else
    {
	/* slave */
	Debug0(5,"TSinit() - I am slave\n");
	if(TSdata.has_direct_time)
	{
	    TSdata.type=TS_direct_slave;
	}
	else
	{
	    if(TSdata.has_event_system)
		TSdata.type=TS_sync_slave;
	    else
		TSdata.type=TS_async_slave;
	}
    }

    /* set up the event system hooks */
    if(TSdata.has_event_system)
    {
	/* register the event handler function */
	if(TSregisterEventHandler(0,TSeventHandler)!=0)
	{
	    TSprintf("Failed to register event handler\n");
	    return -1;
	}

	/* register the error handler function */
	if(TSregisterErrorHandler(0,TSerrorHandler)!=0)
	{
	    TSprintf("Failed to register error handler\n");
	    return -1;
	}
    }

    /* always start the soft clock */
    if(TSdata.has_direct_time==0) TSstartSoftClock();

    /* get time from boot server Unix system */
    if(TSdata.master_timing_IOC)
    {
	/* master */
	if(TSinitClockFromUnix()<0)
	{
	    /* bad, cannot get time - accessing starts ticking */
	    struct timespec tp;
	    clock_gettime(CLOCK_REALTIME,&tp);
	    TSprintf("Failed to set clock from Unix server\n");
	}
	Debug0(5,"TSinit() - tried to get clock from unix\n");

	/* start the time stamp info server */
	if(TSstartStampServer()==ERROR)
	{
	    TSprintf("Failed to start stamp server\n");
	    return -1;
	}
	Debug0(5,"TSinit() - stamp server started \n");

	TSdata.state = TS_master_alive;

	/* a direct master may be capable of delivering sync time stamps? */
	if(TSdata.type==TS_sync_master)
	{
	    /* start the sync udp server */
	    if(TSstartSyncServer()==ERROR)
	    {
		TSprintf("Failed to start sync server\n");
		return -1;
	    }
	    Debug0(5,"TSinit() - sync server started  \n");
	}
    }
    else
    {
	/* slave */
	if(TSinitClockFromUnix()<0)
	{
	    struct timespec tp;
	    clock_gettime(CLOCK_REALTIME,&tp);
	    /* this should work */
	    TSprintf("Failed to set time from Unix server\n");
	}

	if( TSsetClockFromMaster()<0 )
	{
	    /* do nothing here */
	    /* TSprintf("Could not contact a master timing IOC\n"); */
	}
	else
	    TSdata.state = TS_master_alive;

	if(TSdata.type==TS_async_slave)
	{
	    /* this task syncs with master or unix */
	    if(TSstartAsyncClient()==ERROR)
	    {
		TSprintf("Failed to start async client\n");
		return -1;
	    }
	    Debug0(5,"TSinit() - async client started  \n");
	}
	else
	{
	    /* this task sync with master */
	    if(TSstartSyncClient()==ERROR)
	    {
		TSprintf("Failed to start sync client\n");
		return -1;
	    }
	    Debug0(5,"TSinit() - sync client started  \n");
	}
    }

    /*
    This section sets up the vxWorks clock for use with the ansiLib
    functions.  The TIMEZONE environment variable for vxWorks is only
    overwritten if it has not been set.  It seems as though the day
    light saving time does not work (at least following the directions
    in ansiLib).

    The EPICS environment variable EPICS_TS_MIN_WEST holds the minutes
    west of GMT (UTC) time.  This variable should be preset by the EPICS
    administrator for your site.
    */

    if(getenv("TIMEZONE")==(char*)NULL)
    {
	if(envGetConfigParam(&EPICS_TS_MIN_WEST,sizeof(min_west),min_west)==NULL
	  || strlen(min_west)==0)
	{
	    TSprintf("TS initialization: No Time Zone Information\n");
	}
	else
	{
	    sprintf(tz,"TIMEZONE=UTC::%s:040102:100102",min_west);
	    if(putenv(tz)==ERROR)
	    {
		TSprintf("TS initialization: TIMEZONE putenv failed\n");
	    }
	}
    }

    TSdriverInit(); /* Call the user's driver initialization if supplied */
    return 0;
}

/* following are watch dog routines for soft time support */

static void TSstartSoftClock() /*start the soft clock watch dog*/
{
	/* simple watch dog to fire off syncs to slaves */
	Debug(5,"start watch dog at rate %ld\n",
		(long) sysClkRateGet()*TSdata.sync_rate);
	wd = wdCreate();
	if(TSdata.has_event_system)
	{
		Debug0(8,"Starting sync time watch dog\n");
		wdStart(wd,1,(FUNCPTR)TSwdIncTimeSync,NULL);
	}
	else
	{
		Debug0(8,"Starting async time watch dog\n");
		wdStart(wd,1,(FUNCPTR)TSwdIncTime,NULL);
	}
	return;
}

/* NOTE: TSwdIncTime called at interrupt level! */
static void TSwdIncTime() /*increment the time stamp at a 60 Hz rate*/
{
    int key;

    wdStart(wd,1, (FUNCPTR)TSwdIncTime,NULL);
    /* update the event table */
    key=intLock();

    if(correction_count)
    {
	long nsec = TSdata.event_table[TSdata.sync_event].tv_nsec
	    + TSdata.clock_conv;
	nsec += correction_factor;
	if(nsec<0) {
	    nsec += TS_BILLION;
	    TSdata.event_table[TSdata.sync_event].tv_sec--;
	}
	TSdata.event_table[TSdata.sync_event].tv_nsec = nsec;
	if(--correction_count == 0) correction_factor = 0;
    }
    else
	TSdata.event_table[TSdata.sync_event].tv_nsec +=
			TSdata.clock_conv;

    /* adjust seconds if needed */
    if(TSdata.event_table[TSdata.sync_event].tv_nsec >= TS_BILLION)
    {
	TSdata.event_table[TSdata.sync_event].tv_sec++;
	TSdata.event_table[TSdata.sync_event].tv_nsec -= TS_BILLION;
    }
    intUnlock(key);
    return;
}

/* TSwdIncTimeSync called at interrupt level! */
static void TSwdIncTimeSync()
{
	wdStart(wd,1, (FUNCPTR)TSwdIncTimeSync,NULL);
	TSaccurateTimeStamp(&TSdata.event_table[0]);
}

/*	
    TSeventHandler() - receive events from event system; update the event table 

	Note: called at interrupt level!
*/
static void TSeventHandler(int Card,int EventNum,unsigned long Ticks)
{
    struct timespec ts;
    struct timespec* st;
    int key;

#ifdef DIRECT_WITH_EVENTS
    if(TSdata.has_direct_time==1)
    {
	TSgetTime(&ts);
	key=intLock();
	TSdata.event_table[EventNum].tv_sec = ts.tv_sec;
	TSdata.event_table[EventNum].tv_nsec = ts.tv_nsec;
	intUnlock(key);
	return;
    }
#endif


    /* calculate a time stamp from the Tick count */
    ts.tv_sec = Ticks / TSdata.clock_hz;
    ts.tv_nsec = (Ticks - (ts.tv_sec * TSdata.clock_hz)) * TSdata.clock_conv;
    /* obtain a copy of the last sync time */
    st = &TSdata.event_table[TSdata.sync_event];
    /* update the event table */
    key=intLock();
    TSdata.event_table[EventNum].tv_sec = st->tv_sec + ts.tv_sec;
    TSdata.event_table[EventNum].tv_nsec = st->tv_nsec + ts.tv_nsec;
    /* adjust seconds if needed */
    if(TSdata.event_table[EventNum].tv_nsec >= TS_BILLION)
    {
	TSdata.event_table[EventNum].tv_sec++;
	TSdata.event_table[EventNum].tv_nsec -= TS_BILLION;
    }
    intUnlock(key);
    /* signal a time stamp sync if this is sync event */
    if(TSdata.type==TS_sync_master && EventNum==TSdata.sync_event)
    {
	/* validate the soft time for back off in case of event system
	failure, this will not be done in first implementation */

	/* tell broadcast server to send out sync */
	semGive(TSdata.sync_occurred);
    }
    return;
}

/*	TSerrorHandler() - receive errors from event system
	Note: called at interrupt level!
*/
static void TSerrorHandler(int Card, int ErrorNum)
{
    /* probably should do the following:
	mark a "bad" state - timestamp is invalid,
	send a request for sync to master,
	if sync doesn't come when in bad state, then install
	clock hook to increment time stamp until event system back up
	keep a count of errors
	Could put the slave on the vxworks timer until next sync
    */

    if(MAKE_DEBUG)
    {
	switch(ErrorNum)
	{
	case 1:
	  logMsg("***TSerrorHandler: event system error: TAXI violation",
	      0,0,0,0,0,0);
	  break;
	case 2:
	  logMsg("***TSerrorHandler: event system error: lost heartbeat",
	      0,0,0,0,0,0);
	  break;
	case 3:
	  logMsg("***TSerrorHandler: event system error: lost events",
	      0,0,0,0,0,0);
	  break;
	default:
		logMsg("***TSerrorHandler: unknown error %d from event system",
		    ErrorNum,0,0,0,0,0);
	}
    }
    return;
}

/**************************************************************************/
/* the following are utilities for initially getting and setting the time */
/**************************************************************************/
/*	
	TSgetUnixTime() - ask the boot server for the time using the 
	time protocol.  This is only the time to the nearest second
	A better way would be to use the NTP transactions to boot server 
*/

static long TSgetUnixTime(struct timespec* ts)
{
    unsigned long buf_data,timeValue;
    TS_NTP buf_ntp;
    struct sockaddr_in sin;
    int soc;

    soc = TSgetNtpSocketAddr(&sin);
    if(soc<0) {
	TSdata.async_type=TS_async_none;
        return -1;
    }

    /* set up for ntp transaction to boot server */
    TSdata.async_type=TS_async_ntp;
    memset(&buf_ntp,0,sizeof(buf_ntp));
    buf_ntp.info[0]=0x0b;
    if(TSgetData((char*)&buf_ntp,sizeof(buf_ntp),soc,
		(struct sockaddr*)&sin,NULL,NULL)<0)
    {
	Debug0(2,"no reply from NTP server\n");
	/* well known registered time port */
	sin.sin_port = htons(UDP_TIME_PORT);
	TSdata.async_type=TS_async_time;
	buf_data=0;
	if(TSgetData((char*)&buf_data,sizeof(buf_data),soc,
			(struct sockaddr*)&sin,NULL,NULL)<0)
	{
	    Debug0(2,"no reply from Time server\n");
	    TSdata.async_type=TS_async_none;
	    close(soc);
	    return -1;
	}
    }
    /* remember: time and NTP protocol return seconds since 1900, not 1970 */
    /*	to convert sec = timeValue - TS_1900_TO_EPICS_EPOCH */
    if(TSdata.async_type==TS_async_time)
    {
	/* this is time protocol */
	timeValue=ntohl(buf_data);
	ts->tv_sec = timeValue;
	ts->tv_nsec = 0;
	Debug(2,"got the Time protocol time %lu= \n",timeValue);
    }
    else
    {
	/* this is ntp  - need to convert to ns */
	ts->tv_sec=ntohl(buf_ntp.transmit_ts.tv_sec);
	timeValue=ntohl(buf_ntp.transmit_ts.tv_nsec);
	ts->tv_nsec = TSfractionToNano(timeValue);
	if(MAKE_DEBUG>=2)
	    printf("got the NTP time %9.9lu.%9.9lu\n",ts->tv_sec,timeValue);
    }
    close(soc);
    return 0;
}
/*	
	TSinitClockFromUnix() - query the time from boot server and set the 
	vxworks clock.
*/
static long TSinitClockFromUnix(void)
{
    struct timespec tp;
    int key;
    unsigned long ulongtemp;

    if(!TSinitialized) TSinit();
    Debug0(3,"in TSinitClockFromUnix()\n");

    if(TSgetUnixTime(&tp)!=0) return -1;

    ulongtemp = TSepochNtpToUnix(&tp);
    tp.tv_sec = (long)ulongtemp;

    if(MAKE_DEBUG>=9)
	printf("set time: %9.9lu.%9.9lu\n", tp.tv_sec,tp.tv_nsec);

    /* set the vxWorks clock to the correct time */
    if(clock_settime(CLOCK_REALTIME,&tp)<0)
    { Debug0(1,"clock_settime failed\n"); }

    /* adjust time to use the EPICS EPOCH of 1990 */
    /* this is wrong if leap seconds accounted for */
    ulongtemp = TSepochUnixToEpics(&tp);
    tp.tv_sec = ulongtemp;

    key=intLock();
    TSdata.mask |= TS_STAMP_FROM_UNIX;
    TSdata.event_table[TSdata.sync_event]=tp;
    TSdata.event_table[0]=tp;
    intUnlock(key);
    if(MAKE_DEBUG>=9)
	printf("epics time: %9.9lu.%9.9lu\n",
		TSdata.event_table[TSdata.sync_event].tv_sec,
		TSdata.event_table[TSdata.sync_event].tv_nsec);

    return 0;
}

/*	
    TSgetMasterTime() - query the master timing IOC for it's current time
    This routine will be converted to a task and block on a semaphore,
    when it gets the semaphore, it will go to the master timing IOC
    and get a time stamp, the time stamp will be used to adjust the
    vxworks clock.  To use it for software timing, just put a watchdog
    out there that pokes the semaphore.  Another way to manage soft
    timing is to catch broadcasts from the master, round trip
    calculations cannot be made when doing this.
*/
static long TSgetMasterTime(struct timespec* tsp)
{
    TSstampTrans stran;
    struct timespec curr_time,tran_time;
    struct sockaddr_in sin;
    struct sockaddr fs;
    int soc;

    Debug0(3,"TSgetMasterTime() called\n");

    if( (soc=TSgetBroadcastSocket(0,&sin)) <0)
    { Debug0(1,"TSgetBroadcastSocket failed\n"); return -1; }
    sin.sin_port = htons(TSdata.master_port);
    memcpy(&TSdata.hunt,&sin,sizeof(sin));
    stran.type=(TStype)htonl(TS_time_request);
    stran.magic=htonl(TS_MAGIC);

    if(TSgetData((char*)&stran,sizeof(stran),soc,
		(struct sockaddr*)&sin,&fs,&tran_time)<0)
    { Debug0(2,"no reply from master server\n"); close(soc); return -1; }

    /* check the magic number */
    if(ntohl(stran.magic)!=(TS_MAGIC))
    {
    
	TSprintf("TSgetMasterTime: invalid packet received\n");
	close(soc);
	return -1;
    }
    /* we now have several pieces of information:
	the master's address,
	the approximate round trip time,
	and the master's processing time. */
    /* set the global data structure for information from master */
    Debug(8,"master port=%d\n",ntohs(((struct sockaddr_in*)&fs)->sin_port));
    TSdata.master = fs;
    TSdata.state = TS_master_alive;
    if(TSdata.type==TS_sync_slave)
    {
	TSdata.clock_hz = ntohl(stran.clock_hz);
	TSdata.sync_rate = ntohl(stran.sync_rate);
	TSdata.clock_conv = TS_BILLION / TSdata.clock_hz;
    }

    if(MAKE_DEBUG>=6)
    {
	printf("round trip time: %9.9lu.%9.9lu\n",
		tran_time.tv_sec,tran_time.tv_nsec);
	printf("master time: %9.9lu.%9.9lu\n",
		stran.master_time.tv_sec,stran.master_time.tv_nsec);
    }

    /* Halve the round-trip time to estimate the time stamp error */
    tran_time.tv_nsec >>= 1;
    if (tran_time.tv_sec & 1) tran_time.tv_nsec += (TS_BILLION >> 1);
    tran_time.tv_sec >>= 1;

    /* add the error estimate to the time stamp from master */
    curr_time.tv_sec = ntohl(stran.current_time.tv_sec);
    curr_time.tv_nsec = ntohl(stran.current_time.tv_nsec);
    TSaddStamp(tsp,&curr_time,&tran_time);
    close(soc);
    return 0;
}

/*	
	TSsetClockFromUnix() - make next sync event get time from unix
*/
long TSsetClockFromUnix(void)
{
    TSdata.mask |= TS_STAMP_FROM_UNIX;
    return 0;
}

long TSsetSyncReportThreshold(
    unsigned long syncMilliseconds,unsigned long syncUnixMilliseconds)
{
    TSdata.syncReportThreshold = syncMilliseconds;
    TSdata.syncUnixReportThreshold = syncUnixMilliseconds;
    return 0;
}

/*	
	TSsetClockFromMaster() - set the vxworks clock using the time from 
	the master timing IOC
*/
static long TSsetClockFromMaster()
{
    struct timespec tp;
    int key;
    unsigned long secs;

    Debug0(3,"in TSsetClockFromMaster()\n");

    if(TSgetMasterTime(&tp)<0) return -1;

    key=intLock();
    TSdata.event_table[TSdata.sync_event]=tp;
    TSdata.event_table[0]=tp;
    intUnlock(key);

    /* adjust time to use the Unix EPOCH of 1900 - not to good */
    /* making this adjustment is not so good */
    secs = TSepochEpicsToUnix(&tp);
    tp.tv_sec = secs;
    clock_settime(CLOCK_REALTIME,&tp);

    return 0;
}

/**************************************************************************/
/* time stamp utility routines */
/**************************************************************************/

/*
	TSsyncTheTime - given the current time (cts), and a compare time
	stamp (ts), correct the current time clock to match ts.
*/
static long TSsyncTheTime(struct timespec* cts, struct timespec* ts)
{
    int key;
    long diffSec;
    long diffNsec;
    long diffSecAbs;

    diffSec = (long)ts->tv_sec -(long) cts->tv_sec;
    diffNsec = (long)ts->tv_nsec - (long)cts->tv_nsec;
    diffSecAbs = (diffSec<0) ? -diffSec : diffSec;
    /* see if clock can be corrected in sync window - up to 1 secs per tick */
    if(diffSecAbs <= TSdata.sync_rate)
    {
        long diff;
        long count;
	count=sysClkRateGet()*TSdata.sync_rate;
	diff = (TS_BILLION/count)*diffSec + diffNsec/count;
	if(diff) {
	    key=intLock();
	    correction_count = count;
	    correction_factor = diff;
	    intUnlock(key);
	}
	Debug(5,"Correction Factor=.%9.9ld\n",correction_factor);
    }
    else
    {
	/* clock can not be corrected  - jam it for now */
	TSdata.event_table[TSdata.sync_event]=*ts;
	if(MAKE_DEBUG>=7)
	{
	    printf("Slave not in sync: mine=%9.9lu.%9.9lu!=%lu.%lu=other\n",
	    	cts->tv_sec, cts->tv_nsec, ts->tv_sec, ts->tv_nsec);
	    printf("slave diff time: %9.9ld.%9.9ld\n",
	    	diffSec,diffNsec);
	}
    }
    return 0;
}

/**************************************************************************/
/* varies server that can be running */
/**************************************************************************/

long TSstartSyncServer()
{
    /* run at priority just above CA */
    return taskSpawn("ts_syncS",TS_SYNC_SERVER_PRI,VX_FP_TASK|VX_STDIO,5000,
	(FUNCPTR)TSsyncServer,0,0,0,0,0,0,0,0,0,0);
}

/*	
	TSsyncServer() - Server task that broadcasts the sync time stamp every 
	time a sync event is receive by the event system.
*/
static void TSsyncServer()
{
    TSstampTrans stran;
    struct sockaddr_in sinBroadcast;
    int socBroadcast;
    struct sockaddr_in sinNtp;
    int socNtp;
    unsigned long mask;
    int setStampFromUnix;

    if( (socBroadcast=TSgetBroadcastSocket(0,&sinBroadcast)) <0)
    { Debug0(1,"TSgetBroadcastSocket failed\n"); return; }
    sinBroadcast.sin_port = htons(TSdata.slave_port);
    stran.type=(TStype)htonl(TS_sync_msg);
    stran.magic=htonl(TS_MAGIC);
    stran.sync_rate=htonl(TSdata.sync_rate);
    stran.clock_hz=htonl(TSdata.clock_hz);
    socNtp = TSgetNtpSocketAddr(&sinNtp);
    if(socNtp<0) TSprintf("TSsyncServer TSgetNtpSocketAddr failed\n");
    while(1)
    {
	/* wait for a sync to occur */
	semTake(TSdata.sync_occurred,WAIT_FOREVER);

	if(!TSdata.ts_sync_valid)
	{
	    int key;
	    struct timespec ts;
	    /* remember, the ts table has only an approximation to the
		correct time before the event is actually up and running,
		so validate the ts if this is the first sync - the vxWorks
		clock has a good time in it
	    */
	    if(TSgetTime(&ts)==0) TSdata.ts_sync_valid=1;
	    key=intLock();
	    /* don't adjust time from getTime() to EPICS epoch */
	    TSdata.event_table[TSdata.sync_event].tv_nsec=ts.tv_nsec;
	    TSdata.event_table[TSdata.sync_event].tv_sec=ts.tv_sec;
	    intUnlock(key);
	}
        /* Use while so that errors can break out of while*/
        setStampFromUnix = 0;
        while(TSdata.mask & TS_STAMP_FROM_UNIX) {
            TS_NTP buf_ntp;
            struct timespec tp;
            unsigned long ulongtemp;
            int key;
            unsigned long ticksStart,ticksEnd;

            if(socNtp<0) {
                TSprintf("TSsyncServer cant contact NTP server\n");
                break;
            }
            TSgetTicks(0,&ticksStart);
            memset(&buf_ntp,0,sizeof(buf_ntp));
            buf_ntp.info[0]=0x0b;
            if(TSgetData((char*)&buf_ntp,sizeof(buf_ntp),socNtp,
            (struct sockaddr*)&sinNtp,0,0) < 0) {
                TSprintf("TSsyncServer TSgetData for NTP failed\n");
                break;
            }
            tp.tv_sec = ntohl(buf_ntp.transmit_ts.tv_sec);
            ulongtemp = ntohl(buf_ntp.transmit_ts.tv_nsec);
            tp.tv_nsec = TSfractionToNano(ulongtemp);
            ulongtemp = TSepochNtpToUnix(&tp);
            tp.tv_sec = (long)ulongtemp;
            TSgetTicks(0,&ticksEnd);
            if(ticksEnd<ticksStart) {
                TSprintf("TSsyncServer clock overflow\n");
                break;
            }
            if((ticksEnd-ticksStart) > TS_MAX_TICKS_GET_NTP) {
                TSprintf("TSsyncServer NTP TSgetData took %lu clock ticks\n",
                    (ticksEnd - ticksStart));
                break;
            }
            setStampFromUnix = 1;
            if(clock_settime(CLOCK_REALTIME,&tp)<0) {
                TSprintf("TSsyncServer clock_settime failed\n");
            }
            ulongtemp = TSepochUnixToEpics(&tp);
            tp.tv_sec = ulongtemp;
            key=intLock();
            TSdata.event_table[TSdata.sync_event]=tp;
            TSdata.event_table[0]=tp;
            intUnlock(key);
            break;
        }
	stran.master_time.tv_sec=
	    htonl(TSdata.event_table[TSdata.sync_event].tv_sec);
	stran.master_time.tv_nsec=
	    htonl(TSdata.event_table[TSdata.sync_event].tv_nsec);
        mask = TSdata.mask;
        if(!setStampFromUnix) mask &= ~TS_STAMP_FROM_UNIX;
        stran.mask = htonl(mask);
        if(setStampFromUnix)TSdata.mask &= ~TS_STAMP_FROM_UNIX;
        stran.syncReportThreshold = htonl(TSdata.syncReportThreshold);
        stran.syncUnixReportThreshold = htonl(TSdata.syncUnixReportThreshold);
	if(sendto(socBroadcast,(char*)&stran,sizeof(stran),0,
	    (struct sockaddr*)&sinBroadcast,sizeof(sinBroadcast))<0)
	{ perror("sendto failed"); }
	Debug(8,"time send secs  = %lu\n",stran.master_time.tv_sec);
	Debug(8,"time send nsecs = %lu\n",stran.master_time.tv_nsec);
    }
    close(socBroadcast);
    return;
}

/*-----------------------------------------------------------------------*/
long TSstartAsyncClient()
{
    /* run at priority just above CA */
    return taskSpawn("ts_Casync",TS_ASYNC_CLIENT_PRI,VX_FP_TASK|VX_STDIO,5000,
    	(FUNCPTR)TSasyncClient,0,0,0,0,0,0,0,0,0,0);
}

/*
    TSasyncClient() - keep the async slave in sync with ntp server 
    while checking for master timing IOC to come up

    algorithm:
	- sync with master if available
	- sync with unix if no master, checking every so often for master
	- sync with unix if master goes away until master comes back
	- continually check for master if no unix sync available
*/
static long TSasyncClient()
{
    TSstampTrans stran;
    TS_NTP buf_ntp;
    struct sockaddr_in sin_unix,sin_bc,sin_master;
    int count,soc_unix,soc_master,soc_bc,buf_size;
    struct timespec ts,diff_time,cts,curr_time;
    unsigned long nsecs;

    soc_unix = TSgetNtpSocketAddr(&sin_unix);
    if(soc_unix<0) {
        printf("TSasyncClient TSgetNtpSocketAddr failed\n");
        return -1;
    }

    /*------socket for finding master----------*/
    if( (soc_bc=TSgetBroadcastSocket(0,&sin_bc)) <0)
    { Debug0(1,"TSgetBroadcastSocket failed\n"); close(soc_unix); return -1; }

    sin_bc.sin_port = htons(TSdata.master_port);
    /*-----------------------------------------*/

    while(1)
    {
	/* stop the clock from correcting here  - probably better to use
	a watch dog to stop the clock correction */

	if(TSdata.state==TS_master_alive)
	{
	   /* get socket to master */
	   Debug0(5,"async_client(): master_alive\n");
	   if( (soc_master=TSgetSocket(0,&sin_master)) <0)
	   { Debug0(1,"TSgetSocket failed\n"); }

	   while(TSdata.state==TS_master_alive)
	   {
		/* sync with the master as long as it is up */
		Debug0(5,"async_client(): syncing with master\n");
		stran.type=(TStype)htonl(TS_time_request);
		stran.magic=htonl(TS_MAGIC);

	        if(TSgetData((char*)&stran,sizeof(stran),soc_master,
        		&TSdata.master,NULL,&diff_time)<0)
        	{
			Debug0(2,"no reply from master server\n");
			TSdata.state=TS_master_dead;
			close(soc_master);
		}
		else
		{
		    if(ntohl(stran.magic)==TS_MAGIC)
		    {
			/* sync the time with master's here - 2 ticks */
			cts=TSdata.event_table[TSdata.sync_event];
			curr_time.tv_sec=ntohl(stran.current_time.tv_sec);
			curr_time.tv_nsec=ntohl(stran.current_time.tv_nsec);
			TSsyncTheTime(&cts,&curr_time);
			Debug(8,"master sec = %lu\n",curr_time.tv_sec);
				Debug(8,"my sec = %lu\n", cts.tv_sec);
		    }
		    else
		    {
			TSprintf("TSasyncClient: invalid packet recved\n");
		    }
		    taskDelay(sysClkRateGet()*TSdata.sync_rate);
		}
	    }
	}
	else if(TSdata.async_type==TS_async_ntp)
	{
	    /* sync with unix server using NTP - master check now and then */
	    Debug0(5,"async_client(): using NTP sync\n");
	    count=0;
	    while(TSdata.state==TS_master_dead)
	    {
		Debug0(5,"async_client(): syncing with unix\n");
		memset(&buf_ntp,0,sizeof(buf_ntp));
		buf_ntp.info[0]=0x0b;
		buf_size=sizeof(buf_ntp);

		if(TSgetData((char*)&buf_ntp,sizeof(buf_ntp),soc_unix,
			(struct sockaddr*)&sin_unix,NULL,&diff_time)<0)
		{
		    Debug0(2,"no reply from NTP server\n");
		}
		else
		{
		    unsigned long ulongtemp;
		    /* get the current time */
		    cts=TSdata.event_table[TSdata.sync_event];
		    /* adjust the ntp time */
		    ts.tv_sec = ntohl(buf_ntp.transmit_ts.tv_sec);
		    ulongtemp = TSepochNtpToEpics(&ts);
		    ts.tv_sec = (long)ulongtemp;
		    nsecs=ntohl(buf_ntp.transmit_ts.tv_nsec);
		    ts.tv_nsec = TSfractionToNano(nsecs);
		    TSsyncTheTime(&cts,&ts);
		}
		if(count==0)
		{
		    stran.type=(TStype)htonl(TS_time_request);
		    stran.magic=htonl(TS_MAGIC);
		 
		    if(TSgetData((char*)&stran,sizeof(stran),soc_bc,
			(struct sockaddr*)&sin_bc,&TSdata.master,&diff_time)<0)
		    { Debug0(2,"no reply from master server\n"); }
		    else
		    {
			TSdata.state=TS_master_alive;
			Debug(8,"master port = %d\n",
			  ntohs( ((struct sockaddr_in*)
				  &TSdata.master)->sin_port));
		    }

		    count=(TSdata.sync_rate>TS_SECS_ASYNC_TRY_MASTER)?
			TS_SECS_ASYNC_TRY_MASTER:
			TS_SECS_ASYNC_TRY_MASTER/TSdata.sync_rate;
		}
		else count--;
		taskDelay(sysClkRateGet()*TSdata.sync_rate);
	    }
	}
	else
	{
	    /* try to find a master */
	    while(TSdata.state==TS_master_dead)
	    {
		stran.type=(TStype)htonl(TS_time_request);
		stran.magic=htonl(TS_MAGIC);

		if(TSgetData((char*)&stran,sizeof(stran),soc_bc,
		    (struct sockaddr*)&sin_bc,&TSdata.master,&diff_time)<0)
		{ Debug0(2,"no reply from master server\n"); }
		else
		    TSdata.state=TS_master_alive;
		taskDelay(sysClkRateGet()*TS_SECS_SYNC_TRY_MASTER);
	    }
	}
    }
}

static long TSstartSyncClient()
{
    return taskSpawn("ts_syncC",TS_SYNC_CLIENT_PRI,VX_FP_TASK|VX_STDIO,5000,
    	(FUNCPTR)TSsyncClient,0,0,0,0,0,0,0,0,0,0);
}

/*	
	TSsyncClient() - Client task that listens for sync time stamp on a port
	and verifies with this IOC sync time stamp.  This is only to be used
	with event system timing.
*/
static void TSsyncClient()
{
    TSstampTrans stran;
    struct sockaddr_in sin;
    struct sockaddr fs;
    int num,mlen,soc,fl;
    struct timespec mast_time;
    struct timespec mytime;
    fd_set readfds;
    int key;

    /* if no master then use unix and ntp to sync time
    all aync slaves will operate in polled mode */

    if(TSdata.type!=TS_sync_slave)	return;

    /* check for master to be there every so many minutes */
    if(TSdata.state != TS_master_alive)
	while( TSsetClockFromMaster()<0 )
	    taskDelay(sysClkRateGet()*TS_SECS_SYNC_TRY_MASTER);
    if( (soc=TSgetSocket(TSdata.slave_port,&sin)) <0)
    { Debug0(1,"TSgetSocket failed\n"); return; }
    while(1)
    {
        unsigned long mask;
        unsigned long syncReportThreshold;
        unsigned long syncUnixReportThreshold;
        double        reportThreshold;
        double        unixReportThreshold;

	FD_ZERO(&readfds);
	FD_SET(soc,&readfds);

	/* could set up timeout for (sync_rate + epsilon)
	to see if one is missed */

	num=select(FD_SETSIZE,&readfds,(fd_set*)NULL,(fd_set*)NULL,NULL);
	if(num==ERROR) { perror("select failed"); continue; }

        fl = sizeof(fs);
        memset(&stran,0,sizeof(stran));
	if((mlen=recvfrom(soc,(char*)&stran,sizeof(stran),0,&fs,&fl))<0)
	{ perror("recvfrom failed"); continue; }

	if(ntohl(stran.magic)!=TS_MAGIC)
	{
	    TSprintf("TSsyncClient: invalid packet received\n");
	    continue;
	}
        mask = ntohl(stran.mask);
        syncReportThreshold = ntohl(stran.syncReportThreshold);
        syncUnixReportThreshold = ntohl(stran.syncUnixReportThreshold);
        if(TSdata.syncReportThreshold>syncReportThreshold)
            syncReportThreshold = TSdata.syncReportThreshold;
        if(TSdata.syncUnixReportThreshold>syncUnixReportThreshold)
            syncUnixReportThreshold = TSdata.syncUnixReportThreshold;
	mast_time.tv_sec=ntohl(stran.master_time.tv_sec);
	mast_time.tv_nsec=ntohl(stran.master_time.tv_nsec);
        reportThreshold = (double)syncReportThreshold/1000.0;
        unixReportThreshold = (double)syncUnixReportThreshold/1000.0;
        mytime = TSdata.event_table[TSdata.sync_event];
	/* verify the sync event with this one */
	if(mytime.tv_sec!=mast_time.tv_sec || mytime.tv_nsec!=mast_time.tv_nsec)
	{
            int dir;
            struct timespec diffFromMaster;
            double timeDiff;

            dir = TScalcDiff(&mytime, &mast_time,&diffFromMaster);
            timeDiff = diffFromMaster.tv_sec;
            timeDiff += diffFromMaster.tv_nsec/(double)TS_BILLION;
            if(dir<0) timeDiff = -timeDiff;
	    key=intLock();
	    TSdata.event_table[TSdata.sync_event].tv_sec=mast_time.tv_sec;
	    TSdata.event_table[TSdata.sync_event].tv_nsec=mast_time.tv_nsec;
	    intUnlock(key);
            if(mask&TS_STAMP_FROM_UNIX) {
                if(fabs(timeDiff) >= unixReportThreshold) {
                   TSprintf("TSasynClient: Time checked with NTP via master, "
                             "adjusted by %f seconds.\n",timeDiff);
                }
            } else {
                if(fabs(timeDiff) >= reportThreshold) {
                   TSprintf("TSasynClient: Time checked with master, "
                            "adjusted by %f seconds\n",timeDiff);
                }
            }
	}
    }
    close(soc);
    return;
}

static long TSstartStampServer()
{
    return taskSpawn("ts_stamp",TS_STAMP_SERVER_PRI,VX_FP_TASK|VX_STDIO,5000,
	(FUNCPTR)TSstampServer,0,0,0,0,0,0,0,0,0,0);
}

/*	
	TSstampServer() - Server task on master timing IOC that listens for 
	time stamp requests and sync request from slaves.
*/
static void TSstampServer()
{
    TSstampTrans stran;
    struct sockaddr_in sin;
    struct sockaddr fs;
    struct timespec ts;
    int mlen,soc,fl;
    TStype type;

    if( (soc=TSgetSocket(TSdata.master_port,&sin)) <0)
    { Debug0(1,"TSgetSocket failed\n"); return; }

    stran.type=(TStype)htonl(TS_time_request);
    stran.magic=htonl(TS_MAGIC);
	
    while(1)
    {
	fl=sizeof(fs);
	if((mlen=recvfrom(soc,(char*)&stran,sizeof(stran),0,&fs,&fl))<0)
	{ perror("recvfrom failed"); continue; }

	if(ntohl(stran.magic)!=TS_MAGIC)
	{
	    TSprintf("TSstampServer(): invalid packet received\n");
	    continue;
	}

	type=(TStype)ntohl(stran.type);
	switch(type)
	{
	    case TS_time_request:
		TSgetTimeStamp(TSdata.sync_event,&ts);
		stran.master_time.tv_sec=htonl(ts.tv_sec);
		stran.master_time.tv_nsec=htonl(ts.tv_nsec);

		if(TSdata.has_event_system)
		    TSaccurateTimeStamp(&ts);
		else
		    TScurrentTimeStamp(&ts);
		stran.current_time.tv_sec=htonl(ts.tv_sec);
		stran.current_time.tv_nsec=htonl(ts.tv_nsec);
		stran.sync_rate = htonl(TSdata.sync_rate);
		stran.clock_hz = htonl(TSdata.clock_hz);
		Debug0(4,"Slave requesting time\n");
		if(sendto(soc,(char*)&stran,sizeof(stran),0,&fs,fl)<0)
		{ perror("sendto to slave failed"); }
		break;
	    case TS_sync_request: TSforceSync(0); break;
	    default:
		Debug0(1,"Unknown transaction type from slave\n");
	}
    }
    close(soc);
    return;
}

/*************************************************************************/
		/* utility routines for managing time retrieval */
/*************************************************************************/

/* get the current time from time stamp support software */
long TScurrentTimeStamp(struct timespec* sp)
{
    TSgetTimeStamp(0,sp);
    return 0;
}

/* get the current time from time stamp support software */
/* can be called from interrupt level !!*/
long TSaccurateTimeStamp(struct timespec* sp)
{
    struct timespec ts;
    unsigned long ticks;

    if(!TSinitialized) {
        logMsg("TSaccurateTimeStamp called but !TSinitialized\n",0,0,0,0,0,0);
        return -1;
    }
    if(TSdata.has_direct_time==1)
    {
	TSgetTime(sp);
	return 0;
    }

    TSgetTicks(0,&ticks); /* add in the board time */
    *sp = TSdata.event_table[TSdata.sync_event];
    /* calculate a time stamp from the tick count */
    ts.tv_sec = ticks / TSdata.clock_hz;
    ts.tv_nsec=(ticks-(ts.tv_sec*TSdata.clock_hz))*TSdata.clock_conv;

    sp->tv_sec += ts.tv_sec;
    sp->tv_nsec += ts.tv_nsec;

    /* adjust seconds if needed */
    if(sp->tv_nsec >= TS_BILLION)
    {
	sp->tv_sec++;
	sp->tv_nsec -= TS_BILLION;
    }
    return 0;
}

/* get the current time from vxWorks time clock */
static long TSgetCurrentTime(struct timespec* ts)
{
    long rc;
    rc=clock_gettime(CLOCK_REALTIME,ts);
    ts->tv_sec = TSepochUnixToEpics(ts);
    return rc;
}

/* routine for causing sync to occur (not a hardware one) */
static long TSforceSoftSync(int Card)
{
    semGive(TSdata.sync_occurred);
    return 0;
}

/*
	TSaddStamp - Add time stamp op1 to time stamp op2 giving time stamp
	result
*/
static void TSaddStamp(	struct timespec* result,
    struct timespec* op1, struct timespec* op2)
{
    result->tv_sec = op1->tv_sec + op2->tv_sec;
    result->tv_nsec = op1->tv_nsec + op2->tv_nsec;

    /* adjust seconds if needed */
    if(result->tv_nsec >= TS_BILLION)
    {
	result->tv_sec++;
	result->tv_nsec -= TS_BILLION;
    }
    return;
}

/*	
    TScalcDiff() - Calculate the difference between to time stamps. The
    difference between arguments 'a' and 'b' is returned in 'diff'.  The 
    return value from this routine is the direction: (1)=a>b, (-1)=a<b.
*/
static long TScalcDiff(struct timespec* a, 
    struct timespec* b, struct timespec* diff)
{
    struct timespec *big_time,*small_time;
    int dir;

    /* ---------------------this block sucks--------------------*/
    /* calculate difference  - this sucks */
    if(a->tv_sec < b->tv_sec)	     { dir=-1; big_time=b; small_time=a; }
    else if(a->tv_sec > b->tv_sec)   { dir=1; small_time=b; big_time=a; }
    else if(a->tv_nsec == b->tv_nsec){ dir=0; small_time=b; big_time=a;}
    else if(a->tv_nsec < b->tv_nsec) { dir=-1; big_time=b; small_time=a; }
    else			     { dir=1; small_time=b; big_time=a; }

    diff->tv_sec=big_time->tv_sec-small_time->tv_sec;

    if(big_time->tv_nsec>=small_time->tv_nsec)
	diff->tv_nsec=big_time->tv_nsec-small_time->tv_nsec;
    else
    {
	diff->tv_nsec = TS_BILLION + big_time->tv_nsec-small_time->tv_nsec;
	diff->tv_sec--;
    }
    /* ---------------------this block sucks--------------------*/
    return dir;
}

/* network utilities*/
/*	
    TSgetBroadcastAddr() - Determine the broadcast address, this is 
    directly from the Sun Network Programmer's guide.
*/
static long TSgetBroadcastAddr(int soc, struct sockaddr* sin)
{
    struct ifconf ifc;
    struct ifreq* ifr;
    struct ifreq* save;
    char buf[BUFSIZ];
    int tot,i;

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if(ioctl(soc,SIOCGIFCONF,(int)&ifc) < 0)
    { perror("ioctl SIOCGIFCONF failed"); return -1; }

    ifr = ifc.ifc_req;
    tot = ifc.ifc_len/sizeof(struct ifreq);
    save=(struct ifreq*)NULL;
    i=0;
    do
    {
	if(ifr->ifr_addr.sa_family==AF_INET)
	{
	    if(ioctl(soc,SIOCGIFFLAGS,(int)ifr)<0)
	    { perror("ioctl SIOCGIFFLAGS failed"); return -1; }

	    if( (ifr->ifr_flags&IFF_UP) && 
		!(ifr->ifr_flags&IFF_LOOPBACK) &&
		(ifr->ifr_flags&IFF_BROADCAST))
	    { save=ifr; }
	}
        /*ifreq_size is defined in osiSock.h*/
        ifr = (struct ifreq*)((char *)ifr + ifreq_size(ifr));
    } while( !save && ++i<tot );

    if(save)
    {
	if(ioctl(soc,SIOCGIFBRDADDR,(int)save)<0)
	{ perror("ioctl SIOCGIFBRDADDR failed"); return -1; }
	memcpy((char*)sin,(char*)&save->ifr_broadaddr,
				sizeof(save->ifr_broadaddr));
    }
    else
    { Debug0(1,"no broadcast address found\n"); return -1; }
    return 0;
}

static int TSgetNtpSocketAddr(struct sockaddr_in *sin)
{
    char host_addr[BOOT_ADDR_LEN];
    BOOT_PARAMS bootParms;
    int status;
    int soc;

    if(envGetConfigParam(&EPICS_TS_NTP_INET,BOOT_ADDR_LEN,host_addr)==NULL 
    || strlen(host_addr)==0) {
	/* use boot host if the environment variable not set */
	bootStringToStruct(sysBootLine,&bootParms);
	/* bootParms.had = host IP address */
	strncpy(host_addr,bootParms.had,BOOT_ADDR_LEN);
    }
    if( (soc=TSgetSocket(0,sin)) <0) {
        TSprintf("TSgetNtpSocketAddrn TSgetsocket failed\n");
        return -1;
    }
    /* well known registered NTP port - or whatever port they specify */
    status = aToIPAddr (host_addr, UDP_NTP_PORT, sin);
    if (status) {
        TSprintf("TSgetNtpSocketAddr aToIPAddr failed\n");
        close(soc);
        return -1;
    }
    return soc;
}

/* get a UDP socket for a port, return a sockaddr for it.  */
static int TSgetSocket(int port, struct sockaddr_in* sin)
{
    int soc; 

    sin->sin_port=htons(port);
    sin->sin_family=AF_INET;
    sin->sin_addr.s_addr=htonl(INADDR_ANY);
    if( (soc=socket(AF_INET,SOCK_DGRAM,0)) < 0 )
    { perror("socket create failed"); return -1; }
    Debug(5,"sizeof sin = %d\n", (int) sizeof(struct sockaddr_in));
    if( bind(soc,(struct sockaddr*)sin,sizeof(struct sockaddr_in)) < 0 )
    { perror("socket bind failed"); close(soc); return -1; }

    return soc;
}

/*	
	TSgetBroadcastSocket() - return a broadcast socket for a port, return
	a sockaddr also.
*/
static int TSgetBroadcastSocket(int port, struct sockaddr_in* sin)
{
    int on=1;
    int soc;

    sin->sin_port=htons(port);
    sin->sin_family=AF_INET;
    sin->sin_addr.s_addr=htonl(INADDR_ANY);
    if( (soc=socket(AF_INET,SOCK_DGRAM,0)) < 0 )
    { perror("socket create failed"); return -1; }

    setsockopt(soc,SOL_SOCKET,SO_BROADCAST,(char*)&on,sizeof(on));

    if( bind(soc,(struct sockaddr*)sin,sizeof(struct sockaddr_in)) < 0 )
    { perror("socket bind failed"); close(soc); return -1; }

    if( TSgetBroadcastAddr(soc,(struct sockaddr*)sin) < 0 ) return -1;
    return soc;
}

/*    attempt to get data from a socket after sending a request to it.
      0ptionally return round trip time and the sockaddr of the responsedent.
*/
static long TSgetData(char* buf, int buf_size, int soc,
    struct sockaddr* to_sin, struct sockaddr* from_sin,
    struct timespec* round_trip)
{
    int retry_count=0;
    int num,mlen,flen;
    fd_set readfds;
    volatile unsigned long s,us;
    struct timeval timeOut;
    struct timespec send_time,recv_time;
    struct sockaddr local_sin;

    if (!from_sin) {
        /* Tornado 2.2 doesn't like NULLs in recvfrom() */
        from_sin = &local_sin;
    }
    flen = sizeof(*from_sin);

    /*
     * joh 08-26-99
     * added this code which removes responses laying around from
     * requests made in the past that timed out
     */
    Debug(8,"removing stale responses %s\n", "");
    while (1) {
        int status;
        
        status = ioctl (soc, FIONREAD, (int) &mlen);
        if (status<0) {
            Debug(1,"ioctl FIONREAD failed because \"%s\"?\n", strerror(errno));
            break;
        }
        else if (mlen==0) {
            break;
        }
        Debug(1,"removing stale response of %d bytes\n", mlen);
        recvfrom(soc,buf,buf_size,0,from_sin,&flen);
    }
    
    /* convert millisecond time out to seconds/microseconds */
    s=TSdata.time_out/1000; us=(TSdata.time_out-(s*1000))*1000;
    Debug(6,"time_out Second=%lu\n",s);
    Debug(6,"time_out Microsecond=%lu\n",us);
    do
    {
	Debug(8,"sendto port %d\n",
	    ntohs(((struct sockaddr_in*)to_sin)->sin_port));
	if(round_trip) clock_gettime(CLOCK_REALTIME,&send_time);
	if( sendto(soc,buf,buf_size,0,to_sin,sizeof(struct sockaddr)) < 0 )
	    { perror("sendto failed"); return -1; }
	FD_ZERO(&readfds); FD_SET(soc,&readfds);
	timeOut.tv_sec=s; timeOut.tv_usec=us;
	num=select(FD_SETSIZE,&readfds,(fd_set*)NULL,(fd_set*)NULL,&timeOut);
	Debug(9,"select returned %d\n", num);
	if(round_trip) clock_gettime(CLOCK_REALTIME,&recv_time);
	if(num==ERROR) { perror("select failed"); return -1; }
    }
    while(num==0 && ++retry_count<TS_RETRY_COUNT);
    if(retry_count >= TS_RETRY_COUNT)
	{ Debug0(5,"TSgetData - retry count exceeded\n"); return -1; }
    else
    {
	/* data available */
	mlen=recvfrom(soc,buf,buf_size,0,from_sin,&flen);
	if(mlen < 0) { perror("recvfrom failed"); return -1; }
	if(from_sin)
	{
	    Debug(8,"recvfrom port %d\n",
		ntohs(((struct sockaddr_in*)from_sin)->sin_port));
	    Debug(8,"flen = %d\n",flen);
	    Debug(8,"mlen = %d\n",mlen);
	}
	if(round_trip) TScalcDiff(&send_time,&recv_time,round_trip);
    }
    return mlen;
}

/*************************************************************************/
		/* all the functions that follow are test functions */
/*************************************************************************/

/*	test function to print the IOC real time clock value.  */
void TSprintRealTime()
{
    struct timespec tp;

    if(!TSinitialized) TSinit();
    TSgetTime(&tp);
    printf("real time clock = %lu,%lu\n",tp.tv_sec,tp.tv_nsec);
    printf("EPICS clock = %lu,%lu\n",
	TSdata.event_table[TSdata.sync_event].tv_sec,
	TSdata.event_table[TSdata.sync_event].tv_nsec);
    return;
}

/*	test function to print an event time stamp */
void TSprintTimeStamp(int num)
{
    struct timespec tp;

    if(!TSinitialized) TSinit();
    TSgetTimeStamp(num,&tp);
    printf("event %d occurred: %lu.%lu\n",num,tp.tv_sec,tp.tv_nsec);
    return;
}

/*	test function to print the current time using event system */
void TSprintCurrentTime()
{
    struct timespec tp;

    if(!TSinitialized) TSinit();
    TScurrentTimeStamp(&tp);
    printf("Current Event System time: %lu.%lu\n",tp.tv_sec,tp.tv_nsec);
    TSaccurateTimeStamp(&tp);
    printf("Accurate Event System time: %lu.%lu\n",tp.tv_sec,tp.tv_nsec);
    return;
}

/*	test function to query the boot server for the current time.  */
void TSprintUnixTime()
{
    struct timespec ts;

    if(!TSinitialized) TSinit();
    if(TSgetUnixTime(&ts)!=0)
    {
	printf("Could not get Unix time\n");
	return;
    }
    printf("boot server time clock = %lu, %lu\n",ts.tv_sec,ts.tv_nsec);
    return;
}

/*	test function to print the master timing IOC time stamp.  */
void TSprintMasterTime()
{
    struct timespec ts;

    if(!TSinitialized) TSinit();
    if(TSgetMasterTime(&ts)!=0)
    {
	printf("Could not get Unix time\n");
	return;
    }
    printf("master time clock = %lu, %lu\n",ts.tv_sec,ts.tv_nsec);
    return;
}

/* gross and horrid example */
long TSgetFirstOfYearVx(struct timespec* ts)
{
    time_t tloc;
    struct tm t;

    if(!TSinitialized) TSinit();
    time(&tloc); /* retrieve the current time */
    localtime_r(&tloc,&t);
    t.tm_sec=0;
    t.tm_min=0;
    t.tm_hour=t.tm_isdst?1:0;
    t.tm_mday=1;
    t.tm_mon=0;
    tloc=mktime(&t);
    ts->tv_sec=tloc;
    ts->tv_nsec=0;
    return 0;
}
