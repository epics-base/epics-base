/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1990-1994, The Regents of the University of California.
		         Los Alamos National Laboratory
	seq_qry.c,v 1.2 1995/06/27 15:26:02 wright Exp

	DESCRIPTION: Task querry & debug routines for run-time sequencer:
	seqShow - prints state set info.
	seqChanShow - printf channel (pv) info.

	ENVIRONMENT: VxWorks
	HISTORY:
25nov91,ajk	Display task names(s) with id(s).
		Display logfile name and file descriptor.
		Moved wait_rtn() to top of loop.
09dec91,ajk	Modified to used state program linked list.
		Added option to display all programs when tid=0.
19dec91,ajk	Allow task name as well as task id.
25feb92,ajk	V5.0 accepts 0 as a valid task id: fixed it.
26feb92,ajk	Fixed formatting of task/program listing.
29apr92,ajk	Modified to interpret encoded options.
21may92,ajk	Modified format for listing programs & tasks.
21feb93,ajk	Some minor code cleanup.
01mar94,ajk	Major changes to print more meaningful information.
***************************************************************************/

/*#define	DEBUG	1*/

#include	"seq.h"

int	seqShow(int);
int	seqChanShow(int, char *);
LOCAL	int wait_rtn();
LOCAL	VOID printValue(char *, int, int);
LOCAL	SPROG *seqQryFind(int);
LOCAL	seqShowAll();

/*
 * seqShow() - Querry the sequencer for state information.
 * If a non-zero task id is specified then print the information about
 * the state program, otherwise print a brief summary of all state programs
 */
int seqShow(tid)
int	tid;
{
	SPROG		*pSP;
	SSCB		*pSS;
	STATE		*pST;
	CHAN		*pDB;
	int		nss, nst, nch, status, n;
	float		time;
	char		file_name[100];

	/* convert (possible) name to task id */
	if (tid != 0)
	{
		tid = taskIdFigure(tid);
		if (tid == ERROR)
			return 0;
	}
	pSP = seqQryFind(tid);
	if (pSP == NULL)
		return 0;

	/* Print info about state program */
	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("  initial task id=%d=0x%x\n", pSP->taskId, pSP->taskId);
	printf("  task priority=%d\n", pSP->taskPriority);
	printf("  number of state sets=%d\n", pSP->numSS);
	printf("  number of channels=%d\n", pSP->numChans);
	printf("  number of channels assigned=%d\n", pSP->assignCount);
	printf("  number of channels connected=%d\n", pSP->connCount);
	printf("  options: async=%d, debug=%d,  newef=%d, reent=%d, conn=%d\n",
	 ((pSP->options & OPT_ASYNC) != 0), ((pSP->options & OPT_DEBUG) != 0),
	 ((pSP->options & OPT_NEWEF) != 0), ((pSP->options & OPT_REENT) != 0),
	 ((pSP->options & OPT_CONN) != 0) );
	printf("  log file fd=%d\n", pSP->logFd);
	status = ioctl(pSP->logFd, FIOGETNAME, (int)file_name);
	if (status != ERROR)
		printf("  log file name=\"%s\"\n", file_name);

	printf("\n");

	/* Print state set info */
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		printf("  State Set: \"%s\"\n", pSS->pSSName);

		printf("  task name=%s;  ", taskName(pSS->taskId));

		printf("  task id=%d=0x%x\n", pSS->taskId, pSS->taskId);

		pST = pSS->pStates;
		printf("  First state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->currentState;
		printf("  Current state = \"%s\"\n", pST->pStateName);

		pST = pSS->pStates + pSS->prevState;
		printf("  Previous state = \"%s\"\n", pST->pStateName);

		time = (tickGet() - pSS->timeEntered)/60.0;
		printf("\tElapsed time since state was entered = %.1f seconds)\n",
		 time);
#ifdef	DEBUG
		printf("  Queued time delays:\n");
		for (n = 0; n < pSS->numDelays; n++)
		{
			printf("\tdelay[%2d]=%d", n, pSS->delay[n]);
			if (pSS->delayExpired[n])
				printf(" - expired");
			printf("\n");
		}
#endif	/* DEBUG */
		printf("\n");
	}

	return 0;
}
/*
 * seqChanShow() - Show channel information for a state program.
 */
