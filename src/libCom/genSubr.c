/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	03-29-90
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
 * .00	03-29-90	rac	initial version
 * .01	06-18-91	rac	installed in SCCS
 * .02	01-29-92	rac	added wildMatch function
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	genSubr.c - some general use routines
*
* DESCRIPTION
*	This file contains some routines which are generally useful for
*	EPICS programs.  Some routines are for SunOS vs. VxWorks
*	compatibility.
*
* QUICK REFERENCE
*         assert(       expression)
*         assertAlways( expression)
*   char *genMalloc(    nBytes)
*   void  genBufCheck(  pBuf)
*   void  genFree(      pBuf)
*   void  genShellCommand(cmdBuf, resultBuf, resultDim)
*   void  genSigInit(   pHandler)
*    int  perror(       string)
*    int  wildMatch(    string, pattern, ignoreCase)
*
* SEE ALSO
*	genDefs.h
*-***************************************************************************/
#include <genDefs.h>

#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <taskLib.h>
#   include <sigLib.h>
#   include <ctype.h>
#else
#   include <stdio.h>
#   include <signal.h>
#   include <ctype.h>
#endif


/*+/macro*********************************************************************
* NAME	assert - check assertion
*
* DESCRIPTION
*	Checks to see if an expression is true.  If the expression is false,
*	an error message is printed and a signal is raised (SIGABRT or, for
*	VxWorks, SIGUSR1).
*
*	This is actually a macro provided by genDefs.h  .  If the preprocessor
*	symbol NDEBUG is defined, then the expression isn't evaluated or
*	checked.
*
* USAGE
*	assert(expression);
*	assertAlways(expression);	(same as assert(), but isn't disabled
*					by defining NDEBUG)
*
* RETURNS
*	void
*
* NOTES
* 1.	Since evaluation of the expression is conditional (depending on
*	whether NDEBUG is defined), it is poor practice to use expressions
*	which have side effects.
* 2.	assertAlways(0) is recommended rather than abort().
*
*-*/
/*----------------------------------------------------------------------------
* assertFail - handle assertion failure
*	For SunOS, call abort().
*	For VxWorks, try to deliver a SIGUSR1 signal.  If that's not
*		possible, generate a bus error.
*---------------------------------------------------------------------------*/
int
assertFail(fileName, lineNum)
char *fileName;
int lineNum;
{
    (void)fprintf(stderr, "assertFail: in file %s line %d\n",
						fileName, lineNum);
#ifdef vxWorks
    if (kill(taskIdSelf(), SIGUSR1) == ERROR) {
	int	*j;
	j = (int *)(-1);
	j = (int *)(*j);
    }
    exit(1);
#else
    abort();
#endif
}

/*+/subr**********************************************************************
* NAME	genMalloc - malloc() and free() 'pre-processor' with "guards"
*
* DESCRIPTION
*	These routines support catching overwriting a block of memory
*	obtained by malloc().  A 'guard' field is added at the beginning
*	and end of the block of memory when genMalloc() is called to
*	allocate the block.  The 'guard' field is checked by genFree().
*
*	genMalloc() works the same as malloc(), returning a pointer to
*	the 'usable' part of the memory block.  (I.e., the guard fields are
*	'hidden' from the caller.)
*
*	genFree() works the same as free(), except it checks to see if
*	the guard field has been clobbered.
*
*	A third routine, genBufCheck() allows checking at arbitrary times
*	(even from dbx) on whether a block has been clobbered.
*
*****************************************************************************/

#if 0		/* a simple demonstration of usage */
main()
{
    int		*x;
    int		i;

    x = (int *)genMalloc(100);
    genBufCheck(x);
    for (i=0; i<26; i++)
	*(x+i) = i;
    genFree(x);

    exit(0);
}
#endif

#   define CHK_MALLOC_FILL_PATTERN 1

/*----------------------------------------------------------------------------
*    malloc a block with the following layout, with the block and guard
*    fields aligned on longword boundaries
*
*	longword 0	nLong for block, including this longword and the
*			'guard' longwords
*	longword 1	&longword 0
*	user bytes
*	longword n	&longword 0
*---------------------------------------------------------------------------*/
char *genMalloc(nBytes)
int	nBytes;
{
    long	*ptrL;
    int		i;
    unsigned	nLong, sizeC;

    nLong = 3 + (nBytes+sizeof(long)-1)/sizeof(long);
    sizeC = nLong * sizeof(long);
    ptrL = (long *)malloc(sizeC);
    if (ptrL != NULL) {
	for (i=0; i<sizeC; )
	    ((char *)ptrL)[i++] = CHK_MALLOC_FILL_PATTERN;
	*ptrL = nLong;
	*(ptrL+1) = *(ptrL+nLong-1) = (long)ptrL;
	return (char *)(ptrL+2);
    }
    return NULL;
}

void genBufCheck(ptr)
void	*ptr;
{
    long	*ptrL;
    unsigned	nLong;

    assert(ptr != NULL);
    ptrL = (long *)ptr - 2;
    if (*(ptrL+1) != (long)ptrL) {
	(void)fprintf(stderr, "genBufCheck: guard at begin clobbered\n");
	assertAlways(0);
    }
    nLong = *ptrL;
    if (*(ptrL+nLong-1) != (long)ptrL) {
	(void)fprintf(stderr, "genBufCheck: guard at end clobbered\n");
	assertAlways(0);
    }
}

void genFree(ptr)
void	*ptr;
{
    long	*ptrL;

    genBufCheck(ptr);
    ptrL = (long *)ptr - 2;
    (void)free((char *)ptrL);
    return;
}

