/*	
	seq_if.c,v 1.3 1995/10/10 01:56:49 wright Exp
 *   DESCRIPTION: Interface functions from state program to run-time sequencer.
 *	Note: To prevent global name conflicts "seq_" is added by the SNC, e.g.
 *	pvPut() becomes seq_pvPut().
 *
 *	Author:  Andy Kozubal
 *	Date:    1 March, 1994
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
 */

#define		ANSI
#include	"seq.h"
#include	"tickLib.h"
#include	"logLib.h"

/* See seqCom.h for function prototypes (ANSI standard) */

/*#define		DEBUG*/

/* The following "pv" functions are included here:
	seq_pvGet()
	seq_pvGetComplete()
	seq_pvPut()
	seq_pvFlush()
	seq_pvConnect()
	seq_pvMonitor()
	seq_pvStopMonitor()
	seq_pvStatus()
	seq_pvSeverity()
	seq_pvConnected()
	seq_pvAssigned()
	seq_pvChannelCount()
	seq_pvAssignCount()
	seq_pvConnectCount()
	seq_pvCount()
	seq_pvIndex()
	seq_pvTimeStamp()
 */

/* Flush outstanding CA requests */
void seq_pvFlush()
{
	ca_flush_io();
}	

/*
 * seq_pvGet() - Get DB value (uses channel access).
 */
long seq_pvGet(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	SSCB		*pSS;
	CHAN		*pDB;	/* ptr to channel struct */
	int		status, sem_status;
	extern		VOID seq_callback_handler();

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	pDB = pSP->pChan + pvId;

	/* Check for channel connected */
	if (!pDB->connected)
		return ECA_DISCONN;

	/* Flag this pvGet() as not completed */
	pDB->getComplete = FALSE;

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if ((pSP->options & OPT_ASYNC) == 0)
	{
		pDB->getSemId = pSS->getSemId;
		semTake(pSS->getSemId, NO_WAIT);
	}

	/* Perform the CA get operation with a callback routine specified */
	status = ca_array_get_callback(
			pDB->getType,		/* db request type */
			pDB->count,		/* element count */
			pDB->chid,		/* chid */
			seq_callback_handler
,	/* callback handler */
			pDB);			/* user arg */

	if (status != ECA_NORMAL)
	{
		pDB->getComplete = TRUE;
		SEVCHK(status, "pvGet");

		return status;
	}

	ca_flush_io();
	if ((pSP->options & OPT_ASYNC) != 0)
	{	/* +a option: return immediately */
		return ECA_NORMAL;
	}

	/* Synchronous (-a option): wait for completion (10s timeout) */	
	sem_status = semTake(pSS->getSemId, 600);
	if (sem_status != OK)
	{
		logMsg ("semTake error=%d\n", sem_status, 0,0,0,0,0);
		return ECA_TIMEOUT;
	}
        
	return ECA_NORMAL;
}

/*
 * seq_pvGetComplete() - returns TRUE if the last get completed.
 */
long seq_pvGetComplete(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->getComplete;
}


/*
 * seq_pvPut() - Put DB value.
 */
long seq_pvPut(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status, count;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
#ifdef	DEBUG
	logMsg("seq_pvPut: pv name=%s, pVar=0x%x\n", pDB->dbName, pDB->pVar, 0,0,0,0);
#endif	/*DEBUG*/

	if (!pDB->connected)
		return ECA_DISCONN;

	count = pDB->count;
	if (count > pDB->dbCount)
		count = pDB->dbCount; /* don't try to put more than db count */
	status = ca_array_put(pDB->putType, count, pDB->chid, pDB->pVar);

#ifdef	DEBUG
	logMsg("seq_pvPut: status=%d\n", status, 0,0,0,0,0);
	if (status != ECA_NORMAL)
	{
		seq_log(pSP, "pvPut on \"%s\" failed (%d)\n",
		 pDB->dbName, status);
		seq_log(pSP, "  putType=%d\n", pDB->putType);
		seq_log(pSP, "  size=%d, count=%d\n", pDB->size, count);
	}
#endif	/*DEBUG*/

	return status;
}
/*
 * seq_pvAssign() - Assign/Connect to a channel.
 * Assign to a zero-lth string ("") disconnects/de-assignes.
 */
