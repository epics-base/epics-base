#ifndef GDD_UTILS_H
#define GDD_UTILS_H

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 *
 * *Revision 1.2  1996/06/24 03:15:39  jbk
 * *name changes and fixes for aitString and fixed string functions
 * *Revision 1.1  1996/05/31 13:15:36  jbk
 * *add new stuff
 *
 */

#ifdef vxWorks
#include <vxWorks.h>
#include <vme.h>
#include <sysLib.h>
#include <semLib.h>
#endif

#include "aitTypes.h"

typedef enum { gddSemEmpty, gddSemFull } gddSemType;

#if 0
#ifdef vxWorks
#undef vxWorks
#define __temp_vxWorks__ 1
#endif
#endif

// Semaphores default to "Full"

class gddSemaphore
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

