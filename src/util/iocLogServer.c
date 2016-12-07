/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* iocLogServer.c */

/*
 *	archive logMsg() from several IOC's to a common rotating file
 *
 *
 * 	    Author: 	Jeffrey O. Hill 
 *      Date:       080791 
 */

#include	<stdlib.h>
#include	<string.h>
#include	<errno.h>
#include 	<stdio.h>
#include	<limits.h>
#include	<time.h>

#ifdef UNIX
#include 	<unistd.h>
#include	<signal.h>
#endif

#include        "dbDefs.h"
#include	"epicsAssert.h"
#include 	"fdmgr.h"
#include 	"envDefs.h"
#include 	"osiSock.h"
#include	"epicsStdio.h"

static unsigned short ioc_log_port;
static long ioc_log_file_limit;
static char ioc_log_file_name[256];
static char ioc_log_file_command[256];


struct iocLogClient {
	int insock;
	struct ioc_log_server *pserver;
	size_t nChar;
	char recvbuf[1024];
	char name[32];
	char ascii_time[32];
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
 * main()
 *
 */
int main(void)
{
    struct sockaddr_in serverAddr;  /* server's address */
    struct timeval timeout;
    int status;
    struct ioc_log_server *pserver;

    osiSockIoctl_t  optval;

    status = getConfig();
    if (status<0) {
        fprintf(stderr, "iocLogServer: EPICS environment underspecified\n");
        fprintf(stderr, "iocLogServer: failed to initialize\n");
        return IOCLS_ERROR;
    }

    pserver = (struct ioc_log_server *) 
            calloc(1, sizeof *pserver);
    if (!pserver) {
        fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
        return IOCLS_ERROR;
    }

    pserver->pfdctx = (void *) fdmgr_init();
    if (!pserver->pfdctx) {
        fprintf(stderr, "iocLogServer: %s\n", strerror(errno));
        return IOCLS_ERROR;
    }

    /*
     * Open the socket. Use ARPA Internet address format and stream
     * sockets. Format described in <sys/socket.h>.
     */
    pserver->sock = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    if (pserver->sock == INVALID_SOCKET) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf(stderr, "iocLogServer: sock create err: %s\n", sockErrBuf);
        free(pserver);
        return IOCLS_ERROR;
    }
    
    epicsSocketEnableAddressReuseDuringTimeWaitState ( pserver->sock );

