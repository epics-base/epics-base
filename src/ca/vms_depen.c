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
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdLib.h>
#include <stdarg.h>

/*
 * VMS includes
 */
#include <stsdef.h>
#include <ssdef.h>
#include <jpidef.h>

#include "iocinf.h"

#define CONNECTION_TIMER_ID 56


/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
        struct timezone tz;
	int		status;

        status = gettimeofday(pt, &tz);
	assert(status==0);
}


/*
 * CAC_MUX_IO()
 *
 * 	Wait for send ready under VMS
 *	1) Wait no longer than timeout
 *
 *	Under VMS all recv's and input processing
 *	handled by ASTs
 */
void cac_mux_io(struct timeval  *ptimeout)
{
        int                     count;
        int                     newInput;
        struct timeval          timeout;

        cac_clean_iiu_list();

        timeout = *ptimeout;
        do{
                newInput = FALSE;
                do{
                        count = cac_select_io(
                                        &timeout,
                                        CA_DO_RECVS | CA_DO_SENDS);
                        if(count>0){
                                newInput = TRUE;
                        }
                        timeout.tv_usec = 0;
                        timeout.tv_sec = 0;
                }
                while(count>0);

                ca_process_input_queue();

		/*
		 * manage search timers and detect disconnects
		 */
		manage_conn(TRUE);
        }
        while(newInput);

}



/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	cac_mux_io(pTV);
}


/*
 * os_specific_sg_create()
 */
void os_specific_sg_create(CASG *pcasg)
{
}


/*
 * os_specific_sg_delete()
 */
void os_specific_sg_delete(CASG *pcasg)
{
}


/*
 * os_specific_sg_io_complete()
 */
void os_specific_sg_io_complete(CASG *pcasg)
{
}


/*
 * cac_block_for_sg_completion()
 */
void cac_block_for_sg_completion(pTV)
{
	cac_mux_io(pTV);
}


/*
 * CAC_ADD_TASK_VARIABLE()
 */
int cac_add_task_variable(struct ca_static *ca_temp)
{
        ca_static = ca_temp;
	return ECA_NORMAL;
}



/*
 * cac_os_depen_init()
 */
int cac_os_depen_init(struct ca_static *pcas)
{
	return ECA_NORMAL;
}


/*
 *
 * localUserName() - for VMS 
 *
 * o Indicates failure by setting ptr to nill
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
	char		pName[12]; /* the size of VMS user names */
	short		nameLength;
	char		*psrc;
	char		*pdest;
	int		status;
	char		jobType;
	short		jobTypeSize;
	char 		*pTmp;

	item_list[0].buffer_length = sizeof(pName);
	item_list[0].item_code = JPI$_USERNAME; /* fetch the user name */
	item_list[0].pBuf = pName;
	item_list[0].pRetSize = &nameLength;

	item_list[1].buffer_length = sizeof(jobType);
	item_list[1].item_code = JPI$_JOBTYPE; /* fetch the job type */
	item_list[1].pBuf = &jobType;
	item_list[1].pRetSize = &jobTypeSize;

	item_list[2].buffer_length = 0;
	item_list[2].item_code = 0; /* none */
	item_list[2].pBuf = 0;
	item_list[2].pRetSize = 0;

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

	/*
	 * test for remote login
	 */
	if(jobTypeSize != sizeof(jobType)){
		return NULL;
	}

/*
 * This does not appear to change when it is
 * a remote login ??
 */
	if(jobType != JPI$K_LOCAL && jobType != JPI$K_DETACHED){
		pTmp = "REMOTE";
		return pTmp;
	}

	/*
	 * parse the user name string
	 */
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

	return pTmp;
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
#		ifdef DEBUG
			lib$signal(status);
#		endif
        }
}



cac_setup_recv_thread(IIU *piiu)
{
	return ECA_NORMAL;
}


/*
 * caHostFromInetAddr()
 *
 * gethostbyaddr() not called on VMS because
 * the MULTINET socket library requires
 * user mode AST delivery in order to return from
 * gethostbyaddr(). This makes gethostbyaddr()
 * hang when it is called from AST level. 
 */
void caHostFromInetAddr(struct in_addr *pnet_addr, char *pBuf, unsigned size)
{
        char            *pString;

        pString = (char *) inet_ntoa(*pnet_addr);

        /*
         * force null termination
         */
        strncpy(pBuf, pString, size-1);
        pBuf[size-1] = '\0';

        return;
}


/*
 *      ca_printf()
 */
int ca_printf(char *pformat, ...)
{
        va_list         args;
        int             status;

        va_start(args, pformat);

        status = vfprintf(stderr, pformat, args);

        va_end(args);

        return status;
}

