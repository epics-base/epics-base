/****************************************************************************
			GTA PROJECT   AT division

	Copyright, 1990, The Regents of the University of California
		      Los Alamos National Laboratory

	FILE PATH:	cmdClient.c
	ENVIRONMENT:	SunOS, VxWorks
	MAKE OPTIONS:
	SCCS VERSION:	$Id$
*+/mod***********************************************************************
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
* o	need to clean up structure to be more in line with cmdProto's structure
* o	the stdout stream from this program contains, intermixed, the
*	server's stdout and stderr streams
* o	under VxWorks, if the server is on the same IOC, then the IOC itself
*	must be added to the host table with hostAdd()
* o	this program should use tasks.h to establish priorities, stack
*	sizes, etc.
* o	not all signals are caught
*-
* Modification History
* version   date    programmer   comments
* ------- -------- ------------ -----------------------------------
* 1.1     11/26/90 R. Cole      initial version
*
*****************************************************************************/
#include <genDefs.h>
#include <genTasks.h>
#include <cmdDefs.h>
#include <ezsSockSubr.h>

#ifdef vxWorks
/*----------------------------------------------------------------------------
*    includes and defines for VxWorks compile
*---------------------------------------------------------------------------*/
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <ctype.h>
#   include <strLib.h>
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
#   include <strings.h>
#   include <signal.h>
#   include <setjmp.h>
#   define MAXPRIO 50
#endif


/*/subhead CMDCL_CTX-------------------------------------------------------
* CMDCL_CTX - context information
*
*	A cmdcl descriptor is the `master handle' which is used for
*	handling business.
*----------------------------------------------------------------------------*/
#define CmdclLock semTake(pglCmdclCtx->semLock)
#define CmdclUnlock semGive(pglCmdclCtx->semLock)
#define CmdclLockCheck semClear(pglCmdclCtx->semLock)
#define CmdclLockInitAndLock semInit(pglCmdclCtx->semLock)

typedef struct {
    TASK_ID	id;		/* ID of task */
    int		status;		/* status of task--initially ERROR */
    int		stop;		/* task requested to stop if != 0 */
    int		stopped;	/* task has stopped if != 0 */
    int		serviceNeeded;	/* task needs servicing */
    int		serviceDone;	/* task servicing completed */
    jmp_buf	sigEnv;		/* environment for longjmp at signal time */
} CMDCL_TASK_INFO;

typedef struct cmdclCtx {
    SEM_ID	semLock;
    CMDCL_TASK_INFO cmdclTaskInfo;
    CMDCL_TASK_INFO cmdclInTaskInfo;
    int		showStack;		/* show stack stats on task terminate */
} CMDCL_CTX;

/*-----------------------------------------------------------------------------
* prototypes
*----------------------------------------------------------------------------*/
int cmdcl();
void cmdclCmdProcess();
long cmdclInitAtStartup();
long cmdclInTask();
long cmdclTask();
void cmdclTaskSigHandler();
void cmdclTaskSigInit();

/*-----------------------------------------------------------------------------
* global definitions
*----------------------------------------------------------------------------*/
CX_CMD	glCmdclCxCmd;
CX_CMD	*pglCmdclCxCmd=NULL;
CMDCL_CTX	glCmdclCtx;
CMDCL_CTX	*pglCmdclCtx=NULL;

#ifndef vxWorks
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

    return cmdClient(hostName, portNum);

mainUsage:
    printf("Usage: %s serverHostName serverPortNumber\n", argv[0]);
    return -1;
}
#endif

