/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *	$Id$	
 *      Author: Jeffrey O. Hill
 *              hill@luke.lanl.gov
 *              (505) 665 1831
 *      Date:  9-93
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
	cac_mux_io (pTV, TRUE);
}

/*
 * cac_block_for_sg_completion()
 */
void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
	cac_mux_io (pTV, TRUE);
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
	char *pName;
	char *pTmp;
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
    int     fd;
    int     maxfd;
  
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
	 * all open sockets closed by CLOSEXC flag so they will not
	 * be inherited by the repeater task
	 */

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

