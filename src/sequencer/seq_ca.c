/*
	seq_ca.c,v 1.2 1995/06/27 15:25:54 wright Exp

 *   DESCRIPTION: Channel access interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-1994, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *	  The Controls and Automation Group (AT-8)
 *	  Ground Test Accelerator
 *	  Accelerator Technology Division
 *	  Los Alamos National Laboratory
 *
 *	Co-developed with
 *	  The Controls and Computing Group
 *	  Accelerator Systems Division
 *	  Advanced Photon Source
 *	  Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * 03jul91,ajk	.
 * 11dec91,ajk	Cosmetic changes (comments & names)
 * 13feb92,ajk	All seqLog() calls compile only if DEBUG is defined.
 * 28apr92,ajk	Implemented new event flag mode.
 * 21may92,ajk	Will periodically announce number of connected channels
 *		if waiting form some to connect.
 * 17feb93,ajk	Implemented code to allow sharing of a single CA task by
 *		all state programs.  Added seq_disconnect() and ca_import_cancel().
		DB name resolution was moved to seq_main.c.
 * 19feb93,ajk	Added patched version of VxWorks 5.02b taskVarDelete().
 * 01mar94,ajk	Moved "seq_pv*()" functions to seq_if.c.
 * 28mar94,ajk	Restructured event& callback handlers to call proc_db_events().
 * 29mar94,ajk	Removed getPtrToValue().  Offset is now in db_channel structure.
 * 08apr94,ajk	Added support for time stamp.
 */

#define		ANSI
#include	"seq.h"

LOCAL VOID proc_db_events(union db_access_val *, CHAN *, int);

/*#define		DEBUG*/

#ifdef		DEBUG
#undef		LOCAL
#define		LOCAL
#endif		/* DEBUG */
/*
 * seq_connect() - Connect to all database channels through channel access.
 */
seq_connect(pSP)
SPROG		*pSP;
{
	CHAN		*pDB;
	int		status, i;
	extern		VOID seq_conn_handler();
	extern int	seqInitialTaskId;

	/*
	 * For each channel: connect to db & isssue monitor (if monFlag is TRUE).
	 */
	for (i = 0, pDB = pSP->pChan; i < pSP->numChans; i++, pDB++)
	{
		if (pDB->dbName == NULL || pDB->dbName[0] == 0)
			continue; /* skip records without pv names */
		pDB->assigned = TRUE;
		pSP->assignCount += 1; /* keep track of number of *assigned* channels */
#ifdef	DEBUG
		logMsg("seq_connect: connect %s to %s\n",
		 pDB->pVarName, pDB->dbName);
#endif	/* DEBUG */
		/* Connect to it */
		status = ca_build_and_connect(
			pDB->dbName,		/* DB channel name */
			TYPENOTCONN,		/* Don't get initial value */
			0,			/* get count (n/a) */
			&(pDB->chid),		/* ptr to chid */
			0,			/* ptr to value (n/a) */
			seq_conn_handler,	/* connection handler routine */
			pDB);			/* user ptr is CHAN structure */
		if (status != ECA_NORMAL)
		{
			SEVCHK(status, "ca_search");
			ca_task_exit();
			return -1;
		}
		/* Clear monitor indicator */
		pDB->monitored = FALSE;

		/*
		 * Issue monitor request
		 */
		if (pDB->monFlag)
		{
			seq_pvMonitor((SS_ID)pSP->pSS, i);
		}
	}
	ca_flush_io();
	return 0;
}

#define		GET_COMPLETE	0
#define		MON_COMPLETE	1

/*
 * seq_event_handler() - Channel access events (monitors) come here.
 * args points to CA event handler argument structure.  args.usr contains
 * a pointer to the channel structure (CHAN *).
 */
VOID seq_event_handler(args)
struct	event_handler_args args;
{
	/* Process event handling in each state set */
	proc_db_events((union db_access_val *)args.dbr, (CHAN *)args.usr, MON_COMPLETE);

