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
 *      Modification Log:
 *      -----------------
 * 	cjm 20-Nov-95 Add code for gettimeofday
 *
 * $Log$
 * Revision 1.25.6.1  1999/07/15 21:04:10  jhill
 * fixed bug where client disconnects while waiting to send TCP
 *
 * Revision 1.25  1998/06/16 00:43:12  jhill
 * moved code to libCom
 *
 * Revision 1.24  1998/04/13 19:14:35  jhill
 * fixed task variable problem
 *
 * Revision 1.23  1998/02/05 22:36:01  jhill
 * added starlet.h (SLAC's request)
 *
 * Revision 1.22  1997/06/13 09:14:27  jhill
 * connect/search proto changes
 *
 * Revision 1.21  1997/01/22 21:11:49  jhill
 * moved vms includes here
 *
 * Revision 1.20  1996/07/02 23:04:07  jhill
 * took &tz out of gettimeofday()
 *
 * Revision 1.19  1995/12/19  19:35:24  jhill
 * -cjm 20-Nov-95 Add code for gettimeofday
 *
 * Revision 1.18  1995/10/12  01:35:30  jhill
 * Moved cac_mux_io() to iocinf.c
 *
 * Revision 1.17  1995/08/22  00:27:56  jhill
 * added cvs style mod log
 *
 *
 */

/*
 * VMS includes
 */
#include <ssdef.h>
#include <stsdef.h>
#include <iodef.h>
#include <psldef.h>
#include <prcdef.h>
#include <jpidef.h>
#include <descrip.h>
#include <starlet.h>

#include "iocinf.h"


/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
	int		status;

        status = gettimeofday(pt, (struct timezone *) NULL);
	assert(status==0);
}


/*
 * gettimeofday
 */
#ifndef MULTINET
int  gettimeofday(struct timeval *tp, struct timezone *tzp)
{

 unsigned int quadtime[2] ;
 int status ;
 int nanosecs ;
 double secs ;
 int bit31 ;
 double dtime ;            /* vax 64 bit integer as a double */

 if (tp != (struct timeval *)NULL)
 {
  status = sys$gettim(&quadtime) ;
  if (status != SS$_NORMAL)
    return -1 ;
  else
  {
   bit31 = quadtime[0] & 0x80000000 ;
   dtime = quadtime[1]*TWOPOWER32 + (quadtime[0] & 0x7fffffff) ;
   if (bit31 != 0)
     dtime = (dtime + TWOPOWER31)/ 10000000.0 ;
   else
     dtime = dtime / 10000000.0 ;
   secs = dtime - UNIX_EPOCH_AS_MJD * 86400. ;
   tp->tv_sec = (int)secs ;
   tp->tv_usec = (int)((secs - tp->tv_sec)*1000000.0) ;
  }
 }

 if (tzp != (struct timezone *)NULL)
 {
  tzp->tz_minuteswest = 0 ;
  tzp->tz_dsttime = 0 ;
 }

 return 0 ;

}
#endif


/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	cac_mux_io(pTV, TRUE);
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
void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
	cac_mux_io(pTV, TRUE);
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
 * localUserName() - for VMS 
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
		strcpy (pName, "");
	}

	if(jobTypeSize != sizeof(jobType)){
		strcpy (pName, "");
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


/*
 * caSetDefaultPrintfHandler ()
 * use the normal default here
 * ( see access.c )
 */
void caSetDefaultPrintfHandler ()
{
        ca_static->ca_printf_func = epicsVprintf;
}

