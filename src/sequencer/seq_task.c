/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_task.c	1.4	2/1/91
	DESCRIPTION: Seq_tasks.c: Task creation and control for sequencer
	state sets.

	ENVIRONMENT: VxWorks
***************************************************************************/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"


LOCAL ss_task_init();
LOCAL ULONG get_timeout();
LOCAL VOID semTake();


extern TASK *task_area_ptr;

/* Create state set tasks.  Set task variable for each task */
#define	TASK_NAME_SIZE	25
create_ss_tasks(sp_ptr, stack_size)
SPROG	*sp_ptr;
int	stack_size;
{
	SSCB	*ss_ptr;
	int	nss;
	char	task_name[2*TASK_NAME_SIZE+2];
	extern	TASK *task_area_ptr;
	extern	ss_entry();

	ss_ptr = sp_ptr->sscb;
	task_name[0] = 0;
	strncat(task_name, sp_ptr->name, TASK_NAME_SIZE);
	strcat(task_name, "_");
	
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		if (nss != 0)
		{	/* Spawn the ss task */
			/* Form task name from program name + state set name */
			strncat(task_name, ss_ptr->name, TASK_NAME_SIZE);

			ss_ptr->task_id = taskSpawn(task_name,
			 SPAWN_PRIORITY+ss_ptr->task_priority,
			 SPAWN_OPTIONS, stack_size, ss_entry, sp_ptr, ss_ptr);
#ifdef	DEBUG
			printf("task %d: %s\n", ss_ptr->task_id, task_name);
#endif	DEBUG
		}
	}
	/* Main task handles first state set */
	ss_ptr = sp_ptr->sscb;
	ss_ptr->task_id = sp_ptr->task_id;
	ss_entry(sp_ptr, ss_ptr);
}
/* Task entry point for state sets */
/* Provides main loop for state set processing (common, re-entrant code) */
ss_entry(sp_ptr, ss_ptr)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
{
	BOOL	ev_trig;
	STATE	*st_ptr;
	ULONG	delay, get_timeout();

	/* Initialize all tasks except the main task */
	if (ss_ptr->task_id != sp_ptr->task_id)
		ss_task_init(sp_ptr, ss_ptr);

	/* Start in first state */
	st_ptr = ss_ptr->current_state;

	/* Initialize for event flag processing */
	ss_ptr->ev_flag_mask = st_ptr->event_flag_mask;
	ss_ptr->ev_flag = 0;

	/* Main loop */
	while (1)
	{
#ifdef	DEBUG
		printf("State set %s, state %s\n", ss_ptr->name, st_ptr->name);
#endif	DEBUG
		ss_ptr->time = tickGet(); /* record time we entered this state */

		/* Call delay function to set up delays */
		ss_ptr->ndelay = 0;
		st_ptr->delay_func(sp_ptr, ss_ptr);

		/* On 1-st pass fall thru w/o wake up */
		semGive(ss_ptr->sem_id);

		/* Loop until an event is triggered */
		do {
			/* Wake up on CA event, event flag, or expired time delay */
			semTake(ss_ptr->sem_id, get_timeout(ss_ptr, st_ptr));
#ifdef	DEBUG
			printf("%s: semTake returned\n", ss_ptr->name);
#endif	DEBUG
			ss_ptr->time = tickGet();
			/* Call event function to check for event trigger */
			ev_trig = st_ptr->event_func(sp_ptr, ss_ptr);
		} while (!ev_trig);

		/* Event triggered, execute the corresponding action */
		ss_ptr->action_complete = FALSE;
		st_ptr->action_func(sp_ptr, ss_ptr);

		/* Change to next state */
		ss_ptr->current_state = ss_ptr->next_state;
		ss_ptr->next_state = NULL;
		st_ptr = ss_ptr->current_state;
		ss_ptr->ev_flag_mask = st_ptr->event_flag_mask;
		ss_ptr->ev_flag = 0;
		ss_ptr->action_complete = TRUE;
	}
}
/* Initialize state-set tasks */
LOCAL ss_task_init(sp_ptr, ss_ptr)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
{
	/* Import task variable context from main task */
	taskVarAdd(ss_ptr->task_id, &task_area_ptr);
	taskVarGet(sp_ptr->task_id, &task_area_ptr); /* main task's id */

	/* Import Channel Access context from main task */
	ca_import(sp_ptr->task_id);

	return;
}
/* Return time-out for semTake() */
LOCAL ULONG get_timeout(ss_ptr)
SSCB	*ss_ptr;
{
	int	ndelay;
	ULONG	time, timeout, tmin;

	if (ss_ptr->ndelay == 0)
		return 0;

	time = tickGet();
	tmin = 0;
	/* find lowest timeout that's not expired */
	for (ndelay = 0; ndelay < ss_ptr->ndelay; ndelay++)
	{
		timeout = ss_ptr->timeout[ndelay];
		if (time < timeout)
		{
			if (tmin == 0 || timeout < tmin)
				tmin = timeout;
		}
	}
	if (tmin != 0)
		tmin -= time; /* convert timeout to delay */
	return tmin;
}
/* Set-up for delay() on entering a state.  This routine is called
by the state program for each delay in the "when" statement  */
VOID seq_start_delay(sp_ptr, ss_ptr, delay_id, delay)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
int	delay_id;		/* delay id */
float	delay;
{
	ss_ptr->timeout[delay_id] = ss_ptr->time + delay*60.0;
	delay_id += 1;
	if (delay_id > ss_ptr->ndelay)
		ss_ptr->ndelay = delay_id;
	return;
}

/* Test for time-out expired */
BOOL seq_test_delay(sp_ptr, ss_ptr, delay_id)
SPROG	*sp_ptr;
SSCB	*ss_ptr;
int	delay_id;
{
	return (ss_ptr->time >= ss_ptr->timeout[delay_id]);
}
/* This task is run whenever ANY task in the system is deleted */
sprog_delete(pTcbX)
TCBX	*pTcbX;	/* ptr to TCB of task to be deleted */
{
	int	tid, nss, val, tid_ss;
	SPROG	*sp_ptr;
	SSCB	*ss_ptr;
	extern	TASK *task_area_ptr;
	TASK	*ta_ptr;

	tid = pTcbX->taskId;
	/* Note: task_area_ptr is a task variable */
	val = taskVarGet(tid, &task_area_ptr);
	if (val == ERROR)
		return 0; /* not one of our tasks */
	ta_ptr = (TASK *)val;
	printf("  ta_ptr=0x%x, tid=%d=0x%x\n", ta_ptr, tid, tid);
	sp_ptr = ta_ptr->sp_ptr;

	/* Is this a real sequencer task? */
	if (sp_ptr->magic != MAGIC)
	{
		printf("  Not main state program task\n");
		return -1;
	}

	/* Delete state set tasks */
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		tid_ss = ss_ptr->task_id;
		if ( tid_ss > 0)
		{
			/* Don't delete self yet */
			if (tid != tid_ss)
			{
				printf("   delete ss task: tid=%d=0x%x\n",
				 tid_ss, tid_ss);
				taskVarSet(tid_ss, &task_area_ptr, ERROR);
				taskDelete(tid_ss);
			}
		}
	}

	printf("free ta_ptr=0x%x\n", ta_ptr);
	taskDelay(60);
	free(ta_ptr);
	
	return 0;
}
/* semTake - take semaphore:
	VxWorks semTake with timeout added. */
LOCAL VOID semTake (semId, timeout)
SEM_ID semId;	/* semaphore id to take */
UINT timeout;	/* timeout in tics */
{
	int dummy;

	vrtxPend(&semId->count, timeout, &dummy);
}