int seqChanShow(tid, pStr)
int		tid;	/* task id or name */
char		*pStr;	/* optional pattern matching string */
{
	SPROG		*pSP;
	CHAN		*pDB;
	int		nch, n;
	float		time;
	char		tsBfr[50], connQual;
	int		match, showAll;

	/* convert (possible) name to task id */
	if (tid != 0)
	{
		tid = taskIdFigure(tid);
		if (tid == ERROR)
			return 0;
	}
	pSP = seqQryFind(tid);
	if (tid == NULL)
		return 0;

	printf("State Program: \"%s\"\n", pSP->pProgName);
	printf("Number of channels=%d\n", pSP->numChans);

	if (pStr != NULL)
	{
		connQual = pStr[0];
		/* Check for channel connect qualifier */
		if ((connQual == '+') || (connQual == '-'))
		{
			pStr += 1;
		}
	}
	else
		connQual = 0;

	pDB = pSP->pChan;
	for (nch = 0; nch < pSP->numChans; )
	{
		if (pStr != NULL)
		{
			/* Check for channel connect qualifier */
			if (connQual == '+')
				showAll = pDB->connected; /* TRUE if connected */
			else if (connQual == '-')
				showAll = !pDB->connected; /* TRUE if NOT connected */
			else
				showAll = TRUE; /* Always TRUE */

			/* Check for pattern match if specified */
			match = (pStr[0] == 0) || (strstr(pDB->dbName, pStr) != NULL);
			if (!(match && showAll))
			{
				pDB += 1;
				nch += 1;
				continue; /* skip this channel */
			}
		}
		printf("\n#%d of %d:\n", nch+1, pSP->numChans);
		printf("Channel name: \"%s\"\n", pDB->dbName);
		printf("  Unexpanded (assigned) name: \"%s\"\n", pDB->dbAsName);
		printf("  Variable name: \"%s\"\n", pDB->pVarName);
		printf("    address = %d = 0x%x\n", pDB->pVar, pDB->pVar);
		printf("    type = %s\n", pDB->pVarType);
		printf("    count = %d\n", pDB->count);
		printValue(pDB->pVar, pDB->putType, pDB->count);
		printf("  Monitor flag=%d\n", pDB->monFlag);

		if (pDB->monitored)
			printf("    Monitored\n");
		else
			printf("    Not monitored\n");

		if (pDB->assigned)
			printf("  Assigned\n");
		else
			printf("  Not assigned\n");

		if(pDB->connected)
			printf("  Connected\n");
		else
			printf("  Not connected\n");

		if(pDB->getComplete)
			printf("  Last get completed\n");
		else
			printf("  Get not completed or no get issued\n");

		printf("  Status=%d\n", pDB->status);
		printf("  Severity=%d\n", pDB->severity);

		/* Print time stamp in text format: "mm/dd/yy hh:mm:ss.nano-sec" */
		tsStampToText(&pDB->timeStamp, TS_TEXT_MMDDYY, tsBfr);
		if (tsBfr[2] == '/') /* valid t-s? */
			printf("  Time stamp = %s\n", tsBfr);

		n = wait_rtn();
		if (n == 0)
			return 0;
		nch += n;
		if (nch < 0)
			nch = 0;
		pDB = pSP->pChan + nch;
	}

	return 0;
}

/* Read from console until a RETURN is detected */
LOCAL int wait_rtn()
{
	char		bfr[10];
	int		i, n;

	printf("Next? (+/- skip count)\n");
	for (i = 0;  i < 10; i++)
	{
		read(STD_IN, &bfr[i], 1);
		if (bfr[i] == '\n')
			break;
	}
	bfr[i] = 0;
	if (bfr[0] == 'q')
		return 0; /* quit */

	n = atoi(bfr);
	if (n == 0)
		n = 1;
	return n;
}

/* Print the current internal value of a database channel */
LOCAL VOID printValue(pVal, type, count)
char		*pVal;
int		count, type;
{
	int		i;
	char		*c;
	short		*s;
	long		*l;
	float		*f;
	double		*d;

	if (count > 5)
		count = 5;

	printf("  Value =");
	for (i = 0; i < count; i++)
	{
	  switch (type)
	  {
	    case DBR_STRING:
		c = (char *)pVal;
		for (i = 0; i < count; i++, c += MAX_STRING_SIZE)
		{
			printf(" %s", c);
		}
		break;

	     case DBR_CHAR:
		c = (char *)pVal;
		for (i = 0; i < count; i++, c++)
		{
			printf(" %d", *c);
		}
		break;

	    case DBR_SHORT:
		s = (short *)pVal;
		for (i = 0; i < count; i++, s++)
		{
			printf(" %d", *s);
		}
		break;

	    case DBR_LONG:
		l = (long *)pVal;
		for (i = 0; i < count; i++, l++)
		{
			printf(" %d", *l);
		}
		break;

	    case DBR_FLOAT:
		f = (float *)pVal;
		for (i = 0; i < count; i++, f++)
		{
			printf(" %g", *f);
		}
		break;

	    case DBR_DOUBLE:
		d = (double *)pVal;
		for (i = 0; i < count; i++, d++)
		{
			printf(" %g", *d);
		}
		break;
	  }
	}

	printf("\n");
}

/* Find a state program associated with a given task id */
LOCAL SPROG *seqQryFind(tid)
int		tid;
{
	SPROG		*pSP;
	extern SPROG	*seqFindProg();

	if (tid == 0)
	{
		seqShowAll();
		return NULL;
	}

	/* Find a state program that has this task id */
	pSP = seqFindProg(tid);
	if (pSP == NULL)
	{
		printf("No state program exists for task id %d\n", tid);
		return NULL;
	}

	return pSP;
}

LOCAL int	seqProgCount;

/* This routine is called by seqTraverseProg() for seqShowAll() */
LOCAL seqShowSP(pSP)
SPROG		*pSP;
{
	SSCB		*pSS;
	int		nss;
	char		*progName, *ptaskName;;

	if (seqProgCount++ == 0)
		printf("Program Name     Task ID    Task Name        SS Name\n\n");

	progName = pSP->pProgName;
	for (nss = 0, pSS = pSP->pSS; nss < pSP->numSS; nss++, pSS++)
	{
		if (pSS->taskId == 0)
			ptaskName = "(no task)";
		else
			ptaskName = taskName(pSS->taskId);
		printf("%-16s %-10d %-16s %-16s\n",
		 progName, pSS->taskId, ptaskName, pSS->pSSName );
		progName = "";
	}
	printf("\n");
}

/* Print a brief summary of all state programs */
LOCAL seqShowAll()
{
	SPROG		*pSP;

	seqProgCount = 0;
	seqTraverseProg(seqShowSP, 0);
	if (seqProgCount == 0)
		printf("No active state programs\n");
	return 0;
}
