/* iocLogServer.c */
/* share/src/util  $Id$ */

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
 * joh	00	080791	Created
 */

#include	<stdio.h>
#include	<string.h>
#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/socket.h>
#include	<netinet/in.h>
#include        <netdb.h>
#include 	<envDefs.h>

static long 		ioc_log_port;
static struct in_addr 	ioc_log_addr;
static long		ioc_log_file_limit;
static long		ioc_log_file_name[64];


#ifndef TRUE
#define	TRUE			1
#endif
#ifndef FALSE
#define	FALSE			0
#endif

void	acceptNewClient();
void	readFromClient();
void	logTime();
int	getConfig();
void	failureNoptify();

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

extern int errno;

#ifndef ERROR
#define ERROR -1
#endif

#ifndef OK
#define OK 0
#endif


/*
 *
 *	main()
 *
 */
main()
{
	struct sockaddr_in 	serverAddr;	/* server's address */
	struct timeval          timeout;
	int			status;
	struct ioc_log_server	*pserver;

	status = getConfig();
	if(status<0){
		printf("iocLogServer: EPICS environment inderspecified\n");
		printf("iocLogServer: failed to initialize\n");
		exit(ERROR);
	}

	pserver = (struct ioc_log_server *) 
			malloc(sizeof *pserver);
	if(!pserver){
		abort();
	}

	pserver->pfdctx = (void *) fdmgr_init();
	if(!pserver->pfdctx){
		abort();
	}

	/*
	 * Open the socket. Use ARPA Internet address format and stream
	 * sockets. Format described in <sys/socket.h>.
	 */
	pserver->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (pserver->sock<0) {
		abort();
	}
	
	/* Zero the sock_addr structure */
	memset(&serverAddr, 0, sizeof serverAddr);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(ioc_log_port);

	/* get server's Internet address */
	status = bind(pserver->sock, &serverAddr, sizeof serverAddr);
	if (status<0) {
		printf(	"ioc log server allready installed on port %d?\n", 
			ioc_log_port);
		return;
	}

	/* listen and accept new connections */
	status = listen(pserver->sock, 10);
	if (status<0) {
		abort();
	}

	pserver->max_file_size = ioc_log_file_limit; 
	pserver->poutfile = fopen(ioc_log_file_name, "a+");
	if(!pserver->poutfile){
		printf("File access problems `%s'\n", ioc_log_file_name);
		return;
	}

	fdmgr_add_fd(
		pserver->pfdctx, 
		pserver->sock, 
		acceptNewClient, 
		pserver);

	while(TRUE){
		timeout.tv_sec = 60; /* 1 min */
		timeout.tv_usec = 0;
		fdmgr_pend_event(pserver->pfdctx, &timeout);
		fflush(pserver->poutfile);
	}
}


/*
 *	acceptNewClient()
 *
 */
static void
acceptNewClient(pserver)
struct ioc_log_server	*pserver;
{
	struct iocLogClient	*pclient;
	int			size;
	struct sockaddr_in 	addr;
	char			*pname;
	int			status;


	pclient = (struct iocLogClient *) 
			malloc(sizeof *pclient);
	if(!pclient){
		return;
	}

	pclient->insock = accept(pserver->sock, NULL, 0);
	if(pclient->insock<0){
		free(pclient);
		printf("Accept Error %d\n", errno);
		return;
	}

	pclient->pserver = pserver;
	pclient->need_prefix = TRUE;
	pclient->ptopofstack = pclient->recvbuf;

	pname = "<ukn>";
        size = sizeof addr;
	memset(&addr, 0, sizeof addr);
        status = getpeername(
                        pclient->insock,
                        &addr, 
                        &size); 
        if(status>=0){
  		struct hostent      *pent;   

    		pent = gethostbyaddr(
				&addr.sin_addr, 
				sizeof addr.sin_addr, 
				AF_INET);
		if(pent){
			pname = pent->h_name;
		}else{
			pname = (char *) inet_ntoa(&addr.sin_addr);
		}
        }

	strncpy(pclient->name, pname, sizeof(pclient->name));
	pclient->name[sizeof(pclient->name) - 1] = NULL;

	logTime(pclient);
	fprintf(
		pclient->pserver->poutfile,
		"%s %s ----- Client Connected -----\n",
		pclient->name,
		pclient->ascii_time);


