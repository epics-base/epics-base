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
 * Revision 1.30  1998/06/16 02:43:16  jhill
 * use ip addr conversion in libCom - and cosmetic changes
 *
 * Revision 1.29  1998/05/29 20:19:11  jhill
 * use new sock ioctl() typedef
 *
 * Revision 1.28  1998/02/05 23:16:35  jhill
 * fixed truncate return status logic error
 * now uses macros from osiSock.h
 *
 * Revision 1.27  1997/08/05 00:43:19  jhill
 * fixed warning
 *
 * Revision 1.26  1997/06/25 05:59:23  jhill
 * ported log server to win32
 *
 * Revision 1.25  1997/04/10 19:53:03  jhill
 * api changes
 *
 * Revision 1.24  1996/11/22 20:43:03  jhill
 * doc
 *
 * Revision 1.22  1996/11/02 02:20:36  jhill
 * use osiSock.h
 *
 * Revision 1.21  1996/06/21 01:07:46  jhill
 * use sigemptyset() and cc -Xc changes
 *
 * Revision 1.20  1996/06/19 18:03:17  jhill
 * SIGHUP changes added by KECK
 *
 * Revision 1.18  1995/11/27  22:49:36  jhill
 * included <arpa/inet.h>
 *
 * Revision 1.17  1995/11/13  16:55:03  jba
 * Added filio.h include for solaris build.
 *
 * Revision 1.16  1995/11/08  23:48:26  jhill
 * improvents for better client reconnect
 *
 */

static char	*pSCCSID = "@(#)iocLogServer.c	1.9\t05/05/94";

/*
 * Under solaris if dont define _POSIX_C_SOURCE or _XOPEN_SOURCE
 * then none of the POSIX stuff (such as signals or pipes) can be used
 * with cc -v. However if one of _POSIX_C_SOURCE or _XOPEN_SOURCE
 * are defined then we cant use the socket library. Therefore I 
 * have been adding the following in order to use POSIX signals 
 * and also sockets on solaris with cc -v. What a pain....
 */
#if defined(SOLARIS)
#define __EXTENSIONS__ 
#endif

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include 	<stdio.h>
#include	<limits.h>

#ifdef UNIX
#include 	<unistd.h>
#include	<signal.h>
#endif

#include	"epicsAssert.h"
#include 	"fdmgr.h"
#include 	"envDefs.h"
#include 	"osiSock.h"
#include	"truncateFile.h"
#include	"bsdSocketResource.h"

static unsigned short	ioc_log_port;
static long		ioc_log_file_limit;
static char		ioc_log_file_name[256];
static char		ioc_log_file_command[256];


#ifndef TRUE
#define	TRUE	1
#endif
#ifndef FALSE
#define	FALSE	0
#endif

struct iocLogClient {
	int			insock;
	struct ioc_log_server	*pserver;
	size_t			nChar;
	char			recvbuf[1024];
	char			name[32];
	char			ascii_time[32];
};

struct ioc_log_server {
	char outfile[256];
	long filePos;
	FILE *poutfile;
	void *pfdctx;
	SOCKET sock;
	long max_file_size;
};


#define IOCLS_ERROR (-1)
#define IOCLS_OK 0

static void acceptNewClient (void *pParam);
static void readFromClient(void *pParam);
static void logTime (struct iocLogClient *pclient);
static int getConfig(void);
static int openLogFile(struct ioc_log_server *pserver);
static void handleLogFileError(void);
static void envFailureNotify(const ENV_PARAM *pparam);
static void freeLogClient(struct iocLogClient *pclient);
static void writeMessagesToLog (struct iocLogClient *pclient);