/*+/subr**********************************************************************
* NAME	cmdClient - shell callable interface for cmdClient
*
* DESCRIPTION
*	This routine is the only part of cmdClient which is intended to be
*	called directly from the shell.  Several functions are performed here:
*	o   spawn the cmdclTask
*	o   spawn the cmdclInTask
*	o   wait until the cmdclInTask quits, then return to the shell.  If
*	    other tasks belonging to cmdClient are being stopped, then this
*	    routine waits until they, too, are stopped before returning to
*	    the shell.
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
cmdClient(hostName, portNum)
char	*hostName;	/* I host name for server */
int	portNum;	/* I port number for server */
{
    long	stat;		/* status return from calls */

    pglCmdclCtx = &glCmdclCtx;
    pglCmdclCxCmd = &glCmdclCxCmd;
    stat = cmdclInitAtStartup(pglCmdclCtx, pglCmdclCxCmd);
    assert(stat == OK);

#ifdef vxWorks
    assert(taskNameToId("cmdclTask") == ERROR);
    assert(taskNameToId("cmdclInTask") == ERROR);
#endif
    pglCmdclCtx->showStack = 0;

    pglCmdclCtx->cmdclTaskInfo.status = ERROR;
    pglCmdclCtx->cmdclTaskInfo.stop = 0;
    pglCmdclCtx->cmdclTaskInfo.stopped = 1;

    pglCmdclCtx->cmdclInTaskInfo.status = ERROR;
    pglCmdclCtx->cmdclInTaskInfo.stop = 0;
    pglCmdclCtx->cmdclInTaskInfo.stopped = 1;

    pglCmdclCtx->cmdclInTaskInfo.serviceNeeded = 0;
    pglCmdclCtx->cmdclInTaskInfo.serviceDone = 1;

/*-----------------------------------------------------------------------------
* cmdclTask
*	spawn it
*----------------------------------------------------------------------------*/
    pglCmdclCtx->cmdclTaskInfo.id = taskSpawn("cmdclTask", MAXPRIO,
		VX_STDIO | VX_FP_TASK, 50000, cmdclTask,
		&pglCmdclCxCmd, hostName, portNum);
    if (GenTaskNull(pglCmdclCtx->cmdclTaskInfo.id)) {
	(void)fprintf(stdout, "error spawning cmdclTask\n");
	return ERROR;
    }
    pglCmdclCtx->cmdclTaskInfo.status = OK;
    pglCmdclCtx->cmdclTaskInfo.stopped = 0;

/*-----------------------------------------------------------------------------
* cmdclInTask
*	spawn it
*----------------------------------------------------------------------------*/
    pglCmdclCtx->cmdclInTaskInfo.id = taskSpawn("cmdclInTask", MAXPRIO,
		VX_STDIO | VX_FP_TASK, 50000, cmdclInTask, &pglCmdclCxCmd);
    if (GenTaskNull(pglCmdclCtx->cmdclInTaskInfo.id)) {
	(void)fprintf(stdout, "error spawning cmdclInTask\n");
	return ERROR;
    }
    pglCmdclCtx->cmdclInTaskInfo.status = OK;
    pglCmdclCtx->cmdclInTaskInfo.stopped = 0;

/*-----------------------------------------------------------------------------
*    wait for cmdclInTask to exit and then return to the shell.  If other
*    "cmdClient tasks" are also exiting, wait until their wrapups are complete.
*----------------------------------------------------------------------------*/
    while (!pglCmdclCtx->cmdclInTaskInfo.stopped)
	taskSleep(SELF, 1, 0);
    if (pglCmdclCtx->cmdclTaskInfo.stop) {
	while (!pglCmdclCtx->cmdclTaskInfo.stopped)
	    taskSleep(SELF, 1, 0);
    }

    return OK;
}

/*+/subr**********************************************************************
* NAME	cmdclTask - main processing task for cmdClient
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
*-*/
long
cmdclTask(ppCxCmd, hostName, portNum)
CX_CMD	**ppCxCmd;
char	*hostName;	/* I host name for server */
int	portNum;	/* I port number for server */
{
    long	stat;
    CX_CMD	*pCxCmd;
    int		serverSock=-1;	/* socket connected to server */
    char	serverBuf[80];
    FILE	*myIn=NULL;
    FILE	*myOut=NULL;
    char	*findNl;
    char	message[80];

    int		i;

    pCxCmd = *ppCxCmd;

    CmdclLockInitAndLock;
    CmdclUnlock;

    cmdclTaskSigInit();

    if (setjmp(pglCmdclCtx->cmdclTaskInfo.sigEnv) != 0)
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
    while (!pglCmdclCtx->cmdclTaskInfo.stop) {
	if (pglCmdclCtx->cmdclInTaskInfo.serviceNeeded) {
	    fputs(pCxCmd->line, myOut);
	    fflush(myOut);
	    pglCmdclCtx->cmdclInTaskInfo.serviceNeeded = 0;
	    pglCmdclCtx->cmdclInTaskInfo.serviceDone = 1;
	}
	if (ezsCheckFpRead(myIn)) {
	    if (fgets(serverBuf, 80, myIn) != NULL) {
		if (strncmp(serverBuf, "#p#r#", 5) == 0) {
		    if ((findNl = index(serverBuf, '\n')) != NULL)
			*findNl = '\0';
		    fputs(serverBuf+5, stdout);
		}
		else
		    fputs(serverBuf, stdout);
		fflush(stdout);
	    }
	    else {
		if (strncmp(pCxCmd->line, "quit", 4) == 0)
		    break;
		else if (strncmp(pCxCmd->line, "close", 5) == 0)
		    break;
		printf("server gone (?)\n");
		break;
	    }
	}
	taskSleep(SELF, 0, 100000);	/* wait .1 sec */
    }

cmdclTaskWrapup:
    if (myIn != NULL)
	fclose(myIn);
    if (myOut != NULL)
	fclose(myOut);
    if (serverSock >= 0)
	close(serverSock);

    while ((*ppCxCmd)->pPrev != NULL)
	cmdCloseContext(ppCxCmd);
    pCxCmd = *ppCxCmd;

    pglCmdclCtx->cmdclInTaskInfo.stop = 1;
    while (pglCmdclCtx->cmdclInTaskInfo.stopped == 0) {
	taskSleep(SELF, 1, 0);
    }

#ifdef vxWorks
    if (pglCmdclCtx->showStack)
	checkStack(pglCmdclCtx->cmdclTaskInfo.id);
#endif

    pglCmdclCtx->cmdclTaskInfo.stopped = 1;
    pglCmdclCtx->cmdclTaskInfo.status = ERROR;

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
* o	under VxWorks, taskDeleteHookAdd isn't used
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
    longjmp(pglCmdclCtx->cmdclTaskInfo.sigEnv, 1);
}


