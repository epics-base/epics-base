/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCdbDefsh
#define INCdbDefsh 1

#if     defined(NULL)
#undef NULL
#endif
#define NULL    0

#if     defined(NO)
#undef NO
#endif
#define NO	0

#if     defined(YES)
#undef YES
#endif
#define YES            1

#if     defined(TRUE)
#undef TRUE
#endif
#define TRUE            1

#if     defined(FALSE)
#undef FALSE
#endif
#define FALSE           0

#ifndef OK
#define OK              0
#endif
#ifndef ERROR
#define ERROR           (-1)
#endif
#ifndef NONE
#define NONE            (-1)    /* for times when NULL won't do */
#endif

#ifndef NELEMENTS
#define NELEMENTS(array)                /* number of elements in an array */ \
                (sizeof (array) / sizeof ((array) [0]))
#endif

#ifndef max
#define max(x, y)       (((x) < (y)) ? (y) : (x))
#endif
#ifndef min
#define min(x, y)       (((x) < (y)) ? (x) : (y))
#endif

#ifndef OFFSET
#define OFFSET(structure, member)       /* byte offset of member in structure*/\
                ((int) &(((structure *) 0) -> member))
#endif

/* FLDNAME_SZ must be 4 */
#define PVNAME_STRINGSZ		29		/* includes NULL terminator for PVNAME_SZ */
#define	PVNAME_SZ	(PVNAME_STRINGSZ - 1)	/*Process Variable Name Size	*/
#define	FLDNAME_SZ	4	/*Field Name Size		*/

#define DB_MAX_CHOICES	30

#define SUPERVISORY 0
#define CLOSED_LOOP 1

#define NTO1FIRST	0
#define NTO1LOW		1
#define NTO1HIGH	2
#define NTO1AVE		3

#ifdef vxWorks
#define vxTicksPerSecond (sysClkRateGet())	/*clock ticks per second*/
#include <sysLib.h>
#endif

#include "errMdef.h"
#include "epicsTypes.h"

#ifdef __STDC__
int coreRelease(void);
int iocLogInit(void);
int rsrv_init(void);
#else
int coreRelease();
int iocLogInit();
int rsrv_init();
#endif /*__STDC__*/

#endif /* INCdbDefsh */
