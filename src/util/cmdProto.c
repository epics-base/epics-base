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
 * .01	10-24-90	rac	initial version
 * .02	07-30-91	rac	installed in SCCS; changed to use EPICS_ENV..
 * .03  09-11-91	joh	updated for v5 vxWorks
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
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
*	o   under VxWorks, "running in the background" using "bg"
*	o   handling cleanup on exception conditions (such as bus error)
*
*	This skeleton runs under either SunOS (using the lightweight
*	process library) or VxWorks.  Under SunOS, the "bg" command
*	isn't functional--the C shell "bg" and "fg" commands must be used
*	instead.
*
*	To run as a server, specify "server" as a command line option.
*
*	To run as a client, specify "hostName" as a command line option.
*
*	For use under VxWorks, this skeleton "catches" most errors which
*	ordinarily result in just suspending the task.  The result is that
*	this skeleton will wrapup and exit rather than going into "suspended
*	animation".
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
*	Add additional tasks, as necessary, using the scheme supplied
*	Add additional commands in zzzTaskCmdCrunch
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
*	VxWorks
*	o   ioc must have genLib.vxa loaded
*
* BUGS
* o	this program should use tasks.h to establish priorities, stack
*	sizes, etc.
* o	not all signals are caught
* o	using this skeleton, it isn't possible to have several invocations
*	of the program running simultaneously
* o	under VxWorks, it would be possible to use taskDeleteHookAdd() as
*	an additional mechanism for invoking cleanup code.  (There is a
*	corresponding routine in the SunOS LWP library.)  This mechanism
*	isn't used in this skeleton, in large part because most error
*	conditions under VxWorks result in suspending the task rather than
*	deleting it.
*
*-***************************************************************************/
#include <genDefs.h>
#include <genTasks.h>
#include <cmdDefs.h>
#include <envDefs.h>
#include <ezsSockSubr.h>
#include <cadef.h>

#ifdef vxWorks
/*----------------------------------------------------------------------------
*    includes and defines for VxWorks compile
*---------------------------------------------------------------------------*/
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <ctype.h>
#   include <sigLib.h>
#   include <setjmp.h>
#   include <taskLib.h>
#   define MAXPRIO 160
#else
/*----------------------------------------------------------------------------
*    includes and defines for Sun compile
*---------------------------------------------------------------------------*/
#   include <stdio.h>
#   include <ctype.h>
#   include <signal.h>
#   include <setjmp.h>
#   define MAXPRIO 50
#endif


/*/subhead ZZZ_CTX-------------------------------------------------------
* ZZZ_CTX - context information
*
*	A zzz descriptor is the `master handle' which is used for
*	handling business.
*----------------------------------------------------------------------------*/
#define ZzzLock semTake(pglZzzCtx->semLock, WAIT_FOREVER)
#define ZzzUnlock semGive(pglZzzCtx->semLock)
#define ZzzLockCheck semClear(pglZzzCtx->semLock)
#define ZzzLockInitAndLock semInit(pglZzzCtx->semLock)

typedef struct {
    TASK_ID	id;		/* ID of task */
    int		status;		/* status of task--initially OK */
    int		stop;		/* task requested to stop if != 0 */
    int		stopped;	/* task has stopped if != 0 */
    int		serviceNeeded;	/* needs servicing by another task */
    int		serviceDone;	/* servicing by other completed */
    int		reopenIO;	/* task's std... should be re-directed */
    int		reopenIOoption;/* 0,1,2; option for zzzFreopenAllStd() */
    jmp_buf	sigEnv;		/* environment for longjmp at signal time */
} ZZZ_TASK_INFO;

typedef struct zzzCtx {
    SEM_ID	semLock;
    ZZZ_TASK_INFO zzzTaskInfo;
    ZZZ_TASK_INFO zzzInTaskInfo;
    ZZZ_TASK_INFO zzzListenTaskInfo;
    int		listenSock;	/* fd for server's "listen" socket */
    int		inUse;		/* 1 says "presently engaged" by a client */
    char	*inUseName;	/* name of client's host */
    int		clientSock;	/* fd for socket to client */
    int		oldStdinFd;	/* original fd for stdin, before redirect */
    int		oldStdoutFd;	/* original fd for stdout, before redirect */
    int		oldStderrFd;	/* original fd for stderr, before redirect */
    int		showStack;	/* show stack stats on task terminate */
} ZZZ_CTX;

