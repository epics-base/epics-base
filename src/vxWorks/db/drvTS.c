
/*
 * $Log$
 * Revision 1.9  1995/05/22  15:21:39  jbk
 * updates to allow direct read of time stamps from event systems
 *
 * Revision 1.8  1995/02/13  03:54:21  jhill
 * drvTS.c - use errMessage () as discussed with jbk
 * iocInit.c - static => LOCAL for debugging and many changes
 * 		to the parser for resource.def so that we
 * 		allow white space between tokens in env var
 *
 * Revision 1.7  1995/02/02  17:15:55  jbk
 * Removed the stinking message "Cannot contact master timing IOC ".
 *
 * Revision 1.6  1995/02/01  15:29:54  winans
 * Added a type field to the configure command to disable the use of the event
 * system hardware if desired.
 *
 * Revision 1.5  1994/12/16  15:51:21  winans
 * Changed error message in the event system error handler & added a conditional
 * based on a debug flag to print it... defaults to off.  (Per request from MRK.)
 *
 * Revision 1.4  1994/10/28  20:15:10  jbk
 * increased the USP packet time-out to 250ms, added a parm to the configure()
 * routine to let user specify it.
 *
 */

/**************************************************************************
 *
 *     Author:	Jim Kowalkowski
 *
 * Modification Log:
 * -----------------
 * .01	01-06-94	jbk	initial version
 * .02	03-01-94	jbk	magic # in packets, network byte order check
 * .03	03-02-94	jbk	current time always uses 1 tick watch dog update
 *
 ***********************************************************************/
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

#define MAKE_DEBUG TSdriverDebug
#define TS_DRIVER

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
#include <envDefs.h>

#include <errMdef.h>
#include <drvSup.h>
#include <drvTS.h>

#define TSprintf epicsPrintf

#define DEFAULT_TIME	0
#define NO_EVENT_SYSTEM	1

/* functions used by this driver */
static long TSgetUnixTime(struct timespec*);
static long TSgetMasterTime(struct timespec*);
static long TSgetBroadcastAddr(int soc, struct sockaddr*);
static int  TSgetSocket(int port, struct sockaddr_in* sin);
static int  TSgetBroadcastSocket(int port, struct sockaddr_in* sin);
static void TSsyncServer();
static void TSsyncClient();
static long TSasyncClient();
static long TSsyncTheTime(struct timespec* cts,
							struct timespec* ts, unsigned long tol);
static long TSgetData(char* buf, int buf_size, int soc,
					struct sockaddr* to_sin, struct sockaddr* from_sin,
					struct timespec* round_trip);
static void TSaddStamp(	struct timespec* result,
						struct timespec* op1, struct timespec* op2);
static void TSwdIncTime();
static void TSwdIncTimeSync();
static void TSstampServer();
static void TSeventHandler(int Card,int EventNum,unsigned long Ticks);
static void TSerrorHandler(int Card, int ErrorNum);
static long TSsetClockFromUnix();
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

