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
 */

#include	"seq.h"

#ifdef	ANSI
LOCAL void *getPtrToValue(union db_access_val *, chtype);
#else
LOCAL void *getPtrToValue();
#endif


/*#define		DEBUG*/

#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)

/* Connect to the database channels through channel access */
seq_connect(sp_ptr)
SPROG		*sp_ptr;
{
	CHAN		*db_ptr;
	int		status, i;
	MACRO		*macro_tbl;
	extern		seq_conn_handler();

	/* Initialize CA task */
	ca_task_initialize();

	sp_ptr->conn_count = 0;
	macro_tbl = sp_ptr->mac_ptr;

	/* Search for all channels */
	db_ptr = sp_ptr->channels;
	for (i = 0; i < sp_ptr->nchan; i++)
	{
		/* Do macro substitution on channel name */
		db_ptr->db_name = seqAlloc(sp_ptr, MACRO_STR_LEN);
		seqMacEval(db_ptr->db_uname, db_ptr->db_name, MACRO_STR_LEN,
		 macro_tbl);
#ifdef	DEBUG
		printf("connect to \"%s\"\n", db_ptr->db_name);
#endif	DEBUG
		/* Connect to it */
		status = ca_build_and_connect(
			db_ptr->db_name,	/* DB channel name */
			TYPENOTCONN,		/* Don't get initial value */
			0,			/* get count (n/a) */
			&(db_ptr->chid),	/* ptr to chid */
			0,			/* ptr to value (n/a) */
			seq_conn_handler,	/* connection handler routine */
			db_ptr);		/* user ptr is CHAN structure */
		if (status != ECA_NORMAL)
		{
			SEVCHK(status, "ca_search");
			ca_task_exit();
			return -1;
		}
		/* Clear monitor indicator */
		db_ptr->monitored = FALSE;

		/* Issue monitor requests */
		if (db_ptr->mon_flag)
		{
			seq_pvMonitor(sp_ptr, 0, db_ptr);
		}
		db_ptr++;
	}
	ca_flush_io();

	if (sp_ptr->conn_flag)
	{	/* Wait for all connections to complete */
		while (sp_ptr->conn_count < sp_ptr->nchan)
			taskDelay(30);
	}

	return 0;
}
/* Channel access events (monitors) come here */
VOID seq_event_handler(args)
struct	event_handler_args args;
{
	SPROG			*sp_ptr;
	CHAN			*db_ptr;
	struct dbr_sts_char	*dbr_sts_ptr;
	void			*value_ptr;
	int			i, nbytes;

	/* User arg is ptr to db channel structure */
	db_ptr = (CHAN *)args.usr;

	/* Copy value returned into user variable */
	value_ptr = getPtrToValue(
		(union db_access_val *)args.dbr, db_ptr->get_type);
	nbytes = db_ptr->size * db_ptr->count;
	bcopy(value_ptr, db_ptr->var, nbytes);

	/* Copy status & severity */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;
	db_ptr->status = dbr_sts_ptr->status;
	db_ptr->severity = dbr_sts_ptr->severity;

	/* Process event handling in each state set */
	sp_ptr = db_ptr->sprog;	/* State program that owns this db entry */

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(sp_ptr, 0, db_ptr->index + 1);

	return;
}
/* Sequencer connection handler:
	Called each time a connection is established or broken */
VOID seq_conn_handler(args)
struct connection_handler_args	args;
{
	CHAN		*db_ptr;
	SPROG		*sp_ptr;


	/* user argument (from ca_search() */
	db_ptr = (CHAN *)ca_puser(args.chid);

	/* State program that owns this db entry */
	sp_ptr = db_ptr->sprog;

	/* Check for connected */
	if (ca_field_type(args.chid) == TYPENOTCONN)
	{
		db_ptr->connected = FALSE;
		sp_ptr->conn_count--;
		seq_log(sp_ptr, "Channel \"%s\" disconnected\n", db_ptr->db_name);
	}
	else
	{
		db_ptr->connected = TRUE;
		sp_ptr->conn_count++;
		seq_log(sp_ptr, "Channel \"%s\" connected\n", db_ptr->db_name);
		if (db_ptr->count > ca_element_count(args.chid))
		{
			db_ptr->count = ca_element_count(args.chid);
			seq_log(sp_ptr, "\"%s\": reset count to %d\n",
			 db_ptr->db_name, db_ptr->count);
		}
	}

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(sp_ptr, 0, 0);
	
	return;
}

/* Wake up each state set that is waiting for event processing */
VOID seq_efSet(sp_ptr, dummy, ev_flag)
SPROG		*sp_ptr;
int		dummy;
int		ev_flag;	/* event flag */
{
	SSCB		*ss_ptr;
	int		nss;

	ss_ptr = sp_ptr->sscb;
	/* For all state sets: */
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		/* Apply resource lock */
		semTake(sp_ptr->caSemId, WAIT_FOREVER);

		/* Test for possible event trig based on bit mask for this state */
		if ( (ev_flag == 0) || bitTest(ss_ptr->pMask, ev_flag) )
		{
			bitSet(ss_ptr->events, ev_flag);
			semGive(ss_ptr->syncSemId); /* wake up the ss task */
		}

		/* Unlock resource */
		semGive(sp_ptr->caSemId);
	}
}

