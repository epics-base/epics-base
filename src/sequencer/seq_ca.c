/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
	seq_ca.c,v 1.2 1995/06/27 15:25:54 wright Exp

 *   DESCRIPTION: Channel access interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
 *
 */

#define		ANSI
#include	"seq.h"
#include	"string.h"
#include	"taskVarLib.h"

LOCAL VOID proc_db_events(union db_access_val *, CHAN *, long);

/*#define		DEBUG*/

#ifdef		DEBUG
#undef		LOCAL
#define		LOCAL
#endif		/*DEBUG*/
/*
 * seq_connect() - Connect to all database channels through channel access.
 */
long seq_connect(SPROG *pSP)
{
	CHAN		*pDB;
	int		status, i;
	extern		VOID seq_conn_handler();

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
#endif	/*DEBUG*/
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
VOID seq_event_handler(struct event_handler_args args)
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
VOID seq_callback_handler(struct event_handler_args args)
{

	/* Process event handling in each state set */
	proc_db_events((union db_access_val *)args.dbr, (CHAN *)args.usr, GET_COMPLETE);

	return;
}

/* Common code for event and callback handling */
LOCAL VOID proc_db_events(union db_access_val *pAccess, CHAN *pDB, long complete_type)
{
	SPROG			*pSP;
	void			*pVal;

#ifdef	DEBUG
	logMsg("proc_db_events: var=%s, pv=%s\n", pDB->VarName, pDB->dbName);
#endif	/*DEBUG*/

	/* Copy value returned into user variable */
	pVal = (void *)((long)pAccess + pDB->dbOffset); /* ptr to data in CA structure */
	bcopy(pVal, pDB->pVar, pDB->size * pDB->dbCount);

	/* Copy status & severity */
	pDB->status = pAccess->tchrval.status;
	pDB->severity = pAccess->tchrval.severity;

	/* Copy time stamp */
	pDB->timeStamp = pAccess->tchrval.stamp;

	/* Get ptr to the state program that owns this db entry */
	pSP = pDB->sprog;

	/* Indicate completed pvGet() */
	if (complete_type == GET_COMPLETE)
	{
		pDB->getComplete = TRUE;
	}

	/* Wake up each state set that uses this channel in an event */
	seqWakeup(pSP, pDB->eventNum);

	/* If there's an event flag associated with this channel, set it */
	if (pDB-> efId > 0)
		seq_efSet((SS_ID)pSP->pSS, pDB->efId);

	/* Special processing for completed synchronous (-a) pvGet() */
	if ( (complete_type == GET_COMPLETE) && ((pSP->options & OPT_ASYNC) == 0) )
		semGive(pDB->getSemId);

	return;
}

/*	Disconnect all database channels */
/*#define	DEBUG_DISCONNECT*/

long seq_disconnect(SPROG *pSP)
{
	CHAN		*pDB;
	STATUS		status;
	int		i;
	extern int	seqAuxTaskId;
	SPROG		*pMySP; /* will be NULL if this task is not a sequencer task */

	/* Did we already disconnect? */
	if (pSP->connCount < 0)
		return 0;

	/* Import Channel Access context from the auxillary seq. task */
	pMySP = seqFindProg(taskIdSelf() );
	if (pMySP == NULL)
	{
		status = ca_import(seqAuxTaskId); /* not a sequencer task */
		SEVCHK (status, "seq_disconnect: ca_import");
	}

	pDB = pSP->pChan;
	for (i = 0; i < pSP->numChans; i++, pDB++)
	{
		if (!pDB->assigned)
			continue;
#ifdef	DEBUG_DISCONNECT
		logMsg("seq_disconnect: disconnect %s from %s\n",
		 pDB->pVarName, pDB->dbName);
		taskDelay(30);
#endif	/*DEBUG_DISCONNECT*/
		/* Disconnect this channel */
		status = ca_clear_channel(pDB->chid);
		SEVCHK (status, "seq_disconnect: ca_clear_channel");

		/* Clear monitor & connect indicators */
		pDB->monitored = FALSE;
		pDB->connected = FALSE;
	}

	pSP->connCount = -1; /* flag to indicate all disconnected */

	ca_flush_io();

	/* Cancel CA context if it was imported above */
	if (pMySP == NULL)
	{
#ifdef	DEBUG_DISCONNECT
		logMsg("seq_disconnect: ca_import_cancel\n");
#endif	/*DEBUG_DISCONNECT*/
		SEVCHK(ca_import_cancel(taskIdSelf()),
			"seq_disconnect: ca_import_cancel() failed!");
	}

	return 0;
}

/*
 * seq_conn_handler() - Sequencer connection handler.
 * Called each time a connection is established or broken.
 */
VOID seq_conn_handler(struct connection_handler_args args)
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
#ifdef	DEBUG
		logMsg("%s disconnected from %s\n", pDB->VarName, pDB->dbName);
#endif	/*DEBUG*/
                if(pDB->connected) {
                        pDB->connected = FALSE;
                        pSP->connCount--;
                        pDB->monitored = FALSE;
                } else {
                        printf("%s disconnected but already disconnected %s\n",
                                pDB->pVarName,pDB->dbName);
                }
	}
	else	/* PV connected */
	{
#ifdef	DEBUG
		logMsg("%s connected to %s\n", pDB->VarName, pDB->dbName);
#endif	/*DEBUG*/
                if(!pDB->connected) {
                        pDB->connected = TRUE;
                        pSP->connCount++;
                        if (pDB->monFlag)
                                pDB->monitored = TRUE;
                        pDB->dbCount = ca_element_count(args.chid);
                        if (pDB->dbCount > pDB->count)
                                pDB->dbCount = pDB->count;
                } else {
                        printf("%s connected but already connected %s\n",
                                pDB->pVarName,pDB->dbName);
                }
	}

	/* Wake up each state set that is waiting for event processing */
	seqWakeup(pSP, 0);
	
	return;
}

/*
 * seqWakeup() -- wake up each state set that is waiting on this event
 * based on the current event mask.   EventNum = 0 means wake all state sets.
 */
VOID seqWakeup(SPROG *pSP, long eventNum)
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
