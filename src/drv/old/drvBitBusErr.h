/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

/*      Author: John Winans
 *      Date:   12-5-91
 */

#ifndef BITBUS_ERRORS
#define BITBUS_ERRORS

#include <errMdef.h>	/* pick up the M_bitbus value */

#define S_BB_ok		(M_bitbus|0| 0<<1) /* success */
#define S_BB_badPrio	(M_bitbus|1| 1<<1) /* Invalid xact request queue priority */
#define	S_BB_badlink	(M_bitbus|1| 2<<2) /* Invalid bitbus link number */
#define	S_BB_rfu2	(M_bitbus|1| 3<<2) /* reserved for future use #2 */
#define	S_BB_rfu3	(M_bitbus|1| 4<<2) /* reserved for future use #3 */
#define	S_BB_rfu4	(M_bitbus|1| 5<<2) /* reserved for future use #4 */

#endif