/* Test event flag against outstanding events */
seq_efTest(sp_ptr, ss_ptr, ev_flag)
SPROG		*sp_ptr;
SSCB		*ss_ptr;
int		ev_flag;	/* event flag */
{
	return bitTest(ss_ptr->events, ev_flag);
}
/* Get DB value (uses channel access) */
seq_pvGet(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int		status, sem_status;
	extern		seq_callback_handler();

	/* Check for channel connected */
	if (!db_ptr->connected)
		return ECA_DISCONN;

	/* Flag this pvGet() as not completed */
	db_ptr->get_complete = 0;

	/* If synchronous pvGet then clear the pvGet pend semaphore */
	if (!sp_ptr->async_flag)
	{
		db_ptr->getSemId = ss_ptr->getSemId;
		semTake(ss_ptr->getSemId, NO_WAIT);
	}

	/* Perform the CA get operation with a callback routine specified */
	status = ca_array_get_callback(
			db_ptr->get_type,	/* db request type */
			db_ptr->count,		/* element count */
			db_ptr->chid,		/* chid */
			seq_callback_handler,	/* callback handler */
			db_ptr);		/* user arg */

	if (sp_ptr->async_flag || (status != ECA_NORMAL) )
		return status;

	/* Synchronous pvGet() */

	ca_flush_io();

	/* Wait for completion (10s timeout) */	
	sem_status = semTake(ss_ptr->getSemId, 600);
	if (sem_status == ERROR)
		status = ECA_TIMEOUT;
        
	return status;
}

/* Sequencer callback handler - called when a "get" completes */
seq_callback_handler(args)
struct event_handler_args	args;
{
	SPROG			*sp_ptr;
	CHAN			*db_ptr;
	struct dbr_sts_char	*dbr_sts_ptr;
	int			i, nbytes;
	void			*value_ptr;

	/* User arg is ptr to db channel structure */
	db_ptr = (CHAN *)args.usr;

	/* Copy value returned into user variable */
	value_ptr = getPtrToValue(
		(union db_access_val *)args.dbr, db_ptr->get_type);
	nbytes = db_ptr->size * db_ptr->count;
	bcopy(value_ptr, db_ptr->var, nbytes);

	/* Copy status & severity */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;
	db_ptr->status = dbr_sts_ptr->status;
	db_ptr->severity = dbr_sts_ptr->severity;

	/* Set get complete flag */
	db_ptr->get_complete = TRUE;

	/* Wake up each state set that is waiting for event processing) */
	sp_ptr = db_ptr->sprog;	/* State program that owns this db entry */
	seq_efSet(sp_ptr, 0, db_ptr->index + 1);

	/* If syncronous pvGet then notify pending state set */
	if (!sp_ptr->async_flag)
		semGive(db_ptr->getSemId);

	return 0;
}

/* Flush requests */
VOID seq_pvFlush()
{
	ca_flush_io();
}	

/* Put DB value (uses channel access) */
seq_pvPut(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int	status;

	if (!db_ptr->connected)
		return ECA_DISCONN;

	status = ca_array_put(db_ptr->put_type, db_ptr->count,
	 db_ptr->chid, db_ptr->var);
	if (status != ECA_NORMAL)
	{
		seq_log(sp_ptr, "pvPut on \"%s\" failed (%d)\n",
		 db_ptr->db_name, status);
		seq_log(sp_ptr, "  put_type=%d\n", db_ptr->put_type);
		seq_log(sp_ptr, "  size=%d, count=%d\n", db_ptr->size, db_ptr->count);
	}

	return status;
}
/* Initiate a monitor on a channel */
seq_pvMonitor(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int		status;
	extern		seq_event_handler();

#ifdef	DEBUG
	printf("monitor \"%s\"\n", db_ptr->db_name);
#endif	DEBUG

	if (db_ptr->monitored)
		return -1;

	status = ca_add_array_event(
		 db_ptr->get_type,	/* requested type */
		 db_ptr->count,		/* element count */
		 db_ptr->chid,		/* chid */
		 seq_event_handler,	/* function to call */
		 db_ptr,		/* user arg (db struct) */
		 db_ptr->delta,		/* pos. delta value */
		 db_ptr->delta,		/* neg. delta value */
		 db_ptr->timeout,	/* timeout */
		 &db_ptr->evid);	/* where to put event id */

	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_add_array_event");
		ca_task_exit();	/* this is serious */
		return -1;
	}
	ca_flush_io();

	db_ptr->monitored = TRUE;
	return 0;
}

/* Cancel a monitor */
seq_pvStopMonitor(sp_ptr, ss_ptr, db_ptr)
SPROG	*sp_ptr;	/* ptr to state program */
SSCB	*ss_ptr;	/* ptr to current state set */
CHAN	*db_ptr;	/* ptr to channel struct */
{
	int		status;

	if (!db_ptr->monitored)
		return -1;

	status = ca_clear_event(db_ptr->evid);
	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_clear_event");
		return status;
	}

	db_ptr->monitored = FALSE;

	return status;
}

/* Given ptr to value structure & type, return ptr to value */
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

