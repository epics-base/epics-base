#if !defined(vxWorks) && !defined(sun)

/* do nothing if not vxWorks and not sun */

#else

/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	02-03-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .00	02-03-90	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 * .02	06-28-91	rac	don't do lwp_fpset for sun4
 * .03	08-11-93	mrk	removed ifdef V5_vxWorks
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	genTaskSubr.c - SunOS and VxWorks "task support"
*
* DESCRIPTION
*	The code in this file supports compatibility routines between
*	VxWorks and SunOS.  See genTasks.h for a description of the
*	capabilities which are available.
*
* BUGS
* o	see genTasks.h
*
* SEE ALSO:
* o	genTasks.h	definitions for using tasks
*
*-***************************************************************************/
#include <genTasks.h>
#ifndef vxWorks
#   include <sys/time.h>
#endif

/*----------------------------------------------------------------------------
* semTake()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    void semTake(semId)
    SEM_ID semId;
    {   if (mon_enter(semId) != 0) {
	    fprintf(stderr, "in semTake, re-entry not supported; goodbye\n");
	    assertAlways(0);
	}
	return;
    }
#endif

/*----------------------------------------------------------------------------
* semClear()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    STATUS semClear(semId)
    SEM_ID semId;
    {
	int         nestLevel;      /* reentrancy level in mon_enter() */

	nestLevel = mon_cond_enter(semId);

	if (nestLevel < 0)
	    return ERROR;
	else if (nestLevel > 0) {
	    fprintf(stderr, "in semClear, re-entry not supported; goodbye\n");
	    assertAlways(0);
	}
	else
	    return OK;
    }
#endif
/*----------------------------------------------------------------------------
* taskSpawn()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    TASK_ID taskSpawn(name, prio, options, stackSize, entryAddress,
                a1, a2, a3, a4, a5, a6, a7, a8, a9, a10)
    char *name;
    int prio;
    int options;
    int stackSize;
    void (*entryAddress)();
    int             a1, a2, a3, a4, a5, a6, a7, a8, a9, a10;
    {
	TASK_ID     threadId;

	if (lwp_create(&threadId, entryAddress, prio, 0, lwp_newstk(), 10,
		    a1, a2, a3, a4, a5, a6, a7, a8, a9, a10) == 0) {
#ifndef sun4
	    if (options & VX_STDIO) {
		if (lwp_libcset(threadId) != 0) {
		    printf("couldn't do lwp_libcset for %s\n", name);
		    lwp_perror("taskSpawn");
		    assertAlways(0);
		}
	    }
	    if (options & VX_FP_TASK) {
		if (lwp_fpset(threadId) != 0) {
		    printf("couldn't do lwp_fpset for %s\n", name);
		    lwp_perror("taskSpawn");
		    assertAlways(0);
		}
	    }
#else
	    ;	/* no action for sun4 */
#endif
	}    
	else {
	    printf("couldn't do lwp_create for %s\n", name);
	    lwp_perror("taskSpawn");
	    assertAlways(0);
	}

	return threadId;
    }
#endif

/*----------------------------------------------------------------------------
* taskPrioritySet()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    STATUS taskPrioritySet(tId, prio)
    TASK_ID tId;
    int prio;
    {
	if (lwp_setpri(tId, prio) != -1)
	    return OK;
	else
	    return ERROR;
    }
#endif

/*----------------------------------------------------------------------------
* taskResume()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    STATUS taskResume(tId)
    TASK_ID tId;
    {
	if (lwp_resume(tId) == 0)
	    return OK;
	else
	    return ERROR;
    }
#endif

/*----------------------------------------------------------------------------
* taskSleep()
*---------------------------------------------------------------------------*/