#ifdef UNIX
static int setupSIGHUP(struct ioc_log_server *);
static void sighupHandler(int);
static void serviceSighupRequest(void *pParam);
static int getDirectory(void);
static int sighupPipe[2];
#endif



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

	osiSockIoctl_t	optval;

	status = getConfig();
	if(status<0){
		fprintf(stderr, "iocLogServer: EPICS environment underspecified\n");
		fprintf(stderr, "iocLogServer: failed to initialize\n");
		return IOCLS_ERROR;
	}

	pserver = (struct ioc_log_server *) 
			calloc(1, sizeof *pserver);
	if(!pserver){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return IOCLS_ERROR;
	}

	pserver->pfdctx = (void *) fdmgr_init();
	if(!pserver->pfdctx){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return IOCLS_ERROR;
	}

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	pserver->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (pserver->sock==INVALID_SOCKET) {
		fprintf(stderr, "iocLogServer: %d=%s\n", SOCKERRNO, SOCKERRSTR);
		return IOCLS_ERROR;
	}
	
	optval = TRUE;
	status = setsockopt(    pserver->sock,
							SOL_SOCKET,
							SO_REUSEADDR,
							(char *) &optval,
							sizeof(optval));
	if(status<0){
		fprintf(stderr, "iocLogServer: %d=%s\n", SOCKERRNO, SOCKERRSTR);
		return IOCLS_ERROR;
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
		fprintf(stderr, "iocLogServer: %d=%s\n", SOCKERRNO, SOCKERRSTR);
		return IOCLS_ERROR;
	}

	/* listen and accept new connections */
	status = listen(pserver->sock, 10);
	if (status<0) {
		fprintf(stderr, "iocLogServer: %d=%s\n", SOCKERRNO, SOCKERRSTR);
		return IOCLS_ERROR;
	}

	/*
	 * Set non blocking IO
	 * to prevent dead locks
	 */
	optval = TRUE;
	status = socket_ioctl(
					pserver->sock,
					FIONBIO,
					&optval);
	if(status<0){
		fprintf(stderr, "iocLogServer: %d=%s\n", SOCKERRNO, SOCKERRSTR);
		return IOCLS_ERROR;
	}

#	ifdef UNIX
		status = setupSIGHUP(pserver);
		if (status<0) {
			return IOCLS_ERROR;
		}