/* global functions */
#ifdef __cplusplus
extern "C" {
#endif
long TSinit();		/* called by iocInit currently */
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
TSinfo TSdata = { TS_master_dead, TS_async_slave, TS_async_none,
					0,NULL,
					TS_SYNC_RATE_SEC,TS_CLOCK_RATE_HZ,0,TS_TIME_OUT_MS,0,
					TS_MASTER_PORT,TS_SLAVE_PORT,1,0,0,0,0,
					NULL, NULL, NULL };

extern char* sysBootLine;

static WDOG_ID wd;
static long correction_factor = 0;
static long correction_count = 0;

/* ntp time stamp conversion tables */
unsigned long bit_pat[32] = {
    0x80000000, 0x40000000, 0x20000000, 0x10000000,
    0x08000000, 0x04000000, 0x02000000, 0x01000000,
    0x00800000, 0x00400000, 0x00200000, 0x00100000,
    0x00080000, 0x00040000, 0x00020000, 0x00010000,
    0x00008000, 0x00004000, 0x00002000, 0x00001000,
    0x00000800, 0x00000400, 0x00000200, 0x00000100,
    0x00000080, 0x00000040, 0x00000020, 0x00000010,
    0x00000008, 0x00000004, 0x00000002, 0x00000001,
};
 
unsigned long ns_val[32] = {
    500000000,250000000,125000000,62500000,31250000,15625000,7812500,3906250,
    1953125,976562,488281,244140,122070,61035,30517,15258,
    7629,3814,1907,953,476,238,119,59,
    29,14,7,4,2,1,0,0
};

static long TSreturnError() { return -1; }
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

/*-----------------------------------------------------------------------*/
/*	
	TSreport() - report information about the state of the time stamp
	support software
*/
long TSreport()
{
	switch(TSdata.type)
	{
	case TS_direct_master:	TSprintf("Direct timing master\n"); break;
	case TS_sync_master:	TSprintf("Event timing master\n"); break;
	case TS_async_master:	TSprintf("Soft timing master\n"); break;
	case TS_direct_slave:	TSprintf("Direct timing slave\n"); break;
	case TS_sync_slave:		TSprintf("Event timing slave\n"); break;
	case TS_async_slave:	TSprintf("Soft timing slave\n"); break;
	default: break;
	}
	switch(TSdata.state)
	{
	case TS_master_alive:	TSprintf("Master timing IOC alive\n"); break;
	case TS_master_dead:	TSprintf("Master timing IOC dead\n"); break;
	default: break;
	}
	switch(TSdata.async_type)
	{
	case TS_async_none:		TSprintf("No clock synchronization\n"); break;
	case TS_async_private:	TSprintf("Sync protocol with master\n"); break;
	case TS_async_ntp:		TSprintf("NTP sync with unix server\n"); break;
	case TS_async_time:		TSprintf("Time protocol sync with unix\n"); break;
	default: break;
	}
	TSprintf("Clock Rate in Hertz = %lu\n",TSdata.clock_hz);
	TSprintf("Sync Rate in Seconds = %lu\n",TSdata.sync_rate);
	TSprintf("Master communications port = %d\n",TSdata.master_port);
	TSprintf("Slave communications port = %d\n",TSdata.slave_port);
	TSprintf("Total events supported = %d\n",TSdata.total_events);
	TSprintf("Request Time Out = %lu milliseconds\n",TSdata.time_out);

	if(TSdata.UserRequestedType)
		TSprintf("\nForced to not use the event system\n");

	if(TSdata.has_direct_time)
		TSprintf("Event system has time directly available\n");

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

	switch(TSdata.type)
	{
	case TS_async_master:
	case TS_async_slave:
		*sp = TSdata.event_table[TSdata.sync_event];
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
			*sp = TSdata.event_table[0]; /* one tick watch dog maintains */
			break;
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
long TSinit()
{
	SYM_TYPE stype;
	char tz[100],min_west[20];

	Debug(5,"In TSinit()\n",0);

	/* 0=default, 1=none, 2=direct */

	if(	TSdata.UserRequestedType==DEFAULT_TIME)
	{
		/* default configuration probe */
		/* ------------------------------------------------------------- */
		/* find the lower level event system functions */
		if(symFindByName(sysSymTbl,"_ErHaveReceiver",
						(char**)&TShaveReceiver,&stype)==ERROR)
			TShaveReceiver = TShaveReceiverError;
	
		if(symFindByName(sysSymTbl,"_ErGetTicks",
						(char**)&TSgetTicks,&stype)==ERROR)
			TSgetTicks = TSgetTicksError;
	
		if(symFindByName(sysSymTbl,"_ErRegisterEventHandler",
						(char**)&TSregisterEventHandler,&stype)==ERROR)
			TSregisterEventHandler = TSregisterEventHandlerError;
	
		if(symFindByName(sysSymTbl,"_ErRegisterErrorHandler",
						(char**)&TSregisterErrorHandler,&stype)==ERROR)
			TSregisterErrorHandler = TSregisterErrorHandlerError;
	
		if(symFindByName(sysSymTbl,"_ErForceSync",
						(char**)&TSforceSync,&stype)==ERROR)
			TSforceSync = TSforceSoftSync;
	
		if(symFindByName(sysSymTbl,"_ErDirectTime",
						(char**)&TSdirectTime,&stype)==ERROR)
			TSdirectTime = TSdirectTimeError;
	
		if(symFindByName(sysSymTbl,"_ErDriverInit",
						(char**)&TSdriverInit,&stype)==ERROR)
			TSdriverInit = TSdriverInitError;
	
		if(symFindByName(sysSymTbl,"_ErGetTime",
						(char**)&TSgetTime,&stype)==ERROR)
			TSgetTime = TSgetCurrentTime;
	
		if(symFindByName(sysSymTbl,"_ErSyncEvent",
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
		Debug(5,"TSinit() - no event receiver\n",0);
		TSdata.total_events=1;
		TSdata.sync_event=0;
		TSdata.clock_hz=sysClkRateGet();
		TSdata.clock_conv=TS_BILLION / TSdata.clock_hz;
		TSdata.has_event_system = 0;
	}
	else
		TSdata.has_event_system = 1;

	if(TSdirectTime()>0) TSdata.has_direct_time=1;

	/* allocate the event table */
	TSdata.event_table=(struct timespec*)malloc(
						TSdata.total_events*sizeof(struct timespec));

	if(TSdata.master_timing_IOC)
	{
		/* master */
		Debug(5,"TSinit() - I am master\n",0);
		if(TSdata.has_direct_time)
			TSdata.type=TS_direct_master;
		else
		{
			if(TSdata.has_direct_time)
				TSdata.type=TS_sync_master;
			else
				TSdata.type=TS_async_master;
		}
	}
	else
	{
		/* slave */
		Debug(5,"TSinit() - I am slave\n",0);
		if(TSdata.has_direct_time)
			TSdata.type=TS_direct_slave;
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
/*
	old USE_GOOD_TIME code didn't start soft clock if event system present

	else
	{
		Debug(5,"TSinit() - starting soft clock\n",0);
		TSstartSoftClock();
		Debug(5,"TSinit() - started soft clock\n",0);
	}
*/

	/* always start the soft clock */
	if(TSdata.has_direct_time==0) TSstartSoftClock();

	/* get time from boot server Unix system */
	if(TSdata.master_timing_IOC)
	{
		/* master */
		if(TSsetClockFromUnix()<0)
		{
			/* this is bad, cannot get time - accessing starts ticking */
			struct timespec tp;
			clock_gettime(CLOCK_REALTIME,&tp);
			TSprintf("Failed to set clock from Unix server\n");
		}
		Debug(5,"TSinit() - tried to get clock from unix\n",0);

		/* start the time stamp info server */
		if(TSstartStampServer()==ERROR)
		{
			TSprintf("Failed to start stamp server\n");
			return -1;
		}
		Debug(5,"TSinit() - stamp server started \n",0);

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
			Debug(5,"TSinit() - sync server started  \n",0);
		}
	}
	else
	{
		/* slave */
		if(TSsetClockFromUnix()<0)
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
			Debug(5,"TSinit() - async client started  \n",0);
		}
		else
		{
			/* this task sync with master */
			if(TSstartSyncClient()==ERROR)
			{
				TSprintf("Failed to start sync client\n");
				return -1;
			}
			Debug(5,"TSinit() - sync client started  \n",0);
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

/**********************************************************************/
	/* following are watch dog routines for soft time support */
/**********************************************************************/

/*--------------------------------------------------------------------*/
/*	
	TSstartSoftClock() - start the soft clock watch dog
*/
static void TSstartSoftClock()
{
	/* simple watch dog to fire off syncs to slaves */
	Debug(5,"start watch dog at rate %d\n",sysClkRateGet()*TSdata.sync_rate);
	wd = wdCreate();
	if(TSdata.has_event_system)
	{
		Debug(8,"Starting sync time watch dog\n",0);
		wdStart(wd,1,(FUNCPTR)TSwdIncTimeSync,NULL);
	}
	else
	{
		Debug(8,"Starting async time watch dog\n",0);
		wdStart(wd,1,(FUNCPTR)TSwdIncTime,NULL);
	}
	return;
}

/*--------------------------------------------------------------------*/
/*	
	TSwdIncTime() - increment the time stamp at a 60 Hz rate

	Note: called at interrupt level!
*/
static void TSwdIncTime()
{
	int key;

	wdStart(wd,1, (FUNCPTR)TSwdIncTime,NULL);
	/* update the event table */
	key=intLock();

	if(correction_count)
	{
		correction_count--;
		TSdata.event_table[TSdata.sync_event].tv_nsec +=
			TSdata.clock_conv + correction_factor;
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

static void TSwdIncTimeSync()
{
	wdStart(wd,1, (FUNCPTR)TSwdIncTimeSync,NULL);
	TSaccurateTimeStamp(&TSdata.event_table[0]);
}

/**********************************************************************/
	/* following are all interrupt service routines */
/**********************************************************************/

/*-----------------------------------------------------------------------*/
/*	
	TSeventHandler() - receive events here from event system and 
	update the event table 

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

/*-----------------------------------------------------------------------*/
/*	
	TSerrorHandler() - receive errors from event system

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
		  logMsg("***TSerrorHandler: event system error: TAXI violation",0,0,0,0,0,0);
		  break;
		case 2:
		  logMsg("***TSerrorHandler: event system error: lost heartbeat",0,0,0,0,0,0);
		  break;
		case 3:
		  logMsg("***TSerrorHandler: event system error: lost events",0,0,0,0,0,0);
		  break;
		default:
			logMsg("***TSerrorHandler: unknown error %d from event system", ErrorNum,0,0,0,0,0);
		}
	}
	return;
}

/**************************************************************************/
/* the following are utilities for initially getting and setting the time */
/**************************************************************************/

/*-----------------------------------------------------------------------*/
/*	
	TSgetUnixTime() - ask the boot server for the time using the 
	time protocol.  This is only the time to the nearest second
	A better way would be to use the NTP transactions to boot server 
*/

static long TSgetUnixTime(struct timespec* ts)
{
	BOOT_PARAMS bootParms;
	unsigned long buf_data,timeValue;
	TS_NTP buf_ntp;
	struct sockaddr_in sin;
	int soc;
	char host_addr[BOOT_ADDR_LEN];

	Debug(2,"in TSgetUnixTime()\n",0);

	if(envGetConfigParam(&EPICS_TS_NTP_INET,BOOT_ADDR_LEN,host_addr)==NULL ||
		strlen(host_addr)==0)
	{
		/* use boot host if the environment variable not set */
		bootStringToStruct(sysBootLine,&bootParms);
		/* bootParms.had = host IP address */
		strncpy(host_addr,bootParms.had,BOOT_ADDR_LEN);
	}

	if( (soc=TSgetSocket(0,&sin)) <0)
		{ Debug(1,"TSgetsocket failed\n",0); return -1; }

	/* set up for ntp transaction to boot server */
	Debug(5,"host addr = %s\n",host_addr);
	sin.sin_addr.s_addr = inet_addr(host_addr);
	sin.sin_port = UDP_NTP_PORT; /* well known registered NTP port */

	TSdata.async_type=TS_async_ntp;

	memset(&buf_ntp,0,sizeof(buf_ntp));
	buf_ntp.info[0]=0x0b;

	if(TSgetData((char*)&buf_ntp,sizeof(buf_ntp),soc,
		(struct sockaddr*)&sin,NULL,NULL)<0)
	{
		Debug(2,"no reply from NTP server\n",0);

		sin.sin_port = UDP_TIME_PORT; /* well known registered time port */
		TSdata.async_type=TS_async_time;
		buf_data=0;
		if(TSgetData((char*)&buf_data,sizeof(buf_data),soc,
			(struct sockaddr*)&sin,NULL,NULL)<0)
		{
			Debug(2,"no reply from Time server\n",0);
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
		int i;
		/* this is ntp  - need to convert to ns */
		ts->tv_sec=ntohl(buf_ntp.transmit_ts.tv_sec);
		timeValue=ntohl(buf_ntp.transmit_ts.tv_nsec);
		for(i=0,ts->tv_nsec=0;i<32;i++)
			if(bit_pat[i]&timeValue) ts->tv_nsec+=ns_val[i];

		if(MAKE_DEBUG>=2)
			TSprintf("got the NTP time %9.9lu.%9.9lu\n",
				ts->tv_sec,timeValue);
	}
	close(soc);
	return 0;
}

/*-----------------------------------------------------------------------*/
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
	struct timespec curr_time,tran_time,send_time,recv_time;
	struct sockaddr_in sin;
	struct sockaddr fs;
	int soc;

	Debug(3,"TSgetMasterTime() called\n",0);

	if( (soc=TSgetBroadcastSocket(0,&sin)) <0)
		{ Debug(1,"TSgetBroadcastSocket failed\n",0); return -1; }

	sin.sin_port = TSdata.master_port;
	memcpy(&TSdata.hunt,&sin,sizeof(sin));

	stran.type=(TStype)htonl(TS_time_request);
	stran.magic=htonl(TS_MAGIC);

	if(TSgetData((char*)&stran,sizeof(stran),soc,
		(struct sockaddr*)&sin,&fs,&tran_time)<0)
		{ Debug(2,"no reply from master server\n",0); close(soc); return -1; }

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
	Debug(8,"master port=%d\n",((struct sockaddr_in*)&fs)->sin_port);
	TSdata.master = fs;
	TSdata.state = TS_master_alive;

	if(TSdata.type==TS_sync_slave)
	{
		TSdata.clock_hz = ntohl(stran.clock_hz);
		TSdata.sync_rate = ntohl(stran.sync_rate);
		TSdata.clock_conv = TS_BILLION / TSdata.clock_hz;
	}

	/* how long did it take to give transaction? */
	TScalcDiff(&send_time,&recv_time,&tran_time);

	if(MAKE_DEBUG>=6)
	{
		TSprintf("round trip time: %9.9lu.%9.9lu\n",
			tran_time.tv_sec,tran_time.tv_nsec);
		TSprintf("master time: %9.9lu.%9.9lu\n",
			stran.master_time.tv_sec,stran.master_time.tv_nsec);
	}

	tran_time.tv_nsec >>= 2;
	tran_time.tv_sec >>= 2;

	/* add half the round trip estimate to the time stamp from master */
	curr_time.tv_sec = ntohl(stran.current_time.tv_sec);
	curr_time.tv_nsec = ntohl(stran.current_time.tv_nsec);
	TSaddStamp(tsp,&curr_time,&tran_time);

	close(soc);
	return 0;
}

/*-----------------------------------------------------------------------*/
/*	
	TSsetClockFromUnix() - query the time from boot server and set the 
	vxworks clock.
*/
static long TSsetClockFromUnix()
{
	struct timespec tp;

	Debug(3,"in TSsetClockFromUnix()\n",0);

	if(TSgetUnixTime(&tp)!=0) return -1;

	tp.tv_sec-=TS_1900_TO_VXWORKS_EPOCH;

	/* set the vxWorks clock to the correct time */
	if(clock_settime(CLOCK_REALTIME,&tp)<0)
		{ Debug(1,"clock_settime failed\n",0); }

	/* adjust time to use the EPICS EPOCH of 1990 */
	/* this is wrong if leap seconds accounted for */
	tp.tv_sec -= TS_VXWORKS_TO_EPICS_EPOCH;

	if(MAKE_DEBUG>=9)
		TSprintf("set time: %9.9lu.%9.9lu\n", tp.tv_sec,tp.tv_nsec);

	/* set the EPICS event time table sync entry (current time) */
	TSdata.event_table[TSdata.sync_event]=tp;

	if(MAKE_DEBUG>=9)
		TSprintf("epics time: %9.9lu.%9.9lu\n",
			TSdata.event_table[TSdata.sync_event].tv_sec,
			TSdata.event_table[TSdata.sync_event].tv_nsec);

	return 0;
}

/*-----------------------------------------------------------------------*/
/*	
	TSsetClockFromMaster() - set the vxworks clock using the time from 
	the master timing IOC
*/
static long TSsetClockFromMaster()
{
	struct timespec tp;
	int key;

	Debug(3,"in TSsetClockFromMaster()\n",0);

	if(TSgetMasterTime(&tp)<0) return -1;

	key=intLock();
	TSdata.event_table[TSdata.sync_event]=tp;
	intUnlock(key);

	/* adjust time to use the Unix EPOCH of 1900 - not to good */
	/* making this adjustment is not so good */
	tp.tv_sec += TS_VXWORKS_TO_EPICS_EPOCH;
	clock_settime(CLOCK_REALTIME,&tp);

	return 0;
}

/**************************************************************************/
/* broadcast socket creation utilites */
/**************************************************************************/

/*-----------------------------------------------------------------------*/
/*	
	TSgetBroadcastSocket() - return a broadcast socket for a port, return
	a sockaddr also.
*/
static int TSgetBroadcastSocket(int port, struct sockaddr_in* sin)
{
	int on=1;
	int soc;

	sin->sin_port=port;
	sin->sin_family=AF_INET;
	sin->sin_addr.s_addr=htonl(INADDR_ANY);
	
	if( (soc=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0 )
		{ perror("socket create failed"); return -1; }

	setsockopt(soc,SOL_SOCKET,SO_BROADCAST,(char*)&on,sizeof(on));

	if( bind(soc,(struct sockaddr*)sin,sizeof(struct sockaddr_in)) < 0 )
		{ perror("socket bind failed"); close(soc); return -1; }

	if( TSgetBroadcastAddr(soc,(struct sockaddr*)sin) < 0 ) return -1;

	return soc;
}

/*-----------------------------------------------------------------------*/
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
		if(ifr[i].ifr_addr.sa_family==AF_INET)
		{
			if(ioctl(soc,SIOCGIFFLAGS,(int)&ifr[i])<0)
				{ perror("ioctl SIOCGIFFLAGS failed"); return -1; }

			if( (ifr[i].ifr_flags&IFF_UP) && 
				!(ifr[i].ifr_flags&IFF_LOOPBACK) &&
				(ifr[i].ifr_flags&IFF_BROADCAST))
			{ save=&ifr[i]; }
		}
	} while( !save && ++i<tot );

	if(save)
	{
		if(ioctl(soc,SIOCGIFBRDADDR,(int)save)<0)
			{ perror("ioctl SIOCGIFBRDADDR failed"); return -1; }

		Debug(2,"Broadcast address = %8.8x\n",save->ifr_broadaddr);
		memcpy((char*)sin,(char*)&save->ifr_broadaddr,
				sizeof(save->ifr_broadaddr));
	}
	else
		{ Debug(1,"no broadcast address found\n",0); return -1; }
	return 0;
}

/**************************************************************************/
/* time stamp utility routines */
/**************************************************************************/

/*---------------------------------------------------------------------*/
/*
	TSsyncTheTime - given the current time (cts), and a compare time
	stamp (ts), correct the current time clock to match ts to tolorance
	tol.
*/
static long TSsyncTheTime(struct timespec* cts,
							struct timespec* ts, unsigned long tol)
{
	struct timespec diff_time;
	int dir,key;

	dir=TScalcDiff(ts,cts,&diff_time);

	/* see if clock can be corrected in sync window - up to 2 secs per tick */
	if( diff_time.tv_sec <= (TSdata.sync_rate*2) )
	{
		/* clock can be corrected if need be */
		if( diff_time.tv_sec>0 || diff_time.tv_nsec > tol)
		{
			key=intLock();
			correction_count=sysClkRateGet()*TSdata.sync_rate;
			correction_factor=(
				((diff_time.tv_sec/TSdata.sync_rate)*TS_BILLION)+ 
				(diff_time.tv_nsec/correction_count))*dir;
			intUnlock(key);
			Debug(5,"Correction Factor=.%9.9ld\n",correction_factor);
		}
	}
	else
	{
		/* clock can not be corrected  - jam it for now */
		TSdata.event_table[TSdata.sync_event]=*ts;
		if(MAKE_DEBUG>=7)
		{
			TSprintf("Slave not in sync: mine=%9.9lu.%9.9lu!=%lu.%lu=other\n",
				cts->tv_sec, cts->tv_nsec,
				ts->tv_sec, ts->tv_nsec);
			TSprintf("slave diff time: %9.9lu.%9.9lu, tolorance=%lu\n",
				diff_time.tv_sec, diff_time.tv_nsec,tol);
		}
	}
	return 0;
}

/**************************************************************************/
/* varies server that can be running */
/**************************************************************************/

/*-----------------------------------------------------------------------*/
long TSstartSyncServer()
{
	/* run at priority just above CA */
	return taskSpawn("ts_syncS",TS_SYNC_SERVER_PRI,VX_FP_TASK|VX_STDIO,5000,
		(FUNCPTR)TSsyncServer,0,0,0,0,0,0,0,0,0,0);
}
/*-----------------------------------------------------------------------*/
/*	
	TSsyncServer() - Server task that broadcasts the sync time stamp every 
	time a sync event is receive by the event system.
*/
static void TSsyncServer()
{
	TSstampTrans stran;
	struct sockaddr_in sin;
	int soc;

	if( (soc=TSgetBroadcastSocket(0,&sin)) <0)
		{ Debug(1,"TSgetBroadcastSocket failed\n",0); return; }

	sin.sin_port = TSdata.slave_port;

	stran.type=(TStype)htonl(TS_sync_msg);
	stran.magic=htonl(TS_MAGIC);
	stran.sync_rate=htonl(TSdata.sync_rate);
	stran.clock_hz=htonl(TSdata.clock_hz);

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
			clock has a good time in it */

			if(TSgetTime(&ts)==0)
				TSdata.ts_sync_valid=1;
			key=intLock();
			/* adjust time from getTime() to EPICS epoch */
			TSdata.event_table[TSdata.sync_event].tv_nsec=ts.tv_nsec;
			TSdata.event_table[TSdata.sync_event].tv_sec=ts.tv_sec-
				TS_1900_TO_EPICS_EPOCH;
			intUnlock(key);
		}
		stran.master_time.tv_sec=
			htonl(TSdata.event_table[TSdata.sync_event].tv_sec);
		stran.master_time.tv_nsec=
			htonl(TSdata.event_table[TSdata.sync_event].tv_nsec);
	
		if(sendto(soc,(char*)&stran,sizeof(stran),0,
				(struct sockaddr*)&sin,sizeof(sin))<0)
			{ perror("sendto failed"); }
		Debug(8,"time send secs  = %lu\n",stran.master_time.tv_sec);
		Debug(8,"time send nsecs = %lu\n",stran.master_time.tv_nsec);
	}

	close(soc);
	return;
}

/*-----------------------------------------------------------------------*/
long TSstartAsyncClient()
{
	/* run at priority just above CA */
	return taskSpawn("ts_Casync",TS_ASYNC_CLIENT_PRI,VX_FP_TASK|VX_STDIO,5000,
		(FUNCPTR)TSasyncClient,0,0,0,0,0,0,0,0,0,0);
}

/* ----------------------------------------------------------------- */
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
	BOOT_PARAMS bootParms;
	TSstampTrans stran;
	TS_NTP buf_ntp;
	struct sockaddr_in sin_unix,sin_bc,sin_master;
	int count,soc_unix,soc_master,soc_bc,buf_size;
	struct timespec ts,diff_time,cts,curr_time;
	unsigned long nsecs;
	char host_addr[BOOT_ADDR_LEN];

	Debug(2,"in TSasyncClient()\n",0);

	/* could open two sockets here, one to contact unix, one to find master */

	/*------socket for unix server----------*/
	if(envGetConfigParam(&EPICS_TS_NTP_INET,BOOT_ADDR_LEN,host_addr)==NULL ||
		strlen(host_addr)==0)
	{
		/* use boot host if the environment variable not set */
		bootStringToStruct(sysBootLine,&bootParms);
		/* bootParms.had = host IP address */
		strncpy(host_addr,bootParms.had,BOOT_ADDR_LEN);
	}

	if( (soc_unix=TSgetSocket(0,&sin_unix)) <0)
		{ Debug(1,"TSgetSocket failed\n",0); return -1; }

	sin_unix.sin_addr.s_addr = inet_addr(host_addr);
	sin_unix.sin_port = UDP_NTP_PORT;

	/*------socket for finding master----------*/
	if( (soc_bc=TSgetBroadcastSocket(0,&sin_bc)) <0)
		{ Debug(1,"TSgetBroadcastSocket failed\n",0); return -1; }

	sin_bc.sin_port = TSdata.master_port;
	/*-----------------------------------------*/

	while(1)
	{
		/* stop the clock from correcting here  - probably better to use
		a watch dog to stop the clock correction */

		if(TSdata.state==TS_master_alive)
		{
			/* get socket to master */
			Debug(5,"async_client(): master_alive\n",0);
			if( (soc_master=TSgetSocket(0,&sin_master)) <0)
				{ Debug(1,"TSgetSocket failed\n",0); }

			while(TSdata.state==TS_master_alive)
			{
				/* sync with the master as long as it is up */
				Debug(5,"async_client(): syncing with master\n",0);
				stran.type=(TStype)htonl(TS_time_request);
				stran.magic=htonl(TS_MAGIC);

			    if(TSgetData((char*)&stran,sizeof(stran),soc_master,
        			&TSdata.master,NULL,&diff_time)<0)
        		{
					Debug(2,"no reply from master server\n",0);
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
						TSsyncTheTime(&cts,&curr_time,
									1000000000/sysClkRateGet());

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
			Debug(5,"async_client(): using NTP sync\n",0);
			count=0;
			while(TSdata.state==TS_master_dead)
			{
				Debug(5,"async_client(): syncing with unix\n",0);
				memset(&buf_ntp,0,sizeof(buf_ntp));
				buf_ntp.info[0]=0x0b;
				buf_size=sizeof(buf_ntp);

				if(TSgetData((char*)&buf_ntp,sizeof(buf_ntp),soc_unix,
					(struct sockaddr*)&sin_unix,NULL,&diff_time)<0)
				{
					Debug(2,"no reply from NTP server\n",0);
				}
				else
				{
					int i;
					/* get the current time */
					cts=TSdata.event_table[TSdata.sync_event];
					/* adjust the ntp time */
					ts.tv_sec = ntohl(buf_ntp.transmit_ts.tv_sec) -
									TS_1900_TO_EPICS_EPOCH;
					nsecs=ntohl(buf_ntp.transmit_ts.tv_nsec);
					for(i=0,ts.tv_nsec=0;i<32;i++)
						if(bit_pat[i]&nsecs) ts.tv_nsec+=ns_val[i];

					TSsyncTheTime(&cts,&ts,1000000000/sysClkRateGet());
				}
				if(count==0)
				{
					stran.type=(TStype)htonl(TS_time_request);
					stran.magic=htonl(TS_MAGIC);
			 
					if(TSgetData((char*)&stran,sizeof(stran),soc_bc,
						(struct sockaddr*)&sin_bc,&TSdata.master,&diff_time)<0)
						{ Debug(2,"no reply from master server\n",0); }
					else
					{
						TSdata.state=TS_master_alive;
						Debug(8,"master port = %d\n",
							((struct sockaddr_in*)&TSdata.master)->sin_port);
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
					{ Debug(2,"no reply from master server\n",0); }
				else
					TSdata.state=TS_master_alive;

				taskDelay(sysClkRateGet()*TS_SECS_SYNC_TRY_MASTER);
			}
		}
	}
}

/*-----------------------------------------------------------------------*/
static long TSstartSyncClient()
{
	return taskSpawn("ts_syncC",TS_SYNC_CLIENT_PRI,VX_FP_TASK|VX_STDIO,5000,
		(FUNCPTR)TSsyncClient,0,0,0,0,0,0,0,0,0,0);
}
/*-----------------------------------------------------------------------*/
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
		{ Debug(1,"TSgetSocket failed\n",0); return; }

	while(1)
	{
		FD_ZERO(&readfds);
		FD_SET(soc,&readfds);

		/* could set up timeout for (sync_rate + epsilon)
		to see if one is missed */

		num=select(FD_SETSIZE,&readfds,(fd_set*)NULL,(fd_set*)NULL,NULL);
		if(num==ERROR) { perror("select failed"); continue; }

		if((mlen=recvfrom(soc,(char*)&stran,sizeof(stran),0,&fs,&fl))<0)
			{ perror("recvfrom failed"); continue; }

		if(ntohl(stran.magic)!=TS_MAGIC)
		{
			TSprintf("TSsyncClient: invalid packet received\n");
			continue;
		}

		/* get the master time out of packet */
		mast_time.tv_sec=ntohl(stran.master_time.tv_sec);
		mast_time.tv_nsec=ntohl(stran.master_time.tv_nsec);

		Debug(6,"Received sync request from master\n",0);
		if(MAKE_DEBUG>=8)
		{
			TSprintf("time received=%9.9lu.%9.9lu\n",
				mast_time.tv_sec,mast_time.tv_nsec);
		}

		/* verify the sync event with this one */
		if( TSdata.event_table[TSdata.sync_event].tv_sec!=mast_time.tv_sec ||
			TSdata.event_table[TSdata.sync_event].tv_nsec!=mast_time.tv_nsec)
		{
			TSprintf("sync Slave not in sync: %lu,%lu != %lu,%lu\n",
				TSdata.event_table[TSdata.sync_event].tv_sec,
				TSdata.event_table[TSdata.sync_event].tv_nsec,
				mast_time.tv_sec, mast_time.tv_nsec);

			key=intLock();
			TSdata.event_table[TSdata.sync_event].tv_sec=mast_time.tv_sec;
			TSdata.event_table[TSdata.sync_event].tv_nsec=mast_time.tv_nsec;
			intUnlock(key);
		}
	}
	close(soc);
	return;
}

/*-----------------------------------------------------------------------*/
static long TSstartStampServer()
{
	return taskSpawn("ts_stamp",TS_STAMP_SERVER_PRI,VX_FP_TASK|VX_STDIO,5000,
		(FUNCPTR)TSstampServer,0,0,0,0,0,0,0,0,0,0);
}
/*-----------------------------------------------------------------------*/
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
		{ Debug(1,"TSgetSocket failed\n",0); return; }

	stran.type=(TStype)htonl(TS_time_request);
	stran.magic=htonl(TS_MAGIC);
	
	while(1)
	{
		fl=sizeof(fs);
		if((mlen=recvfrom(soc,(char*)&stran,sizeof(stran),0,
					&fs,&fl))<0)
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

			Debug(4,"Slave requesting time\n",0);

			if(sendto(soc,(char*)&stran,sizeof(stran),0,&fs,fl)<0)
				{ perror("sendto to slave failed"); }
			break;
		case TS_sync_request: TSforceSync(0); break;
		default:
			Debug(1,"Unknown transaction type from slave\n",0);
		}
	}

	close(soc);
	return;
}

/*************************************************************************/
		/* utility routines for managing time retrieval */
/*************************************************************************/

/*  set the time stamp support software's current time */
static long TSsetCurrentTime(struct timespec* sp)
{
	TSdata.event_table[TSdata.sync_event]=*sp;
	return 0;
}

/* get the current time from time stamp support software */
long TScurrentTimeStamp(struct timespec* sp)
{
	TSgetTimeStamp(0,sp);
	return 0;
}

/* get the current time from time stamp support software */
long TSaccurateTimeStamp(struct timespec* sp)
{
	struct timespec ts;
	unsigned long ticks;

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
	return clock_gettime(CLOCK_REALTIME,ts);
}

/* routine for causing sync to occur (not a hardware one) */
static long TSforceSoftSync(int Card)
{
	semGive(TSdata.sync_occurred);
	TSforceSync(0);
	return 0;
}

/*------------------------------------------------------------------*/
/*
	TSaddStamp - Add time stamp op1 to time stamp op2 giving time stamp
	result
*/
static void TSaddStamp(	struct timespec* result,
						struct timespec* op1,
						struct timespec* op2)
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

/*-----------------------------------------------------------------------*/
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
	if(a->tv_sec < b->tv_sec)			{ dir=-1; big_time=b; small_time=a; }
	else if(a->tv_sec > b->tv_sec)		{ dir=1; small_time=b; big_time=a; }
	else if(a->tv_nsec == b->tv_nsec)	{ dir=0; small_time=b; big_time=a;}
	else if(a->tv_nsec < b->tv_nsec)	{ dir=-1; big_time=b; small_time=a; }
	else								{ dir=1; small_time=b; big_time=a; }

	diff->tv_sec=big_time->tv_sec-small_time->tv_sec;

	if(big_time->tv_nsec>=small_time->tv_nsec)
		diff->tv_nsec=big_time->tv_nsec-small_time->tv_nsec;
	else
	{
		diff->tv_nsec=small_time->tv_nsec-big_time->tv_nsec;
		diff->tv_sec--;
	}
	/* ---------------------this block sucks--------------------*/
	return dir;
}

/**************************************************************************/
/* more routines for managing sockets */
/**************************************************************************/

/*-----------------------------------------------------------------------*/
/*	
	TSgetSocket - get a UDP socket for a port, return a sockaddr for it.
*/
static int TSgetSocket(int port, struct sockaddr_in* sin)
{
	int soc; 

	sin->sin_port=port;
	sin->sin_family=AF_INET;
	sin->sin_addr.s_addr=htonl(INADDR_ANY);
	
	if( (soc=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0 )
		{ perror("socket create failed"); return -1; }

	Debug(5,"sizeof sin = %d\n",sizeof(struct sockaddr_in));
	if( bind(soc,(struct sockaddr*)sin,sizeof(struct sockaddr_in)) < 0 )
		{ perror("socket bind failed"); close(soc); return -1; }

	return soc;
}

/*-----------------------------------------------------------------------*/
/*	
	TSgetData - attempt to get data from a socket after sending a request
	to it.  Aptionally return round trip time and the sockaddr of the 
	guy who responsed.
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

	/* convert millisecond time out to seconds/microseconds */
	s=TSdata.time_out/1000; us=(TSdata.time_out-(s*1000))*1000;
	Debug(6,"time_out Second=%lu\n",s);
	Debug(6,"time_out Microsecond=%lu\n",us);

	do
	{
		Debug(8,"sednto port %d\n",((struct sockaddr_in*)to_sin)->sin_port);
		if(round_trip) clock_gettime(CLOCK_REALTIME,&send_time);
		if( sendto(soc,buf,buf_size,0,to_sin,sizeof(struct sockaddr)) < 0 )
			{ perror("sendto failed"); return -1; }

		FD_ZERO(&readfds); FD_SET(soc,&readfds);
		timeOut.tv_sec=s; timeOut.tv_usec=us;

		num=select(FD_SETSIZE,&readfds,(fd_set*)NULL,(fd_set*)NULL,&timeOut);
		if(round_trip) clock_gettime(CLOCK_REALTIME,&recv_time);
		if(num==ERROR) { perror("select failed"); return -1; }
	}
	while(num==0 && ++retry_count<TS_RETRY_COUNT);

	if(retry_count >= TS_RETRY_COUNT)
		{ Debug(5,"TSgetData - retry count exceeded\n",0); return -1; }
	else
	{
		/* data available */
		flen=from_sin?sizeof(struct sockaddr):0;
		if((mlen=recvfrom(soc,buf,buf_size,0,from_sin,&flen))<0)
			{ perror("recvfrom failed"); return -1; }

		if(from_sin)
		{
			Debug(8,"recvfrom port %d\n",
				((struct sockaddr_in*)from_sin)->sin_port);
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

	TSgetTime(&tp);
	TSprintf("real time clock = %lu,%lu\n",tp.tv_sec,tp.tv_nsec);
	TSprintf("EPICS clock = %lu,%lu\n",
		TSdata.event_table[TSdata.sync_event].tv_sec,
		TSdata.event_table[TSdata.sync_event].tv_nsec);
	return;
}

/*	test function to print an event time stamp */
void TSprintTimeStamp(int num)
{
	struct timespec tp;

	TSgetTimeStamp(num,&tp);
	TSprintf("event %d occurred: %lu.%lu\n",num,tp.tv_sec,tp.tv_nsec);
	return;
}

/*	test function to print the current time using event system */
void TSprintCurrentTime()
{
	struct timespec tp;

	TScurrentTimeStamp(&tp);
	TSprintf("Current Event System time: %lu.%lu\n",tp.tv_sec,tp.tv_nsec);
	TSaccurateTimeStamp(&tp);
	TSprintf("Accurate Event System time: %lu.%lu\n",tp.tv_sec,tp.tv_nsec);
	return;
}

/*	test function to query the boot server for the current time.  */
void TSprintUnixTime()
{
	struct timespec ts;

	if(TSgetUnixTime(&ts)!=0)
	{
		TSprintf("Could not get Unix time\n");
		return;
	}

	TSprintf("boot server time clock = %lu, %lu\n",ts.tv_sec,ts.tv_nsec);
	return;
}

/*	test function to print the master timing IOC time stamp.  */
void TSprintMasterTime()
{
	struct timespec ts;

	if(TSgetMasterTime(&ts)!=0)
	{
		TSprintf("Could not get Unix time\n");
		return;
	}

	TSprintf("master time clock = %lu, %lu\n",ts.tv_sec,ts.tv_nsec);
	return;
}