long seq_pvAssign(SS_ID ssId, long pvId, char *pvName)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status, nchar;
	extern		VOID seq_conn_handler();

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;

#ifdef	DEBUG
	printf("Assign %s to \"%s\"\n", pDB->pVarName, pvName);
#endif	/*DEBUG*/
	if (pDB->assigned)
	{	/* Disconnect this channel */
		status = ca_clear_channel(pDB->chid);
		if (status != ECA_NORMAL)
			return status;
		free(pDB->dbName);
		pDB->assigned = FALSE;
		pSP->assignCount -= 1;
	}

	if (pDB->connected)
	{
		pDB->connected = FALSE;
		pSP->connCount -= 1;
	}
	pDB->monitored = FALSE;
	nchar = strlen(pvName);
	pDB->dbName = (char *)calloc(1, nchar + 1);
	strcpy(pDB->dbName, pvName);

	/* Connect */
	if (nchar > 0)
	{
		pDB->assigned = TRUE;
		pSP->assignCount += 1;
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
			return status;
		}

		if (pDB->monFlag)
		{
			status = seq_pvMonitor(ssId, pvId);
			if (status != ECA_NORMAL)
				return status;
		}
	}

	ca_flush_io();
	
	return ECA_NORMAL;
}
/*
 * seq_pvMonitor() - Initiate a monitor on a channel.
 */
long seq_pvMonitor(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status;
	extern		VOID seq_event_handler();

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;

#ifdef	DEBUG
	printf("monitor \"%s\"\n", pDB->dbName);
#endif	/*DEBUG*/

	if (pDB->monitored || !pDB->assigned)
		return ECA_NORMAL;

	status = ca_add_array_event(
		 pDB->getType,		/* requested type */
		 pDB->count,		/* element count */
		 pDB->chid,		/* chid */
		 seq_event_handler,	/* function to call */
		 pDB,			/* user arg (db struct) */
		 0.0,			/* pos. delta value */
		 0.0,			/* neg. delta value */
		 0.0,			/* timeout */
		 &pDB->evid);		/* where to put event id */

	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_add_array_event");
		ca_task_exit();	/* this is serious */
		return status;
	}
	ca_flush_io();

	pDB->monitored = TRUE;
	return ECA_NORMAL;
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
long seq_pvStopMonitor(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */
	int		status;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	if (!pDB->monitored)
		return -1;

	status = ca_clear_event(pDB->evid);
	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_clear_event");
		return status;
	}

	pDB->monitored = FALSE;

	return status;
}

/*
 * seq_pvChannelCount() - returns total number of database channels.
 */
long seq_pvChannelCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->numChans;
}

/*
 * seq_pvConnectCount() - returns number of database channels connected.
 */
long seq_pvConnectCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->connCount;
}

/*
 * seq_pvAssignCount() - returns number of database channels assigned.
 */
long seq_pvAssignCount(SS_ID ssId)
{
	SPROG		*pSP;	/* ptr to state program */

	pSP = ((SSCB *)ssId)->sprog;
	return pSP->assignCount;
}

/*
 * seq_pvConnected() - returns TRUE if database channel is connected.
 */
long seq_pvConnected(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->connected;
}

/*
 * seq_pvAssigned() - returns TRUE if database channel is assigned.
 */
long seq_pvAssigned(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->assigned;
}

/*
 * seq_pvCount() - returns number elements in an array, which is the lesser of
 * (1) the array size and (2) the element count returned by channel access.
 */
long seq_pvCount(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->dbCount;
}

/*
 * seq_pvStatus() - returns channel alarm status.
 */
long seq_pvStatus(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->status;
}

/*
 * seq_pvSeverity() - returns channel alarm severity.
 */
long seq_pvSeverity(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->severity;
}

/*
 * seq_pvIndex() - returns index of database variable.
 */
long seq_pvIndex(SS_ID ssId, long pvId)
{
	return pvId; /* index is same as pvId */
}

/*
 * seq_pvTimeStamp() - returns channel time stamp.
 */
