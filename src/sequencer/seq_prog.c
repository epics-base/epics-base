/**************************************************************************
			GTA PROJECT   AT division
	Copyright, 1991, The Regents of the University of California.
		         Los Alamos National Laboratory

	$Id$
	DESCRIPTION: Seq_prog.c: state program list management functions.
	All active state programs are inserted into the list when created
	and removed from the list when deleted.

	ENVIRONMENT: VxWorks

	HISTORY:
09dec91,ajk	original
29apr92,ajk	Added mutual exclusion locks	
17Jul92,rcz	Changed semBCreate call for V5 vxWorks, maybe should be semMCreate ???
***************************************************************************/
#define	DEBUG
#include	"seq.h"

LOCAL SEM_ID	seqProgListSemId;
LOCAL int	seqProgListInited = FALSE;
LOCAL LIST	seqProgList;

typedef struct prog_node
{
	NODE		node;
	SPROG		*pSP;
} PROG_NODE;

#define	seqListFirst(pList)	(PROG_NODE *)lstFirst((LIST *)pList)

#define	seqListNext(pNode)	(PROG_NODE *)lstNext((NODE *)pNode)

/*
 * seqFindProg() - find a program in the state program list from task id.
 */
SPROG *seqFindProg(task_id)
int		task_id;
{
	PROG_NODE	*pNode;
	SPROG		*pSP;
	SSCB		*pSS;
	int		n;

	if (!seqProgListInited || task_id == 0)
		return NULL;

	semTake(seqProgListSemId, WAIT_FOREVER);

	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		pSP = pNode->pSP;
		if (pSP->task_id == task_id)
		{
			semGive(seqProgListSemId);
			return pSP;
		}
		pSS = pSP->sscb;
		for (n = 0; n < pSP->nss; n++, pSS++)
		{
			if (pSS->task_id == task_id)
			{
				semGive(seqProgListSemId);
				return pSP;
			}
		}
	}
	semGive(seqProgListSemId);

	return NULL; /* not in list */
}

/*
 * seqTraverseProg() - travers programs in the state program list and
 * call the specified routine or function.  Passes one parameter of
 * pointer size.
 */
STATUS seqTraverseProg(pFunc, param)
VOID		(*pFunc)();	/* function to call */
VOID		*param;		/* any parameter */
{
	PROG_NODE	*pNode;
	SPROG		*pSP;

	if (!seqProgListInited)
		return ERROR;

	semTake(seqProgListSemId, WAIT_FOREVER);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		pSP = pNode->pSP;
		pFunc(pSP, param);
	}

	semGive(seqProgListSemId);
	return OK;
}

/*
 * seqAddProg() - add a program to the state program list.
 * Returns ERROR if program is already in list, else TRUE.
 */
STATUS seqAddProg(pSP)
SPROG		*pSP;
{
	PROG_NODE	*pNode;

	if (!seqProgListInited)
		seqProgListInit(); /* Initialize list */

	semTake(seqProgListSemId, WAIT_FOREVER);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{

		if (pSP == pNode->pSP)
		{
			semGive(seqProgListSemId);
#ifdef DEBUG
			printf("Task %d already in list\n", pSP->task_id);
#endif
			return ERROR; /* already in list */
		}
	}

	/* Insert at head of list */
	pNode = (PROG_NODE *)malloc(sizeof(PROG_NODE) );
	if (pNode == NULL)
	{
		semGive(seqProgListSemId);
		return ERROR;
	}

	pNode->pSP = pSP;
	lstAdd(&seqProgList, pNode);
	semGive(seqProgListSemId);
#ifdef DEBUG
	printf("Added task %d to list.\n", pSP->task_id);
#endif

	return OK;
}

/* 
 *seqDelProg() - delete a program from the program list.
 * Returns TRUE if deleted, else FALSE.
 */
STATUS seqDelProg(pSP)
SPROG		*pSP;
{
	PROG_NODE	*pNode;

	if (!seqProgListInited)
		return ERROR;

	semTake(seqProgListSemId, WAIT_FOREVER);
	for (pNode = seqListFirst(&seqProgList); pNode != NULL;
	     pNode = seqListNext(pNode) )
	{
		if (pNode->pSP == pSP)
		{
			lstDelete(&seqProgList, pNode);
			semGive(seqProgListSemId);

#ifdef DEBUG
			printf("Deleted task %d from list.\n", pSP->task_id);
#endif
			return OK;
		}
	}	

	semGive(seqProgListSemId);
	return ERROR; /* not in list */
}

/*
 * seqProgListInit() - initialize the state program list.
 */
seqProgListInit()
{
	/* Init linked list */
	lstInit(&seqProgList);

	/* Create a semaphore for mutual exclusion */
	seqProgListSemId = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	semGive(seqProgListSemId);

	seqProgListInited = TRUE;

}

