/* $Id$
 *
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      NOTES:
 *
 * Modification Log:
 * -----------------
 * .00 joh 080791  	Created
 * .01 joh 081591	Added epics env config
 * .02 joh 011995	Allow stdio also	
 * $Log$
 */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include <socket.h>
#include <in.h>

#include <ioLib.h>
#include <taskLib.h>
#include <logLib.h>
#include <inetLib.h>
#include <sockLib.h>
#include <sysLib.h>
#include <semLib.h>
#include <rebootLib.h>

#include <epicsPrint.h>
#include <envDefs.h>
#include <task_params.h>

LOCAL FILE		*iocLogFile = NULL;
LOCAL int 		iocLogFD = ERROR;
LOCAL int 		iocLogDisable = 0;
LOCAL unsigned		iocLogTries = 0U;
LOCAL unsigned		iocLogConnectCount = 0U;

LOCAL long 		ioc_log_port;
LOCAL struct in_addr 	ioc_log_addr;

int 			iocLogInit(void);
LOCAL int 		getConfig(void);
LOCAL void 		failureNotify(ENV_PARAM *pparam);
LOCAL void 		logClientShutdown(void);
LOCAL void 		logRestart(void);
LOCAL int 		iocLogAttach(void);
LOCAL void 		logClientRollLocalPort(void);

LOCAL SEM_ID		iocLogMutex;	/* protects stdio */
LOCAL SEM_ID		iocLogSignal;	/* reattach to log server */


/*
 *	iocLogInit()
 */
int iocLogInit(void)
{
	int	status;
	int	attachStatus;
	int	options;

	if(iocLogDisable){
		return OK;
	}

	options = SEM_Q_PRIORITY|SEM_DELETE_SAFE|SEM_INVERSION_SAFE;
	iocLogMutex = semMCreate(options);
	if(!iocLogMutex){
		return ERROR;
	}

	iocLogSignal = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);
	if(!iocLogSignal){
		return ERROR;
	}

	attachStatus = iocLogAttach();

	status = rebootHookAdd((FUNCPTR)logClientShutdown);
	if (status<0) {
		epicsPrintf("Unable to add log server reboot hook\n");
	}

	status = taskSpawn(	
			LOG_RESTART_NAME, 
			LOG_RESTART_PRI, 
			LOG_RESTART_OPT, 
			LOG_RESTART_STACK, 
			(FUNCPTR)logRestart,
			0,0,0,0,0,0,0,0,0,0);
	if (status<0) {
		epicsPrintf("Unable to start log server connection watch dog\n");
	}

	return attachStatus;
}


/*
 *	iocLogAttach()
 */
int iocLogAttach(void)
{

	int            		sock;
        struct sockaddr_in      addr;
	int			status;
	int			optval;
	FILE			*fp;

	status = getConfig();
	if(status<0){
		epicsPrintf (
		"iocLogClient: EPICS environment under specified\n");
		epicsPrintf ("iocLogClient: failed to initialize\n");
		return ERROR;
	}

	/* allocate a socket       */
	sock = socket(AF_INET,		/* domain       */
		      SOCK_STREAM,	/* type         */
		      0);		/* deflt proto  */
	if (sock < 0){
		epicsPrintf ("iocLogClient: no socket error %s\n", 
			strerror(errno));
		return ERROR;
	}

        /*      set socket domain       */
        addr.sin_family = AF_INET;

        /*      set the port    */
        addr.sin_port = htons(ioc_log_port);

        /*      set the addr */
        addr.sin_addr.s_addr = ioc_log_addr.s_addr;

	/* connect */
	status = connect(
			 sock,
			 (struct sockaddr *)&addr,
			 sizeof(addr));
	if (status < 0) {
		char name[INET_ADDR_LEN];

		inet_ntoa_b(addr.sin_addr, name);
		if (iocLogTries==0U) {
			epicsPrintf(
	"iocLogClient: unable to connect to %s port %d because \"%s\"\n", 
				name,
				addr.sin_port,
				strerror(errno));
		}
		iocLogTries++;
		close(sock);
		return ERROR;
	}

	iocLogTries=0U;
	iocLogConnectCount++;

	/*
	 * discover that the connection has expired
	 * (after a long delay)
	 */
        optval = TRUE;
        status = setsockopt(    sock,
                                SOL_SOCKET,
                                SO_KEEPALIVE,
                                (char *) &optval,
                                sizeof(optval));
        if(status<0){
                epicsPrintf ("iocLogClient: %s\n", strerror(errno));
		close(sock);
                return ERROR;
        }

	/*
	 * set how long we will wait for the TCP state machine
	 * to clean up when we issue a close(). This
	 * guarantees that messages are serialized when we
	 * switch connections.
	 */
	{
		struct  linger		lingerval;

		lingerval.l_onoff = TRUE;
		lingerval.l_linger = 60*5; 
		status = setsockopt(    sock,
					SOL_SOCKET,
					SO_LINGER,
					(char *) &lingerval,
					sizeof(lingerval));
		if(status<0){
			epicsPrintf ("iocLogClient: %s\n", strerror(errno));
			close(sock);
			return ERROR;
		}
	}

	fp = fdopen (sock, "a");

	/*
	 * mutex on
	 */
	status = semTake(iocLogMutex, WAIT_FOREVER);
	assert(status==OK);

	/*
	 * close any preexisting connection to the log server
	 */
	if (iocLogFile) {
		logFdDelete(iocLogFD);
		fclose(iocLogFile);
		iocLogFile = NULL;
		iocLogFD = ERROR;
	}
	else if (iocLogFD!=ERROR) {
		logFdDelete(iocLogFD);
		close(iocLogFD);
		iocLogFD = ERROR;
	}

	/*
	 * export the new connection
	 */
	iocLogFD = sock;
	logFdAdd (iocLogFD);
	iocLogFile = fp;

	/*
	 * mutex off
	 */
	status = semGive(iocLogMutex);
	assert(status==OK);

	return OK;
}


