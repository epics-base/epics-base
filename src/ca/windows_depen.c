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
#include <stdarg.h>

#include "iocinf.h"

#ifndef _WINDOWS
#error This source is specific to DOS/WINDOS
#endif


/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
        SYSTEMTIME st;

	GetSystemTime(&st);
	pt->tv_sec = (long)st.wSecond + (long)st.wMinute*60 + 
		(long)st.wHour*360;
	pt->tv_usec = st.wMilliseconds*1000;
}


/*
 *      CAC_MUX_IO()
 *
 *      Asynch notification of incomming messages under UNIX
 *      1) Wait no longer than timeout
 *      2) Return early if nothing outstanding
 *
 *
 */
void cac_mux_io(struct timeval  *ptimeout)
{
        int                     count;
        int                     newInput;
        struct timeval          timeout;

        cac_clean_iiu_list();

        timeout = *ptimeout;
        do{
		/*
		 * manage search timers and detect disconnects 
		 */ 
		manage_conn(TRUE); newInput = FALSE;

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


void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
	cac_mux_io(pTV);
}


/*
 * cac_os_depen_init()
 */
int cac_os_depen_init(struct ca_static *pcas)
{
        int status;

	ca_static = ca_temp;

	/*
	 * dont allow disconnect to terminate process
	 * when running in UNIX enviroment
	 *
	 * allow error to be returned to sendto()
	 * instead of handling disconnect at interrupt
	 */
	signal(SIGPIPE,SIG_IGN);

#	ifdef _WINSOCKAPI_
		status = WSAStartup(MAKEWORD(1,1), &WsaData));
		assert (status==0);
#	endif

	status = ca_os_independent_init ();

        return status;
}


/*
 * cac_os_depen_exit ()
 */
void cac_os_depen_exit (struct ca_static *pcas)
{
	ca_static = pcas;
        ca_process_exit();
	ca_static = NULL;

	free ((char *)pcas);
}


/*
 *
 * This should work on any POSIX compliant OS
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
        int     length;
        char    *pName;
        char    *pTmp;

        pName = "Joe PC";
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
 	 * running in the repeater process
	 * if here
	 */
	pImageName = "caRepeater";
	status = system(pImageName);
	if(status<0){	
		ca_printf("!!WARNING!!\n");
		ca_printf("Unable to locate the executable \"%s\".\n", 
			pImageName);
		ca_printf("You may need to modify your environment.\n");
	}
}




/*
 *      ca_printf()
 */
int ca_printf(char *pformat, ...)
{
        va_list         args;
        int             status;

        va_start(args, pformat);

        status = vfprintf(
                        stderr,
                        pformat,
                        args);

        va_end(args);

        return status;
}

