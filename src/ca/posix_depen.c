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
 * max_unix_fd()
 *
 * attempt to determine the maximum file descriptor
 * on all UNIX systems
 */
int max_unix_fd( )
{
	int max;
	static const int bestGuess = 1024;

#	if defined(OPEN_MAX)
		max = OPEN_MAX; /* posix */
#	elif defined(_SC_OPEN_MAX)
		max = sysconf (_SC_OPEN_MAX);
		if (max<0) {
			max = bestGuess;
		}
#	else
		max = bestGuess;
#	endif

	return max;
}

/*
 * ca_spawn_repeater()
 */
void ca_spawn_repeater()
{
	int     status;
	char    *pImageName;
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
    if (status) {
        return;
    }

    /*
     * close all open files except for STDIO so they will not
     * be inherited by the repeater task
     */
    maxfd = max_unix_fd (); 
    for (fd = 0; fd<=maxfd; fd++) {
        if (fd==STDIN_FILENO) continue;
        if (fd==STDOUT_FILENO) continue;
        if (fd==STDERR_FILENO) continue;
        close (fd);
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

