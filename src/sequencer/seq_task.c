/**************************************************************************
			GTA PROJECT   AT division
		Copyright, 1990-1994
		The Regents of the University of California and
		the University of Chicago.
		Los Alamos National Laboratory
	seq_task.c,v 1.3 1995/10/19 16:30:18 wright Exp

	DESCRIPTION: Seq_tasks.c: Task creation and control for sequencer
	state sets.

	ENVIRONMENT: VxWorks
	HISTORY
04dec91,ajk	Implemented linked list of state programs, eliminating task
		variables.
11dec91,ajk	Made cosmetic changes and cleaned up comments.
19dec91,ajk	Changed algoritm in seq_getTimeout().
29apr92,ajk	Implemented new event flag mode.
30apr92,ajk	Periodically call ca_pend_event() to detect connection failures.
21may92,ajk	In sprog_delete() wait for loggin semaphore before suspending tasks.
		Some minor changes in the way semaphores are deleted.
18feb92,ajk	Changed to allow sharing of single CA task by all state programs. 
		Added seqAuxTask() and removed ca_pend_event() from ss_entry().
09aug93,ajk	Added calls to taskwdInsert() & taskwdRemove().
24nov93,ajk	Added support for assigning array elements to db channels.
24nov93,ajk	Changed implementation of event bits to support unlimited channels
20may94,ajk	Changed sprog_delete() to spawn a separate cleanup task.
19oct95,ajk/rmw Fixed bug which kept events from being cleared in old eventflag mode
20jul95,ajk	Add user-specified task priority to taskSpwan().
***************************************************************************/
/*#define		DEBUG*/

#include 	<string.h>

#define		ANSI
#include	"seqCom.h"
#include	"seq.h"
#include	"taskwd.h"
#include	"logLib.h"
#include	"tickLib.h"
#include	"taskVarLib.h"

/* Function declarations */
LOCAL	VOID seq_waitConnect(SPROG *pSP, SSCB *pSS);
LOCAL	VOID ss_task_init(SPROG *, SSCB *);
LOCAL	VOID seq_clearDelay(SSCB *);
LOCAL	int seq_getTimeout(SSCB *);
LOCAL	long seq_cleanup(int tid, SPROG *pSP, SEM_ID cleanupSem);

#define		TASK_NAME_SIZE 10


STATUS seqAddProg(SPROG *pSP);
long seq_connect(SPROG *pSP);
long seq_disconnect(SPROG *pSP);
/*
 * sequencer() - Sequencer main task entry point.
 */
