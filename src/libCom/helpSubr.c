/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	10-17-90
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
 * .00	10-17-90	rac	initial version
 * .01	06-18-90	rac	installed in SCCS
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	helpSubr.c - routines to implement embedded help capability
*
* GENERAL DESCRIPTION
*
* QUICK REFERENCE
* #include <genDefs.h>
* HELP_LIST	helpList;
* HELP_TOPIC	topic;
*       void  helpIllegalCommand( pStream,   pHelpList,  command,  keyword    )
*       void  helpInit(           pHelpList				      )
*       void  helpPrintTopics(    pStream,   pHelpList			      )
*       void  helpTopicAdd(       pHelpList, pHelpTopic, helpKeyword, helpText)
* HELP_TOPIC *helpTopicFind(      pHelpList, keyword			      )
*       void  helpTopicPrint(     pStream,   pHelpTopic			      )
*
*-***************************************************************************/
#include <genDefs.h>
#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#else
#   include <stdio.h>
#endif
#if 0		/* some test code for checking out functionality */
main()
{
    HELP_LIST	helpList;
    HELP_TOPIC	helpLegal;
    HELP_TOPIC	helpLegalAddOn;
    HELP_TOPIC	helpUsage;

    helpInit(&helpList);
    helpTopicAdd(&helpList, &helpLegal, "commands", "\n\
generic commands are:\n\
   detach\n\
   help       [topic]\n\
   quit (or ^D)\n\
   source     fileName\n\
");
    helpTopicAdd(&helpList, &helpUsage, "usage", "\n\
This is some\n\
usage information.\n\
");
    helpTopicAdd(&helpList, &helpLegalAddOn, "commands", "\n\
program-specific commands are:\n\
   close      fileName\n\
   open       fileName\n\
");

#if 0
    helpPrintTopics(stdout, &helpList);
    helpTopicPrint(stdout, &helpLegal);
    helpTopicPrint(stdout, &helpUsage);
#endif
    helpIllegalCommand(stdout, &helpList, "xxx", "yyy");
    helpIllegalCommand(stdout, &helpList, "help", "usage");
    helpIllegalCommand(stdout, &helpList, "help", "yyy");
}
#endif

/*+/subr**********************************************************************
* NAME	helpIllegalCommand - process illegal commands
*
* DESCRIPTION
*	This routine accepts a command and a keyword following the command.
*
*	If the command is "help", then the keyword is used to search the
*	help topics.  If a match is found, the information is printed.  If
*	no match is found, an error message is printed, followed by a list
*	of valid topics for more information.
*
*	If the command isn't "help", an "illegal command" error message is
*	printed.  If "commands" is a valid help topic, then that topic is
*	also printed.  (The assumption is that "commands" is the topic which
*	contains a list of legal commands.)
*
* RETURNS
*	void
*
*-*/
void
helpIllegalCommand(pStream, pHelpList, command, keyword)
FILE	*pStream;	/* I stream (such as stdout or stderr) for prints */
HELP_LIST *pHelpList;	/* I pointer to help list */
char	*command;	/* I command */
char	*keyword;	/* I topic keyword */
{
    HELP_TOPIC	*pTopic;	/* temp pointer to topic */

    if (strcmp(command, "help") != 0) {
	(void)fprintf(pStream, "illegal command: %s\n", command);
	if ((pTopic =  helpTopicFind(pHelpList, "commands")) != NULL)
	    helpTopicPrint(pStream, pTopic);
	return;
    }
    if (keyword == NULL || *keyword == '\0')
	helpPrintTopics(pStream, pHelpList);
    else if ((pTopic = helpTopicFind(pHelpList, keyword)) == NULL) {
	(void)fprintf(pStream, "no help information found for: %s\n", keyword);
	helpPrintTopics(pStream, pHelpList);
    }
    else
	helpTopicPrint(pStream, pTopic);
}

/*+/subr**********************************************************************
* NAME	helpInit - initialize a help list
*
* DESCRIPTION
*	This routine initializes a help list prior to adding topics.
*
* RETURNS
*	void
*
*-*/
void
helpInit(pHelpList)
HELP_LIST *pHelpList;	/* IO pointer to help list */
{
    pHelpList->pHead = NULL;
    pHelpList->pTail = NULL;
}