#	endif

	status = openLogFile(pserver);
	if (status<0) {
		fprintf(stderr,
			"File access problems to `%s' because `%s'\n", 
			ioc_log_file_name,
			strerror(errno));
		return IOCLS_ERROR;
	}

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			pserver->sock, 
			fdi_read,
			acceptNewClient,
			pserver);
	if(status<0){
		fprintf(stderr,
			"iocLogServer: failed to add read callback\n");
		return IOCLS_ERROR;
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
static int openLogFile (struct ioc_log_server *pserver)
{
	int status;
	enum TF_RETURN ret;

	if (ioc_log_file_limit==0u) {
		pserver->poutfile = stderr;
		return IOCLS_ERROR;
	}

	if (pserver->poutfile && pserver->poutfile != stderr){
		fclose (pserver->poutfile);
		pserver->poutfile = NULL;
	}

	pserver->poutfile = fopen(ioc_log_file_name, "r+");
	if (pserver->poutfile) {
		fclose (pserver->poutfile);
		pserver->poutfile = NULL;
		ret = truncateFile (ioc_log_file_name, ioc_log_file_limit);
		if (ret==TF_ERROR) {
			return IOCLS_ERROR;
		}
		pserver->poutfile = fopen(ioc_log_file_name, "r+");
	}
	else {
		pserver->poutfile = fopen(ioc_log_file_name, "w");
	}

	if (!pserver->poutfile) {
		pserver->poutfile = stderr;
		return IOCLS_ERROR;
	}
	strcpy (pserver->outfile, ioc_log_file_name);
	pserver->max_file_size = ioc_log_file_limit;

	/*
	 * This is not required under UNIX but does appear
	 * to be required under WIN32. 
	 */
	status = fseek (pserver->poutfile, 0L, SEEK_END);
	if (status!=IOCLS_OK) {
		fclose (pserver->poutfile);
		pserver->poutfile = stderr;
		return IOCLS_ERROR;
	}

	pserver->filePos = ftell (pserver->poutfile);

	return IOCLS_OK;
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
	exit(IOCLS_ERROR);
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
	int			status;
	osiSockIoctl_t	optval;

	pclient = (struct iocLogClient *) 
			malloc(sizeof *pclient);
	if(!pclient){
		return;
	}

	size = sizeof(addr);
	pclient->insock = accept(pserver->sock, (struct sockaddr *)&addr, &size);
	if (pclient->insock<0 || size<sizeof(addr)) {
		free(pclient);
		if (SOCKERRNO!=SOCK_EWOULDBLOCK) {
			fprintf(stderr, "Accept Error %d\n", SOCKERRNO);
		}
		return;
	}

	/*
	 * Set non blocking IO
	 * to prevent dead locks
	 */
	optval = TRUE;
	status = socket_ioctl(
					pclient->insock,
					FIONBIO,
					&optval);
	if(status<0){
		socket_close(pclient->insock);
		free(pclient);
		fprintf(stderr, "%s:%d %s\n", 
			__FILE__, __LINE__, SOCKERRSTR);
		return;
	}

	pclient->pserver = pserver;
	pclient->nChar = 0u;

	ipAddrToA (&addr, pclient->name, sizeof(pclient->name));

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

#	define SOCKET_SHUTDOWN_WRITE_SIDE 1
	status = shutdown(pclient->insock, SOCKET_SHUTDOWN_WRITE_SIDE);
	if(status<0){
		socket_close(pclient->insock);
		free(pclient);
		printf("%s:%d %s\n", __FILE__, __LINE__,
				SOCKERRSTR);
		return;
	}

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			pclient->insock, 
			fdi_read,
			readFromClient,
			pclient);
	if (status<0) {
		socket_close(pclient->insock);
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
	int             	recvLength;
	int			size;

	logTime(pclient);

	size = (int) (sizeof(pclient->recvbuf) - pclient->nChar);
	recvLength = recv(pclient->insock,
		      &pclient->recvbuf[pclient->nChar],
		      size,
		      0);
	if (recvLength <= 0) {
		if (recvLength<0) {
			if (SOCKERRNO==SOCK_EWOULDBLOCK || SOCKERRNO==SOCK_EINTR) {
				return;
			}
			if (	SOCKERRNO != SOCK_ECONNRESET &&
				SOCKERRNO != SOCK_ECONNABORTED &&
				SOCKERRNO != SOCK_EPIPE &&
				SOCKERRNO != SOCK_ETIMEDOUT
				) {
				fprintf(stderr, 
		"%s:%d socket=%d size=%d read error=%s errno=%d\n",
					__FILE__, __LINE__, pclient->insock, 
					size, SOCKERRSTR, SOCKERRNO);
			}
		}
		/*
		 * disconnect
		 */
		freeLogClient (pclient);
		return;
	}

	pclient->nChar += (size_t) recvLength;

	writeMessagesToLog (pclient);
}

/*
 * writeMessagesToLog()
 */
static void writeMessagesToLog (struct iocLogClient *pclient)
{
	struct ioc_log_server	*pserver = pclient->pserver;
	int             	status;
	char			*pcr;
	char			*pline;
	
	pline = pclient->recvbuf;
	while (TRUE) {
		size_t nchar;
		size_t nTotChar;
		int ntci;

		if (pline >= &pclient->recvbuf[pclient->nChar]) {
			pclient->nChar = 0u;
			break;
		}

		/*
		 * find the first carrage return and create
		 * an entry in the log for the message associated
		 * with it. If a carrage return does not exist and 
		 * the buffer isnt full then move the partial message 
		 * to the front of the buffer and wait for a carrage 
		 * return to arrive. If the buffer is full and there
		 * is no carrage return then force the message out and 
		 * insert an artificial carrage return.
		 */
		pcr = strchr(pline, '\n');
		if (pcr) {
			nchar = pcr-pline;
		}
		else {
			nchar = pclient->nChar - (pline - pclient->recvbuf);
			if (nchar<sizeof(pclient->recvbuf)) {
				if (pline != pclient->recvbuf) {
					pclient->nChar = nchar;
					memmove (pclient->recvbuf, pline, nchar);
				}
				break;
			}
		}

		/*
		 * reset the file pointer if we hit the end of the file
		 */
		nTotChar = strlen(pclient->name) +
				strlen(pclient->ascii_time) + nchar + 3u;
		assert (nTotChar <= INT_MAX);
		ntci = (int) nTotChar;
		if (pserver->filePos+ntci >= pserver->max_file_size) {
			if (pserver->max_file_size>=pserver->filePos) {
				unsigned nPadChar;
				/*
				 * this gets rid of leftover junk at the end of the file
				 */
				nPadChar = pserver->max_file_size - pserver->filePos;
				while (nPadChar--) {
					status = putc(' ', pserver->poutfile);
					if (status == EOF) {
						handleLogFileError();
					}
				}
			}

#			ifdef DEBUG
				fprintf(stderr,
					"ioc log server: resetting the file pointer\n");
#			endif
			fflush (pserver->poutfile);
			rewind (pserver->poutfile);
			pserver->filePos = ftell(pserver->poutfile);
		}
	
		/*
		 * NOTE: !! change format string here then must
		 * change nTotChar calc above !!
		 */
		assert (nchar<INT_MAX);
		status = fprintf(
			pserver->poutfile,
			"%s %s %.*s\n",
			pclient->name,
			pclient->ascii_time,
			(int) nchar,
			pline);
		if (status<0) {
			handleLogFileError();
		}
        else {
		    if (status != ntci) {
			    fprintf(stderr, "iocLogServer: didnt calculate number of characters correctly?\n");
		    }
		    pserver->filePos += status;
        }
		pline += nchar+1u;
	}
}


/*
 * freeLogClient ()
 */
static void freeLogClient(struct iocLogClient     *pclient)
{
	int		status;

#	ifdef	DEBUG
	if(length == 0){
		fprintf(stderr, "iocLogServer: nil message disconnect\n");
	}
#	endif

	/*
	 * flush any left overs
	 */
	if (pclient->nChar) {
		/*
		 * this forces a flush
		 */
		if (pclient->nChar<sizeof(pclient->recvbuf)) {
			pclient->recvbuf[pclient->nChar] = '\n';
		}
		writeMessagesToLog (pclient);
	}

	status = fdmgr_clear_callback(
		       pclient->pserver->pfdctx,
		       pclient->insock,
		       fdi_read);
	if (status!=IOCLS_OK) {
		fprintf(stderr, "%s:%d fdmgr_clear_callback() failed\n",
			__FILE__, __LINE__);
	}

	if(socket_close(pclient->insock)<0)
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
	if(status>=0){
		if (ioc_log_file_limit<=0) {
			envFailureNotify (&EPICS_IOC_LOG_FILE_LIMIT);
			return IOCLS_ERROR;
		}
	}
	else {
		ioc_log_file_limit = 10000;
	}

	pstring = envGetConfigParam(
			&EPICS_IOC_LOG_FILE_NAME, 
			sizeof ioc_log_file_name,
			ioc_log_file_name);
	if(pstring == NULL){
		envFailureNotify(&EPICS_IOC_LOG_FILE_NAME);
		return IOCLS_ERROR;
	}

	/*
	 * its ok to not specify the IOC_LOG_FILE_COMMAND
	 */
	pstring = envGetConfigParam(
			&EPICS_IOC_LOG_FILE_COMMAND, 
			sizeof ioc_log_file_command,
			ioc_log_file_command);
	return IOCLS_OK;
}



/*
 *
 *	failureNotify()
 *
 *
 */
static void envFailureNotify(const ENV_PARAM *pparam)
{
	fprintf(stderr,
		"iocLogServer: EPICS environment variable `%s' undefined\n",
		pparam->name);
}



#ifdef UNIX
static int setupSIGHUP(struct ioc_log_server *pserver)
{
	int status;
	struct sigaction sigact;

	status = getDirectory();
	if (status<0){
		fprintf(stderr, "iocLogServer: failed to determine log file "
			"directory\n");
		return IOCLS_ERROR;
	}

	/*
	 * Set up SIGHUP handler. SIGHUP will cause the log file to be
	 * closed and re-opened, possibly with a different name.
	 */
	sigact.sa_handler = sighupHandler;
	sigemptyset (&sigact.sa_mask);
	sigact.sa_flags = 0;
	if (sigaction(SIGHUP, &sigact, NULL)){
		fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
		return IOCLS_ERROR;
	}
	
	status = pipe(sighupPipe);
	if(status<0){
                fprintf(stderr,
                        "iocLogServer: failed to create pipe because `%s'\n",
                        strerror(errno));
                return IOCLS_ERROR;
        }

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			sighupPipe[0], 
			fdi_read,
			serviceSighupRequest,
			pserver);
	if(status<0){
		fprintf(stderr,
			"iocLogServer: failed to add SIGHUP callback\n");
		return IOCLS_ERROR;
	}
	return IOCLS_OK;
}