void
cmdclTaskSigInit()
{
    (void)signal(SIGTERM, cmdclTaskSigHandler);	/* SunOS plain kill (not -9) */
    (void)signal(SIGQUIT, cmdclTaskSigHandler);	/* SunOS ^\ */
    (void)signal(SIGINT, cmdclTaskSigHandler);	/* SunOS ^C */
    (void)signal(SIGILL, cmdclTaskSigHandler);	/* illegal instruction */
#ifndef vxWorks
    (void)signal(SIGABRT, cmdclTaskSigHandler);	/* SunOS assert */
#else
    (void)signal(SIGUSR1, cmdclTaskSigHandler);	/* VxWorks assert */
#endif
    (void)signal(SIGBUS, cmdclTaskSigHandler);	/* bus error */
    (void)signal(SIGSEGV, cmdclTaskSigHandler);	/* segmentation violation */
    (void)signal(SIGFPE, cmdclTaskSigHandler);	/* arithmetic exception */
}

/*+/subr**********************************************************************
* NAME	cmdclInTask - handle the keyboard and keyboard commands
*
* DESCRIPTION
*	Gets input text and passes the input to cmdclTask.
*
*	This task exists to avoid the possibility of blocking cmdclTask
*	while waiting for operator input.
*
*	This task waits for input to be available from the keyboard.  When
*	an input line is ready, this task does some preliminary processing:
*
*	o   if the command is control-D, "^D" is echoed and the command is
*	    changed to "quit"
*
*	Then cmdclTask is signalled and this task goes into a sleeping loop
*	until cmdclTask signals that it is ready for the next command.
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
long
cmdclInTask(ppCxCmd)
CX_CMD	**ppCxCmd;	/* IO ptr to pointer to command context */
{
/*----------------------------------------------------------------------------
*    wait for input from keyboard.  When some is received, signal caller,
*    wait for caller to process it, and then wait for some more input.
*
*    stay in the main loop until .stop flag is set by cmdclTask.
*---------------------------------------------------------------------------*/
    while (1) {
	while (pglCmdclCtx->cmdclInTaskInfo.serviceDone == 0) {
	    if (pglCmdclCtx->cmdclInTaskInfo.stop == 1)
		goto cmdclInTaskDone;
	    taskSleep(SELF, 0, 500000);		/* sleep .5 sec */
	}
	cmdRead(ppCxCmd, &pglCmdclCtx->cmdclInTaskInfo.stop);

	if (pglCmdclCtx->cmdclInTaskInfo.stop == 1)
	    goto cmdclInTaskDone;

	if (strncmp((*ppCxCmd)->line, "quit", 4) == 0) {
	    char	*prompt;
	    if (*ppCxCmd != (*ppCxCmd)->pCxCmdRoot) {
		(void)printf("can't use quit command in source file\n");
		(*ppCxCmd)->line[0] = '\0';
	    }
	    else {
		prompt = (*ppCxCmd)->prompt;
		(*ppCxCmd)->prompt = "are you sure? ";
		cmdRead(ppCxCmd, &pglCmdclCtx->cmdclInTaskInfo.stop);
		if ((*ppCxCmd)->line[0] == 'y' || (*ppCxCmd)->line[0] == 'Y')
		    (void)strcpy((*ppCxCmd)->line, "quit\n");
		else
		    (*ppCxCmd)->line[0] = '\0';
		(*ppCxCmd)->prompt = prompt;
	    }
	}
	pglCmdclCtx->cmdclInTaskInfo.serviceDone = 0;
	pglCmdclCtx->cmdclInTaskInfo.serviceNeeded = 1;
    }

cmdclInTaskDone:
    cmdCloseContext(ppCxCmd);

#ifdef vxWorks
    if (pglCmdclCtx->showStack)
	checkStack(pglCmdclCtx->cmdclInTaskInfo.id);
#endif
    pglCmdclCtx->cmdclInTaskInfo.stop = 1;
    pglCmdclCtx->cmdclInTaskInfo.stopped = 1;
    pglCmdclCtx->cmdclInTaskInfo.status = ERROR;

    return;
}

/*+/subr**********************************************************************
* NAME	cmdclInitAtStartup - initialization for cmdClient
*
* DESCRIPTION
*	Perform several initialization duties:
*	o   initialize the command context block
*
* RETURNS
*	OK, or
*	ERROR
*
*-*/
long
cmdclInitAtStartup(pCmdclCtx, pCxCmd)
CMDCL_CTX *pCmdclCtx;
CX_CMD	*pCxCmd;
{
    pCxCmd->prompt = NULL;
    pCxCmd->input = stdin;
    pCxCmd->inputName = NULL;
    pCxCmd->dataOut = stdout;
    pCxCmd->pPrev = NULL;
    pCxCmd->pCxCmdRoot = pCxCmd;

    return OK;
}
