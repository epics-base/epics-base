/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef GDD_SEMAPHORE
#define GDD_SEMAPHORE

/*
 * Author:	Jim Kowalkowski
 * Date:	3/97
 *
 */

#ifdef vxWorks
#include <semLib.h>
#include <sysLib.h>
#endif

#include "aitTypes.h"
#include "shareLib.h"


// ------------------------- semaphores ---------------------------

typedef enum { gddSemEmpty, gddSemFull } gddSemType;

#if 0
#ifdef vxWorks
#undef vxWorks
#define __temp_vxWorks__ 1
#endif
#endif

// Semaphores default to "Full"

class epicsShareClass gddSemaphore
{
public:
	gddSemaphore(void);
	gddSemaphore(gddSemType);
	~gddSemaphore(void);

	void give(void);
	void take(void);
	void take(aitUint32 usec);
private:
#ifdef vxWorks
	SEM_ID sem;
#endif
};

#ifdef vxWorks
inline gddSemaphore::gddSemaphore(void)
	{ sem=semBCreate(SEM_Q_PRIORITY,SEM_FULL); }
inline gddSemaphore::gddSemaphore(gddSemType s)
{
	if(s==gddSemEmpty)	sem=semBCreate(SEM_Q_PRIORITY,SEM_EMPTY);
	else				sem=semBCreate(SEM_Q_PRIORITY,SEM_FULL);
}
inline gddSemaphore::~gddSemaphore(void)	{ semDelete(sem); }
inline void gddSemaphore::give(void)		{ semGive(sem); }
inline void gddSemaphore::take(void)		{ semTake(sem,WAIT_FOREVER); }
inline void gddSemaphore::take(aitUint32 usec)
	{ semTake(sem,usec/(sysClkRateGet()*1000000)); }
#else
inline gddSemaphore::gddSemaphore(void)			{ }
inline gddSemaphore::gddSemaphore(gddSemType)	{ }
inline gddSemaphore::~gddSemaphore(void)		{ }
inline void gddSemaphore::give(void)			{ }
inline void gddSemaphore::take(void)			{ }
inline void gddSemaphore::take(aitUint32)		{ }
#endif

#if 0
class gddIntLock
{
public:
private:
};
#endif

#ifdef __temp_vxWorks__
#define vxWorks 1
#undef __temp_vxWorks__
#endif

#endif
