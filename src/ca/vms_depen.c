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
#include "stdLib.h"

#include "stsdef.h"
#include "ssdef.h"

#include "iocinf.h"


/*
 *
 * localUserName() - for VMS 
 *
 * o Indicates failure by setting ptr to nill
 *
 * !! NEEDS TO BE TESTED !!
 *
 */
char *localUserName()
{
	struct { 
		short		buffer_length;
		short		item_code;
		void		*pBuf;
		void		*pRetSize;
		unsigned long	end_of_list;
	}item_list;
	int		length;
	char		pName[8]; /* the size of VMS account names */
	short		nameLength;
	char		*psrc;
	char		*pdest;
	int		status;
	char 		*pTmp;

	item_list.buffer_length = sizeof(pName);
	item_list.item_code = JPI$ACCOUNT; /* fetch the account name */
	item_list.pBuf = pName;
	item_list.pRetSize = &nameLength;
	item_list.end_of_list = NULL;

	status = sys$getjpiw(
			NULL,
			NULL,
			NULL,
			&item_list,
			NULL,
			NULL,
			NULL);
	if(status != SS$_NORMAL){
		return NULL;
	}

	psrc = pName;
	length = 0;
	while(psrc<&pName[sizeof(pName)] && *psrc != ' '){
		length++;
		psrc++;
	}

	pTmp = (char *)malloc(length+1);
	if(!pTmp){
		return pTmp;
	}
	strncpy(pTmp, pName, length);
	pTmp[length] = '\0';

	return pTmp;
}

