/*	$Id$
 *   DESCRIPTION: Channel access interface for sequencer.
 *
 *	Author:  Andy Kozubal
 *	Date:    July, 1991
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
 * .01 07-03-91  ajk	.
 * .02 12-11-91  ajk	Cosmetic changes (comments & names)
 */

#include	"seq.h"

#ifdef	ANSI
LOCAL void *getPtrToValue(union db_access_val *, chtype);
#else
LOCAL void *getPtrToValue();
#endif


/*#define		DEBUG*/

#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)

/*
 * seq_connect() - Connect to all database channels through channel access.
 */
seq_connect(pSP)
SPROG		*pSP;
{
	CHAN		*pDB;
	int		status, i;
	MACRO		*macro_tbl;
	extern		VOID seq_conn_handler();

	/* Initialize CA task */
	ca_task_initialize();

	pSP->conn_count = 0;
	macro_tbl = pSP->mac_ptr;

	/*
	 * For each channel: substitute macros, connect to db,
	 * & isssue monitor (if monitor request flaf is TRUE).
	 */
	pDB = pSP->channels;
	for (i = 0; i < pSP->nchan; i++)
	{
		/* Do macro substitution on channel name */
		pDB->db_name = seqAlloc(pSP, MACRO_STR_LEN);
		seqMacEval(pDB->db_uname, pDB->db_name, MACRO_STR_LEN,
		 macro_tbl);
#ifdef	DEBUG
		printf("connect to \"%s\"\n", pDB->db_name);
#endif	DEBUG
		/* Connect to it */
		status = ca_build_and_connect(
			pDB->db_name,		/* DB channel name */
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
		if (pDB->mon_flag)
		{
			seq_pvMonitor(pSP, 0, pDB);
		}
		pDB++;
	}
	ca_flush_io();

	if (pSP->conn_flag)
	{	/* Wait for all connections to complete */
		while (pSP->conn_count < pSP->nchan)
			taskDelay(30);
	}

	return 0;
}
/*
 * seq_event_handler() - Channel access events (monitors) come here.
 * args points to CA event handler argument structure.  args.usr contains
 * a pointer to the channel structure (CHAN *).
 */
VOID seq_event_handler(args)
struct	event_handler_args args;
{
	SPROG			*pSP;
	CHAN			*pDB;
	struct dbr_sts_char	*dbr_sts_ptr;
	void			*pVal;
	int			i, nbytes;

	/* User arg is ptr to db channel structure */
	pDB = (CHAN *)args.usr;

	/* Copy value returned into user variable */
	pVal = getPtrToValue((union db_access_val *)args.dbr, pDB->get_type);
	nbytes = pDB->size * pDB->count;
	bcopy(pVal, pDB->var, nbytes);

	/* Copy status & severity */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;
	pDB->status = dbr_sts_ptr->status;
	pDB->severity = dbr_sts_ptr->severity;

	/* Process event handling in each state set */
	pSP = pDB->sprog;	/* State program that owns this db entry */

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(pSP, 0, pDB->index + 1);

	return;
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
		pSP->conn_count--;
		seq_log(pSP, "Channel \"%s\" disconnected\n", pDB->db_name);
	}
	else
	{
		pDB->connected = TRUE;
		pSP->conn_count++;
		seq_log(pSP, "Channel \"%s\" connected\n", pDB->db_name);
		if (pDB->count > ca_element_count(args.chid))
		{
			pDB->count = ca_element_count(args.chid);
			seq_log(pSP, "\"%s\": reset count to %d\n",
			 pDB->db_name, pDB->count);
		}
	}

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(pSP, 0, 0);
	
	return;
}

/*
 * seq_efSet() - Set an event flag.  The result is to wake up each state
 * set that is waiting for event processing.
 */
VOID seq_efSet(pSP, dummy, ev_flag)
SPROG		*pSP;
int		dummy;
int		ev_flag;	/* event flag */
{
	SSCB		*pSS;
	int		nss;

	pSS = pSP->sscb;
	/* For all state sets: */
	for (nss = 0; nss < pSP->nss; nss++, pSS++)
	{
		/* Apply resource lock */
		semTake(pSP->caSemId, WAIT_FOREVER);

		/* Test for possible event trig based on bit mask for this state */
		if ( (ev_flag == 0) || bitTest(pSS->pMask, ev_flag) )
		{
			bitSet(pSS->events, ev_flag);
			semGive(pSS->syncSemId); /* wake up the ss task */
		}

		/* Unlock resource */
		semGive(pSP->caSemId);
	}
}

/*
 * seq_efTest() - Test event flag against outstanding events.
 */
seq_efTest(pSP, pSS, ev_flag)
SPROG		*pSP;
SSCB		*pSS;
int		ev_flag;	/* event flag */
{
	return bitTest(pSS->events, ev_flag);
}
/*
 * seq_pvGet() - Get DB value (uses channel access).
 */
seq_pvGet(pSP, pSS, pDB)
SPROG	*pSP;	/* ptr to state program */
SSCB	*pSS;	/* ptr to current state set */
CHAN	*pDB;	/* ptr to channel struct */
{
	int		status, sem_status;
	extern		VOID seq_callback_handler();

	/* Check for channel connected */
	if (!pDB->connected)
		return ECA_DISCONN;

	/* Flag this pvGet() as not completed */
	pDB->get_complete = FALSE;

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if (!pSP->async_flag)
	{
		pDB->getSemId = pSS->getSemId;
		semTake(pSS->getSemId, NO_WAIT);
	}

	/* Perform the CA get operation with a callback routine specified */
	status = ca_array_get_callback(
			pDB->get_type,		/* db request type */
			pDB->count,		/* element count */
			pDB->chid,		/* chid */
			seq_callback_handler,	/* callback handler */
			pDB);			/* user arg */

	if (pSP->async_flag || (status != ECA_NORMAL) )
		return status;

	/* Synchronous pvGet() */

	ca_flush_io();

	/* Wait for completion (10s timeout) */	
	sem_status = semTake(pSS->getSemId, 600);
	if (sem_status == ERROR)
		status = ECA_TIMEOUT;
        
	return status;
}

