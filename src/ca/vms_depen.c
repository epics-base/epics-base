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
#include <assert.h>
#include <string.h>
#include <stdLib.h>

/*
 * VMS includes
 */
#include <stsdef.h>
#include <ssdef.h>
#include <jpidef.h>

#include "iocinf.h"

#define CONNECTION_TIMER_ID 56

void connectionTimer(void *astarg);

struct time{
	int		lval;
	int		hval;
}

struct time timer = {-10000000,-1};


/*
 *
 */
void setupConnectionTimer()
{
	struct time	tmo;
	int 		status;

	status = sys$setimr(NULL, &timer, connectionTimer, CONNECTION_TIMER_ID, 0);
	assert(status == SS$_NORMAL);
}


/*
 * connectionTimer()
 */
LOCAL void connectionTimer(void *astarg);
	struct time	tmo;
	int 		status;

	manage_conn(TRUE);

	status = sys$setimr(NULL, &timer, connectionTimer, CONNECTION_TIMER_ID, 0);
	assert(status == SS$_NORMAL);
}


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
	}item_list[3];
	int		length;
	char		pName[8]; /* the size of VMS account names */
	short		nameLength;
	char		*psrc;
	char		*pdest;
	int		status;
	int		jobType;
	int		jobTypeSize;
	char 		*pTmp;

	item_list[0].buffer_length = sizeof(pName);
	item_list[0].item_code = JPI$_ACCOUNT; /* fetch the account name */
	item_list[0].pBuf = pName;
	item_list[0].pRetSize = &nameLength;
	item_list[1].buffer_length = sizeof(jobtype);
	item_list[1].item_code = JPI$_JOBTYPE; /* fetch the account name */
	item_list[1].pBuf = &jobType;
	item_list[1].pRetSize = &jobTypeSize;
	item_list[2].buffer_length = 0;

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
	while(psrc<&pName[nameLength] && !isspace(*psrc)){
		length++;
		psrc++;
	}

	pTmp = (char *)malloc(length+1);
	if(!pTmp){
		return pTmp;
	}
	strncpy(pTmp, pName, length);
	pTmp[length] = '\0';

	/*
	 * test for remote login
	 */
	if(jobTypeSize == sizeof(jobtype)){
		if(jobType != JPI$K_LOCAL){
		}
	}

	return pTmp;
}



/*
 * ca_check_for_fp()
 */
int ca_check_for_fp()
{
	return ECA_NORMAL;
}




/*
 *      ca_spawn_repeater()
 *
 *      Spawn the repeater task as needed
 */
void ca_spawn_repeater()
{
	static          $DESCRIPTOR(image,      "EPICS_CA_REPEATER");
	static          $DESCRIPTOR(io,         "EPICS_LOG_DEVICE");
	static          $DESCRIPTOR(name,       "CA repeater");
	int             status;
	unsigned long   pid;

	status = sys$creprc(
                                    &pid,
                                    &image,
                                    NULL,       /* input (none) */
                                    &io,        /* output */
                                    &io,        /* error */
                                    NULL,       /* use parents privs */
                                    NULL,       /* use default quotas */
                                    &name,
                                    4,  /* base priority */
                                    NULL,
                                    NULL,
                                    PRC$M_DETACH);
	if (status != SS$_NORMAL){
		SEVCHK(ECA_NOREPEATER, NULL);
#ifdef DEBUG
		lib$signal(status);
#endif
        }
}