long sequencer (pSP, stack_size, pTaskName)
SPROG		*pSP;	/* ptr to original (global) state program table */
long		stack_size;	/* stack size */
char		*pTaskName;	/* Parent task name */
{
	SSCB		*pSS;
	int		nss, task_id;
	char		task_name[TASK_NAME_SIZE+10];
	extern		VOID ss_entry();
	extern		int seqAuxTaskId;

	pSP->taskId = taskIdSelf(); /* my task id */
	pSS = pSP->pSS;
	pSS->taskId = pSP->taskId;

	/* Add the program to the state program list */
	seqAddProg(pSP);

	/* Import Channel Access context from the auxillary seq. task */
	ca_import(seqAuxTaskId);

	/* Initiate connect &  monitor requests to database channels. */
	seq_connect(pSP);

	/* Additional state set task names are derived from the first ss */
	if (strlen(pTaskName) > TASK_NAME_SIZE)
		pTaskName[TASK_NAME_SIZE] = 0;

	/* Create each additional state set task */
	for (nss = 1, pSS = pSP->pSS + 1; nss < pSP->numSS; nss++, pSS++)
	{
		/* Form task name from program name + state set number */
		sprintf(task_name, "%s_%d", pTaskName, nss);

		/* Spawn the task */
		task_id = taskSpawn(
		  task_name,				/* task name */
		  pSP->taskPriority,	/* priority */
		  SPAWN_OPTIONS,			/* task options */
		  stack_size,				/* stack size */
		  (FUNCPTR)ss_entry,			/* entry point */
		  (int)pSP, (int)pSS, 0,0,0,0,0,0,0,0);	/* pass 2 parameters */

		seq_log(pSP, "Spawning task %d: \"%s\"\n", task_id, task_name);
	}

	/* First state set jumps directly to entry point */
	ss_entry(pSP, pSP->pSS);

	return 0;
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
	char		*pVar;
	int		nWords;
	SS_ID		ssId;

	/* Initialize all ss tasks */
	ss_task_init(pSP, pSS);
	
	/* If "+c" option, wait for all channels to connect */
	if ((pSP->options & OPT_CONN) != 0)
		seq_waitConnect(pSP, pSS);

	/* Initilaize state set to enter the first state */
	pST = pSS->pStates;
	pSS->currentState = 0;

	/* Use the event mask for this state */
	pSS->pMask = (pST->pEventMask);
	nWords = (pSP->numEvents + NBITS - 1) / NBITS;

	/* Local ptr to user variables (for reentrant code only) */
	pVar = pSP->pVar;

	/* State set id */
	ssId = (SS_ID)pSS;

	/*
	 * ============= Main loop ==============
	 */
	while (1)
	{
		seq_clearDelay(pSS); /* Clear delay list */
		pST->delayFunc(ssId, pVar); /* Set up new delay list */

		/* Setting this semaphor here guarantees that a when() is always
		 * executed at least once when a state is first entered. */
		semGive(pSS->syncSemId);

		/*
		 * Loop until an event is triggered, i.e. when() returns TRUE
		 */
		do {
			/* Wake up on CA event, event flag, or expired time delay */
			delay = seq_getTimeout(pSS); /* min. delay from list */
			if (delay > 0)
				semTake(pSS->syncSemId, delay);

			/* Call the event function to check for an event trigger.
			 * The statement inside the when() statement is executed.
			 * Note, we lock out CA events while doing this.
			 */
			semTake(pSP->caSemId, WAIT_FOREVER);

			ev_trig = pST->eventFunc(ssId, pVar,
			 &pSS->transNum, &pSS->nextState); /* check events */

		        /* Clear all event flags (old ef mode only) */
			if ( ev_trig && ((pSP->options & OPT_NEWEF) == 0) )
			{    /* Clear all event flags (old mode only) */
			    register	int i;

			    for (i = 0; i < nWords; i++)
					pSP->pEvents[i] = pSP->pEvents[i] & !pSS->pMask[i];
				
			}

			semGive(pSP->caSemId);

		} while (!ev_trig);


		/*
		 * An event triggered:
		 * execute the action statements and enter the new state.
		 */

		/* Change event mask ptr for next state */
		pStNext = pSS->pStates + pSS->nextState;
		pSS->pMask = (pStNext->pEventMask);

		/* Execute the action for this event */
		pST->actionFunc(ssId, pVar, pSS->transNum);

		/* Flush any outstanding DB requests */
		ca_flush_io();

		/* Change to next state */
		pSS->prevState = pSS->currentState;
		pSS->currentState = pSS->nextState;
		pST = pSS->pStates + pSS->currentState;
	}
}
/* Initialize state-set tasks */
LOCAL VOID ss_task_init(pSP, pSS)
SPROG	*pSP;
SSCB	*pSS;
{
	extern	int	seqAuxTaskId;

	/* Get this task's id */
	pSS->taskId = taskIdSelf();

	/* Import Channel Access context from the auxillary seq. task */
	if (pSP->taskId != pSS->taskId)
		ca_import(seqAuxTaskId);

	/* Register this task with the EPICS watchdog (no callback function) */
	taskwdInsert(pSS->taskId, (VOIDFUNCPTR)0, (VOID *)0);

	return;
}

/* Wait for all channels to connect */
LOCAL VOID seq_waitConnect(SPROG *pSP, SSCB *pSS)
{
	STATUS		status;
	long		delay;

	delay = 600; /* 10, 20, 30, 40, 40,... sec */
	while (pSP->connCount < pSP->assignCount)
	{
		status = semTake(pSS->syncSemId, delay);
		if ((status != OK) && (pSP-> taskId == pSS->taskId))
		{
			logMsg("%d of %d assigned channels have connected\n",
			 pSP->connCount, pSP->assignCount, 0,0,0,0);
		}
		if (delay < 2400)
			delay = delay + 600;
	}
}
/*
 * seq_clearDelay() - clear the time delay list.
 */
LOCAL VOID seq_clearDelay(pSS)
SSCB		*pSS;
{
	int		ndelay;

	pSS->timeEntered = tickGet(); /* record time we entered this state */

	for (ndelay = 0; ndelay < MAX_NDELAY; ndelay++)
	{
		pSS->delay[ndelay] = 0;
		pSS->delayExpired[ndelay] = FALSE;
	}

	pSS->numDelays = 0;

	return;
}

/*
 * seq_getTimeout() - return time-out for pending on events.
 * Returns number of tics to next expected timeout of a delay() call.
 * Returns INT_MAX if no delays pending 
 * An "int" is returned because this is what semTake() expects
 */
