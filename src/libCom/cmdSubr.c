/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	10-24-90
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
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .00	10-24-90	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	cmdSubr - routines for implementing keyboard command processing
*
* DESCRIPTION
*	intended for use with either UNIX lwp library or with VxWorks
*
*   void cmdCloseContext(   ppCxCmd					)
*   long cmdBgCheck(        pCxCmd					)
*   void cmdRead(           ppCxCmd, pStopFlag				)
*   void cmdSource(         ppCxCmd					)
*
* BUGS
* o	if changes in the command context (e.g., re-directing output) are
*	made at other than root context level, such changes aren't preserved
*	when closing out the level and moving to the previous level.
*
*-***************************************************************************/
#include <genDefs.h>
#include <genTasks.h>
#include <cmdDefs.h>
#include <cadef.h>

#ifdef vxWorks
/*----------------------------------------------------------------------------
*    includes and defines for VxWorks compile
*---------------------------------------------------------------------------*/
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <ctype.h>
#else
/*----------------------------------------------------------------------------
*    includes and defines for Sun compile
*---------------------------------------------------------------------------*/
#   include <stdio.h>
#   include <ctype.h>
#   include <sys/types.h>	/* for 'select' operations */
#   include <sys/time.h>	/* for 'select' operations */
#endif

/*+/subr**********************************************************************
* NAME	cmdBgCheck - validate a ``bg'' command
*
* DESCRIPTION
*	Check to see if a bg command can be honored.  This depends on
*	whether the command was in a source'd file (can't have bg there),
*	and on whether running under VxWorks (bg is OK) or UNIX (not OK).
*
*	If the command shouldn't be honored, this routine prints a message.
*
* RETURNS
*	OK, or
*	ERROR if the bg command should not be honored
*
*-*/
long
cmdBgCheck(pCxCmd)
CX_CMD	*pCxCmd;	/* IO pointer to command context */
{
    HELP_TOPIC	*pBgTopic;

    if (pCxCmd != pCxCmd->pCxCmdRoot) {
	(void)printf("can't use bg command in source file\n");
	return ERROR;
    }
#ifdef vxWorks
    return OK;
#else
    pBgTopic = helpTopicFind(&pCxCmd->helpList, "bg");
    if (pBgTopic != NULL)
	helpTopicPrint(stdout, pBgTopic);
    return ERROR;
#endif
}

/*+/subr**********************************************************************
* NAME	cmdRead - read the next input line
*
* DESCRIPTION
*	Prints a prompt, waits for input to be available, and then reads
*	the input line into the buffer in the command context.
*
*	If input is from a source'd file, no prompt is printed, and the
*	input line is printed.  At EOF on the file, the "source level"
*	in the command context is closed, changing to the previous
*	source level; the line buffer will contain a zero length line.
*
* RETURNS
*	void
*
* BUGS
* o	under VxWorks, with stdout redirected to a socket, a prompt which
*	doesn't end with '\n' doesn't get sent
*
* SEE ALSO
*	cmdSource()
*
*-*/
void
cmdRead(ppCxCmd, pStopFlag)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
int	*pStopFlag;	/* I pointer to flag; 1 says to "stop" */
{
#ifndef vxWorks
    fd_set      fdSet;          /* set of fd's to watch with select */
    int         fdSetWidth;     /* width of select bit mask */
    struct timeval fdSetTimeout;/* timeout interval for select */
#endif
    CX_CMD	*pCxCmd;	/* pointer to present command context */

    pCxCmd = *ppCxCmd;
/*-----------------------------------------------------------------------------
* handle keyboard input as a special case.  Only for keyboard input is a
* prompt printed.  Only for keyboard input is there protection against blocking
*----------------------------------------------------------------------------*/
    if (pCxCmd->inputName == NULL) {
	if (pCxCmd->prompt != NULL) {
	    (void)printf("%s", pCxCmd->prompt);
	    (void)fflush(stdout);
	}

#ifndef vxWorks
/*----------------------------------------------------------------------------
*	under SunOS, with lightweight processes, just doing a simple
*	gets() call would block the pod.  Instead, under SunOS, do a
*	select() to check for input available.
*---------------------------------------------------------------------------*/
	fdSetWidth = getdtablesize();
	while (1) {
	    if (*pStopFlag != 0)
		break;
	    fdSetTimeout.tv_sec = 0;
	    fdSetTimeout.tv_usec = 0;
	    FD_ZERO(&fdSet);
	    FD_SET(fileno(stdin), &fdSet);

	    if (select(fdSetWidth, &fdSet, NULL, NULL, &fdSetTimeout) != 0)
		break;			/* input's ready !!! */
	    taskSleep(SELF, 1, 0);
	}
#endif
    }

    if (*pStopFlag != 0)
	pCxCmd->line[0] = '\0';
    else {
	if (fgets(pCxCmd->line, 80, pCxCmd->input) == NULL) {
	    if (pCxCmd->inputName != NULL) {
		(void)printf("EOF on source'd file: %s\n", pCxCmd->inputName);
	    }
	    else {
		(void)printf("^D\n");
		pCxCmd->inputEOF = 1;
	    }
	    clearerr(pCxCmd->input);
	    cmdCloseContext(ppCxCmd);
	    pCxCmd = *ppCxCmd;
	}
	else if (pCxCmd->inputName != NULL)
	    (void)printf("%s", pCxCmd->line);
    }

    pCxCmd->pLine = pCxCmd->line;
}