/*
 *
 *	sighupHandler()
 *
 *
 */
static void sighupHandler(int signo)
{
	(void) write(sighupPipe[1], "SIGHUP\n", 7);
}



/*
 *	serviceSighupRequest()
 *
 */
static void serviceSighupRequest(void *pParam)
{
	struct ioc_log_server	*pserver = (struct ioc_log_server *)pParam;
	char			buff[256];
	int			status;

	/*
	 * Read and discard message from pipe.
	 */
	(void) read(sighupPipe[0], buff, sizeof buff);

	/*
	 * Determine new log file name.
	 */
	status = getDirectory();
	if (status<0){
		fprintf(stderr, "iocLogServer: failed to determine new log "
			"file name\n");
		return;
	}

	/*
	 * If it's changed, open the new file.
	 */
	if (strcmp(ioc_log_file_name, pserver->outfile) == 0) {
		fprintf(stderr,
			"iocLogServer: log file name unchanged; not re-opened\n");
	}
	else {
		status = openLogFile(pserver);
		if(status<0){
			fprintf(stderr,
				"File access problems to `%s' because `%s'\n", 
				ioc_log_file_name,
				strerror(errno));
			strcpy(ioc_log_file_name, pserver->outfile);
			status = openLogFile(pserver);
			if(status<0){
				fprintf(stderr,
                                "File access problems to `%s' because `%s'\n",
                                ioc_log_file_name,
                                strerror(errno));
				return;
			}
			else {
				fprintf(stderr,
				"iocLogServer: re-opened old log file %s\n",
				ioc_log_file_name);
			}
		}
		else {
			fprintf(stderr,
				"iocLogServer: opened new log file %s\n",
				ioc_log_file_name);
		}
	}
}