/*+/subr**********************************************************************
* NAME	genShellCommand - issue a command to the "sh" shell
*
* DESCRIPTION
*	Issues a command to the Bourne shell, "sh".  The text which results
*	from executing the command is returned in the buffer supplied by
*	the caller.
*
* RETURNS
*	void
*
* NOTES
* 1.	Any errors which are explicitly detected by this routine result in
*	an error message in the caller's result buffer.
* 2.	This routine will result in a SIGCHLD signal, which the caller will
*	have to handle.
* 3.	If no output results from executing the command, then resultBuf[0]
*	will be set to '\0'.
*
*-*/
#ifndef vxWorks
void
genShellCommand(cmdBuf, resultBuf, resultDim)
char	*cmdBuf;	/* I line to send to shell */
char	*resultBuf;	/* O buffer to hold result of shell command */
int	resultDim;	/* I size of result buffer */
{
    FILE	*shellPipe;
    int		i;

    if ((shellPipe = popen(cmdBuf, "r")) == NULL) {
	strcpy(resultBuf, "couldn't issue shell command\n");
	return;
    }
    i = fread(resultBuf, 1, resultDim-1, shellPipe);
    resultBuf[i] = '\0';
    pclose(shellPipe);
}
#endif

/*+/subr**********************************************************************
* NAME	genSigInit - initialize for catching signals
*
* DESCRIPTION
*	Set up for catching signals.
*
* RETURNS
*	void
*
* BUGS
* o	it isn't clear how multiple calls fit in with SunOS lwp library
*
* NOTES
* 1.	Under VxWorks, assert() generates SIGUSR1, rather than the
*	customary UNIX SIGABRT.
*
* EXAMPLE
*    #include <setjmp.h>
*    #include <signal.h>	(or <sigLib.h> for VxWorks)
*
*    jmp_buf  mySigEnv;
*
*    void myHandler(sigNum)
*    int   sigNum;
*    {
*	signal(sigNum, SIG_DFL);
*	longjmp(mySigEnv, sigNum);
*    }
*
*    main()
*    {
*	int   sigNum;
*
*	genSigInit(myHandler);
*	if ((sigNum = setjmp(mySigEnv)) != 0)
*	    goto sigOccurred;
*	.
*	.
*	exit(0);
*    sigOccurred:
*	printf("signal %d detected\n", sigNum);
*	exit(1);
*    }
*	
*-*/
void
genSigInit(handler)
void	(* handler)();	/* I pointer to signal handler */
{
    (void)signal(SIGTERM, handler);	/* SunOS plain kill (not -9) */
    (void)signal(SIGQUIT, handler);	/* SunOS ^\ */
    (void)signal(SIGINT, handler);	/* SunOS ^C */
    (void)signal(SIGILL, handler);	/* illegal instruction */
#ifndef vxWorks
    (void)signal(SIGABRT, handler);	/* SunOS assert */
#else
    (void)signal(SIGUSR1, handler);	/* VxWorks assert */
#endif
    (void)signal(SIGBUS, handler);	/* bus error */
    (void)signal(SIGSEGV, handler);	/* segmentation violation */
    (void)signal(SIGFPE, handler);	/* arithmetic exception */
    (void)signal(SIGPIPE, handler);	/* write to disconnected socket */
}

#ifdef vxWorks
/*+/subr**********************************************************************
* NAME	perror - print message corresponding to errno
*
* DESCRIPTION
*	Under VxWorks, provides the capability provided by perror() under
*	UNIX.  (This routine isn't present in genLib except for VxWorks
*	version.)
*
*-*/
perror(str)
char    *str;	/* I string to print in conjunction with error message */
{
    if (str != NULL)
	(void)printf("%s: ", str);
    printErrno(0);
}
#endif

int doMatch();

/*+/subr**********************************************************************
* NAME	wildMatch - do wildcard matching on strings
*
* DESCRIPTION
*	Search a text string for a match of a pattern, where the pattern
*	can contain wildcard characters.
*
*	The wildcard characters and their meanings are:
*	? matches any single character
*	* matches any sequence of zero or more characters
*
* RETURNS
*	1	if a match is obtained, or
*	0
*
*-*/
int
wildMatch(text, p, ignoreCase)
char	*text;		/* I the text to be checked */
char	*p;		/* I the pattern, possibly with wildcards */
int	ignoreCase;	/* I 0,1 to honor,ignore upper/lower case differences */
{
    char	*starP=NULL;	/* pointer at beginning of * wildcard */
    char	*starText=NULL;	/* pointer at beginning of * wildcard */
    int		matched;	/* char in text matches char in pattern */
    char	a, b;

    if (*text == '\0')
	return 0;
    while (*p != '\0' && *text != '\0') {
	switch (*p) {
	    case '*':
		starP = ++p;
		if (*p == '\0')
		    return 1;
		starText = text;
		break;
	    default:
		matched = (*text == *p || *p == '?') ? 1 : 0;
		if (ignoreCase && !matched) {
		    a = *text; b = *p;
		    if (isascii(a) && isascii(b) && isalpha(a) && isalpha(b)) {
			if (isupper(a)) a = tolower(a);
			if (isupper(b)) b = tolower(b);
			if (a == b)
			    matched = 1;
		    }
		}
		if (matched) {
		    text++, p++;
		    if (*p == '\0') {
			if (*text == '\0' || *(p-1) == '*')
			    return 1;
			text = ++starText;
			if (starP == NULL)
			    return 0;
			p = starP;
		    }
		}
		else if (starP == NULL)
		    return 0;
		else {
		    text = ++starText;
		    p = starP;
		}
		break;
	}
    }
    if (*text == '\0') {
	if (*p == '\0')
	    return 1;
	if (*p != '*')
	    return 0;
	if (*(++p) == '\0')
	    return 1;
    }
    return 0;
}
