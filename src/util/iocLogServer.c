/* iocLogServer.c */
/* base/src/util $Id$ */

/*
 *	archive logMsg() from several IOC's to a common rotating file	
 *
 *
 * 	Author: 	Jeffrey O. Hill 
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
 *	NOTES:
 *	.01	currently runs under UNIX. could be made to run under
 *		vxWorks if NFS is used.
 *
 * Modification Log:
 * -----------------
 * .01 080791 joh	Created
 * .02 102591 joh	Dont try to reopen the log file if a write fails
 * .03 110691 joh	Disconnect if sent a zero length message
 * .04 091092 joh	Print routine messages only when in debug mode	
 * .05 091092 joh	now uses SO_REUSEADDR
 * .06 091192 joh	now uses SO_KEEPALIVE
 * .07 091192 joh	added SCCS ID
 * .08 092292 joh	improved message sent to the log
 * .08 092292 joh	improved message sent to the log
 * .09 050494 pg        HPUX port changes.
 * .10 021694 joh	ANSI C	
 * $Log$
 */

static char	*pSCCSID = "@(#)iocLogServer.c	1.9\t05/05/94";

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include	<assert.h>

#include 	<unistd.h>

#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/socket.h>
#include	<sys/ioctl.h>
#include	<netinet/in.h>
#include        <netdb.h>

#include 	<bsdProto.h>

#include 	<envDefs.h>
#include 	<fdmgr.h>

static unsigned short	ioc_log_port;
static long		ioc_log_file_limit;
static char		ioc_log_file_name[64];


#ifndef TRUE
#define	TRUE			1
#endif
#ifndef FALSE
#define	FALSE			0
#endif


struct iocLogClient {
	int			insock;
	struct ioc_log_server	*pserver;
	int			need_prefix;
	char			*ptopofstack;
	char			recvbuf[1024];
	char			name[32];
	char			ascii_time[32];
};

struct ioc_log_server {
	int		sock;
	FILE		*poutfile;
	unsigned	max_file_size;
	void   		*pfdctx;
};

#ifndef ERROR
#define ERROR -1
#endif

#ifndef OK
#define OK 0
#endif

static void acceptNewClient (void *pParam);
static void readFromClient(void *pParam);
static void logTime (struct iocLogClient *pclient);
static int getConfig(void);
static int openLogFile(struct ioc_log_server *pserver);
static void handleLogFileError(void);
static void envFailureNotify(ENV_PARAM *pparam);
static void freeLogClient(struct iocLogClient *pclient);


/*
 *
 *	main()
 *
 */
int main()
{
	struct sockaddr_in 	serverAddr;	/* server's address */
	struct timeval          timeout;
	int			status;
	struct ioc_log_server	*pserver;
	int			optval;

	status = getConfig();
	if(status<0){
		fprintf(stderr, "iocLogServer: EPICS environment underspecified\n");
		fprintf(stderr, "iocLogServer: failed to initialize\n");
		return ERROR;
	}

	pserver = (struct ioc_log_server *) 
			calloc(1, sizeof *pserver);
	if(!pserver){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
	}

	pserver->pfdctx = (void *) fdmgr_init();
	if(!pserver->pfdctx){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
	}

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	pserver->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (pserver->sock<0) {
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
	}
	
	optval = TRUE;
        status = setsockopt(    pserver->sock,
                                SOL_SOCKET,
                                SO_REUSEADDR,
                                (char *) &optval,
                                sizeof(optval));
        if(status<0){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
        }

	/* Zero the sock_addr structure */
	memset((void *)&serverAddr, 0, sizeof serverAddr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(ioc_log_port);

	/* get server's Internet address */
	status = bind (	pserver->sock, 
			(struct sockaddr *)&serverAddr, 
			sizeof (serverAddr) );
	if (status<0) {
		fprintf(stderr,
			"iocLogServer: a server is already installed on port %u?\n", 
			(unsigned)ioc_log_port);
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
	}

	/* listen and accept new connections */
	status = listen(pserver->sock, 10);
	if (status<0) {
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
	}

        /*
         * Set non blocking IO
         * to prevent dead locks
         */
	optval = TRUE;
        status = ioctl(
                       	pserver->sock,
                        FIONBIO,
                        &optval);
        if(status<0){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return ERROR;
        }

	status = openLogFile(pserver);
	if(status<0){
		fprintf(stderr,
			"File access problems to `%s' becuse `%s'\n", 
			ioc_log_file_name,
			strerror(errno));
		return ERROR;
	}

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			pserver->sock, 
			fdi_read,
			acceptNewClient,
			pserver);
	if(status<0){
		return ERROR;
	}

	while(TRUE){
		timeout.tv_sec = 60; /* 1 min */
		timeout.tv_usec = 0;
		fdmgr_pend_event(pserver->pfdctx, &timeout);
		fflush(pserver->poutfile);
	}
}


