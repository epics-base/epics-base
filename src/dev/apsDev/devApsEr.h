/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef EPICS_DEVAPSER_H
#define EPICS_DEVAPSER_H

/* Error numbers passed to ERROR_FUNC routines */
#define ERROR_TAXI	1	/* Taxi violation */
#define ERROR_HEART	2	/* Lost the system heart beat */
#define ERROR_LOST	3	/* Events were lost */
	
/* Globally reserved event numbers */
#define	ER_EVENT_NULL			0x00	/* NULL event */
#define ER_EVENT_END			0x7f	/* Event sequence end */
#define ER_EVENT_FREEZE			0x7e	/* Freeze the event sequence */
#define ER_EVENT_RESET_TICK		0x7d	/* Reset the tick counter */
#define ER_EVENT_TICK			0x7c	/* Add 1 to the tick counter */
#define ER_EVENT_RESET_PRESCALERS	0x7b
#define ER_EVENT_HEARTBEAT		0x7a

typedef void (*EVENT_FUNC)(int Card, int EventNum, unsigned long Ticks);
typedef void (*ERROR_FUNC)(int Card, int ErrorNum);

long ErRegisterEventHandler(int Card, EVENT_FUNC func);
long ErRegisterErrorHandler(int Card, ERROR_FUNC func);
long ErGetTicks(int Card, unsigned long *Ticks);
long ErHaveReceiver(int Card);

#endif
