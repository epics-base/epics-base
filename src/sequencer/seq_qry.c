/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Task querry & debug routines for run-time sequencer:
	seqShow - prints state set info.
	seqChanShow - printf channel (pv) info.

	ENVIRONMENT: VxWorks
***************************************************************************/

/*	#define	DEBUG	1	*/

#include	"seq.h"

#ifdef	ANSI
int	seqShow(int);
int	seqChanShow(int);
LOCAL	VOID wait_rtn();
static	void printValue(void *, int, int, int);
static	void printType(int);
#else
int	seqShow();
int	seqChanShow();
LOCAL	VOID wait_rtn();
static	void printValue();
static	void printType();
#endif

/* Querry the sequencer for state information */
int seqShow(tid)
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
	printf("  Memory layout:\n");
	printf("\tsp_ptr=%d=0x%x\n", sp_ptr, sp_ptr);
	printf("\tdyn_ptr=%d=0x%x\n", sp_ptr->dyn_ptr, sp_ptr->dyn_ptr);
	printf("\tsscb=%d=0x%x\n", sp_ptr->sscb, sp_ptr->sscb);
	printf("\tstates=%d=0x%x\n", sp_ptr->states, sp_ptr->states);
	printf("\tuser_area=%d=0x%x\n", sp_ptr->user_area, sp_ptr->user_area);
	printf("\tuser_size=%d=0x%x\n", sp_ptr->user_size, sp_ptr->user_size);
	printf("\tmac_ptr=%d=0x%x\n", sp_ptr->mac_ptr, sp_ptr->mac_ptr);
	printf("\tscr_ptr=%d=0x%x\n", sp_ptr->scr_ptr, sp_ptr->scr_ptr);
	printf("\tscr_nleft=%d=0x%x\n\n", sp_ptr->scr_nleft, sp_ptr->scr_nleft);
	printf("  initial task id=%d=0x%x\n", sp_ptr->task_id, sp_ptr->task_id);
	printf("  task priority=%d\n", sp_ptr->task_priority);
	printf("  number of state sets=%d\n", sp_ptr->nss);
	printf("  number of channels=%d\n", sp_ptr->nchan);
	printf("  number of channels connected=%d\n", sp_ptr->conn_count);
	printf("  async flag=%d, debug flag=%d, reent flag=%d, conn flag=%d\n",
	 sp_ptr->async_flag, sp_ptr->debug_flag, sp_ptr->reent_flag,
	 sp_ptr->conn_flag);

	printf("\n");
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
int seqChanShow(tid)
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

	/* seq_task_ptr is a task variable */
	sp_ptr = (SPROG *)taskVarGet(tid, &seq_task_ptr);

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
		printType(db_ptr->put_type);
		printValue((void *)db_ptr->var, db_ptr->size, db_ptr->count,
			   db_ptr->put_type);
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
			printf("  Get not completed or no get issued\n");

		printf("  Status=%d\n", db_ptr->status);
		printf("  Severity=%d\n", db_ptr->severity);

		printf("  Delta=%g\n", db_ptr->delta);
		printf("  Timeout=%g\n", db_ptr->timeout);
		wait_rtn();
	}

	return 0;
}


LOCAL VOID wait_rtn()
{
	char	bfr;
	printf("Hit RETURN to continue\n");
	do {
		read(STD_IN, &bfr, 1);
	} while (bfr != '\n');
}

struct	dbr_union {
	union {
		char	c;
		short	s;
		long	l;
		float	f;
		double	d;
	} u;
};

static void printValue(pvar, size, count, type)
void		*pvar;
int		size, count, type;
{
	struct dbr_union	*pv = pvar;

	if (count > 5)
		count = 5;

	printf("  Value =");
	while (count-- > 0)
	{
		switch (type)
		{
		    case DBR_STRING:
			printf(" %s", pv->u.c);
			break;

	  	    case DBR_CHAR:
			printf(" %d", pv->u.c);
			break;

		    case DBR_SHORT:
			printf(" %d", pv->u.s);
			break;

		    case DBR_LONG:
			printf(" %d", pv->u.l);
			break;

		    case DBR_FLOAT:
			printf(" %g", pv->u.f);
			break;

		    case DBR_DOUBLE:
			printf(" %lg", pv->u.d);
			break;
		}

		pv = (struct dbr_union *)((char *)pv + size);
	}

	printf("\n");
}

static void printType(type)
int		type;
{
	char		*type_str;

	switch (type)
	{
	    case DBR_STRING:
		type_str = "string";
		break;

	    case DBR_CHAR:
		type_str = "char";
		break;

	    case DBR_SHORT:
		type_str = "short";
		break;

	    case DBR_LONG:
		type_str = "long";
		break;

	    case DBR_FLOAT:
		type_str = "float";
		break;

	    case DBR_DOUBLE:
		type_str = "double";
		break;
	}
	printf("  Type = %s\n", type_str);
}