/*-----------------------------------------------------------------------------
* prototypes
*----------------------------------------------------------------------------*/
int zzz();
void zzzInTask();
void zzzInTaskSigHandler();
void zzzListenTask();
void zzzListenTSetReopen();
void zzzListenTaskSigHandler();
void zzzTask();
void zzzTaskCmdCrunch();
void zzzTaskSigHandler();
long zzzInitAtStartup();
long zzzInitInfo();
long zzzSpawnIfNonexist();
long zzzStartup();

/*-----------------------------------------------------------------------------
* global definitions
*----------------------------------------------------------------------------*/
static CX_CMD	glZzzCxCmd;
static CX_CMD	*pglZzzCxCmd=NULL;
static ZZZ_CTX	glZzzCtx;
static ZZZ_CTX	*pglZzzCtx=NULL;
static char	*glZzzId="zzz 11/27/90";
static int	glZzzIPPort;


#ifndef vxWorks
    main(argc, argv)
    int     argc;		/* number of command line args */
    char   *argv[];		/* command line args */
    {
/*----------------------------------------------------------------------------
*    do some lwp initialization:
*    o  set allowable priorities between 1 and MAXPRIO
*    o  set up a cache of stacks for MAXTHREAD stacks
*
*    A side effect is that this routine is turned into a lwp thread with a
*    priority of MAXPRIO.
*---------------------------------------------------------------------------*/
        (void)pod_setmaxpri(MAXPRIO);
        lwp_setstkcache(100000, 9);

	if (argc > 2) {
	    printf("Usage: %s [server or hostName]\n", argv[0]);
	    exit(1);
	}
	else if (argc == 2)
	    return zzz(argv[1]);
	else
	    return zzz((char *)NULL);
    }
#endif

