/* share/epicsH/drvGpibErr.h $Id$ */
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
 * .01  09-13-91	jrw	created
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