/*+/subr**********************************************************************
* NAME	cmdCloseContext - closes a command context
*
* DESCRIPTION
*	Closes a command context.  The action depends on source of input:
*
*	keyboard (or socket):
*	o   store "quit\n" in the line buffer
*
*	source'd file:
*	o   close out this "source level"
*	o   move to the previous "source level"
*	o   store '\0' in the line buffer
*
* RETURNS
*	void
*
* BUGS
* o	text
*
*-*/
void
cmdCloseContext(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    CX_CMD	*pCxCmd;	/* temp pointer */

    assert(ppCxCmd != NULL);

    if ((*ppCxCmd)->inputName == NULL)
	strcpy((*ppCxCmd)->line, "quit\n");
    else {
	(void)fclose((*ppCxCmd)->input);
	assert((*ppCxCmd)->pPrev != NULL);
	pCxCmd = (*ppCxCmd)->pPrev;
	GenFree((char *)(*ppCxCmd));
	*ppCxCmd = pCxCmd;
	(*ppCxCmd)->line[0] = '\0';
    }
}

/*+/subr**********************************************************************
* NAME	cmdSource - process a ``source'' command
*
* DESCRIPTION
*	Processes a "source fileName" command.  A new command context is
*	built and set up for reading from the named file.  If an error
*	is detected, the present command context is preserved.
*
* RETURNS
*	void
*
*-*/
void
cmdSource(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context.  The
				pCommand item is assumed to point to the
				source command.  This routine scans for
				the file name. */
{
    CX_CMD	*pCxCmdNew;	/* new command context structure */
    CX_CMD	*pCxCmd;	/* present command context structure */

    pCxCmd = *ppCxCmd;
    pCxCmd->fldLen =
	    nextNonSpaceField(&pCxCmd->pLine, &pCxCmd->pField, &pCxCmd->delim);
    if (pCxCmd->fldLen <= 1) {
	(void)printf("you must specify a file name\n");
	return;
    }
    if ((pCxCmdNew = (CX_CMD *)GenMalloc(sizeof(CX_CMD))) == NULL) {
	(void)printf("couldn't malloc command structure\n");
	return;
    }
    *pCxCmdNew = *pCxCmd;	/* inherit useful info from present context */
    if ((pCxCmdNew->input = fopen(pCxCmd->pField, "r")) == NULL) {
	(void)printf("couldn't open file\n");
	GenFree((char *)pCxCmdNew);
	return;
    }
    pCxCmdNew->pPrev = pCxCmd;
    pCxCmdNew->inputName = pCxCmd->pField;
    *ppCxCmd = pCxCmdNew;
}