/*+/subr**********************************************************************
* NAME	helpPrintTopics - prints a list of valid topics
*
* DESCRIPTION
*	This prints the list of topic keywords for a help list, preceded
*	by a short "topics available" message.
*
* RETURNS
*	void
*
*-*/
void
helpPrintTopics(pStream, pHelpList)
FILE	*pStream;	/* I stream (such as stdout or stderr) for printing */
HELP_LIST *pHelpList;	/* I pointer to help list */
{
    HELP_TOPIC	*pTopic;	/* temp pointer to topic */

    (void)fprintf(pStream,
		"You can get help info for the following with:  help topic\n");
    pTopic = pHelpList->pHead;
    while (pTopic != NULL) {
	(void)fprintf(pStream, "    %s", pTopic->keyword);
	pTopic = pTopic->pNextTopic;
    }
    (void)fprintf(pStream, "\n\n");
}

/*+/subr**********************************************************************
* NAME	helpTopicAdd - add a help topic to a help list
*
* DESCRIPTION
*	This routine adds a help topic, including a keyword for the topic
*	and the textual information, to a help list.  If the topic already
*	exists on the help list, then the textual information is appended
*	to the already existing textual information for the topic.
*
* RETURNS
*	void
*
*-*/
void
helpTopicAdd(pHelpList, pHelpTopic, helpKeyword, helpText)
HELP_LIST *pHelpList;	/* IO pointer to help list */
HELP_TOPIC *pHelpTopic;	/* IO pointer to help topic */
char	*helpKeyword;	/* I keyword for topic */
char	*helpText;	/* I text for topic */
{
    HELP_TOPIC	*pTopicBase;	/* base item for an existing topic */

    if (pHelpList->pHead != NULL &&
	(pTopicBase = helpTopicFind(pHelpList, helpKeyword)) != NULL) {

	pTopicBase->pLastItem->pNextItem = pHelpTopic;
	pHelpTopic->text = helpText;
	pHelpTopic->keyword = NULL;
	pHelpTopic->pNextTopic = NULL;
	pHelpTopic->pNextItem = NULL;
	pHelpTopic->pLastItem = NULL;

	return;
    }
    if (pHelpList->pHead == NULL)
	pHelpList->pHead = pHelpTopic;
    else
	pHelpList->pTail->pNextTopic = pHelpTopic;

    pHelpList->pTail = pHelpTopic;
    pHelpTopic->text = helpText;
    pHelpTopic->keyword = helpKeyword;
    pHelpTopic->pNextTopic = NULL;
    pHelpTopic->pNextItem = NULL;
    pHelpTopic->pLastItem = pHelpTopic;
}

/*+/subr**********************************************************************
* NAME	helpTopicFind - search a help list for topic
*
* DESCRIPTION
*	This routine searches a help list for a topic whose keyword matches
*	the desired keyword.
*
* RETURNS
*	pointer to topic, or
*	NULL if topic wasn't found
*
*-*/
HELP_TOPIC *
helpTopicFind(pHelpList, keyword)
HELP_LIST *pHelpList;	/* I pointer to help list */
char	*keyword;	/* I topic keyword */
{
    HELP_TOPIC	*pTopic;	/* temp pointer to topic */

    pTopic = pHelpList->pHead;
    while (pTopic != NULL) {
	if (strcmp(keyword, pTopic->keyword) == 0)
	    return pTopic;
	pTopic = pTopic->pNextTopic;
    }

    return NULL;
}

/*+/subr**********************************************************************
* NAME	helpTopicPrint - prints a help topic
*
* DESCRIPTION
*	This routine prints the help information for a help topic.
*
* RETURNS
*	void
*
*-*/
void
helpTopicPrint(pStream, pHelpTopic)
FILE	*pStream;	/* I stream (such as stdout or stderr) for printing */
HELP_TOPIC *pHelpTopic;	/* I pointer to help topic */
{
    while (pHelpTopic != NULL) {
	(void)fprintf(pStream, "%s", pHelpTopic->text);
	pHelpTopic = pHelpTopic->pNextItem;
    }
    (void)fprintf(pStream, "\n");
}