/*
 *	openLogFile()
 *
 */
static int openLogFile(struct ioc_log_server *pserver)
{
	if(pserver->poutfile){
		pserver->poutfile = freopen(
					ioc_log_file_name, 
					"a+", 
					pserver->poutfile);
	}else{
		pserver->max_file_size = ioc_log_file_limit; 
		pserver->poutfile = fopen(ioc_log_file_name, "a+");
	}
	if(!pserver->poutfile){
		return ERROR;
	}
	return OK;
}


/*
 *	handleLogFileError()
 *
 */
static void handleLogFileError(void)
{
	fprintf(stderr,
		"iocLogServer: log file access problem (errno=%s)\n", 
		strerror(errno));
	exit(ERROR);

}
		


/*
 *	acceptNewClient()
 *
 */
static void acceptNewClient(void *pParam)
{
	struct ioc_log_server	*pserver = (struct ioc_log_server *)pParam;
	struct iocLogClient	*pclient;
	int			size;
	struct sockaddr_in 	addr;
	char			*pname;
	int			status;
	int			optval;

	pclient = (struct iocLogClient *) 
			malloc(sizeof *pclient);
	if(!pclient){
		return;
	}

	pclient->insock = accept(pserver->sock, NULL, 0);
	if(pclient->insock<0){
		free(pclient);
		if (errno!=EWOULDBLOCK) {
			fprintf(stderr, "Accept Error %d\n", errno);
		}
		return;
	}

        /*
         * Set non blocking IO
         * to prevent dead locks
         */
	optval = TRUE;
        status = ioctl(
                       	pclient->insock,
                        FIONBIO,
                        &optval);
        if(status<0){
		close(pclient->insock);
		free(pclient);
		fprintf(stderr, "%s:%d %s\n", __FILE__, __LINE__, strerror(errno));
		return;
        }

	pclient->pserver = pserver;
	pclient->need_prefix = TRUE;
	pclient->ptopofstack = pclient->recvbuf;

	pname = "<ukn>";
        size = sizeof addr;
	memset((void *)&addr, 0, sizeof addr);
        status = getpeername(
                        pclient->insock,
                        (struct sockaddr *) &addr, 
                        &size); 
        if(status>=0){
  		struct hostent      *pent;   

    		pent = gethostbyaddr(
				(char *)&addr.sin_addr, 
				sizeof addr.sin_addr, 
				AF_INET);
		if(pent){
			pname = pent->h_name;
		}else{
			pname = inet_ntoa (addr.sin_addr);
		}
        }

	strncpy(pclient->name, pname, sizeof(pclient->name));
	pclient->name[sizeof(pclient->name) - 1] = NULL;

	logTime(pclient);
	
#if 0
	status = fprintf(
		pclient->pserver->poutfile,
		"%s %s ----- Client Connect -----\n",
		pclient->name,
		pclient->ascii_time);
	if(status<0){
		handleLogFileError();
	}
#endif

        /*
         * turn on KEEPALIVE so if the client crashes
         * this task will find out and exit
         */
	{
		long	true = true;

		status = setsockopt(
				pclient->insock,
				SOL_SOCKET,
				SO_KEEPALIVE,
				(char *)&true,
				sizeof(true) );
		if(status<0){
			fprintf(stderr, "Keepalive option set failed\n");
		}
	}

#       define SOCKET_SHUTDOWN_WRITE_SIDE 1
        status = shutdown(pclient->insock, SOCKET_SHUTDOWN_WRITE_SIDE);
        if(status<0){
                close(pclient->insock);
		free(pclient);
                printf("%s:%d %s\n", __FILE__, __LINE__,
                        strerror(errno));
                return;
        }

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			pclient->insock, 
			fdi_read,
			readFromClient,
			pclient);
	if (status<0) {
		close(pclient->insock);
		free(pclient);
		fprintf(stderr, "%s:%d client fdmgr_add_callback() failed\n", 
			__FILE__, __LINE__);
		return;
	}
}



/*
 * readFromClient()
 * 
 */
#define NITEMS 1