TS_STAMP seq_pvTimeStamp(SS_ID ssId, long pvId)
{
	SPROG		*pSP;	/* ptr to state program */
	CHAN		*pDB;	/* ptr to channel struct */

	pSP = ((SSCB *)ssId)->sprog;
	pDB = pSP->pChan + pvId;
	return pDB->timeStamp;
}
/*
 * seq_efSet() - Set an event flag, then wake up each state
 * set that might be waiting on that event flag.
 */
VOID seq_efSet(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

#ifdef	DEBUG
	logMsg("seq_efSet: pSP=0x%x, pSS=0x%x, ev_flag=0x%x\n", pSP, pSS, ev_flag, 
	 0,0,0);
	taskDelay(10);
#endif	/*DEBUG*/

	/* Set this bit (apply resource lock) */
	semTake(pSP->caSemId, WAIT_FOREVER);
	bitSet(pSP->pEvents, ev_flag);

	/* Wake up state sets that are waiting for this event flag */
	seqWakeup(pSP, ev_flag);

	/* Unlock resource */
	semGive(pSP->caSemId);
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
long seq_efTest(SS_ID ssId, long ev_flag)
/* event flag */
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;
	isSet = bitTest(pSP->pEvents, ev_flag);
#ifdef	DEBUG
	logMsg("seq_efTest: ev_flag=%d, event=0x%x, isSet=%d\n",
	 ev_flag, pSP->pEvents[0], isSet, 0,0,0,0);
#endif	/*DEBUG*/
	return isSet;
}

/*
 * seq_efClear() - Test event flag against outstanding events, then clear it.
 */
long seq_efClear(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	isSet = bitTest(pSP->pEvents, ev_flag);
	bitClear(pSP->pEvents, ev_flag);
	return isSet;
}

/*
 * seq_efTestAndClear() - Test event flag against outstanding events, then clear it.
 */
long seq_efTestAndClear(SS_ID ssId, long ev_flag)
{
	SPROG		*pSP;
	SSCB		*pSS;
	int		isSet;

	pSS = (SSCB *)ssId;
	pSP = pSS->sprog;

	isSet = bitTest(pSP->pEvents, ev_flag);
	bitClear(pSP->pEvents, ev_flag);
	return isSet;
}

/* seq_delay() - test for delay() time-out expired */
long seq_delay(SS_ID ssId, long delayId)
{
	SSCB		*pSS;
	ULONG		timeElapsed;

	pSS = (SSCB *)ssId;

	/* Calc. elapsed time since state was entered */
	timeElapsed = tickGet() - pSS->timeEntered;

	/* Check for delay timeout */
	if (timeElapsed >= pSS->delay[delayId])		
	{
		pSS->delayExpired[delayId] = TRUE; /* mark as expired */
		return TRUE;
	}

	return FALSE;
}

/*
 * seq_delayInit() - initialize delay time (in seconds) on entering a state.
 */
VOID seq_delayInit(SS_ID ssId, long delayId, float delay)
{
	SSCB		*pSS;
	int		ndelay;

	pSS = (SSCB *)ssId;

	/* Convert delay time to tics & save */
	pSS->delay[delayId] = delay * 60.0;

	ndelay = delayId + 1;
	if (ndelay > pSS->numDelays)
		pSS->numDelays = ndelay;
}
/*
 * seq_optGet: return the value of an option (e.g. "a").
 * FALSE means "-" and TRUE means "+".
 */
long seq_optGet(SS_ID ssId, char *opt)
{
	SPROG		*pSP;

	pSP = ((SSCB *)ssId)->sprog;
	switch (opt[0])
	{
	    case 'a': return ( (pSP->options & OPT_ASYNC) != 0);
	    case 'c': return ( (pSP->options & OPT_CONN) != 0);
	    case 'd': return ( (pSP->options & OPT_DEBUG) != 0);
	    case 'e': return ( (pSP->options & OPT_NEWEF) != 0);
	    case 'r': return ( (pSP->options & OPT_REENT) != 0);
	    case 'v': return ( (pSP->options & OPT_VXWORKS) != 0);
	    default:  return FALSE;
	}
}
