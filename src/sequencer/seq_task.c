/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Seq_tasks.c: Task creation and control for sequencer
	state sets.

	ENVIRONMENT: VxWorks
	HISTORY
04dec91,ajk	Implemented linked list of state programs, eliminating task
		variables.
11dec91,ajk	Made cosmetic changes and cleaned up comments.
19dec91,ajk	Changed algoritm in get_timeout().
***************************************************************************/

#include	"seq.h"

/* Function declarations */
#ifdef	ANSI
LOCAL	VOID ss_task_init(SPROG *, SSCB *);
LOCAL	long get_timeout(SSCB *);
#else
LOCAL	VOID ss_task_init();
LOCAL	long get_timeout();
#endif	ANSI

#define		TASK_NAME_SIZE 10

#define	MAX_TIMEOUT	(1<<30) /* like 2 years */

/*
 * sequencer() - Sequencer main task entry point.
 *  */
sequencer(pSP, stack_size, ptask_name)
SPROG		*pSP;	/* ptr to original (global) state program table */
int		stack_size;	/* stack size */
char		*ptask_name;	/* Parent task name */
{
	SSCB		*pSS;
	STATE		*pST;
	int		nss, task_id;
	char		task_name[TASK_NAME_SIZE+10];
	extern		VOID ss_entry();

	pSP->task_id = taskIdSelf(); /* my task id */

	/* Add the program to the state program list */
	seqAddProg(pSP);

	/*
	 * Connect to database channels & initiate monitor requests.
	 * Returns here immediately if "connect" option is not set (-c),
	 * otherwise, waits for all channels to connect (+c).
	 */
	seq_connect(pSP);

	/* Additional state set task names are derived from the first ss */
	if (strlen(ptask_name) > TASK_NAME_SIZE)
		ptask_name[TASK_NAME_SIZE] = 0;

	/* Create each additional state set task */
	pSS = pSP->sscb + 1;
	for (nss = 1; nss < pSP->nss; nss++, pSS++)
	{
		/* Form task name from program name + state set number */
		sprintf(task_name, "%s_%d", ptask_name, nss);

		/* Spawn the task */
		task_id = taskSpawn(
		  task_name,				/* task name */
		  SPAWN_PRIORITY+pSS->task_priority,	/* priority */
		  SPAWN_OPTIONS,			/* task options */
		  stack_size,				/* stack size */
		  ss_entry,				/* entry point */
		  pSP, pSS);				/* pass 2 parameters */

		seq_log(pSP, "Spawning task %d: \"%s\"\n", task_id, task_name);
	}

	/* First state set jumps directly to entry point */
	ss_entry(pSP, pSP->sscb);
}
/*
 * ss_entry() - Task entry point for all state sets.
 * Provides the main loop for state set processing.
 */