	fdmgr_add_fd(
		pserver->pfdctx, 
		pclient->insock, 
		readFromClient,
		pclient);
}



/*
 * readFromClient()
 * 
 */
#define NITEMS 1

static void
readFromClient(pclient)
	struct iocLogClient *pclient;
{
	int             status;
	int             length;
	char		*pcr;
	char		*pline;
	unsigned	stacksize;

	logTime(pclient);

	stacksize = pclient->ptopofstack - pclient->recvbuf;
	length = read(pclient->insock,
		      pclient->ptopofstack,
		      sizeof(pclient->recvbuf)-stacksize-1);
	if (length < 0) {
		/*
		 * flush any leftovers
		 */
		if(stacksize){
			fprintf(
				pclient->pserver->poutfile,
				"%s %s ",
				pclient->name,
				pclient->ascii_time);
			status = fwrite(
					pclient->recvbuf,
					stacksize,
					NITEMS,
					pclient->pserver->poutfile);
			if (status != NITEMS) {
				printf("ioc log server: failed to flush %d\n",
				       errno);
			}
			fprintf(pclient->pserver->poutfile,"\n");
		}

		fprintf(
			pclient->pserver->poutfile,
			"%s %s ----- Client Disconnect -----\n",
			pclient->name,
			pclient->ascii_time);

		fdmgr_clear_fd(
			       pclient->pserver->pfdctx,
			       pclient->insock);
		if(close(pclient->insock)<0)
			abort();
		if(free(pclient)<0)
			abort();
		return;
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

		fprintf(
			pclient->pserver->poutfile,
			"%s %s ",
			pclient->name,
			pclient->ascii_time);

		status = fwrite(
				pline,
				nchar,
				NITEMS,
				pclient->pserver->poutfile);
		if (status != NITEMS) {
			printf("ioc log server: failed to write log %d\n",
			       errno);
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
#define 	FILE_BEGIN 0
		printf("ioc log server: resetting the file pointer\n");
		fseek(pclient->pserver->poutfile, 0, FILE_BEGIN);
		status = ftruncate(
				   fileno(pclient->pserver->poutfile),
				   length);
		if (status < 0) {
			printf("truncation error %d\n", errno);
		}
	}
}




/*
 *
 *	logTime()
 *
 */
static void
logTime(pclient)
	struct iocLogClient *pclient;
{
	unsigned	sec;
	char		*pcr;

	sec = time(NULL);
	strncpy(pclient->ascii_time, 
		ctime(&sec), 
		sizeof(pclient->ascii_time) );
	pclient->ascii_time[sizeof(pclient->ascii_time)-1] = NULL;
	pcr = strchr(pclient->ascii_time, '\n');
	if(pcr){
		*pcr = NULL;
	}
}


/*
 *
 *	getConfig()
 *	Get Server Configuration
 *
 *
 */
static int
getConfig()
{
	char	inet_address_string[64];
	int	status;
	char	*pstring;

	status = envGetIntegerConfigParam(
			&EPICS_IOC_LOG_PORT, 
			&ioc_log_port);
	if(status<0){
		failureNoptify(&EPICS_IOC_LOG_PORT);
		return ERROR;
	}

	status = envGetInetAddrConfigParam(
			&EPICS_IOC_LOG_INET, 
			&ioc_log_addr);
	if(status<0){
		failureNoptify(&EPICS_IOC_LOG_INET);
		return ERROR;
	}

	status = envGetIntegerConfigParam(
			&EPICS_IOC_LOG_FILE_LIMIT, 
			&ioc_log_file_limit);
	if(status<0){
		failureNoptify(&EPICS_IOC_LOG_FILE_LIMIT);
		return ERROR;
	}

	pstring = envGetConfigParam(
			&EPICS_IOC_LOG_FILE_NAME, 
			ioc_log_file_name,
			sizeof(ioc_log_file_name));
	if(pstring == NULL){
		failureNoptify(&EPICS_IOC_LOG_FILE_NAME);
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
static void
failureNoptify(pparam)
ENV_PARAM       *pparam;
{
	printf(	"IocLogServer: EPICS environment variable `%s' undefined\n",
		pparam->name);
}
