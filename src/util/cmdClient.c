/*	$Id$
 *	Author:	Roger A. Cole
 *	Date:	11-26-90
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
 * .01	11-26-90	rac	initial version
 * .02	07-30-91	rac	installed in SCCS
 * .03  09-11-91	joh	updated for v5 vxWorks
 * .03	11-07-91	rac	be more forgiving of unexpected interactions
 * .04	12-06-91	rac	total rewrite, Unix-only, no LWP
 *
 * make options
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	cmdClient.c - general purpose client for command-based servers
*
* DESCRIPTION
*	Connects to a text-command-based server.
*
*	Usage on SunOS:
*		% cmdClient hostName portNum
*	    or
*		execl("cmdClient", "cmdClient", "hostName", "portNum",
*								(char *)0);
*
*	Usage on VxWorks:
*		> cmdClient "hostName",portNum
*
* BUGS
* o	the stdout stream from this program contains, intermixed, the
*	server's stdout and stderr streams
* o	not all signals are caught
*-***************************************************************************/
#include <genDefs.h>
#include <tsDefs.h>
#include <ezsSockSubr.h>

#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/time.h>



/*-----------------------------------------------------------------------------
* prototypes
*----------------------------------------------------------------------------*/
int cmdcl();
void cmdclCmdProcess();
char *cmdclInTask();
long cmdclTask();
void cmdclTaskSigHandler();

/*-----------------------------------------------------------------------------
* global definitions
*----------------------------------------------------------------------------*/

jmp_buf	sigEnv;		/* environment for longjmp at signal time */

main(argc, argv)
int     argc;           /* number of command line args */
char   *argv[];         /* command line args */
{
    char	*hostName;	/* host name for server */
    int		portNum;	/* port number for server */

    if (argc != 3)		/* must be command and 2 args */
	goto mainUsage;
    hostName = argv[1];
    if (sscanf(argv[2], "%d", &portNum) != 1) {
	printf("error scanning port number\n");
	goto mainUsage;
    }

    return cmdClient(hostName, portNum);

mainUsage:
    printf("Usage: %s serverHostName serverPortNumber\n", argv[0]);
    return -1;
}

