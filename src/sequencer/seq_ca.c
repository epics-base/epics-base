/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_ca.c	1.7	2/4/91
	DESCRIPTION: Seq_ca.c: Channel access interface for sequencer

	ENVIRONMENT: VxWorks
***************************************************************************/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"

#define		MACRO_STR_LEN	(MAX_STRING_SIZE+1)

/* Connect to the database channels through channel access */
connect_db_channels(sp_ptr, macro_tbl)
SPROG	*sp_ptr;
struct	macro *macro_tbl;
{
	CHAN	*db_ptr;
	int	status, i;

	/* Initialize CA task */
	ca_task_initialize();

	/* Search for all channels */
	db_ptr = sp_ptr->channels;
	for (i = 0; i < sp_ptr->nchan; i++)
	{
		/* Do macro substitution */
		db_ptr->db_name = seqAlloc(MACRO_STR_LEN);
		macEval(db_ptr->db_uname, db_ptr->db_name, MACRO_STR_LEN,
		 macro_tbl);
		printf("  %d: %s\n", i, db_ptr->db_name);
		status = ca_search(db_ptr->db_name, &(db_ptr->chid));
		if (status != ECA_NORMAL)
		{
			SEVCHK(status, "ca_search");
			ca_task_exit();
			return -1;
		}
		db_ptr++;
	}

	/* Wait for returns from "ca_search" */
	status = ca_pend_io(10.0);
	if (status != ECA_NORMAL)
	{
		SEVCHK(status, "ca_pend_io");
		return -1;
	}
#ifdef	DEBUG
	db_ptr = sp_ptr->channels;
	for (i = 0; i < sp_ptr->nchan; i++)
	{
		printf("\"%s\" ca_search() returned: chid=0x%x\n",
		 db_ptr->db_name, db_ptr->chid);
		db_ptr++;
	}
#endif
	printf("All db channels connected\n");
	return 0;
}
/* Issue monitor requests */
issue_monitor_requests(sp_ptr)
SPROG	*sp_ptr;
{
	CHAN	*db_ptr;
	int	status, i;
	int	seq_event_handler();

	db_ptr = sp_ptr->channels;
	for (i = 0; i < sp_ptr->nchan; i++)
	{
		if (db_ptr->mon_flag)
		{
			status = ca_add_general_event(
			 db_ptr->dbr_type, /* requested type */
			 db_ptr->chid,	/* chid */
			 seq_event_handler,	/* function to call */
			 db_ptr,	/* user arg is db entry */
			 db_ptr->delta,	/* pos. delta value */
			 db_ptr->delta,	/* neg. delta value */
			 db_ptr->timeout, /* timeout */
			 0		/* no event id */);
			if (status != ECA_NORMAL)
			{
				SEVCHK(status, "ca_search");
				ca_task_exit();
				return -1;
			}
		}
		db_ptr++;
	}
	ca_flush_io();

	return 0;
}
/* Channel access events (monitors) come here */
seq_event_handler(args)
struct	event_handler_args args;
{
	SPROG	*sp_ptr;
	SSCB	*ss_ptr;
	CHAN	*db_ptr;
	int	i, nbytes, count;
	int	nss;

	/* User arg is ptr to db channel structure */
	db_ptr = (CHAN *)args.usr;
	/* Copy value returned into user variable */
	count = db_ptr->count;
	if (count == 0)
		count = 1;
	nbytes = db_ptr->size * count;
	bcopy(args.dbr, db_ptr->var, nbytes);
#ifdef	DEBUG
	printf("seq_event_handler: ");
	if (db_ptr->dbr_type == DBR_FLOAT)
		printf("%s=%g\n", db_ptr->db_name, *(float *)db_ptr->var);
	else if (db_ptr->dbr_type == DBR_INT)
	{
		i =  *(short *)args.dbr;
		printf("%s=%d\n", db_ptr->db_name, i);
	}
	else
		printf("?\n");
#endif	DEBUG

	/* Process event handling in each state set */
	sp_ptr = db_ptr->sprog;	/* State program that owns this db entry */

	/* Set synchronization event flag (if it exists) for each state set */
	if (db_ptr->ev_flag != 0)
		seq_efSet(sp_ptr, 0, db_ptr->ev_flag);

	/* Wake up each state set that is waiting for event processing */
	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		if (ss_ptr->action_complete)
		{
			semGive(ss_ptr->sem_id);
		}
	}
	
	return 0;
}

