#ifndef __DRVTS_h__
#define __DRVTS_h__

/*
 * $Log$
 * Revision 1.6  1999/12/14 21:35:34  jhill
 * use osiTime.h to get TS_STAMP
 *
 * Revision 1.5  1999/11/18 13:30:07  mrk
 * new makefile method; moved drvTS.h to vxWorks specific code
 *
 * Revision 1.3  1998/09/29 14:11:03  mrk
 * TSsetClockFromUnix was made global
 *
 * Revision 1.2  1998/03/19 20:41:16  mrk
 * Checked for Y2K complience. It turns out it was even ok when NTP time overflows
 * in 2036. However it was modified so that no overflows should occur while convert
 * ing between NTP, UNIX, and EPICS epochs.
 * In addition the conversion of fractions of a second to nanaoseconds was changed
 * Formatting was changed so that interesting code does not run off right side of page.
 * Hopefully EPICS base is now fine for Y2K.
 * In fact it should be fine (as far as time is converned) until the Unix epoch
 * overflows a 32 unsigned integer in the year 2106.
 *
 * Revision 1.1  1996/01/25 21:11:56  mrk
 * moved includes; .ascii=> .db; path changes
 *
 * Revision 1.12  1995/08/30  15:39:07  jbk
 * Added global variables for force accurate time stamps and direct time.
 *
 * Revision 1.11  1995/08/18  13:18:13  mrk
 * Added function prototypes for ansi c
 *
 * Revision 1.10  1995/08/17  20:35:52  jbk
 * fixed the debug macro to work with -pendantic option (yuck)
 *
 * Revision 1.9  1995/08/17  19:44:08  jbk
 * Added a new utility function to get the first of the year time stamp.
 *
 * Revision 1.8  1995/08/16  19:04:20  jbk
 * corrected vxworks time troubles
 *
 * Revision 1.7  1995/05/22  15:22:24  jbk
 * changes TS_EXTERN thing
 *
 * Revision 1.6  1995/02/01  15:30:17  winans
 * Added a type field to the configure command to disable the use of the event
 * system hardware if desired.
 *
 * Revision 1.5  1994/10/31  20:36:17  jbk
 * added new stuff
 *
 */

/**************************************************************************
 *
 *     Author:	Jim Kowalkowski
 *
 * Modification Log:
 * -----------------
 * .01	01-06-94	jbk	initial version
 *
 ***********************************************************************/

/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

#include <vxWorks.h>
#include <timers.h>
#include <semLib.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "tsStamp.h"

#ifdef TS_DRIVER
#define TS_EXTERN
#else
#define TS_EXTERN extern
#endif

#define ER_EVENT_RESET_TICK     0x7d    /* Reset the tick counter */

#define TS_MAGIC ('T'<<24|'S'<<16|'d'<<8|'r')
#define TS_SLAVE_PORT 18322
#define TS_MASTER_PORT 18323
#define TS_RETRY_COUNT 4
#define TS_TIME_OUT_MS 250
#define TS_SECS_ASYNC_TRY_MASTER (60*5) /* every five minutes */
#define TS_SECS_SYNC_TRY_MASTER (60*1) /* every one minute */

#define UDP_TIME_PORT 37
#define UDP_NTP_PORT 123

#define TS_BILLION 1000000000
#define TS_SYNC_RATE_SEC 10
#define TS_CLOCK_RATE_HZ 1000
#define TS_TOTAL_EVENTS 128
/*Following is (SEC_IN_YEAR*90)+(22*SEC_IN_DAY) */
/*22 is leap years from 1900 to 1990*/
#define TS_1900_TO_EPICS_EPOCH 2840140800UL
/*Following is (SEC_IN_YEAR*70)+(17*SEC_IN_DAY) */
/*17 is leap years from 1900 to 1970*/
#define TS_1900_TO_VXWORKS_EPOCH 2208988800UL
/*Following is (SEC_IN_YEAR*20)+(5*SEC_IN_DAY) */
/*5 is leap years from 1970 to 1990*/
#define TS_VXWORKS_TO_EPICS_EPOCH 631152000UL

#define TS_STAMP_SERVER_PRI 70
#define TS_SYNC_SERVER_PRI 70
#define TS_SYNC_CLIENT_PRI 70
#define TS_ASYNC_CLIENT_PRI 70

typedef enum { TS_time_request, TS_sync_request, TS_sync_msg } TStype;
typedef enum { TS_master_alive, TS_master_dead } TSstate;
typedef enum {	TS_async_none, TS_async_private,
		TS_async_ntp, TS_async_time } TStime_protocol;
typedef enum {	TS_sync_master, TS_async_master, 
		TS_sync_slave, TS_async_slave,
		TS_direct_master, TS_direct_slave} TStime_type;

struct TSstampTransStruct {
	unsigned long magic;		/* identifier */
	TStype type;			/* transaction type */
	struct timespec master_time; /* master time stamp - last sync time */
	struct timespec current_time; /* master current time stamp 1990 epoch */
	struct timespec unix_time;	/* time using 1900 epoch */
	unsigned long sync_rate; 	/* master sends sync at this rate */
	unsigned long clock_hz;		/* master clock this frequency (tick rate) */
};
typedef struct TSstampTransStruct TSstampTrans;

struct TSinfoStruct {
	TSstate state;
	TStime_type type;
	TStime_protocol async_type;
	int ts_sync_valid;

