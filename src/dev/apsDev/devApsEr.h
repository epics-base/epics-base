/*
 * *****************************************************************
 *                           COPYRIGHT NOTIFICATION
 * *****************************************************************
 * 
 * THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
 * AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
 * AND IN ALL SOURCE LISTINGS OF THE CODE.
 *  
 * (C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 *  
 * Argonne National Laboratory (ANL), with facilities in the States of 
 * Illinois and Idaho, is owned by the United States Government, and
 * operated by the University of Chicago under provision of a contract
 * with the Department of Energy.
 * 
 * Portions of this material resulted from work developed under a U.S.
 * Government contract and are subject to the following license:  For
 * a period of five years from March 30, 1993, the Government is
 * granted for itself and others acting on its behalf a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, and perform
 * publicly and display publicly.  With the approval of DOE, this
 * period may be renewed for two additional five year periods. 
 * Following the expiration of this period or periods, the Government
 * is granted for itself and others acting on its behalf, a paid-up,
 * nonexclusive, irrevocable worldwide license in this computer
 * software to reproduce, prepare derivative works, distribute copies
 * to the public, perform publicly and display publicly, and to permit
 * others to do so.
 * 
 * *****************************************************************
 *                                 DISCLAIMER
 * *****************************************************************
 * 
 * NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
 * THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
 * MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
 * LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
 * USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
 * DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
 * OWNED RIGHTS.  
 * 
 * *****************************************************************
 * LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
 * DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 */

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