	return;
}

/*
 * seq_callback_handler() - Sequencer callback handler.
 * Called when a "get" completes.
 * args.usr points to the db structure (CHAN *) for tis channel.
 */
VOID seq_callback_handler(args)
struct event_handler_args	args;
{

	/* Process event handling in each state set */
	proc_db_events((union db_access_val *)args.dbr, (CHAN *)args.usr, GET_COMPLETE);

	return;
}

/* Common code for event and callback handling */
LOCAL VOID proc_db_events(pAccess, pDB, complete_type)
union db_access_val	*pAccess;
CHAN			*pDB;
int			complete_type;
{
	SPROG			*pSP;
	void			*pVal;
	int			i;

#ifdef	DEBUG
	logMsg("proc_db_events: var=%s, pv=%s\n", pDB->VarName, pDB->dbName);
#endif	/* DEBUG */

	/* Copy value returned into user variable */
	pVal = (void *)pAccess + pDB->dbOffset; /* ptr to data in CA structure */
	bcopy(pVal, pDB->pVar, pDB->size * pDB->dbCount);

	/* Copy status & severity */
	pDB->status = pAccess->tchrval.status;
	pDB->severity = pAccess->tchrval.severity;

	/* Copy time stamp */
	pDB->timeStamp = pAccess->tchrval.stamp;

	/* Get ptr to the state program that owns this db entry */
	pSP = pDB->sprog;

	/* Wake up each state set that uses this channel in an event */
	seqWakeup(pSP, pDB->eventNum);

	/* If there's an event flag associated with this channel, set it */
	if (pDB-> efId > 0)
		seq_efSet((SS_ID)pSP->pSS, pDB->efId);

	/* Special processing for completed pvGet() */
	if (complete_type == GET_COMPLETE)
	{
		pDB->getComplete = TRUE;

		/* If syncronous pvGet then notify pending state set */
		if (pDB->getSemId != NULL)
			semGive(pDB->getSemId);
	}

	return;
}

/*	Disconnect all database channels */
seq_disconnect(pSP)
SPROG		*pSP;
{
	CHAN		*pDB;
	STATUS		status;
	int		i;
	extern int	seqAuxTaskId;
	SPROG		*pMySP; /* NULL if this task is not a sequencer task */

	/* Did we already disconnect? */
	if (pSP->connCount < 0)
		return 0;

	/* Import Channel Access context from the auxillary seq. task */
	pMySP = seqFindProg(taskIdSelf() );
	if (pMySP == NULL)
	{
		ca_import(seqAuxTaskId); /* not a sequencer task */
	}

	pDB = pSP->pChan;
	for (i = 0; i < pSP->numChans; i++, pDB++)
	{
		if (!pDB->assigned)
			continue;
#ifdef	DEBUG
		logMsg("seq_disconnect: disconnect %s from %s\n",
		 pDB->pVarName, pDB->dbName);
		taskDelay(30);
#endif	/* DEBUG */
		/* Disconnect this channel */
		status = ca_clear_channel(pDB->chid);

		if (status != ECA_NORMAL)
		{
			/* SEVCHK(status, "ca_clear_chan"); */
			/* ca_task_exit(); */
			/* return -1; */
		}

		/* Clear monitor & connect indicators */
		pDB->monitored = FALSE;
		pDB->connected = FALSE;
	}

	pSP->connCount = -1; /* flag to indicate all disconnected */

	ca_flush_io();

	/* Cancel CA context if it was imported above */
	if (pMySP == NULL)
	{
logMsg("seq_disconnect: cancel import CA context\n");
		ca_import_cancel(taskIdSelf());
	}

	return 0;
}

/*
 * seq_conn_handler() - Sequencer connection handler.
 * Called each time a connection is established or broken.
 */
VOID seq_conn_handler(args)
struct connection_handler_args	args;
{
	CHAN		*pDB;
	SPROG		*pSP;

	/* User argument is db ptr (specified at ca_search() ) */
	pDB = (CHAN *)ca_puser(args.chid);