/*+/subr**********************************************************************
* NAME	cmdClient - shell callable interface for cmdClient
*
*-*/
int
cmdClient(hostName, portNum)
char	*hostName;	/* I host name for server */
int	portNum;	/* I port number for server */
{
    long	stat;		/* status return from calls */
    int		serverSock=-1;	/* socket connected to server */
    char	serverBuf[80];
    char	*pBuf;
    FILE	*myIn=NULL;
    FILE	*myOut=NULL;
    char	*findNl;
    char	message[80];
    int		i;
    char	keyboard[80];	/* line from keyboard */
    int		eofFlag=0;
    int		discardNL=0;

    genSigInit(cmdclTaskSigHandler);

    if (setjmp(sigEnv) != 0)
	goto cmdclTaskWrapup;

/*-----------------------------------------------------------------------------
*    attempt to connect to the server.  If the connection is successful,
*    print the message returned by the server.  Then open two streams to
*    the socket, one for characters from keyboard to server, the other
*    for characters from server to stdout.
*----------------------------------------------------------------------------*/
    if ((i=ezsConnectToServer(&serverSock, portNum, hostName, serverBuf)) < 0) {
	if (i == -1) {
	    (void)sprintf(message, "connect to %s port:%d", hostName, portNum);
	    perror(message);
	}
	else
	    printf("%s\n", serverBuf);
	serverSock = -1;
	goto cmdclTaskWrapup;
    }
    printf("%s\n", serverBuf);
    if (ezsFopenToFd(&myIn, &serverSock) < 0) {
	perror("myIn");
	myIn = NULL;
	goto cmdclTaskWrapup;
    }
    setbuf(myIn, NULL);
    if (ezsFopenToFd(&myOut, &serverSock) < 0) {
	perror("myOut");
	myOut = NULL;
	goto cmdclTaskWrapup;
    }

/*----------------------------------------------------------------------------
*    "processing loop"
*	do the interactions with the server, attempting as much as possible
*	to show the messages as they come in from the server.
*---------------------------------------------------------------------------*/
    while (!eofFlag) {
        if (cmdclInTask(keyboard) != NULL) {
	    fputs(keyboard, myOut);
	    fflush(myOut);
	}

	if (ezsCheckFpRead(myIn)) {
	    int		i, c;

	    i = 0;
	    while (ezsCheckFpRead(myIn)) {
		if ((c = fgetc(myIn)) == EOF) {
		    eofFlag++;
		    break;
		}
		serverBuf[i++] = c;
		if (i >= 79 || c == '\n')
		    break;
	    }
	    pBuf = serverBuf;
	    serverBuf[i] = '\0';
	    if (i > 0) {
		char	*findChr;
		if ((findChr = index(pBuf, '#')) != NULL) {
		    while (pBuf != findChr) {
			putchar(*pBuf);
			pBuf++;
		    }
		    if (strncmp(pBuf, "#p#r#", 5) == 0) {
			pBuf += 5;
			discardNL = 1;
		    }
		}
		else
		    pBuf = serverBuf;
		if (discardNL) {
		    if ((findNl = index(pBuf, '\n')) != NULL) {
			*findNl = '\0';
			discardNL = 0;
		    }
		}
		fputs(pBuf, stdout);
		fflush(stdout);
	    }
	    else {
		if (strncmp(keyboard, "quit", 4) == 0)
		    break;
		else if (strncmp(keyboard, "close", 5) == 0)
		    break;
		else
		    printf("null message from server\n");
	    }
	}
	usleep(100000);
    }

cmdclTaskWrapup:
    if (myIn != NULL)
	fclose(myIn);
    if (myOut != NULL)
	fclose(myOut);
    if (serverSock >= 0)
	close(serverSock);

    return 0;
}

/*+/subr**********************************************************************
* NAME	cmdclTaskSig - signal handling and initialization
*
* DESCRIPTION
*	These routines set up for the signals to be caught for cmdclTask
*	and handle the signals when they occur.
*
* RETURNS
*	void
*
* BUGS
* o	not all signals are caught
* o	it's not clear how useful it is to catch the signals which originate
*	from the keyboard
*
*-*/
void
cmdclTaskSigHandler(signo)
int	signo;
{
    printf("entered cmdclTaskSigHandler for signal:%d\n", signo);
    signal(signo, SIG_DFL);
    longjmp(sigEnv, 1);
}

/*+/subr**********************************************************************
* NAME	cmdclInTask - handle keyboard input
*
* DESCRIPTION
*	Checks to see if input is available and reads it, if so.  If
*	there is EOF, then question is asked whether to shut down the
*	server.
*
* RETURNS
*	char *, or
*	NULL
*
*-*/
char *
cmdclInTask(buf)
char	*buf;
{
    char	*input;
    char	message[80];
    fd_set      fdSet;          /* set of fd's to watch with select */
    int         fdSetWidth;     /* width of select bit mask */
    struct timeval fdSetTimeout;/* timeout interval for select */

    fdSetWidth = getdtablesize();
    fdSetTimeout.tv_sec = 0;
    fdSetTimeout.tv_usec = 0;
    FD_ZERO(&fdSet);
    FD_SET(fileno(stdin), &fdSet);

    if (select(fdSetWidth, &fdSet, NULL, NULL, &fdSetTimeout) == 0)
	return NULL;
    if (fgets(buf, 80, stdin) == NULL) {
	strcpy(buf, "quit\n");
	clearerr(stdin);
    }
    if (strncmp(buf, "quit", 4) == 0) {
	(void)printf("stop the server (y/n) ? ");
	if (fgets(message, 80, stdin) == NULL) {
	    strcpy(message, "y");
	}
	if (message[0] == 'y' || message[0] == 'Y')
	    strcpy(buf, "quit\n");
	else
	    strcpy(buf, "\n");
    }

    return buf;
}
