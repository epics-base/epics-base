/* share/src/util $Id$
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
 * .01 10-24-90 rac	initial version
 * .02 07-30-91 rac	installed in SCCS; changed to use EPICS_ENV..
 * .03 09-11-91 joh	updated for v5 vxWorks
 * .04 12-08-91 rac	rewrite for Unix-only, without LWP
 *
 * make options
 *	-DNDEBUG	don't compile assert() checking
 */
/*+/mod***********************************************************************
* TITLE	cmdProto - prototype for a multi-threaded program with command interface
*
* DESCRIPTION
*	This skeleton can be used as the basis for a multi-threaded program
*	which interfaces to a user with text strings, as from a keyboard.
*	Several generic capabilities are provided:
*
*	o   scanning of input control lines
*	o   re-directing of input to come from a file
*	o   running as a stand-alone program, or as a server or client,
*	    with socket-based communications
*	o   on-line help
*	o   handling cleanup on exception conditions (such as bus error)
*
*	To run as a server, specify "server" as a command line option.
*
*	To run as a client, specify "hostName" as a command line option.
*
*	Under SunOS, running under dbx doesn't allow testing the error
*	handling features of the code--the debugger catches the signals.
*	To do such testing, run without dbx.
*
* To use this skeleton:
*
*	Change Zzz, zzz, and ZZZ to a desired name
*	Customize a context block
*	Remove the "server" items, if they aren't going to be used
*	Add additional commands in zzzCmdCrunch
*	Add additional help info in zzzInitAtStartup.  Usually, this
*		will mean adding to helpCmdsSpec and helpUsage.  For
*		any commands needing an individual help topic, then a
*		specific HELP_TOPIC will have to be defined.
*	If you're using sockets, assign a port number in <ports.h>, and
*		change glZzzIPPort to the IPPORT_xxx you've chosen.
*
*	SunOS
*	o   link with genLib.a
*
* BUGS
* o	this program should use tasks.h to establish priorities, stack
*	sizes, etc.
* o	not all signals are caught
* o	using this skeleton, it isn't possible to have several invocations
*	of the program running simultaneously
*
*-***************************************************************************/
#include <genDefs.h>
#include <cmdDefs.h>
#include <envDefs.h>
#include <ezsSockSubr.h>
#include <cadef.h>

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <setjmp.h>


/*/subhead ZZZ_CTX-------------------------------------------------------
* ZZZ_CTX - context information
*
*	A zzz descriptor is the `master handle' which is used for
*	handling business.
*----------------------------------------------------------------------------*/

typedef struct zzzCtx {
    jmp_buf     sigEnv;         /* environment for longjmp at signal time */
    int		listenSock;	/* fd for server's "listen" socket */
    int		inUse;		/* 1 says "presently engaged" by a client */
    int		inUseInit;	/* 1 says initialized */
    char	*inUseName;	/* name of client's host */
    char        *oldPrompt;	/* prompt prior to socket connect */
    int		stop;		/* 1 says stop operations */
    int		clientSock;	/* fd for socket to client */
    int         reopenIO;       /* std... should be re-directed */
    int         reopenIOoption; /* 0,1,2; option for arFreopenAllStd */
    int		oldStdinFd;	/* original fd for stdin, before redirect */
    int		oldStdoutFd;	/* original fd for stdout, before redirect */
    int		oldStderrFd;	/* original fd for stderr, before redirect */
} ZZZ_CTX;

/*-----------------------------------------------------------------------------
* prototypes
*----------------------------------------------------------------------------*/
int zzz();
void zzzCmdCrunch();
void zzzListenCheck();
void zzzListenSetReopen();
void zzzSigHandler();
long zzzInitAtStartup();

/*-----------------------------------------------------------------------------
* global definitions
*----------------------------------------------------------------------------*/
static CX_CMD	glZzzCxCmd;
static CX_CMD	*pglZzzCxCmd=NULL;
static ZZZ_CTX	glZzzCtx;
static ZZZ_CTX	*pglZzzCtx=NULL;
static char	*glZzzId="zzz 11/27/90";
static int	glZzzIPPort;
static int	serverMode=0;
static chid	pCh;


