/*
 *	$Id$
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
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
 *      Modification Log:
 *      -----------------
 *
 */

/*
 * ANSI includes
 */
#include "assert.h"
#include "string.h"
#include "stdlib.h"

#include "remLib.h"

#include "iocinf.h"


/*
 *
 * localUserName() - for vxWorks
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
	char	*pTmp;
	int	length;
	char    pName[MAX_IDENTITY_LEN];

	remCurIdGet(pName, NULL);

	length = strlen(pName)+1;
	pTmp = malloc(length);
	if(!pTmp){
		return;
	}
	strncpy(pTmp, pName, length-1);
	pTmp[length-1] = '\0';

	return pTmp;
}