/*
 *
 *	getDirectory()
 *
 *
 */
static int getDirectory()
{
	FILE		*pipe;
	char		dir[256];
	int		i;

	if (ioc_log_file_command[0] != '\0') {

		/*
		 * Use popen() to execute command and grab output.
		 */
		pipe = popen(ioc_log_file_command, "r");
		if (pipe == NULL) {
			fprintf(stderr,
				"Problem executing `%s' because `%s'\n", 
				ioc_log_file_command,
				strerror(errno));
			return IOCLS_ERROR;
		}
		if (fgets(dir, sizeof(dir), pipe) == NULL) {
			fprintf(stderr,
				"Problem reading o/p from `%s' because `%s'\n", 
				ioc_log_file_command,
				strerror(errno));
			return IOCLS_ERROR;
		}
		(void) pclose(pipe);

		/*
		 * Terminate output at first newline and discard trailing
		 * slash character if present..
		 */
		for (i=0; dir[i] != '\n' && dir[i] != '\0'; i++)
			;
		dir[i] = '\0';

		i = strlen(dir);
		if (i > 1 && dir[i-1] == '/') dir[i-1] = '\0';

		/*
		 * Use output as directory part of file name.
		 */
		if (dir[0] != '\0') {
			char *name = ioc_log_file_name;
			char *slash = strrchr(ioc_log_file_name, '/');
			char temp[256];

			if (slash != NULL) name = slash + 1;
			strcpy(temp,name);
			sprintf(ioc_log_file_name,"%s/%s",dir,temp);
		}
	}
	return IOCLS_OK;
}
#endif