static void readFromClient(void *pParam)
{
	struct iocLogClient	*pclient = (struct iocLogClient *)pParam;
	int             	status;
	int             	length;
	char			*pcr;
	char			*pline;
	int			stacksize;
	int			size;

	logTime(pclient);

	stacksize = pclient->ptopofstack - pclient->recvbuf;
	size = sizeof(pclient->recvbuf)-stacksize-1;
	assert(size>0);
	length = recv(pclient->insock,
		      pclient->ptopofstack,
		      size,
		      0);
	if (length <= 0) {
		if (length<0) {
			if (errno != EWOULDBLOCK) {
				fprintf(stderr, 
					"%s:%d socket=%d addr=%x size=%d read error=%s\n",
					__FILE__, __LINE__, pclient->insock, 
					pclient->ptopofstack, size, strerror(errno));
				freeLogClient(pclient);
			}
			return;
		}
		else {
			/*
			 * disconnect
			 */
			freeLogClient(pclient);
			return;
		}
	}

	pclient->ptopofstack[length] = NULL;
	pline = pclient->recvbuf;
	while(TRUE){
		unsigned nchar;

		pcr = strchr(pline, '\n');

		if(pcr){
			nchar = pcr-pline+1;
		}
		else{
			nchar = strlen(pline);
			pclient->ptopofstack = pclient->recvbuf + nchar;
			memcpy(	pclient->recvbuf, 
				pline, 
				nchar);
			break;
		}

		status = fprintf(
			pclient->pserver->poutfile,
			"%s %s ",
			pclient->name,
			pclient->ascii_time);
		if(status<0){
			handleLogFileError();
		}

		status = fwrite(
				pline,
				nchar,
				NITEMS,
				pclient->pserver->poutfile);
		if (status != NITEMS) {
			handleLogFileError();
			return;
		}

		pline = pcr+1;
	}


	/*
	 * limit file length by reseting to the beginning of the file if it
	 * becomes to large
	 */
	length = ftell(pclient->pserver->poutfile);
	if (length > pclient->pserver->max_file_size) {
#		ifdef DEBUG
			fprintf(stderr,
				"ioc log server: resetting the file pointer\n");
#		endif
		rewind (pclient->pserver->poutfile);
		status = ftruncate(
				   fileno(pclient->pserver->poutfile),
				   length);
		if (status < 0) {
			fprintf(stderr,"truncation error %d\n", errno);
		}
	}
}


/*
 * freeLogClient ()
 */
static void freeLogClient(struct iocLogClient     *pclient)
{
	unsigned	stacksize;
	int		status;

	stacksize = pclient->ptopofstack - pclient->recvbuf;

#	ifdef	DEBUG
	if(length == 0){
		fprintf(stderr, "iocLogServer: nil message disconnect\n");
	}
#	endif

	/*
	 * flush any left overs
	 */
	if(stacksize){
		status = fprintf(
			pclient->pserver->poutfile,
			"%s %s ",
			pclient->name,
			pclient->ascii_time);
		if(status<0){
			handleLogFileError();
		}
		status = fwrite(
				pclient->recvbuf,
				stacksize,
				NITEMS,
				pclient->pserver->poutfile);
		if (status != NITEMS) {
			handleLogFileError();
		}
		status = fprintf(pclient->pserver->poutfile,"\n");
		if(status<0){
			handleLogFileError();
		}
	}

#if 0
	status = fprintf(
		pclient->pserver->poutfile,
		"%s %s ----- Client Disconnect -----\n",
		pclient->name,
		pclient->ascii_time);
	if(status<0){
		handleLogFileError();
	}
#endif

	status = fdmgr_clear_callback(
		       pclient->pserver->pfdctx,
		       pclient->insock,
		       fdi_read);
	if (status!=OK) {
		fprintf(stderr, "%s:%d fdmgr_clear_callback() failed\n",
			__FILE__, __LINE__);
	}

	if(close(pclient->insock)<0)
		abort();

	free (pclient);

	return;
}


/*
 *
 *	logTime()
 *
 */
static void logTime(struct iocLogClient *pclient)
{
	time_t		sec;
	char		*pcr;
	char		*pTimeString;

	sec = time (NULL);
	pTimeString = ctime (&sec);
	strncpy (pclient->ascii_time, 
		pTimeString, 
		sizeof (pclient->ascii_time) );
	pclient->ascii_time[sizeof(pclient->ascii_time)-1] = '\0';
	pcr = strchr(pclient->ascii_time, '\n');
	if (pcr) {
		*pcr = '\0';
	}
}


/*
 *
 *	getConfig()
 *	Get Server Configuration
 *
 *
 */
static int getConfig(void)
{
	int	status;
	char	*pstring;
	long	param;

	status = envGetLongConfigParam(
			&EPICS_IOC_LOG_PORT, 
			&param);
	if(status>=0){
		ioc_log_port = (unsigned short) param;
	}
	else {
		ioc_log_port = 7004U;
	}

	status = envGetLongConfigParam(
			&EPICS_IOC_LOG_FILE_LIMIT, 
			&ioc_log_file_limit);
	if(status<0){
		ioc_log_file_limit = 10000;
	}

	pstring = envGetConfigParam(
			&EPICS_IOC_LOG_FILE_NAME, 
			sizeof ioc_log_file_name,
			ioc_log_file_name);
	if(pstring == NULL){
		envFailureNotify(&EPICS_IOC_LOG_FILE_NAME);
		return ERROR;
	}
	return OK;
}



/*
 *
 *	failureNotify()
 *
 *
 */
static void envFailureNotify(ENV_PARAM *pparam)
{
	fprintf(stderr,
		"iocLogServer: EPICS environment variable `%s' undefined\n",
		pparam->name);
}
