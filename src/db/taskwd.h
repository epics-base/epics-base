/* taskwd.h */
/* share/epicsH/taskwd.h  $Id$ */

/* includes for general purpose taskwd tasks		*/
/*
 *      Original Author:        Marty Kraimer
 *      Date:   	        07-18-91
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
 * Modification Log:
 * -----------------
 * .01	12-12-91	mrk	Initial version
*/

#ifndef INCtaskwdh
#define INCtaskwdh 1

#ifdef __STDC__
void taskwdInit();
void taskwdInsert(int tid, VOIDFUNCPTR callback,void *arg);
void taskwdAnyInsert(void *userpvt, VOIDFUNCPTR callback,void *arg);
void taskwdRemove(int tid);
void taskwdAnyRemove(void *userpvt);
#else
void taskwdInit();
void taskwdInsert();
void taskwdAnyInsert();
void taskwdAnyRemove();
#endif /*__STDC__*/
#endif /*INCtaskwdh*/