/*+/subr**********************************************************************
* NAME	main
*
* DESCRIPTION
*	This program is a prototype skeleton for building programs
*	which interact with a keyboard/window environment.  The program
*	can be started with any of:
*
*	% cmdProto		(runs as normal interactive program)
*	% cmdProto server	(runs as a server)
*	% cmdProto server&	(runs as a server)
*	% cmdProto hostName	(runs as a client to cmdProto on hostName)
*
*-*/
main(argc, argv)
int     argc;		/* number of command line args */
char   *argv[];		/* command line args */
{
    long	stat;		/* status return from calls */
    char	portText[10];
    int		sigNum;
    int		gooseOutput=0;

    setbuf(stdout, NULL);
    pglZzzCtx = &glZzzCtx;
    pglZzzCxCmd = &glZzzCxCmd;

    stat = zzzInitAtStartup(pglZzzCtx, pglZzzCxCmd);
    assertAlways(stat == OK);

    if (argc > 2) {
	printf("Usage: %s [server or hostName]\n", argv[0]);
	exit(1);
    }
    else if (argc == 2) {
	if (envGetConfigParam(&EPICS_CMD_PROTO_PORT, 10, portText) == NULL) {
	    printf("error getting %s\n", EPICS_CMD_PROTO_PORT.name);
	    return ERROR;
	}
	sscanf(portText, "%d", &glZzzIPPort);
	if (strcmp(argv[1], "server") != 0) {
	    execlp("cmdClient", "cmdClient", argv[1], portText, (char *)0);
	    (void)printf("couldn't exec to cmdClient\n");
	    return ERROR;
	}
	serverMode = 1;
	pglZzzCtx->inUseName = argv[1];
/*-----------------------------------------------------------------------------
*	establish a socket to use to listen for potential clients to connect
*----------------------------------------------------------------------------*/
	if (ezsCreateListenSocket(&pglZzzCtx->listenSock, glZzzIPPort) != 0) {
	    perror("create listen socket");
	    exit(1);
	}
    }
    else
	pglZzzCtx->inUseName = "shell";

#if 1
    stat = ca_task_initialize();
    assert(stat == ECA_NORMAL);
#endif

    genSigInit(zzzSigHandler);
    if ((sigNum = setjmp(pglZzzCtx->sigEnv)) != 0) {
	(void)printf("zzzTask signal received: %d\n", sigNum);
	goto zzzTaskWrapup;
    }

    while (!pglZzzCtx->stop) {
	if (serverMode)
	    zzzListenCheck(&pglZzzCxCmd);
	if (!serverMode || pglZzzCtx->inUse) {
#if 0
	    if (gooseOutput == 0)
		gooseOutput = 30 * pglZzzCxCmd->promptFlag;
#endif
	    if (cmdRead(&pglZzzCxCmd) != NULL) {
		if (serverMode && pglZzzCxCmd->inputEOF) {
		    pglZzzCxCmd->pCommand = "close";
		    pglZzzCxCmd->inputEOF = 0;
		}
		zzzCmdCrunch(&pglZzzCxCmd, pglZzzCtx);
	    }
#if 0
	    if (gooseOutput && serverMode && pglZzzCtx->inUse) {
		if (gooseOutput % 2 == 0) {
		    (void)printf("\001");
		}
		gooseOutput--;
	    }
#endif
	}
#if 1
	stat = ca_pend_event(0.001);
	assert(stat != ECA_EVDISALLOW);
#endif
        fflush(stdout);         	/* flushes needed for socket I/O */
        fflush(stderr);
        fflush(pglZzzCxCmd->dataOut);
	usleep(100000);		/* sleep .1 second */
    }

    return OK;

zzzTaskWrapup:
    if (pglZzzCtx->reopenIO) {
	(void)zzzFreopenAllStd(pglZzzCtx,pglZzzCtx->reopenIOoption);
	pglZzzCtx->reopenIO = 0;
    }
    zzzListenQuit(&pglZzzCxCmd);

    /* put code for cleanup here */

#if 1
    stat = ca_task_exit();
    if (stat != ECA_NORMAL)
	(void)printf("zzz: ca_task_exit error: %s\n", ca_message(stat));
#endif

    if (pglZzzCxCmd->dataOutRedir)
	fclose(pglZzzCxCmd->dataOut);

    fflush(stdout);		/* flushes needed for sockets */
    fflush(stderr);
}