	/* State program that owns this db entry */
	pSP = pDB->sprog;

	/* Check for connected */
	if (ca_field_type(args.chid) == TYPENOTCONN)
	{
		pDB->connected = FALSE;
		pSP->connCount--;
		pDB->monitored = FALSE;
#ifdef	DEBUG
		logMsg("%s disconnected from %s\n", pDB->VarName, pDB->dbName);
#endif	/* DEBUG */
	}
	else	/* PV connected */
	{
		pDB->connected = TRUE;
		pSP->connCount++;
		if (pDB->monFlag)
			pDB->monitored = TRUE;
#ifdef	DEBUG
		logMsg("%s connected to %s\n", pDB->VarName, pDB->dbName);
#endif	/* DEBUG */
		pDB->dbCount = ca_element_count(args.chid);
		if (pDB->dbCount > pDB->count)
			pDB->dbCount = pDB->count;
	}

	/* Wake up each state set that is waiting for event processing */
	seqWakeup(pSP, 0);
	
	return;
}

/*
 * seqWakeup() -- wake up each state set that is waiting on this event
 * based on the current event mask.   EventNum = 0 means wake all state sets.
 */
VOID seqWakeup(pSP, eventNum)
SPROG		*pSP;
int		eventNum;
{
	int		nss;
	SSCB		*pSS;

	/* Check event number against mask for all state sets: */
	for (nss = 0, 	pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		/* If event bit in mask is set, wake that state set */
		if ( (eventNum == 0) || bitTest(pSS->pMask, eventNum) )
		{
			semGive(pSS->syncSemId); /* wake up the ss task */
		}

	}
	return;
}
#include "memLib.h"
#include "taskVarLib.h"
#include "taskLib.h"
/*******************************************************************************
* P A  T C H E D  5.02b -- allows call from taskDeleteHook routine -- ajk
* taskVarDelete - remove a task variable from a task
*
* This routine removes the specified task variable from the calling
* task's context.  The private value of that variable is lost.
*
* RETURNS
* OK, or
* ERROR if the calling task does not own the specified task variable.
*
* SEE ALSO: taskVarAdd(2), taskVarGet(2), taskVarSet(2)
*/

LOCAL STATUS LtaskVarDelete (tid, pVar)
    int tid;	/* task id whose task variable is to be retrieved */
    int *pVar;	/* pointer to task variable to be removed from task */

    {
    FAST TASK_VAR **ppTaskVar;		/* ptr to ptr to next node */
    FAST TASK_VAR *pTaskVar;
    WIND_TCB *pTcb = (WIND_TCB *)tid;	/* P A T C H -- ajk 19feb93*/

    if (pTcb == NULL)			/* check that task is valid */
	return (ERROR);

    /* find descriptor for specified task variable */

    for (ppTaskVar = &pTcb->pTaskVar;
	 *ppTaskVar != NULL;
	 ppTaskVar = &((*ppTaskVar)->next))
	{
	pTaskVar = *ppTaskVar;

	if (pTaskVar->address == pVar)
	    {
	    /* if active task, replace background value */

	    if (taskIdCurrent == pTcb)
		*pVar = pTaskVar->value;

	    *ppTaskVar = pTaskVar->next;/* delete variable from list */

	    free ((char *)pTaskVar);	/* free storage of deleted cell */

	    return (OK);
	    }
	}

    /* specified address is not a task variable for specified task */

    errnoSet (S_taskLib_TASK_VAR_NOT_FOUND);
    return (ERROR);
    }
/* Temporary routine to cancel ca_import() -- THIS SHOULD GO INTO CHANNEL ACCESS! */
ca_import_cancel(tid)
int		tid;
{
	extern int	ca_static;
	int		status;

	status = LtaskVarDelete(tid, &ca_static);
	if (status != OK)
	{
		logMsg("Seq: taskVarDelete failed for tid = 0x%x\n", tid);
	}

	return status;
}
