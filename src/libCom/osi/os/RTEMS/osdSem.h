/*
 * RTEMS osdSem.h
 *	$Id$
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */
#ifndef osdSemh
#define osdSemh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc INLINE void epicsShareAPI semMutexDestroy(semMutexId id)
{
	semBinaryDestroy (id);
}

epicsShareFunc INLINE void epicsShareAPI semMutexGive(semMutexId id)
{
	semBinaryGive (id);
}

epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTake(semMutexId id)
{
	return semBinaryTake (id);
}

epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTakeTimeout(
    semMutexId id, double timeOut)
{
	return semBinaryTakeTimeout (id, timeOut);
}

epicsShareFunc INLINE semTakeStatus epicsShareAPI semMutexTakeNoWait(semMutexId id)
{
	return semBinaryTakeNoWait (id);
}

epicsShareFunc void epicsShareAPI semMutexShow(semMutexId id)
{
	semBinaryShow (id);
}

#ifdef __cplusplus
}
#endif

#endif /*osdSemh*/