LOCAL int seq_getTimeout(pSS)
SSCB		*pSS;
{
	int	ndelay, delayMinInit;
	ULONG	cur, delay, delayMin, delayN;

	if (pSS->numDelays == 0)
		return INT_MAX;

	/*
	 * calculate the delay since this state was entered
	 */
	cur = tickGet();
	if (cur >= pSS->timeEntered) {
		delay = cur - pSS->timeEntered;
	}
	else {
		delay = cur + (ULONG_MAX - pSS->timeEntered);
	}

	delayMinInit = 0;
	delayMin = ULONG_MAX;

	/* Find the minimum  delay among all non-expired timeouts */
	for (ndelay = 0; ndelay < pSS->numDelays; ndelay++)
	{
		if (pSS->delayExpired[ndelay])
			continue;	/* skip if this delay entry already expired */

		delayN = pSS->delay[ndelay];
		if (delay >= delayN)
		{	/* just expired */
			pSS->delayExpired[ndelay] = TRUE; /* mark as expired */
			return 0;
		}

		if (delayN<=delayMin) {
			delayMinInit=1;
			delayMin = delayN;  /* this is the min. delay so far */
		}
	}

	/*
	 * If there is no unexpired delay in the list
	 * then wait forever until there is a PV state
	 * change
	 */
	if (!delayMinInit) {
		return INT_MAX;
	}

	/*
	 * unexpired delay _is_ in the list
	 */
	if (delayMin>delay) {
		delay = delayMin - delay;

		/*
		 * clip to the range of a valid delay in semTake()
		 */
		if (delay<INT_MAX) {
			return delay;
		}
		else {
			return INT_MAX;
		}
	}
	else {
		return 0;
	}
}

/*
 * Delete all state set tasks and do general clean-up.
 * General procedure is to spawn a task that does the following:
 * 1.  Suspend all state set tasks except self.
 * 2.  Call the user program's exit routine.
 * 3.  Disconnect all channels for this state program.
 * 4.  Cancel the channel access context.
 * 5.  Delete all state set tasks except self.
 * 6.  Delete semaphores, close log file, and free allocated memory.
 * 7.  Return, causing self to be deleted.
 *
 * This task is run whenever ANY task in the system is deleted.
 * Therefore, we have to check the task belongs to a state program.
 * Note:  We could do this without spawning a task, except for the condition
 * when executed in the context of ExcTask.  ExcTask is not spawned with
 * floating point option, and fp is required to do the cleanup.
 */
long sprog_delete(tid)
int		tid; /* task being deleted */
{
	SPROG		*pSP;
	SEM_ID		cleanupSem;
	int		status;
	
	pSP = seqFindProg(tid);
	if (pSP == NULL)
		return -1; /* not a state program task */

	/* Create a semaphore for cleanup task */
	cleanupSem = semBCreate (SEM_Q_FIFO, SEM_EMPTY);

	/* Spawn the cleanup task */
	taskSpawn("tSeqCleanup", SPAWN_PRIORITY-1, VX_FP_TASK, 8000,
	 (FUNCPTR)seq_cleanup, tid, (int)pSP, (int)cleanupSem, 0,0,0,0,0,0,0);

	/* Wait for cleanup task completion */
	for (;;)
	{
		status = semTake(cleanupSem, 600);
		if (status == OK)
			break;
		logMsg("sprog_delete waiting for seq_cleanup\n", 0,0,0,0,0,0);
	}
	semDelete(cleanupSem);

	return 0;
}

/* Cleanup task */
/*#define	DEBUG_CLEANUP*/
STATUS seqDelProg(SPROG *pSP);

LOCAL long seq_cleanup(int tid, SPROG *pSP, SEM_ID cleanupSem)
{
	int		nss;
	SSCB		*pSS;

#ifdef	DEBUG_CLEANUP
	logMsg("Delete %s: pSP=%d=0x%x, tid=%d\n", pSP->pProgName, pSP, pSP, tid, 0,0);
#endif	/*DEBUG_CLEANUP*/

	/* Wait for log semaphore (in case a task is doing a write) */
	semTake(pSP->logSemId, 600);

	/* Remove tasks' watchdog & suspend all state set tasks except self */
#ifdef	DEBUG_CLEANUP
	logMsg("   Suspending state set tasks:\n");
#endif	/*DEBUG_CLEANUP*/
	pSS = pSP->pSS;
	for (nss = 0; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->taskId == 0)
			continue;
#ifdef	DEBUG_CLEANUP
		logMsg("      tid=%d\n", pSS->taskId);
#endif	/*DEBUG_CLEANUP*/
		/* Remove the task from EPICS watchdog */
		taskwdRemove(pSS->taskId);

		/* Suspend the task */
		if (pSS->taskId != tid)
			taskSuspend(pSS->taskId);
	}

	/* Give back log semaphore */
	semGive(pSP->logSemId);

	/* Call user exit routine (only if task has run) */
	if (pSP->pSS->taskId != 0)
	{
#ifdef	DEBUG_CLEANUP
		logMsg("   Call exit function\n");
#endif	/*DEBUG_CLEANUP*/
		pSP->exitFunc( (SS_ID)pSP->pSS, pSP->pVar);
	}

	/* Disconnect all channels */
