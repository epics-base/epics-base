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

#ifndef NONE
#define NONE            (-1)    /* for times when NULL won't do */
#endif

#ifndef LOCAL 
#define LOCAL static
#endif

#ifndef NELEMENTS
#define NELEMENTS(array)                /* number of elements in an array */ \
                (sizeof (array) / sizeof ((array) [0]))
#endif

#ifndef __cplusplus

#ifndef max
#define max(x, y)       (((x) < (y)) ? (y) : (x))
#endif
#ifndef min
#define min(x, y)       (((x) < (y)) ? (x) : (y))
#endif

#endif

#ifndef OFFSET
#define OFFSET(structure, member)       /* byte offset of member in structure*/\
                ((int) &(((structure *) 0) -> member))
#endif

#define PVNAME_STRINGSZ		61		/* includes NULL terminator for PVNAME_SZ */
#define	PVNAME_SZ	(PVNAME_STRINGSZ - 1)	/*Process Variable Name Size	*/

#define DB_MAX_CHOICES	30

#include "errMdef.h"
#include "epicsTypes.h"

#endif /* INCdbDefsh */