VOID ss_entry(pSP, pSS)
SPROG	*pSP;
SSCB	*pSS;
{
	BOOL		ev_trig;
	STATE		*pST, *pStNext;
	long		delay;
	int		i;
	char		*pVar;

	pSS->task_id = taskIdSelf();

	/* Initialize all tasks except the main task */
	if (pSS->task_id != pSP->task_id)
		ss_task_init(pSP, pSS);

	/* Initilaize state set to enter the first state */
	pST = pSS->states;
	pSS->current_state = 0;

	/* Use the event mask for this state */
	pSS->pMask = (pST->eventMask);

	/* Local ptr to user variables (for reentrant code only) */
	pVar = pSP->user_area;

	/*
	 * ============= Main loop ==============
	 */
	while (1)
	{
		/* Clear event bits */
		semTake(pSP->caSemId, WAIT_FOREVER); /* Lock CA event update */
		for (i = 0; i < NWRDS; i++)
			pSS->events[i] = 0;
		semGive(pSP->caSemId); /* Unlock CA event update */

		pSS->time = tickGet(); /* record time we entered this state */

		/* Call delay function to set up delays */
		pSS->ndelay = 0;
		pST->delay_func(pSP, pSS, pVar);

		/* Generate a phoney event.
		 * This guarantees that a when() is always executed at
		 * least once when a state is entered. */
		semGive(pSS->syncSemId);

		/*
		 * Loop until an event is triggered, i.e. when()
		 * returns TRUE.
		 */
		do {
			/* Wake up on CA event, event flag, or expired time delay */
			delay = get_timeout(pSS);
			if (delay > 0)
				semTake(pSS->syncSemId, delay);

			/* Apply resource lock: any new events coming in will
			   be deferred until next state is entered */
			semTake(pSP->caSemId, WAIT_FOREVER);

			/* Call the event function to check for an event trigger.
			 * Everything inside the when() statement is executed.
			 */
			ev_trig = pST->event_func(pSP, pSS, pVar);

			/* Unlock CA resource */
			semGive(pSP->caSemId);

		} while (!ev_trig);
		/*
		 * An event triggered:
		 * execute the action statements and enter the new state.
		 */

		/* Change event mask ptr for next state */
		pStNext = pSS->states + pSS->next_state;
		pSS->pMask = (pStNext->eventMask);

		/* Execute the action for this event */
		pSS->action_complete = FALSE;
		pST->action_func(pSP, pSS, pVar);

		/* Flush any outstanding DB requests */
		ca_flush_io();

		/* Change to next state */
		pSS->prev_state = pSS->current_state;
		pSS->current_state = pSS->next_state;
		pST = pSS->states + pSS->current_state;
		pSS->action_complete = TRUE;
	}
}
/* Initialize state-set tasks */
LOCAL VOID ss_task_init(pSP, pSS)
SPROG	*pSP;
SSCB	*pSS;
{
	/* Import Channel Access context from the main task */
	ca_import(pSP->task_id);

	return;
}
/* Return time-out for delay() */
LOCAL long get_timeout(pSS)
SSCB	*pSS;
{
	int		ndelay;
	long		timeout, timeoutMin;	/* expiration clock time */
	long		delay;		/* min. delay (tics) */

	if (pSS->ndelay == 0)
		return MAX_TIMEOUT;

	timeoutMin = MAX_TIMEOUT; /* start with largest timeout */

	/* Find the minimum abs. timeout (0 means already expired) */
	for (ndelay = 0; ndelay < pSS->ndelay; ndelay++)
	{
		timeout = pSS->timeout[ndelay];
		if (timeout == 0)
			continue;	/* already expired */
		if (pSS->time >= timeout)
		{	/* just expired */
			timeoutMin = pSS->time;
			pSS->timeout[ndelay] = 0; /* mark as expired */
		}
		else if (timeout < timeoutMin)
		{
			timeoutMin = timeout;
		}
	}

	/* Convert minimum timeout to delay */
	delay = timeoutMin - tickGet();
	return delay;
}
/* Set-up for delay() on entering a state.  This routine is called
by the state program for each delay in the "when" statement  */
VOID seq_start_delay(pSP, pSS, delay_id, delay)
SPROG	*pSP;
SSCB	*pSS;
int	delay_id;		/* delay id */
float	delay;
{
	int		td_tics;

	/* Note: the following 2 lines could be combined, but doing so produces
	 * the undefined symbol "Mund", which is not in VxWorks library */
	td_tics = delay*60.0;
	pSS->timeout[delay_id] = pSS->time + td_tics;
	delay_id += 1;
	if (delay_id > pSS->ndelay)
		pSS->ndelay = delay_id;
	return;
}

/* Test for time-out expired */
BOOL seq_test_delay(pSP, pSS, delay_id)
SPROG	*pSP;
SSCB	*pSS;
int	delay_id;
{
	if (pSS->timeout[delay_id] == 0)
		return TRUE; /* previously expired t-o */

	if (tickGet() >= pSS->timeout[delay_id])
	{
		pSS->timeout[delay_id] = 0; /* mark as expired */
		return TRUE;
	}
	return FALSE;
}
/*
 * Delete the state set tasks and do general clean-up.
 * General procedure is:
 * 1.  Suspend all state set tasks except self.
 * 2.  Call the user program's exit routine.
 * 3.  Delete all state set tasks except self.
 * 4.  Delete semaphores, close log file, and free allocated memory.
 * 5.  Return, causing self to be deleted.
 *
 * This task is run whenever ANY task in the system is deleted.
 * Therefore, we have to check the task belongs to a state program.
 * With VxWorks 5.0 we need to change references to the TCBX to handle
 * the Wind kernel.
 */