#ifdef	DEBUG_CLEANUP
	logMsg("   Disconnect all channels\n");
#endif	/*DEBUG_CLEANUP*/

	seq_disconnect(pSP);

	/* Cancel the CA context for each state set task */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->taskId == 0)
			continue;
#ifdef	DEBUG_CLEANUP
		logMsg("   ca_import_cancel(0x%x)\n", pSS->taskId);
#endif	/*DEBUG_CLEANUP*/
		SEVCHK (ca_import_cancel(pSS->taskId),
			"seq_cleanup:ca_import_cancel() failed")
	}

	/* Close the log file */
	if ( (pSP->logFd > 0) && (pSP->logFd != ioGlobalStdGet(1)) )
	{
#ifdef	DEBUG_CLEANUP
		logMsg("Closing log fd=%d\n", pSP->logFd);
#endif	/*DEBUG_CLEANUP*/
		close(pSP->logFd);
		pSP->logFd = ioGlobalStdGet(1);
	}

	/* Remove the state program from the state program list */
	seqDelProg(pSP);

	/* Delete state set tasks (except self) & delete their semaphores */
	pSS = pSP->pSS;
	for (nss = 0; nss < pSP->numSS; nss++, pSS++)
	{
		if ( (pSS->taskId != tid) && (pSS->taskId != 0) )
		{
#ifdef	DEBUG_CLEANUP
			logMsg("   delete ss task: tid=%d\n", pSS->taskId);
#endif	/*DEBUG_CLEANUP*/
			taskDelete(pSS->taskId);
		}

		if (pSS->syncSemId != 0)
			semDelete(pSS->syncSemId);

		if (pSS->getSemId != 0)
			semDelete(pSS->getSemId);
	}

	/* Delete program-wide semaphores */
	semDelete(pSP->caSemId);
	semDelete(pSP->logSemId);

	/* Free all allocated memory */
	taskDelay(5);
	seqFree(pSP);

	semGive(cleanupSem);
	
	return 0;
}
/* seqFree()--free all allocated memory */
VOID seqFree(pSP)
SPROG		*pSP;
{
	SSCB		*pSS;
	CHAN		*pDB;
	MACRO		*pMac;
	int		n;

	/* Free macro table entries */
	for (pMac = pSP->pMacros, n = 0; n < MAX_MACROS; pMac++, n++)
	{
		if (pMac->pName != NULL)
			free(pMac->pName);
		if (pMac->pValue != NULL)
			free(pMac->pValue);
	}

	/* Free MACRO table */
	free(pSP->pMacros);

	/* Free channel names */
	for (pDB = pSP->pChan, n = 0; n < pSP->numChans; pDB++, n++)
	{
		if (pDB->dbName != NULL)
			free(pDB->dbName);
	}

	/* Free channel structures */
	free(pSP->pChan);

	/* Free STATE blocks */
	pSS = pSP->pSS;
	free(pSS->pStates);

	/* Free event words */
	free(pSP->pEvents);

	/* Free SSCBs */
	free(pSP->pSS);

	/* Free SPROG */
	free(pSP);
}

/* 
 * Sequencer auxillary task -- loops on ca_pend_event().
 */
long seqAuxTask()
{
	extern		int seqAuxTaskId;

	/* Register this task with the EPICS watchdog */
	taskwdInsert(taskIdSelf(),(VOIDFUNCPTR)0, (VOID *)0);
	/* Set up so all state program tasks will use a common CA context */
	ca_task_initialize();
	seqAuxTaskId = taskIdSelf(); /* must follow ca_task_initialize() */

	/* This loop allows CA to check for connect/disconnect on channels */
	for (;;)
	{
		ca_pend_event(10.0); /* returns every 10 sec. */
	}
}

