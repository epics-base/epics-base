/* $Id$
 *
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  mm-dd-yy        iii     Comment
 * .02  12-03-91        rcz     defines from vxWorks.h for Unix
 * .03  12-03-91        rcz     added define PVNAME_STRINGSZ
 * .04  05-22-92        mrk     cleanup
 * .05  07-22-93	mrk	Cleanup defs for NO and YES
 * .06  08-11-93	joh	included errMdef.h
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

#define vxTicksPerSecond (sysClkRateGet())	/*clock ticks per second*/

#include <errMdef.h>
#include <epicsTypes.h>

#endif /* INCdbDefsh */
