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

#define semMutexDestroy semBinaryDestroy
#define semMutexGive semBinaryGive
#define semMutexTake semBinaryTake
#define semMutexMustTake semBinaryMustTake
#define semMutexTakeTimeout semBinaryTakeTimeout
#define semMutexTakeNoWait semBinaryTakeNoWait
#define semMutexShow semBinaryShow

#ifdef __cplusplus
}
#endif

#endif /*osdSemh*/