    /* Zero the sock_addr structure */
    memset((void *)&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(ioc_log_port);

    /* get server's Internet address */
    status = bind ( pserver->sock, 
            (struct sockaddr *)&serverAddr, 
            sizeof (serverAddr) );
    if (status < 0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf(stderr, "iocLogServer: bind err: %s\n", sockErrBuf );
        fprintf (stderr,
            "iocLogServer: a server is already installed on port %u?\n", 
            (unsigned)ioc_log_port);
        return IOCLS_ERROR;
    }

    /* listen and accept new connections */
    status = listen(pserver->sock, 10);
    if (status < 0) {
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf(stderr, "iocLogServer: listen err %s\n", sockErrBuf);
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
    if (status < 0){
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
        fprintf(stderr, "iocLogServer: ioctl FIONBIO err %s\n", sockErrBuf);
        return IOCLS_ERROR;
    }

#   ifdef UNIX
        status = setupSIGHUP(pserver);
        if (status < 0) {
            return IOCLS_ERROR;
        }
#   endif

    status = openLogFile(pserver);
    if (status < 0) {
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
    if (status < 0) {
        fprintf(stderr,
            "iocLogServer: failed to add read callback\n");
        return IOCLS_ERROR;
    }


    while (TRUE) {
        timeout.tv_sec = 60; /* 1 min */
        timeout.tv_usec = 0;
        fdmgr_pend_event(pserver->pfdctx, &timeout);
        fflush(pserver->poutfile);
    }
}

/*
 * seekLatestLine (struct ioc_log_server *pserver)
 */
static int seekLatestLine (struct ioc_log_server *pserver)
{
    static const time_t invalidTime = (time_t) -1;
    time_t theLatestTime = invalidTime;
    long latestFilePos = -1;
    int status;

    /*
     * start at the beginning
     */
    rewind (pserver->poutfile);

    while (1) {
        struct tm theDate;
        int convertStatus;
        char month[16];

        /*
         * find the line in the file with the latest date
         *
         * this assumes ctime() produces dates of the form:
         * DayName MonthName dayNum 24hourHourNum:minNum:secNum yearNum
         */
        convertStatus = fscanf (
            pserver->poutfile, " %*s %*s %15s %d %d:%d:%d %d %*[^\n] ",
            month, &theDate.tm_mday, &theDate.tm_hour, 
            &theDate.tm_min, &theDate.tm_sec, &theDate.tm_year);
        if (convertStatus==6) {
            static const char *pMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            static const unsigned nMonths = sizeof(pMonths)/sizeof(pMonths[0]);
            time_t lineTime = (time_t) -1;
            unsigned iMonth;

            for (iMonth=0; iMonth<nMonths; iMonth++) {
                if ( strcmp (pMonths[iMonth], month)==0 ) {
                    theDate.tm_mon = iMonth;
                    break;
                }
            }
            if (iMonth<nMonths) {
                static const int tm_epoch_year = 1900;
                if (theDate.tm_year>tm_epoch_year) {
                    theDate.tm_year -= tm_epoch_year;
                    theDate.tm_isdst = -1; /* dont know */
                    lineTime = mktime (&theDate);
                    if ( lineTime != invalidTime ) {
                        if (theLatestTime == invalidTime || 
                            difftime(lineTime, theLatestTime)>=0) {
                            latestFilePos =  ftell (pserver->poutfile);
                            theLatestTime = lineTime;
                        }
                    }
                    else {
                        char date[128];
                        size_t nChar;
                        nChar = strftime (date, sizeof(date), "%a %b %d %H:%M:%S %Y\n", &theDate);
                        if (nChar>0) {
                            fprintf (stderr, "iocLogServer: strange date in log file: %s\n", date);
                        }
                        else {
                            fprintf (stderr, "iocLogServer: strange date in log file\n");
                        }
                    }
                }
                else {
                    fprintf (stderr, "iocLogServer: strange year in log file: %d\n", theDate.tm_year);
                }
            }
            else {
                fprintf (stderr, "iocLogServer: strange month in log file: %s\n", month);
            }
        }
        else {
            int c = fgetc (pserver->poutfile);
 
            /*
             * bypass the line if it does not match the expected format
             */
            while ( c!=EOF && c!='\n' ) {
                c = fgetc (pserver->poutfile);
            }

            if (c==EOF) {
                break;
            }
        }
    }

    /*
     * move to the proper location in the file
     */
    if (latestFilePos != -1) {
	    status = fseek (pserver->poutfile, latestFilePos, SEEK_SET);
	    if (status!=IOCLS_OK) {
		    fclose (pserver->poutfile);
		    pserver->poutfile = stderr;
		    return IOCLS_ERROR;
	    }
    }
    else {
	    status = fseek (pserver->poutfile, 0L, SEEK_END);
	    if (status!=IOCLS_OK) {
		    fclose (pserver->poutfile);
		    pserver->poutfile = stderr;
		    return IOCLS_ERROR;
	    }
    }

    pserver->filePos = ftell (pserver->poutfile);

    if (theLatestTime==invalidTime) {
        if (pserver->filePos!=0) {
            fprintf (stderr, "iocLogServer: **** Warning ****\n");
            fprintf (stderr, "iocLogServer: no recognizable dates in \"%s\"\n", 
                ioc_log_file_name);
            fprintf (stderr, "iocLogServer: logging at end of file\n");
        }
    }

	return IOCLS_OK;
}


/*
 *	openLogFile()
 *
 */
static int openLogFile (struct ioc_log_server *pserver)
{
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

    return seekLatestLine (pserver);
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
static void acceptNewClient ( void *pParam )
{
	struct ioc_log_server *pserver = (struct ioc_log_server *) pParam;
	struct iocLogClient	*pclient;
	osiSocklen_t addrSize;
	struct sockaddr_in addr;
	int status;
	osiSockIoctl_t optval;

	pclient = ( struct iocLogClient * ) malloc ( sizeof ( *pclient ) );
	if ( ! pclient ) {
		return;
	}

	addrSize = sizeof ( addr );
	pclient->insock = epicsSocketAccept ( pserver->sock, (struct sockaddr *)&addr, &addrSize );
	if ( pclient->insock==INVALID_SOCKET || addrSize < sizeof (addr) ) {
        static unsigned acceptErrCount;
        static int lastErrno;
        int thisErrno;

		free ( pclient );
		if ( SOCKERRNO == SOCK_EWOULDBLOCK || SOCKERRNO == SOCK_EINTR ) {
            return;
		}

        thisErrno = SOCKERRNO;
        if ( acceptErrCount % 1000 || lastErrno != thisErrno ) {
            fprintf ( stderr, "Accept Error %d\n", SOCKERRNO );
        }
        acceptErrCount++;
        lastErrno = thisErrno;

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
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		fprintf(stderr, "%s:%d ioctl FBIO client er %s\n", 
			__FILE__, __LINE__, sockErrBuf);
		epicsSocketDestroy ( pclient->insock );
		free(pclient);
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
		long true = 1;

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

	status = shutdown(pclient->insock, SHUT_WR);
	if(status<0){
        char sockErrBuf[64];
        epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
		fprintf (stderr, "%s:%d shutdown err %s\n", __FILE__, __LINE__,
				sockErrBuf);
        epicsSocketDestroy ( pclient->insock );
		free(pclient);

		return;
	}

	status = fdmgr_add_callback(
			pserver->pfdctx, 
			pclient->insock, 
			fdi_read,
			readFromClient,
			pclient);
	if (status<0) {
		epicsSocketDestroy ( pclient->insock );
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
            int errnoCpy = SOCKERRNO;
			if (errnoCpy==SOCK_EWOULDBLOCK || errnoCpy==SOCK_EINTR) {
				return;
			}
			if (errnoCpy != SOCK_ECONNRESET &&
				errnoCpy != SOCK_ECONNABORTED &&
				errnoCpy != SOCK_EPIPE &&
				errnoCpy != SOCK_ETIMEDOUT
				) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString ( sockErrBuf, sizeof ( sockErrBuf ) );
				fprintf(stderr, 
		"%s:%d socket=%d size=%d read error=%s\n",
					__FILE__, __LINE__, pclient->insock, 
					size, sockErrBuf);
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
	int status;
    size_t lineIndex = 0;
	
	while (TRUE) {
		size_t nchar;
		size_t nTotChar;
        size_t crIndex;
		int ntci;

		if ( lineIndex >= pclient->nChar ) {
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
		nchar = pclient->nChar - lineIndex;
        for ( crIndex = lineIndex; crIndex < pclient->nChar; crIndex++ ) {
            if ( pclient->recvbuf[crIndex] == '\n' ) {
                break;
            }
        }
		if ( crIndex < pclient->nChar ) {
			nchar = crIndex - lineIndex;
        }
        else {
		    nchar = pclient->nChar - lineIndex;
			if ( nchar < sizeof ( pclient->recvbuf ) ) {
				if ( lineIndex != 0 ) {
					pclient->nChar = nchar;
					memmove ( pclient->recvbuf, 
                        & pclient->recvbuf[lineIndex], nchar);
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
		if ( pclient->pserver->filePos+ntci >= pclient->pserver->max_file_size ) {
			if ( pclient->pserver->max_file_size >= pclient->pserver->filePos ) {
				unsigned nPadChar;
				/*
				 * this gets rid of leftover junk at the end of the file
				 */
				nPadChar = pclient->pserver->max_file_size - pclient->pserver->filePos;
				while (nPadChar--) {
					status = putc ( ' ', pclient->pserver->poutfile );
					if ( status == EOF ) {
						handleLogFileError();
					}
				}
			}

#			ifdef DEBUG
				fprintf ( stderr,
					"ioc log server: resetting the file pointer\n" );
#			endif
			fflush ( pclient->pserver->poutfile );
			rewind ( pclient->pserver->poutfile );
			pclient->pserver->filePos = ftell ( pclient->pserver->poutfile );
		}
	
		/*
		 * NOTE: !! change format string here then must
		 * change nTotChar calc above !!
		 */
		assert (nchar<INT_MAX);
		status = fprintf(
			pclient->pserver->poutfile,
			"%s %s %.*s\n",
			pclient->name,
			pclient->ascii_time,
			(int) nchar,
			&pclient->recvbuf[lineIndex]);
		if (status<0) {
			handleLogFileError();
		}
        else {
		    if (status != ntci) {
			    fprintf(stderr, "iocLogServer: didnt calculate number of characters correctly?\n");
		    }
		    pclient->pserver->filePos += status;
        }
		lineIndex += nchar+1u;
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

	epicsSocketDestroy ( pclient->insock );

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
	 * Try (re)opening the file.
	 */
	status = openLogFile(pserver);
	if(status<0){
		fprintf(stderr,
			"File access problems to `%s' because `%s'\n", 
			ioc_log_file_name,
			strerror(errno));
		/* Revert to old filename */
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



/*
 *
 *	getDirectory()
 *
 *
 */
static int getDirectory(void)
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
			(void) pclose(pipe);
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