	struct timespec *event_table;	/* timestamp table */

	unsigned long sync_rate; /* master send sync at this rate */
	unsigned long clock_hz;	/* master clock is this frequency */
	unsigned long clock_conv;/* conversion factor for tick_rate->ns */
	unsigned long time_out; /* udp packet time-out in milliseconds */
	int master_timing_IOC;	/* 1=master, 0=slave */
	int master_port;	/* port that master listens on */
	int slave_port;		/* port that slave listens on */
	int total_events;	/* this is the total event in the event system*/
	int sync_event;		/* this is the sync event number */
	int has_event_system;	/* 1=has event system, 0=no event system */
	int has_direct_time;	/* 1=has direct time, 0=no direct time */
	int UserRequestedType;	/* let user force the setting of type */

	SEM_ID sync_occurred;

	struct sockaddr hunt;	/* broadcast address info */
	struct sockaddr master;	/* socket info for contacting master */
};
typedef struct TSinfoStruct TSinfo;

/* global functions */
#ifdef __cplusplus
extern "C" {
#endif
TS_EXTERN long TSinit(void);
TS_EXTERN long TSgetTimeStamp(int event_number,struct timespec* ts);
TS_EXTERN unsigned long TSepochNtpToUnix(struct timespec* ts);
TS_EXTERN unsigned long TSfractionToNano(unsigned long fraction);
TS_EXTERN unsigned long TSepochNtpToEpics(struct timespec* ts);
TS_EXTERN unsigned long TSepochUnixToEpics(struct timespec* ts);
TS_EXTERN unsigned long TSepochEpicsToUnix(struct timespec* ts);
TS_EXTERN long TScurrentTimeStamp(struct timespec* ts);
TS_EXTERN long TSaccurateTimeStamp(struct timespec* ts);
TS_EXTERN long TSgetFirstOfYearVx(struct timespec* ts);
TS_EXTERN void TSconfigure(int master, int sync_rate_sec, int clock_rate_hz,
	int master_port, int slave_port,
	unsigned long millisecond_request_time_out, int type);
TS_EXTERN long TSsetClockFromUnix(void);

#ifndef TS_DRIVER
TS_EXTERN TSinfo TSdata;
TS_EXTERN TSdirectTimeVar; /* set to !=0 to indicate direct time available*/
TS_EXTERN TSgoodTimeStamps; /* force best time stamps by setting != 0 */
#endif

#ifdef __cplusplus
};
#endif

/* NTP information - all this is backwards and documentation only */
#define VN_SHIFT 2 /* Version - 3 bits */
#define VN_version 3<<VN_SHIFT

#define LI_SHIFT		0 /* Leap Indicator LI - 2 bits */
#define LI_no_warning		(0x00<<LI_SHIFT)
#define LI_61_sec_minute	(0x01<<LI_SHIFT)
#define LI_59_sec_minute	(0x02<<LI_SHIFT)
#define LI_alarm_condition	(0x03<<LI_SHIFT)

#define MODE_SHIFT		5 /* Mode MODE - 3 bits */
#define MODE_reserved		(0x00<<MODE_SHIFT)
#define MODE_sym_active		(0x01<<MODE_SHIFT)
#define MODE_sym_passive	(0x02<<MODE_SHIFT)
#define MODE_client		(0x03<<MODE_SHIFT)
#define MODE_server		(0x04<<MODE_SHIFT)
#define MODE_broadcast		(0x05<<MODE_SHIFT)
#define MODE_reserved_NTP	(0x06<<MODE_SHIFT)
#define MODE_reserved_pvt	(0x07<<MODE_SHIFT)

#define STRAT_SHIFT		8 /* Stratum STRAT - 8 bits */
#define STRAT_unspecified	(0x00<<STRAT_SHIFT)
#define STRAT_ascii		(0x00<<STRAT_SHIFT)
#define STRAT_GPS		(0x01<<STRAT_SHIFT)
#define STRAT_ATOM		(0x01<<STRAT_SHIFT)
#define STRAT_address		(0x02<<STRAT_SHIFT)

#define POLL_SHIFT 16 /* eight bits */
#define PREC_SHIFT 24 /* eight bits */

struct TS_ntp {
	/* unsigned int info; */
	unsigned char info[4];
	unsigned int root_delay;
	unsigned int root_disp;
	unsigned int reference_id;
	struct timespec reference_ts;
	struct timespec originate_ts;
	struct timespec receive_ts;
	struct timespec transmit_ts;
	/* char authenticator[96]; (optional) */
};
typedef struct TS_ntp TS_NTP;


/* debug macro creation */
#ifdef NODEBUG
#define Debug(l,f,v) ;
#else
#ifdef MAKE_DEBUG
volatile int MAKE_DEBUG = 0;
#define Debug(l,f,v) { if(l<= MAKE_DEBUG ) \
	{ printf("%s:%d: ",__FILE__,__LINE__); printf(f,v); }}
#define Debug0(l,f) { if(l<= MAKE_DEBUG ) \
	{ printf("%s:%d: ",__FILE__,__LINE__); printf(f); }}
#else
#define Debug(l,f,v) { if(l) \
	{ printf("%s:%d: ",__FILE__,__LINE__); printf(f,v); }}
#define Debug0(l,f) { if(l) \
	{ printf("%s:%d: ",__FILE__,__LINE__); printf(f); }}
#endif
#endif

#endif