#ifdef	V5_vxWorks
sprog_delete(tid)
int		tid;
{
	int		nss, tid_ss;
	SPROG		*pSP, *seqFindProg();
	SSCB		*pSS;

#else
sprog_delete(pTcbX)
TCBX	*pTcbX;	/* ptr to TCB of task to be deleted */
{
	int		tid, nss, tid_ss;
	SPROG		*pSP, *seqFindProg();
	SSCB		*pSS;

	tid = pTcbX->taskId;
#endif
	pSP = seqFindProg(tid);
	if (pSP == NULL)
		return; /* not a state program task */

	logMsg("Delete %s: pSP=%d=0x%x, tid=%d\n",
	 pSP->name, pSP, pSP, tid);

	/* Is this a real sequencer task? */
	if (pSP->magic != MAGIC)
	{
		logMsg("  Not main state program task\n");
		return -1;
	}

	/* Suspend all state set tasks except self */
	pSS = pSP->sscb;
#ifdef	DEBUG
	logMsg("   Suspending state set tasks:\n");
#endif	DEBUG
	for (nss = 0; nss < pSP->nss; nss++, pSS++)
	{
		tid_ss = pSS->task_id;
		if ( (tid_ss > 0) && (tid != tid_ss) )
		{
#ifdef	DEBUG
			logMsg("    suspend task: tid=%d\n", tid_ss);
#endif	DEBUG
			taskSuspend(tid_ss);
		}
	}

	/* Call user exit routine (only if task has run) */
	if (pSP->sscb->task_id > 0)
	{
#ifdef	DEBUG
		logMsg("   Call exit function\n");
#endif	DEBUG
		pSP->exit_func(pSP, pSP->user_area);
	}

	/* Close the log file */
	if (pSP->logFd > 0 && pSP->logFd != ioGlobalStdGet(1))
	{
#ifdef	DEBUG
		logMsg("Closing log fd=%d\n", pSP->logFd);
#endif	DEBUG
		close(pSP->logFd);
		pSP->logFd = ioGlobalStdGet(1);
	}

	/* Remove the state program from the state program list */
	seqDelProg(pSP);

	/* Delete state set tasks (except self) & delete their semaphores */
	pSS = pSP->sscb;
	for (nss = 0; nss < pSP->nss; nss++, pSS++)
	{
		tid_ss = pSS->task_id;
		if ( tid_ss > 0)
		{
			if ( (tid != tid_ss) && (tid_ss > 0) )
			{
#ifdef	DEBUG
				logMsg("   delete ss task: tid=%d\n", tid_ss);
#endif	DEBUG
				taskDelete(tid_ss);
			}
			semDelete(pSS->syncSemId);
			if (!pSP->async_flag)
				semDelete(pSS->getSemId);
		}
	}

	/* Delete program-wide semaphores */
	semDelete(pSP->caSemId);

	/* Free the memory that was allocated for the task area */
#ifdef	DEBUG
	logMsg("free pSP->dyn_ptr=0x%x\n", pSP->dyn_ptr);
#endif	DEBUG
	taskDelay(5);
	free(pSP->dyn_ptr);
	
	return 0;
}
/* VxWorks Version 4 only */
#ifndef	V5_vxWorks

/* seq_semTake - take semaphore:
 * VxWorks semTake() with timeout added. (emulates VxWorks 5.0)
 */
VOID seq_semTake(semId, timeout)
SEM_ID		semId;		/* semaphore id to take */
long		timeout;	/* timeout in tics */
{
	int		dummy;

	if (timeout == WAIT_FOREVER)
		vrtxPend(&semId->count, 0, &dummy);
	else if (timeout == NO_WAIT)
		semClear(semId);
	else
		vrtxPend(&semId->count, timeout, &dummy);
}
#endif	V5_vxWorks