/*+/subr**********************************************************************
* NAME	zzzSigHandler - signal handling
*
* DESCRIPTION
*	These routines set up for the signals to be caught for zzzTask
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
static void
zzzSigHandler(signo)
int	signo;
{
    if (signo == SIGPIPE) {
	if (serverMode)
	    zzzListenClose(&pglZzzCxCmd);
	return;
    }
    printf("entered zzzSigHandler for signal:%d\n", signo);
    signal(signo, SIG_DFL);
    longjmp(pglZzzCtx->sigEnv, signo);
}

/*+/subr**********************************************************************
* NAME	zzzCmdCrunch - process a command line
*
* DESCRIPTION
*
* RETURNS
*	void
*
* BUGS
* o	text
*
*-*/
static void
zzzCmdCrunch(ppCxCmd, pZzzCtx)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
ZZZ_CTX *pZzzCtx;	/* IO pointer to zzz context */
{
    CX_CMD	*pCxCmd;	/* local copy of pointer, for convenience */

    pCxCmd = *ppCxCmd;
    if (strcmp(pCxCmd->pCommand,			"assert0") == 0) {
/*-----------------------------------------------------------------------------
*	under SunOS, this generates SIGABRT
*----------------------------------------------------------------------------*/
	assertAlways(0);
    }
    else if (strcmp(pCxCmd->pCommand,			"quit") == 0) {
	pZzzCtx->stop = 1;
	if (serverMode)
	    zzzListenClose(ppCxCmd);
    }
    else if (strcmp(pCxCmd->pCommand,			"trap") == 0) {
	int	*j;
	j = (int *)(-1);
	j = (int *)(*j);
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"close") == 0) {
	if (*ppCxCmd != (*ppCxCmd)->pCxCmdRoot)
	    (void)printf("close illegal in source'd file\n");
	else if (strcmp(pglZzzCtx->inUseName, "shell") == 0)
	    (void)printf("close illegal--zzz not running as server\n");
	else if (serverMode) {
	    zzzListenClose(ppCxCmd);
	}
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"dataOut") == 0) {
	if ((*ppCxCmd)->dataOutRedir)
	    fclose ((*ppCxCmd)->dataOut);
	(*ppCxCmd)->dataOutRedir = 0;
	if (nextNonSpaceField( &(*ppCxCmd)->pLine, &(*ppCxCmd)->pField,
						&(*ppCxCmd)->delim) < 1)
	    (*ppCxCmd)->dataOut = stdout;
	else {
	    (*ppCxCmd)->dataOut = fopen((*ppCxCmd)->pField, "a");
	    if ((*ppCxCmd)->dataOut == NULL) {
		(void)printf("couldn't open %s\n", (*ppCxCmd)->pField);
		(*ppCxCmd)->dataOut = stdout;
		(*ppCxCmd)->dataOutRedir = 0;
	    }
	    else
		(*ppCxCmd)->dataOutRedir = 1;
	}
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"search") == 0) {
	ca_search("jjj", &pCh);
	ca_pend_io(.001);
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"clear") == 0) {
	ca_clear_channel(pCh);
	ca_pend_io(.001);
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"server") == 0) {
	(void)printf( "\"server\" legal only on command line\n");
	(void)printf( "use   help usage   for more information\n");
    }
    else if (strcmp((*ppCxCmd)->pCommand,		"source") == 0) {
	cmdSource(ppCxCmd);
    }
    else {
/*----------------------------------------------------------------------------
* help (or illegal command)
*----------------------------------------------------------------------------*/
	(void)nextNonSpaceField(&pCxCmd->pLine, &pCxCmd->pField,
							&pCxCmd->delim);
	helpIllegalCommand(stdout, &pCxCmd->helpList, pCxCmd->pCommand,
							pCxCmd->pField);
    }

    return;
}

