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
#include <stdlib.h>

#include <unistd.h>
#include <pwd.h>
#include <sys/param.h>

#include "iocinf.h"


/*
 *
 * This should work on any POSIX compliant OS
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
	int	length;
	char	*pName;
	char	*pTmp;

	pName = getlogin();
	if(!pName){
		pName = getpwuid(getuid())->pw_name;
		if(!pName){
			return NULL;
		}
	}

	length = strlen(pName)+1;
	pTmp = malloc(length);
	if(!pTmp){
		return pTmp;
	}
	strncpy(pTmp, pName, length-1);
	pTmp[length-1] = '\0';

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
 * ca_spawn_repeater()
 */
void ca_spawn_repeater()
{
	int     status;
	char	*pImageName;

	/*
	 * create a duplicate process
	 */
	status = fork();
	if (status < 0){
		SEVCHK(ECA_NOREPEATER, NULL);
		return;
	}

	/*
 	 * return to the caller
	 * if its the in the initiating process
	 */
	if (status){
		return;
	}

	/*
 	 * running in the repeater process
	 * if here
	 */
	pImageName = "caRepeater";
	status = execlp(pImageName, NULL);
	if(status<0){	
		ca_printf("!!WARNING!!\n");
		ca_printf("The executable \"%s\" couldnt be located.\n", pImageName);
		ca_printf("You may need to modify your PATH environment variable.\n");
		ca_printf("Creating CA repeater with fork() system call.\n");
		ca_printf("Repeater will inherit parents process name and resources.\n");
		ca_printf("Duplicate resource consumption may occur.\n");
		ca_repeater();
		assert(0);
	}
	exit(0);
}