/*
 * seq_callback_handler() - Sequencer callback handler.
 * Called when a "get" completes.
 * args.usr points to the db structure (CHAN *) for tis channel.
 */
VOID seq_callback_handler(args)
struct event_handler_args	args;
{
	SPROG			*pSP;
	CHAN			*pDB;
	struct dbr_sts_char	*dbr_sts_ptr;
	int			i, nbytes;
	void			*pVal;

	/* User arg is ptr to db channel structure */
	pDB = (CHAN *)args.usr;

	/* Copy value returned into user variable */
	pVal = getPtrToValue((union db_access_val *)args.dbr, pDB->get_type);
	nbytes = pDB->size * pDB->count;
	bcopy(pVal, pDB->var, nbytes);

	/* Copy status & severity */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;
	pDB->status = dbr_sts_ptr->status;
	pDB->severity = dbr_sts_ptr->severity;

	/* Set get complete flag */
	pDB->get_complete = TRUE;

	/* Wake up each state set that is waiting for event processing) */
	pSP = pDB->sprog;	/* State program that owns this db entry */
	seq_efSet(pSP, 0, pDB->index + 1);

	/* If syncronous pvGet then notify pending state set */
	if (!pSP->async_flag)
		semGive(pDB->getSemId);

	return;
}

/* Flush requests */
VOID seq_pvFlush()
{
	ca_flush_io();
}	

/*
 * seq_pvPut() - Put DB value (uses channel access).
 */
seq_pvPut(pSP, pSS, pDB)
SPROG	*pSP;	/* ptr to state program */
SSCB	*pSS;	/* ptr to current state set */
CHAN	*pDB;	/* ptr to channel struct */
{
	int	status;

	if (!pDB->connected)
		return ECA_DISCONN;

	status = ca_array_put(pDB->put_type, pDB->count,
	 pDB->chid, pDB->var);
	if (status != ECA_NORMAL)
	{
		seq_log(pSP, "pvPut on \"%s\" failed (%d)\n",
		 pDB->db_name, status);
		seq_log(pSP, "  put_type=%d\n", pDB->put_type);
		seq_log(pSP, "  size=%d, count=%d\n", pDB->size, pDB->count);
	}

	return status;
}
/*
 * seq_pvMonitor() - Initiate a monitor on a channel.
 */
seq_pvMonitor(pSP, pSS, pDB)
SPROG	*pSP;	/* ptr to state program */
SSCB	*pSS;	/* ptr to current state set */
CHAN	*pDB;	/* ptr to channel struct */
{
	int		status;
	extern		VOID seq_event_handler();

#ifdef	DEBUG
	printf("monitor \"%s\"\n", pDB->db_name);
#endif	DEBUG

	if (pDB->monitored)
		return;

	status = ca_add_array_event(
		 pDB->get_type,		/* requested type */
		 pDB->count,		/* element count */
		 pDB->chid,		/* chid */
		 seq_event_handler,	/* function to call */
		 pDB,			/* user arg (db struct) */
		 pDB->delta,		/* pos. delta value */
		 pDB->delta,		/* neg. delta value */
		 pDB->timeout,		/* timeout */
		 &pDB->evid);		/* where to put event id */

	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_add_array_event");
		ca_task_exit();	/* this is serious */
		return;
	}
	ca_flush_io();

	pDB->monitored = TRUE;
	return;
}

/*
 * seq_pvStopMonitor() - Cancel a monitor
 */
seq_pvStopMonitor(pSP, pSS, pDB)
SPROG	*pSP;	/* ptr to state program */
SSCB	*pSS;	/* ptr to current state set */
CHAN	*pDB;	/* ptr to channel struct */
{
	int		status;

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
 * getPtr() - Given ptr to value structure & type, return ptr to value.
 */
LOCAL void *getPtrToValue(pBuf, dbrType)
union db_access_val *pBuf;
chtype	dbrType;
{
    switch (dbrType) {
	case DBR_STRING:	return (void *)pBuf->strval;
	case DBR_STS_STRING:	return (void *)pBuf->sstrval.value;

	case DBR_SHORT:		return (void *)&pBuf->shrtval;
	case DBR_STS_SHORT:	return (void *)&pBuf->sshrtval.value;

	case DBR_FLOAT:		return (void *)&pBuf->fltval;
	case DBR_STS_FLOAT:	return (void *)&pBuf->sfltval.value;

	case DBR_ENUM:		return (void *)&pBuf->enmval;
	case DBR_STS_ENUM:	return (void *)&pBuf->senmval.value;

	case DBR_CHAR:		return (void *)&pBuf->charval;
	case DBR_STS_CHAR:	return (void *)&pBuf->schrval.value;

	case DBR_LONG:		return (void *)&pBuf->longval;
	case DBR_STS_LONG:	return (void *)&pBuf->slngval.value;

	case DBR_DOUBLE:	return (void *)&pBuf->doubleval;
	case DBR_STS_DOUBLE:	return (void *)&pBuf->sdblval.value;
	default:		return NULL;
    }
}