/*+/subr**********************************************************************
* NAME	zzzListenCheck - listen for client connections
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	cleanup of listen socket isn't dealt with.  Under SunOS, the system
*	seems to clean up OK.
*
*	In fact, under SunOS, shutdown and/or close seem to inhibit proper
*	cleanup by the system.
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
void
zzzListenCheck(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    long	stat;
    char	clientName[30];
    static char	sockPrompt[80];

/*-----------------------------------------------------------------------------
*	check to see if a connect attempt has been made.  If the server is
*	already in use by a client, connect attempts will be rejected.
*----------------------------------------------------------------------------*/
    if (ezsListenExclusiveUse(&pglZzzCtx->clientSock,
			    &pglZzzCtx->listenSock, &pglZzzCtx->inUse,
			    clientName, glZzzId) < 0) {
	perror("check for connect attempt");
	exit(1);
    }

    if (pglZzzCtx->inUse && pglZzzCtx->inUseInit == 0) {
/*-----------------------------------------------------------------------------
*	When a connect attempt succeeds, change stdin and stdout to use
*	the socket, so that server I/O (i.e., I/O for this program) will
*       automatically be re-directed to the client.  This must be done
*	for all tasks in this program.
*
*	The prompt is changed to a special prompt which ends witn \n.  This
*	is done to avoid buffer flushing problems on some operating systems.
*	A special prefix of "#p#r#" is added to the normal prompt so that
*	the client knows that some massaging needs to be done before
*	displaying the prompt to the user.
*----------------------------------------------------------------------------*/
	(void)printf("%s connected\n", clientName);
	if (zzzFreopenAllStd(pglZzzCtx, 1) != OK)
	    assertAlways(0);
	pglZzzCtx->oldPrompt = (*ppCxCmd)->prompt;
	assert(strlen(pglZzzCtx->oldPrompt) < 72);
	(void)sprintf(sockPrompt, "#p#r#%s\n", pglZzzCtx->oldPrompt);
	(*ppCxCmd)->prompt = sockPrompt;
	zzzListenSetReopen(2);	/* reopen all other tasks to socket */
	pglZzzCtx->inUseInit = 1;
	(*ppCxCmd)->promptFlag = 1;
#if 0
	stat = zzzStartup(serverMode, clientName);
	assertAlways(stat == OK);
#endif
    }

}

/*+/subr**********************************************************************
* NAME	zzzListenClose
*
*-*/
long
zzzListenClose(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    if (pglZzzCtx->inUse) {
#if 0
	shutdown(pglZzzCtx->clientSock, 2);
#endif
	close(pglZzzCtx->clientSock);
	pglZzzCtx->clientSock = -1;
	pglZzzCtx->inUse = 0;
	pglZzzCtx->inUseInit = 0;
	if (zzzFreopenAllStd(pglZzzCtx, 0) != OK)
	    assertAlways(0);
	(void)printf("%s disconnected\n", pglZzzCtx->inUseName);
	(*ppCxCmd)->prompt = pglZzzCtx->oldPrompt;
	zzzListenSetReopen(0);	/* reopen all other tasks to std... */
    }
}

/*+/subr**********************************************************************
* NAME	zzzListenQuit
*
*-*/
long
zzzListenQuit(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    if (pglZzzCtx->clientSock != -1) {
	shutdown(pglZzzCtx->clientSock, 2);
	close(pglZzzCtx->clientSock);
    }
#if 0
    shutdown(pglZzzCtx->listenSock, 2);
    close(pglZzzCtx->listenSock);
#endif
    return OK;
}

/*+/subr**********************************************************************
* NAME	zzzListenSetReopen
*
*-*/
void
zzzListenSetReopen(option)
int	option;		/* I option for reopen for use with zzzFreopenAllStd */
{
    pglZzzCtx->reopenIOoption = option;
    pglZzzCtx->reopenIO = 1;
}

/*+/subr**********************************************************************
* NAME	zzzFreopenAllStd - reopen stdin, stdout, and stderr
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR	(an error message has been printed)
*
* BUGS
* o	text
*
*-*/
static int
zzzFreopenAllStd(pCtx, option)
ZZZ_CTX *pCtx;		/* IO pointer to zzz context */
int	option;		/* I activity selector:
				0  set back to use old fd's
				1  use client socket, save old fd's
				2  use client socket, don't save old fd's */
{
    fflush(stdout);
    fflush(stderr);
    if (option == 0) {
	if (ezsFreopenToFd(stdin, &pCtx->oldStdinFd, NULL) < 0) {
	    perror("restore stdin");
	    return ERROR;
	}
	if (ezsFreopenToFd(stdout, &pCtx->oldStdoutFd, NULL) < 0) {
	    perror("restore stdout");
	    return ERROR;
	}
	if (ezsFreopenToFd(stderr, &pCtx->oldStderrFd, NULL) < 0) {
	    perror("restore stderr");
	    return ERROR;
	}
    }
    else if (option == 1) {
	if (ezsFreopenToFd(stdin, &pCtx->clientSock, &pCtx->oldStdinFd) < 0) {
	    perror("reopen stdin");
	    return ERROR;
	}
	if (ezsFreopenToFd(stdout, &pCtx->clientSock, &pCtx->oldStdoutFd) < 0) {
	    perror("reopen stdout");
	    return ERROR;
	}
	if (ezsFreopenToFd(stderr, &pCtx->clientSock, &pCtx->oldStderrFd) < 0) {
	    perror("reopen stderr");
	    return ERROR;
	}
    }
    else if (option == 2) {
	if (ezsFreopenToFd(stdin, &pCtx->clientSock, NULL) < 0) {
	    perror("reopen stdin");
	    return ERROR;
	}
	if (ezsFreopenToFd(stdout, &pCtx->clientSock, NULL) < 0) {
	    perror("reopen stdout");
	    return ERROR;
	}
	if (ezsFreopenToFd(stderr, &pCtx->clientSock, NULL) < 0) {
	    perror("reopen stderr");
	    return ERROR;
	}
    }
    return OK;
}

