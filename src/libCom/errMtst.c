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
 *      Author:          Bob Zieman
 *      Date:            09-01-93
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
