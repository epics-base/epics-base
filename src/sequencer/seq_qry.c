/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_qry.c	1.4	4/17/91
	DESCRIPTION: Task querry & debug routines for run-time sequencer

	ENVIRONMENT: VxWorks
***************************************************************************/

/*	#define	DEBUG	1	*/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"

/* Querry the sequencer for state information */
seqShow(tid)
int	tid;
{
	extern SPROG	*seq_task_ptr;
	SPROG		*sp_ptr;
	SSCB		*ss_ptr;
	STATE		*st_ptr;
	CHAN		*db_ptr;
	int		nss, nst, nch;
	float		time;

	/* Info? */
	if (tid == 0)
	{
		printf("Usage:  seqShow <task id>\n");
		return 0;
	}

	/* Get the seq_task_ptr (it's task variable) */
	sp_ptr = (SPROG *)taskVarGet(tid, &seq_task_ptr);

	if (sp_ptr->magic != MAGIC)
	{
		printf("seqShow:  wrong magic number\n");
		return -1;
	}

	/* Print info about state program */
	printf("State Program: \"%s\"\n", sp_ptr->name);
	printf("  sp_ptr=%d=0x%x\n", sp_ptr, sp_ptr);
	printf("  initial task id=%d=0x%x\n", sp_ptr->task_id, sp_ptr->task_id);
	printf("  task priority=%d\n", sp_ptr->task_priority);
	printf("  number of state sets=%d\n", sp_ptr->nss);
	printf("  number of channels=%d\n", sp_ptr->nchan);
	printf("  async flag=%d, debug flag=%d, reent flag=%d, conn flag=%d\n",
	 sp_ptr->async_flag, sp_ptr->debug_flag, sp_ptr->reent_flag, sp_ptr->conn_flag);

	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		printf("  State Set: \"%s\"\n", ss_ptr->name);

		printf("  task id=%d=0x%x\n", ss_ptr->task_id, ss_ptr->task_id);

		st_ptr = ss_ptr->states;
		printf("  First state = \"%s\"\n", ss_ptr->states->name);

		st_ptr = ss_ptr->states + ss_ptr->current_state;
		printf("  Current state = \"%s\"\n", st_ptr->name);

		st_ptr = ss_ptr->states + ss_ptr->prev_state;
		printf("  Previous state = \"%s\"\n", st_ptr->name);

		time = (tickGet() - ss_ptr->time)/60.0;
		printf("\tTime since state was entered = %.1f seconds)\n", time);

		printf("\tNumber delays queued=%d\n", ss_ptr->ndelay);

		wait_rtn();
	}

	return 0;
}
/* Querry the sequencer for channel information */
seqChanShow(tid)
int	tid;
{
	extern SPROG	*seq_task_ptr;
	SPROG		*sp_ptr;
	CHAN		*db_ptr;
	int		nch;
	float		time;

	/* Info? */
	if (tid == 0)
	{
		printf("Usage:  seqChanShow <task id>\n");
		return 0;
	}

	sp_ptr = (SPROG *)taskVarGet(tid, &seq_task_ptr); /* seq_task_ptr is task variable */

	if (sp_ptr->magic != MAGIC)
	{
		printf("seqChanQry:  wrong magic number\n");
		return -1;
	}

	db_ptr = sp_ptr->channels;
	printf("State Program: \"%s\"\n", sp_ptr->name);
	printf("Number of channels=%d\n", sp_ptr->nchan);

	for (nch = 0; nch < sp_ptr->nchan; nch++, db_ptr++)
	{
		printf("\nChannel name: \"%s\"\n", db_ptr->db_name);
		printf("  Unexpanded name: \"%s\"\n", db_ptr->db_uname);
		printf("  Variable=%d=0x%x\n", db_ptr->var, db_ptr->var);
		printf("  Size=%d bytes\n", db_ptr->size);
		printf("  Count=%d\n", db_ptr->count);
		printf("  DB get request type=%d\n", db_ptr->get_type);
		printf("  DB put request type=%d\n", db_ptr->put_type);
		printf("  monitor flag=%d\n", db_ptr->mon_flag);

		if (db_ptr->monitored)
			printf("  Monitored\n");
		else
			printf("  Not monitored\n");

		if(db_ptr->connected)
			printf("  Connected\n");
		else
			printf("  Not connected\n");

		if(db_ptr->get_complete)
			printf("  Last get completed\n");
		else
			printf("  Get not completed\n");

		printf("  Status=%d\n", db_ptr->status);
		printf("  Severity=%d\n", db_ptr->severity);

		printf("  Delta=%g\n", db_ptr->delta);
		printf("  Timeout=%g\n", db_ptr->timeout);
		wait_rtn();
	}

	return 0;
}


wait_rtn()
{
	char	bfr;
	printf("Hit RETURN to continue\n");
	do {
		read(STD_IN, &bfr, 1);
	} while (bfr != '\n');
}