/*+/subr**********************************************************************
* NAME	zzz - shell callable interface for zzz
*
* DESCRIPTION
*	This routine is the only part of zzz which is intended to be
*	called directly from the shell.  Several functions are performed here:
*	o   in CLIENT mode, exec appropriately to cmdClient
*	o   if the zzzTask doesn't exist, spawn it.  Ideally, this
*	    stage would also detect if the zzzTask is suspended
*	    and take appropriate action.
*	o   if the zzzInTask doesn't exist, spawn it.  If it does exist, an
*	    error exists.
*	o   if the zzzListenTask doesn't exist, spawn it.  If it does exist,
*	    an error exists.
*	o   in NON-SERVER mode, wait until the zzzInTask quits, then return
*	    to the shell.  If other tasks belonging to zzz are being
*	    stopped, then this routine waits until they, too, are stopped
*	    before returning to the shell.
*	o   in SERVER mode, set up for socket-based I/O and return to the
*	    shell.
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	stack size and priority should come from tasks.h
* o	there are lots of "holes" in detecting whether tasks exist, are
*	suspended, etc.
*
*-*/
int
zzz(option)
char	*option;	/* I NULL, "hostName", or "server" */
{
    long	stat;		/* status return from calls */
    int		serverStart=0;	/* 1 says start as server */
    char	portText[10];

    if (envGetConfigParam(&EPICS_CMD_PROTO_PORT, 10, portText) == NULL) {
	printf("error getting %s\n", EPICS_CMD_PROTO_PORT.name);
	return ERROR;
    }
    sscanf(portText, "%d", &glZzzIPPort);

/*-----------------------------------------------------------------------------
* if the option is present and not "server", then run as a client
*----------------------------------------------------------------------------*/
    if (option != NULL) {
	if (strcmp(option, "server") != 0) {
#ifdef vxWorks
	    (void)printf("can't operate as client under VxWorks\n");
#else
	    execl("cmdClient", "cmdClient", option, portText, (char *)0);
	    (void)printf("couldn't exec to cmdClient\n");
#endif
	    return ERROR;
	}
	else
	    serverStart = 1;
    }
    if (pglZzzCxCmd == NULL) {
/*-----------------------------------------------------------------------------
* startup initialization is done only if it's never been done before
*----------------------------------------------------------------------------*/
	pglZzzCtx = &glZzzCtx;
	pglZzzCxCmd = &glZzzCxCmd;
	stat = zzzInitAtStartup(pglZzzCtx, pglZzzCxCmd);
	assertAlways(stat == OK);
    }

    if (!serverStart) {
	ZzzLock;
	if (pglZzzCtx->inUse) {
	    ZzzUnlock;
	    (void)printf("zzz in use from %s\n", pglZzzCtx->inUseName);
	    return ERROR;
	}
	pglZzzCtx->inUse = 1;
	ZzzUnlock;
    }
    stat = zzzStartup(serverStart, "shell");
    assertAlways(stat == OK);

/*-----------------------------------------------------------------------------
*    wait for zzzInTask to exit and then return to the shell.  If other
*    "zzz tasks" are also exiting, wait until their wrapups are complete.
*
*    In server mode, though, just exit.
*----------------------------------------------------------------------------*/
    if (!serverStart) {
	while (!pglZzzCtx->zzzInTaskInfo.stopped)
	    taskSleep(SELF, 1, 0);
	if (pglZzzCtx->zzzTaskInfo.stop) {
	    while (!pglZzzCtx->zzzTaskInfo.stopped)
		taskSleep(SELF, 1, 0);
	}
	if (pglZzzCtx->zzzListenTaskInfo.stop) {
	    while (!pglZzzCtx->zzzListenTaskInfo.stopped)
		taskSleep(SELF, 1, 0);
	}
	pglZzzCxCmd = NULL;
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	zzzTask - main processing task for zzz
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
* RETURNS
*	void
*
* BUGS
* o	text
*
*-*/
void
zzzTask(ppCxCmd)
CX_CMD	**ppCxCmd;
{
    long	stat;
    CX_CMD	*pCxCmd;
    int		sigNum;
    int		i;

    pCxCmd = *ppCxCmd;

    genSigInit(zzzTaskSigHandler);
    if ((sigNum = setjmp(pglZzzCtx->zzzTaskInfo.sigEnv)) != 0) {
	(void)printf("zzzTask signal received: %d\n", sigNum);
	pglZzzCtx->zzzTaskInfo.status = ERROR;
	goto zzzTaskWrapup;
    }

    stat = ca_task_initialize();
    assert(stat == ECA_NORMAL);

/*----------------------------------------------------------------------------
*    "processing loop"
*---------------------------------------------------------------------------*/
    i = 20;
    while (!pglZzzCtx->zzzTaskInfo.stop) {
	stat = ca_pend_event(0.001);
	assert(stat != ECA_EVDISALLOW);
	if (--i <= 0) {
	    (void)printf(".\n");
	    i = 20;
	}
	if (pglZzzCtx->zzzTaskInfo.reopenIO) {
	    (void)zzzFreopenAllStd(pglZzzCtx,
	    			pglZzzCtx->zzzTaskInfo.reopenIOoption);
	    pglZzzCtx->zzzTaskInfo.reopenIO = 0;
	}
	if (pglZzzCtx->zzzInTaskInfo.serviceNeeded) {
	    zzzTaskCmdCrunch(ppCxCmd, pglZzzCtx);
	    pCxCmd = *ppCxCmd;
	    pglZzzCtx->zzzInTaskInfo.serviceNeeded = 0;
	    pglZzzCtx->zzzInTaskInfo.serviceDone = 1;
	}

	fflush(stdout);		/* flushes needed for VxWorks and */
	fflush(stderr);		/* sockets */
	fflush(pCxCmd->dataOut);

	taskSleep(SELF, 0, 100000);	/* wait .1 sec */
    }

zzzTaskWrapup:
    pglZzzCtx->zzzInTaskInfo.stop = 1;
    pglZzzCtx->zzzListenTaskInfo.stop = 1;
    while (pglZzzCtx->zzzInTaskInfo.stopped == 0 ||
	   pglZzzCtx->zzzListenTaskInfo.stopped == 0) {
	taskSleep(SELF, 1);
    }

    /* put code for cleanup here */

    stat = ca_task_exit();
    if (stat != ECA_NORMAL)
	(void)printf("zzz: ca_task_exit error: %s\n", ca_message(stat));

    if (pCxCmd->dataOutRedir) {
	(void)printf("close for dataOut\n");
#if 0
	fclose(pCxCmd->dataOut);
#endif
    }

#ifdef vxWorks
    if (pglZzzCtx->showStack)
	checkStack(pglZzzCtx->zzzTaskInfo.id);
#endif

    pglZzzCtx->zzzTaskInfo.stopped = 1;

    fflush(stdout);		/* flushes needed for VxWorks and */
    fflush(stderr);		/* sockets */

#ifdef vxWorks
    taskDelete(SELF);
#endif
}

/*+/subr**********************************************************************
* NAME	zzzTaskCmdCrunch - process a command line
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
zzzTaskCmdCrunch(ppCxCmd, pZzzCtx)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
ZZZ_CTX *pZzzCtx;	/* IO pointer to zzz context */
{
    CX_CMD	*pCxCmd;	/* local copy of pointer, for convenience */

    pCxCmd = *ppCxCmd;
    if (strcmp(pCxCmd->pCommand,			"assert0") == 0) {
/*-----------------------------------------------------------------------------
*	under SunOS, this generates SIGABRT; for VxWorks, SIGUSR1
*----------------------------------------------------------------------------*/
	assertAlways(0);
    }
    else if (strcmp(pCxCmd->pCommand,			"quit") == 0) {
	pglZzzCtx->zzzInTaskInfo.stop = 1;
	pglZzzCtx->zzzTaskInfo.stop = 1;
#ifndef vxWorks
	pglZzzCtx->zzzListenTaskInfo.stop = 1;
#endif
    }
#ifdef vxWorks
    else if (strcmp(pCxCmd->pCommand,			"showStack") == 0)
	pZzzCtx->showStack = 1;
#endif
    else if (strcmp(pCxCmd->pCommand,			"trap") == 0) {
	int	*j;
	j = (int *)(-1);
	j = (int *)(*j);
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
* NAME	zzzTaskSigHandler - signal handling
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
* o	under VxWorks, taskDeleteHookAdd isn't used
* o	it's not clear how useful it is to catch the signals which originate
*	from the keyboard
*
*-*/
static void
zzzTaskSigHandler(signo)
int	signo;
{
#ifndef vxWorks
    if (signo == SIGPIPE) {
	if (zzzFreopenAllStd(pglZzzCtx, 0) != OK)
	    assertAlways(0);
	if (!pglZzzCtx->zzzInTaskInfo.stop) {
	    (void)printf("client connection broken\n");
	    pglZzzCtx->zzzInTaskInfo.stop = 1;
	}
	return;
    }
#endif
    printf("entered zzzTaskSigHandler for signal:%d\n", signo);
    signal(signo, SIG_DFL);
    longjmp(pglZzzCtx->zzzTaskInfo.sigEnv, signo);
}

/*+/subr**********************************************************************
* NAME	zzzInTask - handle the keyboard and keyboard commands
*
* DESCRIPTION
*	Gets input text and passes the input to zzzTask.
*
*	This task exists for two primary purposes: 1) avoid the possibility
*	of blocking zzzTask while waiting for operator input; and
*	2) allow detaching zzzTask from the keyboard while still
*	allowing zzzTask to run.
*
*	It is important to note that this task does no command processing
*	on zzzTask's behalf--all commands relating to zzzTask
*	specifically are executed in zzzTask's own context.  Some
*	commands are related to getting input--these commands are processed
*	by this task and are never visible to zzzTask.
*
*	This task waits for input to be available from the keyboard.  When
*	an input line is ready, this task does some preliminary processing:
*	o   if the command is "bg", this task wraps itself up and returns.
*	o   if the command is "close", this task wraps itself up and returns.
*	o   if the command is "dataOut", this task sets a new destination
*	    for data output, closing the previous destination, if appropriate,
*	    and opening the new destination, if appropriate.
*	o   if the command is "source", this task pushes down a level in
*	    the command context and begins reading commands from the file.
*	    When EOF occurs on the file, zzzInTask closes the file, pops
*	    to the previous level in the command context, and resumes
*	    reading at that level.
*	o   if the command is "quit" (or ^D), zzzTask is signalled; this
*	    task continues running.  The expectation is that zzzTask
*	    will eventually set this task's .stop flag.
*	o   otherwise, zzzTask is signalled and this task
*	    goes into a sleeping loop until zzzTask signals
*	    that it is ready for the next command.
*
*	Ideally, this task would also support a mechanism for zzzTask
*	to wait explicitly for keyboard input.  An example would be when
*	operator permission is needed prior to taking an action.
*
* RETURNS
*	void
*
* BUGS
* o	doesn't handle errors from zzzFreopenAllStd
* o	doesn't support flexible redirection of output
* o	the implementation of "dataOut" is clumsy
*
*-*/
void
zzzInTask(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    int		readDone=0;
    int		sigNum;

    pglZzzCtx->zzzInTaskInfo.serviceDone = 1;

#ifdef vxWorks
    genSigInit(zzzInTaskSigHandler);
    if ((sigNum = setjmp(pglZzzCtx->zzzInTaskInfo.sigEnv)) != 0) {
	(void)printf("zzzInTask signal received: %d\n", sigNum);
	pglZzzCtx->zzzInTaskInfo.status = ERROR;
	goto zzzInTaskWrapup;
    }
#endif

    while (!pglZzzCtx->zzzInTaskInfo.stop) {

	if (pglZzzCtx->zzzInTaskInfo.reopenIO) {
	    (void)zzzFreopenAllStd(pglZzzCtx,
	    			pglZzzCtx->zzzInTaskInfo.reopenIOoption);
	    pglZzzCtx->zzzInTaskInfo.reopenIO = 0;
	}

	if (pglZzzCtx->zzzInTaskInfo.serviceDone == 1) {
/*-----------------------------------------------------------------------------
*	if processing of the previous input has completed, acquire the next
*	input line.  If EOF is signalled for input (which never happens for
*	a source'd file) and if input is NOT from the keyboard (i.e., input
*	is from a socket), then simulate a close command.
*----------------------------------------------------------------------------*/
	    cmdRead(ppCxCmd, &pglZzzCtx->zzzInTaskInfo.stop);
	    if ((*ppCxCmd)->inputEOF &&
			strcmp(pglZzzCtx->inUseName, "shell") != 0) {
		pglZzzCtx->zzzInTaskInfo.stop = 1;
	    }
	    if (!pglZzzCtx->zzzInTaskInfo.stop)
		readDone = 1;
	}

/*-----------------------------------------------------------------------------
*	When some input is actually received, process it.
*----------------------------------------------------------------------------*/
	if (readDone) {
	    readDone = 0;

	    if (nextANField(&(*ppCxCmd)->pLine, &(*ppCxCmd)->pCommand,
							&(*ppCxCmd)->delim) < 1)
		;	/* ignore blank line (and those with non-AN commands) */
	    else if (strcmp((*ppCxCmd)->pCommand,	"bg") == 0) {
		if (strcmp(pglZzzCtx->inUseName, "shell") != 0)
		    (void)printf("bg allowed only from shell\n");
		else if (cmdBgCheck(*ppCxCmd) == OK)
		    goto zzzInTaskDone;
	    }
	    else if (strcmp((*ppCxCmd)->pCommand,	"close") == 0) {
		if (*ppCxCmd != (*ppCxCmd)->pCxCmdRoot)
		    (void)printf("close illegal in source'd file\n");
		else if (strcmp(pglZzzCtx->inUseName, "shell") == 0)
		    (void)printf("close illegal--zzz not running as server\n");
		else
		    pglZzzCtx->zzzInTaskInfo.stop = 1;
	    }
	    else if (strcmp((*ppCxCmd)->pCommand,	"dataOut") == 0) {
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
		    }
		    else
			(*ppCxCmd)->dataOutRedir = 1;
		}
	    }
	    else if (strcmp((*ppCxCmd)->pCommand,	"server") == 0) {
		(void)printf( "\"server\" legal only on command line\n");
		(void)printf( "use   help usage   for more information\n");
	    }
	    else if (strcmp((*ppCxCmd)->pCommand,	"source") == 0) {
		cmdSource(ppCxCmd);
	    }
	    else {
		pglZzzCtx->zzzInTaskInfo.serviceDone = 0;
		pglZzzCtx->zzzInTaskInfo.serviceNeeded = 1;
	    }
	}

	if (!pglZzzCtx->zzzInTaskInfo.stop)
	    taskSleep(SELF, 0, 500000);		/* sleep .5 sec */
    }

zzzInTaskWrapup:
zzzInTaskDone:
    while ((*ppCxCmd)->pPrev != NULL)
	cmdCloseContext(ppCxCmd);
    (*ppCxCmd)->inputEOF = 0;

#ifdef vxWorks
    if (pglZzzCtx->showStack)
	checkStack(pglZzzCtx->zzzInTaskInfo.id);
#endif
    pglZzzCtx->zzzInTaskInfo.stopped = 1;
    pglZzzCtx->zzzListenTaskInfo.serviceNeeded = 0;
    pglZzzCtx->zzzListenTaskInfo.serviceDone = 1;

#ifdef vxWorks
    taskDelete(SELF);
#endif
}

/*+/subr**********************************************************************
* NAME	zzzInTaskSigHandler - signal handling
*
* DESCRIPTION
*	These routines set up for the signals to be caught for zzzInTask
*	and handle the signals when they occur.
*
* RETURNS
*	void
*
* BUGS
* o	not all signals are caught
* o	under VxWorks, taskDeleteHookAdd isn't used
* o	it's not clear how useful it is to catch the signals which originate
*	from the keyboard
*
*-*/
static void
zzzInTaskSigHandler(signo)
int	signo;
{
    printf("entered zzzInTaskSigHandler for signal:%d\n", signo);
    signal(signo, SIG_DFL);
    longjmp(pglZzzCtx->zzzInTaskInfo.sigEnv, signo);
}

/*+/subr**********************************************************************
* NAME	zzzListenTask - listen for client connections
*
* DESCRIPTION
*
* RETURNS
*
* BUGS
* o	should use VxWorks pty facility, which would allow more flexible
*	output to socket--output wouldn't be line buffered
* o	cleanup of listen socket isn't dealt with.  Under SunOS, the system
*	seems to clean up OK.  Under VxWorks, NOT CHECKED YET.
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
zzzListenTask(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
    long	stat;
    int		sigNum;
    char	clientName[30];
    char	*oldPrompt;
    char	sockPrompt[80];
    int		serverStart=0;
    int		inUseInit=0;

    (void)printf("listen task started\n");

#ifdef vxWorks
    genSigInit(zzzListenTaskSigHandler);
    if ((sigNum = setjmp(pglZzzCtx->zzzListenTaskInfo.sigEnv)) != 0) {
	(void)printf("zzzListenTask signal received: %d\n", sigNum);
	pglZzzCtx->zzzListenTaskInfo.status = ERROR;
	goto zzzListenTaskWrapup;
    }
#endif

/*-----------------------------------------------------------------------------
*	establish a socket to use to listen for potential clients to connect
*----------------------------------------------------------------------------*/
    if (ezsCreateListenSocket(&pglZzzCtx->listenSock, glZzzIPPort) != 0) {
	perror("create listen socket");
	goto zzzListenTaskWrapup;
    }

    while (!pglZzzCtx->zzzListenTaskInfo.stop) {
/*-----------------------------------------------------------------------------
*	check to see if a connect attempt has been made.  If the server is
*	already in use by a client, connect attempts will be rejected.
*----------------------------------------------------------------------------*/
	ZzzLock;
	if (ezsListenExclusiveUse(&pglZzzCtx->clientSock,
			    &pglZzzCtx->listenSock, &pglZzzCtx->inUse,
			    clientName, glZzzId) < 0) {
	    ZzzUnlock;
	    perror("check for connect attempt");
	    assertAlways(0);
	}
	ZzzUnlock;

	if (pglZzzCtx->inUse && inUseInit == 0) {
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
	    oldPrompt = (*ppCxCmd)->prompt;
	    assert(strlen(oldPrompt) < 72);
	    (void)sprintf(sockPrompt, "#p#r#%s\n", oldPrompt);
	    (*ppCxCmd)->prompt = sockPrompt;
	    zzzListenTSetReopen(2);	/* reopen all other tasks to socket */
	    inUseInit = 1;
	    pglZzzCtx->zzzListenTaskInfo.serviceNeeded = 1;
	    pglZzzCtx->zzzListenTaskInfo.serviceDone = 0;
	    stat = zzzStartup(serverStart, clientName);
	    assertAlways(stat == OK);
	}

	if (pglZzzCtx->zzzListenTaskInfo.serviceDone) {
/*-----------------------------------------------------------------------------
*	When zzzInTask is done, reset everything back to normal
*----------------------------------------------------------------------------*/
	    pglZzzCtx->zzzListenTaskInfo.serviceDone = 0;
#if 0
	    shutdown(pglZzzCtx->clientSock, 2);
#endif
	    close(pglZzzCtx->clientSock);
	    pglZzzCtx->clientSock = -1;
	    pglZzzCtx->inUse = 0;
	    inUseInit = 0;
	    if (zzzFreopenAllStd(pglZzzCtx, 0) != OK)
		assertAlways(0);
	    (void)printf("%s disconnected\n", pglZzzCtx->inUseName);
	    (*ppCxCmd)->prompt = oldPrompt;
	    zzzListenTSetReopen(0);	/* reopen all other tasks to std... */
	}
	taskSleep(SELF, 2, 0);		/* sleep 2 seconds */
    }

zzzListenTaskWrapup:
    if (pglZzzCtx->clientSock != -1) {
	shutdown(pglZzzCtx->clientSock, 2);
	close(pglZzzCtx->clientSock);
    }
#if 0
    shutdown(pglZzzCtx->listenSock, 2);
    close(pglZzzCtx->listenSock);
#endif
#ifdef vxWorks
    if (pglZzzCtx->showStack)
	checkStack(pglZzzCtx->zzzListenTaskInfo.id);
#endif
    pglZzzCtx->zzzInTaskInfo.stop = 1;
    pglZzzCtx->zzzTaskInfo.stop = 1;
    pglZzzCtx->zzzListenTaskInfo.stopped = 1;

#ifdef vxWorks
    taskDelete(SELF);
#endif
}
void
zzzListenTSetReopen(option)
int	option;		/* I option for reopen for use with zzzFreopenAllStd */
{
    pglZzzCtx->zzzTaskInfo.reopenIOoption = option;
    pglZzzCtx->zzzTaskInfo.reopenIO = 1;
    pglZzzCtx->zzzInTaskInfo.reopenIOoption = option;
    pglZzzCtx->zzzInTaskInfo.reopenIO = 1;
}

/*+/subr**********************************************************************
* NAME	zzzListenTaskSigHandler - signal handling
*
* DESCRIPTION
*	These routines set up for the signals to be caught for zzzListenTask
*	and handle the signals when they occur.
*
* RETURNS
*	void
*
* BUGS
* o	not all signals are caught
* o	under VxWorks, taskDeleteHookAdd isn't used
* o	it's not clear how useful it is to catch the signals which originate
*	from the keyboard
*
*-*/
static void
zzzListenTaskSigHandler(signo)
int	signo;
{
#ifdef vxWorks
    if (signo == SIGPIPE) {
	if (zzzFreopenAllStd(pglZzzCtx, 0) != OK)
	    assertAlways(0);
	(void)printf("client connection broken\n");
	pglZzzCtx->zzzInTaskInfo.stop = 1;
	return;
    }
#endif
    printf("entered zzzListenTaskSigHandler for signal:%d\n", signo);
    signal(signo, SIG_DFL);
    longjmp(pglZzzCtx->zzzListenTaskInfo.sigEnv, signo);
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
*	o   initialize ZzzLock
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
    pZzzCtx->showStack = 0;

    ZzzLockInitAndLock;
    ZzzUnlock;

    pCxCmd->input = stdin;
    pCxCmd->inputEOF = 0;
    pCxCmd->inputName = NULL;
    pCxCmd->dataOut = stdout;
    pCxCmd->dataOutRedir = 0;
    pCxCmd->prompt = "  zzz:  ";
    pCxCmd->pPrev = NULL;
    pCxCmd->pCxCmdRoot = pCxCmd;

/*-----------------------------------------------------------------------------
* initialize the tasks; none of them should exist
*----------------------------------------------------------------------------*/
    if (zzzInitInfo("zzzTask", &pglZzzCtx->zzzTaskInfo) != OK)
	return ERROR;
    if (zzzInitInfo("zzzInTask", &pglZzzCtx->zzzInTaskInfo) != OK)
	return ERROR;
    if (zzzInitInfo("zzzListenT", &pglZzzCtx->zzzListenTaskInfo) != OK)
	return ERROR;

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
The bg command under vxWorks allows the zzz process to continue\n\
running without accepting commands from the keyboard.  This allows the\n\
vxWorks shell to be used for other purposes without the need to\n\
terminate zzz.\n\
\n\
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
    under VxWorks:  > zzz \"server\"\n\
\n\
zzz can be run as a client connecting to a zzz server on \"hostName\" by:\n\
    under SunOS:    % zzz hostName\n\
    under VxWorks:  > zzz \"hostName\"\n\
\n\
    When running zzz as a client, two commands are of special interest:\n\
\n\
    close	shuts down the client but leaves the server running\n\
    quit	shuts down both the client and the server\n\
");
}

