/* fast_lock.h */
/* share/epicsH $Id$ */

/*
 *      Author:          Jeff Hill
 *      Date:            4-11-89
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
 * .01 joh 041189	initial release into middle age
 * .02 joh 071989	added fast lock test
 * .03 joh 090391	VRTX kernel ifdef for V5 vxWorks
 * .04 joh 091091	Now uses V5 vxWorks binary semaphore
 * .05 joh 092491	Dont use V5 semaphore on V3 EPICS yet
 * .06 jba 030692	added cast to vxTas arg in FASTLOCK vxWorks5
 * .07 jba 081092	added cast to vxTas arg in FASTLOCKNOWAIT
 * .08 mgb 082493	Removed V5/V4 conditional defines
 * .09 joh 082493	include vxLib.h for vxWorks V5.1
 * .10 joh 082593	made lock char as vxTas expects	
 *			(padded structure to identical size)
 * .11 joh 082593	made fast lock structure fields volatile
 * .12 joh 091493	added sem and task lock based fast locks
 *			with ifdefs - removed volatile 
 */

/*
 * 
 *
 *	Macros and data structures for a much faster (than vrtx semaphore) 
 *	mutual exclusion lock/unlock.
 *	(semaphore in this case is only used to wait for unlock)
 *
 *      On a 68020 the following times include lock and unlock
 *
 *	FASTLOCK-FASTUNLOCK     8 microsecs
 *	semTake-semGive(Macro) 80 microsecs
 *	semTake-semGive	       92 microsecs
 *
 */

#ifndef INCLfast_lockh
#define INCLfast_lockh

#ifndef INCLsemLibh
#include <semLib.h>
#endif
#ifndef INCLvxLibh
#include <vxLib.h>
#endif


#define FAST_LOCK SEM_ID


#define FASTLOCKINIT(PFAST_LOCK)\
	(*(PFAST_LOCK) = \
		semMCreate(SEM_DELETE_SAFE|SEM_INVERSION_SAFE|SEM_Q_PRIORITY)) 
#define FASTLOCKFREE(PFAST_LOCK)\
	semDelete(*(PFAST_LOCK))
#define FASTLOCK(PFAST_LOCK)\
	semTake(*(PFAST_LOCK), WAIT_FOREVER);
#define FASTUNLOCK(PFAST_LOCK)\
  	semGive(*(PFAST_LOCK))
#define FASTLOCKNOWAIT(PFAST_LOCK) \
	(semTake(*(PFAST_LOCK),NO_WAIT)==0) ? TRUE : FALSE)
#define FASTLOCKTEST(PFAST_LOCK) \
(\
	(semTake(*(PFAST_LOCK),NO_WAIT)==0 )\
	? (semGive(*(PFAST_LOCK),FALSE)\
	: TRUE) \
)

#endif /* Nothing after this endif */