STATUS taskSleep(tId, seconds, usec)
TASK_ID tId;
int	seconds;	/* seconds to delay */
int	usec;		/* micro-seconds to delay */
{
#ifndef vxWorks
    struct timeval lwpSleep;	/* time interval for lwp_sleep */

    lwpSleep.tv_sec = seconds;
    lwpSleep.tv_usec = usec;
    lwp_sleep(&lwpSleep);
#else
    int		ticks;
    static int ticksPerSec=0;
    static int usecPerTick;

    if (ticksPerSec == 0) {
	ticksPerSec = sysClkRateGet();
	usecPerTick = 1000000 / ticksPerSec;
    }

    ticks = seconds*ticksPerSec + usec/usecPerTick + 1;
    taskDelay(ticks);
#endif
    return OK;
}

/*----------------------------------------------------------------------------
* taskSuspend()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    STATUS taskSuspend(tId)
    TASK_ID tId;
    {
	if (lwp_suspend(tId) == 0)
	    return OK;
	else
	    return ERROR;
    }
#endif

/*----------------------------------------------------------------------------
* taskDelete()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
    STATUS taskDelete(tId)
    TASK_ID tId;
    {
	if (lwp_destroy(tId) == 0)
	    return OK;
	else
	    return ERROR;
    }
#endif

/*----------------------------------------------------------------------------
* lwpThreadStat()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
lwpThreadStat(msg, ptId)
char        *msg;
thread_t        *ptId;
{
    int     stat;
    statvec_t statVec;
    int     th;
    int     thCount;
    thread_t thVec[20];

    if (ptId == NULL)
        thCount = lwp_enumerate(thVec, 20);
    else {
        printf("%s:", msg);
        thCount = 1;
    }

    if (thCount <= 0)
        printf("%s: no threads exist\n", msg);
    else {
        if (ptId == NULL)
            printf("%s: %d tasks exist\n", msg, thCount);
        for (th=0; th<thCount; th++) {
            if (ptId == NULL)
                stat = lwp_getstate(thVec[th], &statVec);
            else
                stat = lwp_getstate(*ptId, &statVec);
            if (stat != 0)
                lwp_perror("threadStat");
            else {
                if (ptId == NULL) {
                    printf("\t%#x:%d prio=%d; ",
                            thVec[th].thread_id, thVec[th].thread_key,
                            statVec.stat_prio);
                }
                else {
                    printf("\t%#x:%d prio=%d; ",
                            ptId->thread_id, ptId->thread_key,
                            statVec.stat_prio);
                }
                switch (statVec.stat_blocked.any_kind) {
                    case NO_TYPE:
                        printf("not blocked\n");
                        break;
                    case CV_TYPE:
                        printf("blocked for cv: %#x:%d\n",
                                    statVec.c_id, statVec.c_key);
                        break;
                    case MON_TYPE:
                    printf("blocked for mon: %#x:%d\n",
                                    statVec.m_id, statVec.m_key);
                        break;
                    case LWP_TYPE:
                        printf("blocked for other: %#x:%d\n",
                                    statVec.l_id, statVec.l_key);
                        break;
                    default:
                        printf("unrecognized type\n");
                        break;
                }
            }    
        }    
    }    
}
#endif

/*----------------------------------------------------------------------------
* lwpMonStat()
*---------------------------------------------------------------------------*/

#ifndef vxWorks
lwpMonStat(msg)
char    *msg;
{
    int mon;
    int monCount;
    mon_t       monVec[20];
    thread_t owner;
    thread_t waitVec[20];
    int waiter;
    int waiterCount;

    monCount = mon_enumerate(monVec, 20);
    if (monCount <= 0)
        printf("%s: no monitors exist\n", msg);
    else {
        printf("%s: %d monitors exist\n", msg, monCount);
        for (mon=0; mon<monCount; mon++) {
            waiterCount = mon_waiters(&monVec[mon], &owner, waitVec, 20);
            printf("\t%2d %#x:%d owned by %#x:%d, %d waiters\n",
                    mon, monVec[mon].monit_id, monVec[mon].monit_key,
                    owner.thread_id, owner.thread_key,
                    waiterCount);
            for (waiter=0; waiter<waiterCount; waiter++) {
                printf("\t\t%#x:%d\n",
                    waitVec[waiter].thread_id, waitVec[waiter].thread_key);
            }
        }    
    }    
}
#endif

#endif
