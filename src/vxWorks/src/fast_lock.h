/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* fast_lock.h */
/* share/epicsH $Id$ */

/*
 *      Author:          Jeff Hill
 *      Date:            4-11-89
 */

/*
 * 
 *
 *	Macros and data structures for a much faster (than vrtx semaphore) 
 *	mutual exclusion lock/unlock.
 *	(semaphore in this case is only used to wait for unlock)
 *
 *      On a 68020 the following times include lock and unlock
 *
 *	FASTLOCK-FASTUNLOCK     8 microsecs
 *	semTake-semGive(Macro) 80 microsecs
 *	semTake-semGive	       92 microsecs
 *
 */

#ifndef INCLfast_lockh
#define INCLfast_lockh

#ifndef INCLsemLibh
#include <semLib.h>
#endif
#ifndef INCLvxLibh
#include <vxLib.h>
#endif


#define FAST_LOCK SEM_ID


#define FASTLOCKINIT(PFAST_LOCK)\
	(*(PFAST_LOCK) = \
		semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY)) 
#define FASTLOCKFREE(PFAST_LOCK)\
	semDelete(*(PFAST_LOCK))
#define FASTLOCK(PFAST_LOCK)\
	semTake(*(PFAST_LOCK), WAIT_FOREVER);
#define FASTUNLOCK(PFAST_LOCK)\
  	semGive(*(PFAST_LOCK))
#define FASTLOCKNOWAIT(PFAST_LOCK) \
	(semTake(*(PFAST_LOCK),NO_WAIT)==0) ? TRUE : FALSE)
#define FASTLOCKTEST(PFAST_LOCK) \
(\
	(semTake(*(PFAST_LOCK),NO_WAIT)==0 )\
	? (semGive(*(PFAST_LOCK),FALSE)\
	: TRUE) \
)

#endif /* Nothing after this endif */