/*+/subr**********************************************************************
* NAME	zzzInitInfo - initialize a task info block
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR if name != NULL and task already exists
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
static long
zzzInitInfo(name, pInfo)
char	*name;		/* I name of task */
ZZZ_TASK_INFO *pInfo;	/* IO pointer to task information block */
{
#ifdef vxWorks
    if (name != NULL && taskNameToId(name) != ERROR)
	return ERROR;
#endif

    pInfo->status = OK;
    pInfo->stop = 0;
    pInfo->stopped = 1;
    pInfo->serviceNeeded = 0;
    pInfo->serviceDone = 0;
    pInfo->reopenIO = 0;
    pInfo->reopenIOoption = 0;

    return OK;
}

/*+/subr**********************************************************************
* NAME	zzzSpawnIfNonexist - spawn a task if it doesn't exist
*
* DESCRIPTION
*	Checks to see if the task exists and spawns it if not.
*
* RETURNS
*	OK, or
*	ERROR if an error occurs during the spawn
*
* BUGS
* o	handles only 3 arguments
*
*-*/
static long
zzzSpawnIfNonexist(pCxCmd, name, pInfo, priority, stackSize,
					routine, pArg1, pArg2, pArg3)
CX_CMD	*pCxCmd;	/* I pointer to command context */
char	*name;		/* I name to use for task */
ZZZ_TASK_INFO *pInfo;	/* IO pointer to task info for task */
int	priority;	/* I priority for spawn */
int	stackSize;	/* I size of stack for task */
int	(*routine)();	/* I routine to spawn */
void	*pArg1;		/* I argument */
void	*pArg2;		/* I argument */
void	*pArg3;		/* I argument */
{
    if (pInfo->stopped) {
	pInfo->stopped = 0;
	pInfo->stop = 0;
	pInfo->status = OK;
	pInfo->id = taskSpawn(name, priority, VX_STDIO | VX_FP_TASK,
				stackSize, routine, pArg1, pArg2, pArg3);
    }
    if (GenTaskNull(pInfo->id)) {
	pInfo->stopped = 1;
	pInfo->status = ERROR;
	(void)printf("error spawning %s\n", name);
	return ERROR;
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	zzzStartup - start zzz into operation
*
* DESCRIPTION
*
* RETURNS
*	OK, or
*	ERROR
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
static long
zzzStartup(serverStart, name)
int	serverStart;	/* I 1 says starting up as server */
char	*name;		/* I name of client */
{
    long	stat;		/* status return from calls */

    pglZzzCtx->inUseName = name;

    if (serverStart) {
	if (!pglZzzCtx->zzzTaskInfo.stopped) {
	    (void)printf("zzz already running--can't specify server");
	    pglZzzCtx->inUse = 0;
	    return ERROR;
	}
    }

/*-----------------------------------------------------------------------------
*    spawn the necessary tasks, depending on startup mode.  If the
*    zzzInTask is being spawned, set its .serviceNeeded flag.
*----------------------------------------------------------------------------*/
    stat = zzzSpawnIfNonexist(pglZzzCxCmd, "zzzTask",
		&pglZzzCtx->zzzTaskInfo, MAXPRIO, 50000, zzzTask,
		(void *)&pglZzzCxCmd, (void *)NULL, (void *)NULL);
    if (stat == ERROR)
	return ERROR;

    if (!serverStart) {
	stat = zzzSpawnIfNonexist(pglZzzCxCmd, "zzzInTask",
		    &pglZzzCtx->zzzInTaskInfo, MAXPRIO, 50000, zzzInTask,
		    (void *)&pglZzzCxCmd, (void *)NULL, (void *)NULL);
	if (stat == ERROR) {
	    pglZzzCtx->inUse = 0;
	    return ERROR;
	}
    }
    else {
	pglZzzCtx->inUse = 0;
	stat = zzzSpawnIfNonexist(pglZzzCxCmd, "zzzListenT",
		&pglZzzCtx->zzzListenTaskInfo, MAXPRIO, 50000, zzzListenTask,
		(void *)&pglZzzCxCmd, (void *)NULL, (void *)NULL);
	if (stat == ERROR)
	    return ERROR;
    }
    return OK;
}
