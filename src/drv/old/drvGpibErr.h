/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/epicsH/drvGpibErr.h $Id$ */
/*      Author: John Winans
 *      Date:   12-5-91
 */


#ifndef	GPIB_ERRORS
#define GPIB_ERRORS

#include <errMdef.h>	/* pick up the M_gpib value */

#define S_IB_ok		(M_gpib|0| 0<<1) /* success */
#define S_IB_badPrio	(M_gpib|1| 1<<1) /* invalid xact request queue priority */
#define	S_IB_A24	(M_gpib|1| 2<<2) /* Out of A24 RAM */
#define	S_IB_SIZE	(M_gpib|1| 3<<2) /* GPIB I/O buffer size exceeded */
#define	S_IB_rfu3	(M_gpib|1| 4<<2) /* reserved for future use #3 */
#define	S_IB_rfu4	(M_gpib|1| 5<<2) /* reserved for future use #4 */

#endif
