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

/*
 *        Macro equivalent of vxWorks glue for better performance
 */
#ifdef VRTX_KERNEL
#	define semGive(SEMID)\
	{    if ((SEMID)->count == 0)vrtxPost (&((SEMID)->count), 1);    }

#	define semTake(SEMID)\
	{int dummy;    vrtxPend (&((SEMID)->count), 0, &dummy);    }
#endif

typedef struct{
  	SEM_ID		ppend;		/* wait for lock sem */
  	unsigned short	count;		/* cnt of tasks waiting for lock */
  	unsigned char	lock;		/* test and set lock bit */
  	char		pad;		/* structure alignment */
}FAST_LOCK;

#define SEM_FAST_LOCK

#if defined(SEM_FAST_LOCK) /* no lock test */

#define FASTLOCKINIT(PFAST_LOCK)\
	(((FAST_LOCK *)(PFAST_LOCK))->ppend = \
		semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY)) 
#define FASTLOCKFREE(PFAST_LOCK)\
	semDelete( ((FAST_LOCK *)(PFAST_LOCK))->ppend )
#define FASTLOCK(PFAST_LOCK)\
	semTake(((FAST_LOCK *)(PFAST_LOCK))->ppend, WAIT_FOREVER);
#define FASTUNLOCK(PFAST_LOCK)\
  	semGive(((FAST_LOCK *)(PFAST_LOCK))->ppend);
#define FASTLOCKNOWAIT(PFAST_LOCK) \
	((semTake(((FAST_LOCK *)(PFAST_LOCK))->ppend,NO_WAIT)==0) ? TRUE : FALSE)
#define FASTLOCKTEST(PFAST_LOCK) \
(\
	(semTake(((FAST_LOCK *)(PFAST_LOCK))->ppend,NO_WAIT)==0 )\
	? (semGive(((FAST_LOCK *)(PFAST_LOCK))->ppend),FALSE)\
	: TRUE \
)
	

#elif defined(TASK_LOCK_FAST_LOCK)

#define FASTLOCKINIT(PFAST_LOCK)\
	(\
	((FAST_LOCK *)(PFAST_LOCK))->count =0, \
	((FAST_LOCK *)(PFAST_LOCK))->lock =0, \
	((FAST_LOCK *)(PFAST_LOCK))->ppend = \
		semBCreate(SEM_Q_PRIORITY, SEM_EMPTY) \
	)
#define FASTLOCKFREE(PFAST_LOCK)\
  		semDelete( ((FAST_LOCK *)(PFAST_LOCK))->ppend )

#define FASTLOCK(PFAST_LOCK)\
	{\
	TASK_LOCK;\
	while( ((FAST_LOCK *)(PFAST_LOCK))->lock ){\
		((FAST_LOCK *)(PFAST_LOCK))->count++;\
		TASK_UNLOCK;\
		semTake(((FAST_LOCK *)(PFAST_LOCK))->ppend, WAIT_FOREVER);\
		TASK_LOCK;\
		(((FAST_LOCK *)(PFAST_LOCK))->count)--;\
	}\
	((FAST_LOCK *)(PFAST_LOCK))->lock= TRUE;
	TASK_UNLOCK;
	}

#define FASTUNLOCK(PFAST_LOCK)\
	{\
	((FAST_LOCK *)(PFAST_LOCK))->lock = FALSE;\
	if( ((FAST_LOCK *)(PFAST_LOCK))->count )\
  		semGive(((FAST_LOCK *)(PFAST_LOCK))->ppend);\
	};

#define FASTLOCKTEST(PFAST_LOCK)\
( ((FAST_LOCK *)(PFAST_LOCK))->lock )

#else /* vxTas() fast lock */

/*
 *	extra paren avoids order of ops problems
 *	(returns what semBCreate returns on v5 vxWorks)
 */
#define FASTLOCKINIT(PFAST_LOCK)\
	(\
	((FAST_LOCK *)(PFAST_LOCK))->count =0, \
	((FAST_LOCK *)(PFAST_LOCK))->lock =0, \
	((FAST_LOCK *)(PFAST_LOCK))->ppend = \
		semBCreate(SEM_Q_PRIORITY, SEM_EMPTY) \
	)

/* 
 * new requirement with v5 vxWorks
 */
#define FASTLOCKFREE(PFAST_LOCK)\
  		semDelete( ((FAST_LOCK *)(PFAST_LOCK))->ppend )

#define FASTLOCK(PFAST_LOCK)\
	{\
	((FAST_LOCK *)(PFAST_LOCK))->count++;\
	while(!vxTas( (char *)&( ((FAST_LOCK *)(PFAST_LOCK))->lock ) ))\
  		semTake(((FAST_LOCK *)(PFAST_LOCK))->ppend, WAIT_FOREVER);\
	(  ((FAST_LOCK *)(PFAST_LOCK))->count)--;\
	}

#define FASTUNLOCK(PFAST_LOCK)\
	{\
	((FAST_LOCK *)(PFAST_LOCK))->lock = FALSE;\
	if( ((FAST_LOCK *)(PFAST_LOCK))->count )\
  		semGive(((FAST_LOCK *)(PFAST_LOCK))->ppend);\
	};

#define FASTLOCKNOWAIT(PFAST_LOCK) (vxTas((char *)&(((FAST_LOCK *)(PFAST_LOCK))->lock)))

#define FASTLOCKTEST(PFAST_LOCK)\
( ((FAST_LOCK *)(PFAST_LOCK))->lock )

#endif

#endif /* Nothing after this endif */
