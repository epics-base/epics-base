/* share/src/drv/drvBitBusErr.h  $Id$ */
/*      Author: John Winans
 *      Date:   12-5-91
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
 * .01  12-05-91	jrw	Written
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
