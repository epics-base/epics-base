/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_ca.c	1.10	4/24/91
	DESCRIPTION: Seq_ca.c: Channel access interface for sequencer

	ENVIRONMENT: VxWorks
***************************************************************************/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"

/*#define		DEBUG*/

#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)

/* Connect to the database channels through channel access */
connect_db_channels(sp_ptr)
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
seq_event_handler(args)
struct	event_handler_args args;
{
	SPROG			*sp_ptr;
	CHAN			*db_ptr;
	struct dbr_sts_char	*dbr_sts_ptr;
	int			i, nbytes;

	/* User arg is ptr to db channel structure */
	db_ptr = (CHAN *)args.usr;

	/* Get ptr to data & status structure */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;

	/* Copy value returned into user variable */
	nbytes = db_ptr->size * db_ptr->count;
	bcopy(&(dbr_sts_ptr->value), db_ptr->var, nbytes);

	/* Copy status & severity */
	db_ptr->status = dbr_sts_ptr->status;
	db_ptr->severity = dbr_sts_ptr->severity;

	/* Process event handling in each state set */
	sp_ptr = db_ptr->sprog;	/* State program that owns this db entry */

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(sp_ptr, 0, db_ptr->index + 1);

	return 0;
}
/* Sequencer connection handler:
	Called each time a connection is established or broken */
seq_conn_handler(args)
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
		seqLog(sp_ptr, "Channel \"%s\" disconnected\n", db_ptr->db_name);
	}
	else
	{
		db_ptr->connected = TRUE;
		sp_ptr->conn_count++;
		seqLog(sp_ptr, "Channel \"%s\" connected\n", db_ptr->db_name);
		if (db_ptr->count > ca_element_count(args.chid))
		{
			db_ptr->count = ca_element_count(args.chid);
			seqLog(sp_ptr, "\"%s\": reset count to %d\n",
			 db_ptr->db_name, db_ptr->count);
		}
	}

	/* Wake up each state set that is waiting for event processing */
	seq_efSet(sp_ptr, 0, 0);
	
	return;
}

/* Wake up each state set that is waiting for event processing */
seq_efSet(sp_ptr, dummy, ev_flag)
SPROG		*sp_ptr;
void		*dummy;
int		ev_flag;	/* event flag */
{
	SSCB		*ss_ptr;
	int		nss;

	ss_ptr = sp_ptr->sscb;
	/* For all state sets: */
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		/* Apply resource lock */
		semTake(sp_ptr->sem_id, 0);

		/* Test for possible event trig based on bit mask for this state */
		if ( (ev_flag == 0) || bitTest(ss_ptr->pMask, ev_flag) )
		{
			bitSet(ss_ptr->events, ev_flag);
			semGive(ss_ptr->sem_id); /* wake up the ss task */
		}

		/* Unlock resource */
		semGive(sp_ptr->sem_id);
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
	int		status;
	extern		seq_callback_handler();
	extern int	async_flag;

	/* Check for channel connected */
	if (!db_ptr->connected)
		return ECA_DISCONN;

	db_ptr->get_complete = 0;
	/* Asynchronous get? */
	if (sp_ptr->async_flag)
	{
		status = ca_array_get_callback(
			db_ptr->get_type,	/* db request type */
			db_ptr->count,		/* element count */
			db_ptr->chid,		/* chid */
			seq_callback_handler,	/* callback handler */
			db_ptr);		/* user arg */

		/* Note:  CA buffer is not flushed */

		return status;
	}

	/* Synchronous get (wait for completion) */
	status = ca_array_get(
		db_ptr->put_type,	/* db request type */
		db_ptr->count,		/* element count */
		db_ptr->chid,		/* chid */
		db_ptr->var);		/* value */

	if (status == ECA_BADTYPE)
	{
		seqLog(sp_ptr, "Bad type: pvGet() on %s\n", db_ptr->db_name);
		return status;
	}

	if (status == ECA_BADCHID)
	{
		seqLog(sp_ptr, "Disconencted: pvGet() on %s\n", db_ptr->db_name);
		return status;
	}

	/* Wait for completion */	
	status = ca_pend_io(10.0);
	db_ptr->get_complete = TRUE;
	if (status != ECA_NORMAL)
	{
		seqLog(sp_ptr, "time-out: pvGet() on %s\n", db_ptr->db_name);
	}
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

	/* User arg is ptr to db channel structure */
	db_ptr = (CHAN *)args.usr;

	/* Get ptr to data & status structure */
	dbr_sts_ptr = (struct dbr_sts_char *)args.dbr;

	/* Copy value returned into user variable */
	nbytes = db_ptr->size * db_ptr->count;
	bcopy(&(dbr_sts_ptr->value), db_ptr->var, nbytes);

	/* Copy status & severity */
	db_ptr->status = dbr_sts_ptr->status;
	db_ptr->severity = dbr_sts_ptr->severity;

	/* Set get complete flag */
	db_ptr->get_complete = TRUE;

	return 0;
}

/* Flush requests */
seq_pvFlush()
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
		seqLog(sp_ptr, "pvPut on \"%s\" failed (%d)\n",
		 db_ptr->db_name, status);
		seqLog(sp_ptr, "  put_type=%d\n", db_ptr->put_type);
		seqLog(sp_ptr, "  size=%d, count=%d\n", db_ptr->size, db_ptr->count);
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

	return 0;
}
