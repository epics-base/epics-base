/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	@(#)seq_qry.c	1.3	11/9/90
	DESCRIPTION: Task querry & debug routines for run-time sequencer

	ENVIRONMENT: VxWorks
***************************************************************************/

/*	#define	DEBUG	1	*/

#include	"seq.h"
#include	"vxWorks.h"
#include	"taskLib.h"

/* querry the sequencer */
seqQry(tid)
int	tid;
{
	extern	TASK *task_area_ptr;
	TASK	*ta_ptr;
	SPROG	*sp_ptr;
	SSCB	*ss_ptr;
	STATE	*st_ptr;
	CHAN	*db_ptr;
	int	nss, nst, nch;
	float	time;

	ta_ptr = (TASK *)taskVarGet(tid, &task_area_ptr); /* task_area_ptr is task variable */
	sp_ptr = ta_ptr->sp_ptr;
	if (sp_ptr->magic != MAGIC)
	{
		printf("tqry:  wrong magic number\n");
		return -1;
	}
	/* Print info about state program */
	printf("State Program: \"%s\"\n", sp_ptr->name);
	printf("  ta_ptr=%d=0x%x\n", ta_ptr, ta_ptr);
	printf("  sp_ptr=%d=0x%x\n", sp_ptr, sp_ptr);

	ss_ptr = sp_ptr->sscb;
	for (nss = 0; nss < sp_ptr->nss; nss++, ss_ptr++)
	{
		printf("  State Set: \"%s\"\n", ss_ptr->name);
		printf("\tss_ptr=%d=0x%x\n", ss_ptr, ss_ptr);
		st_ptr = ss_ptr->current_state;
		if (st_ptr != NULL) {
			printf("\tcurrent state: \"%s\"\n", st_ptr->name);
		}
		time = ss_ptr->time/60.0;
		printf("\ttime state was entered=%d tics (%g seconds)\n",
		 ss_ptr->time, time);
		st_ptr = ss_ptr->next_state;
		printf("\tevent flag=%d\n", ss_ptr->ev_flag);
		printf("\tevent flag mask=0x%x\n", ss_ptr->ev_flag_mask);
		printf("\tnumer delays queued=%d\n", ss_ptr->ndelay);
		if (st_ptr != NULL)
			printf("\tnext state: \"%s\"\n", st_ptr->name);
	}
	wait_rtn();

	/* Print info about db channels */
	db_ptr = sp_ptr->channels;
	for (nch = 0; nch < sp_ptr->nchan; nch++, db_ptr++)
	{
		printf("\nChannel name: \"%s\"\n", db_ptr->db_name);
		printf("  unexpanded name: \"%s\"\n", db_ptr->db_uname);
		printf("  size=%d bytes\n", db_ptr->size);
		printf("  length=%d elements\n", db_ptr->length);
		printf("  DB request type=%d\n", db_ptr->dbr_type);
		printf("  monitor flag=%d\n", db_ptr->mon_flag);
		printf("  count=%d\n", db_ptr->count);
		printf("  event flag=%d=0x%x\n", db_ptr->ev_flag, db_ptr->ev_flag);
		printf("  delta=%g\n", db_ptr->delta);
		printf("  timeout=%g\n", db_ptr->timeout);
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
