/* $Id$ 
 *
 *      Author:          Bob Zieman
 *      Date:            09-01-93
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
 */

#ifdef vxWorks
#include <vxWorks.h>
#else
#include <string.h>
#endif

#include <stdio.h>



/****************************************************************
 * MAIN FOR errMtst
****************************************************************/
#ifndef vxWorks
main()
{
	errSymBld();
#if 0
	errSymDump();
#endif
#if 0
	errSymFindTst();
#endif
#if 1
	errSymTest(501, 1, 17);
#endif
}
#endif
