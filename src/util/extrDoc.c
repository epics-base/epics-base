/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	02-11-90
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
 * .01	02-11-90	rac	initial version
 *
 */
/*+/mod***********************************************************************
* TITLE	extrDoc - extract source code documentation
*
* DESCRIPTION
*	`extrDoc' processes a C source file with specially formatted embedded
*	documentation, producing a text file containing the extracted
*	documentation.
*
*	Usage:  % extrDoc < xxx.c >! xxx.doc
*
*	The activities of `extrDoc' center around comment blocks of the
*	general form shown below.  (To avoid confusing the C compiler
*	when it processes this source file, '* and *' are shown in the
*	example when what is really meant are the usual C commend start
*	and end characters, with / used instead of the ' character.)
*
*		'*+/keyword****
*		* KEY
*		*	comments
*		*-************'
*
*	The +/ turns on extracting, with the action depending on the
*	keyword.  As the comments in the block are written to the output,
*	the leading * is removed.  If the character following the * is a
*	blank followed by a printing character, the blank is also removed.
*
*	The keywords and their actions are:
*	/mod	the block constitutes a module header.  The comments are
*		extracted as described above.  Extracting stops when the
*		*- is encountered.
*
*	/subr	the block is a subroutine header.  The comments are extracted
*		as described above.  When the *- is encountered, a line with
*		"SYNOPSIS" is printed to the output.  The lines between the
*		*- and a line with { in column 1 are taken to be the synopsis
*		for the subroutine, and are written directly to the output.
*
*	/macro	the block is a description for one or more macros.  The
*		treatment is the same as for /subr, except that no SYNOPSIS
*		processing is done.
*
*	/internal the block is a description for an `internal' routine.  The
*		block is not extracted.
*		
* WISH LIST
* o	generate output in the usual man pages form
* o	generate output for PostScript
* o	generate headings and subheadings based on TITLE in /mod blocks
*	and NAME in /subr and /macro blocks
* o	generate a table of contents with NAME items as TOC entries
*-***************************************************************************/
#include <stdio.h>

main(argc, argv)
int     argc;		/* number of command line args */
char   *argv[];		/* command line args */
{
    char	inputBuf[120];	/* input buffer */
    int		extr=0;		/* extract flag;
					0: no extracting
					1: extracting in progress
					2: initialize for extracting */
    int		sendFf=0;	/* send a form feed; 1 says send it */
    int		subr=0;		/* within /subr block */
    int		macro=0;	/* within /macro block */
    int		synopsis=0;	/* extracting synopsis following /subr */

    while (fgets(inputBuf, 120, stdin) != NULL) {
	if (strncmp(inputBuf, "/*+", 3) == 0)
	    extr = 2;
	else if (strncmp(inputBuf, "*+", 2) == 0)
	    extr = 2;
	else if (strncmp(inputBuf, "*-", 2) == 0) {
	    extr = 0;
	    sendFf = 1;
	    if (subr) {
		synopsis = 1;
		printf("SYNOPSIS\n\n");
	    }
	    else if (macro)
		macro = 0;
	}
	else if (extr) {
	    if (sendFf)
		printf("\f\n");
	    sendFf = 0;
	    if (inputBuf[1] == ' ' && !isspace(inputBuf[2]))
		printf("%s", &inputBuf[2]);
	    else
		printf("%s", &inputBuf[1]);
	}
	else if (synopsis) {
	    if (inputBuf[0] == '{') {
		subr = 0;
		synopsis = 0;
	    }
	    else
		printf("%s", inputBuf);
	}
/*----------------------------------------------------------------------------
*    see if need to initialize for extraction
*---------------------------------------------------------------------------*/
        if (extr == 2) {
	    extr = 1;
	    if (strIndex(inputBuf, "/mod*")) {
		
	    }
	    else if (strIndex(inputBuf, "/subr*")) {
		subr = 1;
	    }
	    else if (strIndex(inputBuf, "/macro*")) {
		macro = 1;
	    }
	    else if (strIndex(inputBuf, "/internal*")) {
		extr = 0;
	    }
	    else
		sendFf = 0;	/* don't send FF unless new section */
	}
    }
    return 0;
}

/*+/subr**********************************************************************
* NAME	strIndex - look for substring
*
* DESCRIPTION
*	Search a string, looking for a specific substring.
*
* RETURNS
*	0	if the substring wasn't found, else
*	1
*
*-*/
strIndex(string, key)
char	*string;	/* I string to search */
char	*key;		/* I key to look for */
{
    int		keyCol;		/* column within key */
    int		keyLen;		/* length of key */
    int		searchCol;	/* current column in string */
    int		strCol;		/* column within string */
    int		strLen;		/* length of string */

    strLen = strlen(string);
    keyLen = strlen(key);

    strCol = 0;
    next: {
	searchCol = strCol++;
	if (searchCol + keyLen > strLen)
	    return 0;		/* FAILED!! */
	else {
	    keyCol = 0;
	    loop: {
		if (string[searchCol++] != key[keyCol++])
		    goto next;
		else if (keyCol == keyLen)
		    return 1;
		else
		    goto loop;
	    }
	}
    }
}