/*+/subr**********************************************************************
* NAME	zzzInitAtStartup - initialization for zzz
*
* DESCRIPTION
*	Perform several initialization duties:
*	o   initialize the zzz context
*	o   initialize the command context block
*	o   initialize the help information
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	text
*
*-*/
static long
zzzInitAtStartup(pZzzCtx, pCxCmd)
ZZZ_CTX	*pZzzCtx;
CX_CMD	*pCxCmd;
{
    /* code to initialize zzz context here */
    pZzzCtx->inUse = 0;
    pZzzCtx->listenSock = -1;
    pZzzCtx->clientSock = -1;
    pZzzCtx->oldStdinFd = -1;
    pZzzCtx->oldStdoutFd = -1;
    pZzzCtx->oldStderrFd = -1;

    cmdInitContext(pCxCmd, "  zzz:  ");

/*-----------------------------------------------------------------------------
* help information initialization
*----------------------------------------------------------------------------*/
    helpInit(&pCxCmd->helpList);
/*-----------------------------------------------------------------------------
* help info--generic commands
*----------------------------------------------------------------------------*/
	helpTopicAdd(&pCxCmd->helpList, &pCxCmd->helpCmds, "commands", "\n\
Generic commands are:\n\
   bg\n\
   close                     (use    help usage   for more info)\n\
   dataOut    [filePath]     (default is to normal output)\n\
   help       [topic]\n\
   quit (or ^D)              (use    help usage   for more info)\n\
   source     filePath\n\
");
/*-----------------------------------------------------------------------------
* help info--bg command
*----------------------------------------------------------------------------*/
    helpTopicAdd(&pCxCmd->helpList, &pCxCmd->helpBg, "bg", "\n\
Under SunOS, no bg command is directly available.  Instead, if zzz\n\
is being run from the C shell (csh), it can be put in the background\n\
using several steps from the keyboard (with the first % on each line being\n\
the prompt from csh):\n\
	type ^Z, then type\n\
	% bg %zzz\n\
\n\
To move zzz to the foreground, type\n\
	% fg %zzz\n\
");
/*-----------------------------------------------------------------------------
* help info--zzz-specific commands
*----------------------------------------------------------------------------*/
	helpTopicAdd(&pCxCmd->helpList, &pCxCmd->helpCmdsSpec, "commands", "\n\
zzz-specific commands are:\n\
   abcdef\n\
\n\
Output from commands flagged with * can be routed to a file by using the\n\
\"dataOut filePath\" command.  The present contents of the file are\n\
preserved, with new output being written at the end.\n\
");
/*-----------------------------------------------------------------------------
* help info--zzz usage information
*----------------------------------------------------------------------------*/
	helpTopicAdd(&pCxCmd->helpList, &pCxCmd->helpUsage, "usage", "\n\
zzz performs some functions.  This is a skeleton program.\n\
\n\
For information on moving zzz into the background, use the \"help bg\"\n\
command.\n\
\n\
zzz can be run as a server by:\n\
    under SunOS:    % zzz server\n\
\n\
zzz can be run as a client connecting to a zzz server on \"hostName\" by:\n\
    under SunOS:    % zzz hostName\n\
\n\
    When running zzz as a client, two commands are of special interest:\n\
\n\
    close	shuts down the client but leaves the server running\n\
    quit	shuts down both the client and the server\n\
");
    return OK;
}
