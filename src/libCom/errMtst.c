/* share/src/libCom $Id$ 
 * errMtst.c
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
#include <errMdef.h>


/****************************************************************
 * MAIN FOR errMtst
****************************************************************/
#ifndef vxWorks
main()
{
	printf("calling errSymBld from main in errMtst.c\n");
	errSymBld();
#if 0
	printf("calling errSymDump from main in errMtst.c\n");
	errSymDump();
#endif
#if 0
	printf("calling errSymFindTst from main in errMtst.c\n");
	errSymFindTst();
#endif
#if 1
	printf("calling errSymTest from main in errMtst.c\n");
	errSymTest((unsigned short)501, 1, 17);
#endif
}
#endif

/****************************************************************
 * ERRSYMTEST
****************************************************************/
#ifdef __STDC__
void errSymTest(unsigned short modnum, unsigned short begErrNum, unsigned short endErrNum)
#else
void errSymTest(modnum, begErrNum, endErrNum)
unsigned short modnum;
unsigned short begErrNum;
unsigned short endErrNum;
#endif /* __STDC__ */
{
    long            errNum;
    unsigned short  errnum;
    if (modnum < 501)
	return;

    /* print range of error messages */
    for (errnum = begErrNum; errnum < endErrNum+1; errnum++) {
	errNum = modnum << 16;
	errNum |= (errnum & 0xffff);
	errSymTestPrint(errNum);
    }
}
