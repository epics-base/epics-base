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
 * $Log$
 * Revision 1.23  1997/08/04 23:37:14  jhill
 * added beacon anomaly flag init/allow ip 255.255.255.255
 *
 * Revision 1.22  1997/06/13 09:14:23  jhill
 * connect/search proto changes
 *
 * Revision 1.21  1997/04/10 19:26:17  jhill
 * asynch connect, faster connect, ...
 *
 * Revision 1.20  1996/11/02 00:51:02  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.19  1996/07/09 22:41:02  jhill
 * pass nill 2nd arg to gettimeofday()
 *
 * Revision 1.18  1996/06/20 21:19:29  jhill
 * fixed posix signal problem with "cc -Xc"
 *
 * Revision 1.17  1995/12/19  19:33:42  jhill
 * added missing arg to execlp()
 *
 * Revision 1.16  1995/10/12  01:35:28  jhill
 * Moved cac_mux_io() to iocinf.c
 *
 * Revision 1.15  1995/08/22  00:22:07  jhill
 * Dont recompute connection timers if the time stamp hasnt changed
 *
 *
 */

#include <unistd.h>
#include <pwd.h>

#include "iocinf.h"


/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
	int		status;

	/*
	 * Not POSIX but available on most of the systems that we use
	 */
        status = gettimeofday(pt, NULL);
	assert(status == 0);
}


/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	cac_mux_io (pTV);
}

/*
 * cac_block_for_sg_completion()
 */
void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
	cac_mux_io (pTV);
}


/*
 * os_specific_sg_io_complete()
 */
void os_specific_sg_io_complete(CASG *pcasg)
{
}


/*
 * does nothing but satisfy undefined
 */
void os_specific_sg_create(CASG *pcasg)
{
}
void os_specific_sg_delete(CASG *pcasg)
{
}


/*
 * CAC_ADD_TASK_VARIABLE()
 */
int cac_add_task_variable(struct CA_STATIC *ca_temp)
{
	ca_static = ca_temp;
	return ECA_NORMAL;
}


/*
 *	ca_task_initialize()
 */
int epicsShareAPI ca_task_initialize(void)
{
	int status;

	if (ca_static) {
		return ECA_NORMAL;
	}

	ca_static = (struct CA_STATIC *) 
		calloc(1, sizeof(*ca_static));
	if (!ca_static) {
		return ECA_ALLOCMEM;
	}

	status = ca_os_independent_init ();

	return status;
}


/*
 * ca_task_exit ()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
int epicsShareAPI ca_task_exit (void)
{
	if (!ca_static) {
		return ECA_NOCACTX;
	}
	ca_process_exit();
	free ((char *)ca_static);
	ca_static = NULL;
	return ECA_NORMAL;
}

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
		struct passwd *p;
		p = getpwuid(getuid());
		if (p) {
			pName = p->pw_name;
			if (!pName) {
				pName = "";
			}
		}
		else {
			pName = "";
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
	 * if its in the initiating process
	 */
	if (status){
		return;
	}

	/*
 	 * running in the repeater process
	 * if here
	 */
	pImageName = "caRepeater";
	status = execlp(pImageName, pImageName, NULL);
	if(status<0){	
		ca_printf("!!WARNING!!\n");
		ca_printf("The executable \"%s\" couldnt be located\n", pImageName);
		ca_printf("because of errno = \"%s\"\n", strerror(errno));
		ca_printf("You may need to modify your PATH environment variable.\n");
		ca_printf("Creating CA repeater with fork() system call.\n");
		ca_printf("Repeater will inherit parents process name and resources.\n");
		ca_printf("Duplicate resource consumption may occur.\n");
		ca_repeater();
		assert(0);
	}
	exit(0);
}


/*
 * caSetDefaultPrintfHandler ()
 * use the normal default here
 * ( see access.c )
 */
void caSetDefaultPrintfHandler ()
{
        ca_static->ca_printf_func = epicsVprintf;
}