/*
 * logRestart()
 */
LOCAL void logRestart(void)
{
	int 	status;
	int	reattach;
	int	delay = LOG_RESTART_DELAY;	


	/*
	 * roll the local port forward so that we dont collide
	 * with the first port assigned when we reboot 
	 */
	logClientRollLocalPort();

	while (1) {
		semTake(iocLogSignal, delay);

		/*
		 * mutex on
		 */
		status = semTake(iocLogMutex, WAIT_FOREVER);
		assert(status==OK);

		if (iocLogFile==NULL) {
			reattach = TRUE;
		}
		else {
			reattach = ferror(iocLogFile);
		}

		/*
		 * mutex off
		 */
		status = semGive(iocLogMutex);
		assert(status==OK);

		if (reattach==FALSE) {
			continue;
		}

		/*
		 * restart log server
		 */
		iocLogConnectCount = 0U;
		logClientRollLocalPort();
	}
}


/*
 * logClientRollLocalPort()
 */
LOCAL void logClientRollLocalPort(void)
{
	int	status;

	/*
	 * roll the local port forward so that we dont collide
	 * with it when we reboot
	 */
	while (iocLogConnectCount<10U) {
		/*
		 * switch to a new log server connection 
		 */
		status = iocLogAttach();
		if (status==OK) {
			/*
			 * only print a message after the first connect
			 */
			if (iocLogConnectCount==1U) {
				printf(
		"iocLogClient: reconnected to the log server\n");
			}
		}
		else {
			/*
			 * if we cant connect then we will roll
			 * the port later when we can
			 * (we must not spin on connect fail)
			 */
			if (errno!=ETIMEDOUT) {
				return;
			}
		}
	}
}


/*
 * logClientShutdown()
 */
LOCAL void logClientShutdown(void)
{
	if (iocLogFD!=ERROR) {
	/*
	 * unfortunately this does not currently work because WRS
	 * runs the reboot hooks in the order the order that
	 * they are installed (and the network is already shutdown 
	 * by the time we get here)
	 */
#if 0
		/*
		 * this aborts the connection because we 
		 * have specified a nill linger interval
		 */
		printf("log client: lingering for connection close...");
		close(iocLogFD);
		printf("done\n");
#endif 
	}	
}


/*
 *
 *	getConfig()
 *	Get Server Configuration
 *
 *
 */
LOCAL int getConfig(void)
{
	long	status;

	status = envGetLongConfigParam(
			&EPICS_IOC_LOG_PORT, 
			&ioc_log_port);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_PORT);
		return ERROR;
	}

	status = envGetInetAddrConfigParam(
			&EPICS_IOC_LOG_INET, 
			&ioc_log_addr);
	if(status<0){
		failureNotify(&EPICS_IOC_LOG_INET);
		return ERROR;
	}

	return OK;
}



/*
 *	failureNotify()
 */
LOCAL void failureNotify(ENV_PARAM *pparam)
{
	epicsPrintf(
	"IocLogClient: EPICS environment variable \"%s\" undefined\n",
		pparam->name);
}


/*
 * iocLogVPrintf()
 */
int iocLogVPrintf(const char *pFormat, va_list pvar)
{
	int status;
	int semStatus;

	if (!pFormat || iocLogDisable) {
		return 0;
	}

	/*
	 * mutex on
	 */
	semStatus = semTake(iocLogMutex, WAIT_FOREVER);
	assert(semStatus==OK);

	if (iocLogFile) {
		status = vfprintf(iocLogFile, pFormat, pvar);
		if (status>0) {
			status = fflush(iocLogFile);
		}

		if (status<0) {
			logFdDelete(iocLogFD);
			fclose(iocLogFile);
			iocLogFile = NULL;
			iocLogFD = ERROR;
			semStatus = semGive(iocLogSignal);
			assert(semStatus==OK);
		}
	}
	else {
		status = EOF;
	}

	/*
	 * mutex off
	 */
	semStatus = semGive(iocLogMutex);
	assert(semStatus==OK);

	return status;
}

