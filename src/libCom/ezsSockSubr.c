/*	@(#)ezsSockSubr.c	1.8		5/17/94
 *	Author:	Roger A. Cole
 *	Date:	11-23-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991-92, the Regents of the University of California,
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
 *  .00 11-23-90 rac	initial version
 *  .01 06-18-91 rac	installed in SCCS
 *  .02 09-05-91 joh	updated for v5 vxWorks; included systime.h for utime.h
 *  .03 12-08-91 rac	added a comment for ezsFopenToFd
 *  .04 10-09-92 rac	use SO_REUSEADDR with socket
 *  .05 08-11-93 mrk	removed V5_vxWorks
 *  .06 05-04-94 pg	HPUX port changes.
 *
 * make options
 *	-DvxWorks	makes a version for VxWorks
 *	-DNDEBUG	don't compile assert() checking
 *      -DDEBUG         compile various debug code, including checks on
 *                      malloc'd memory
 */
/*+/mod***********************************************************************
* TITLE	ezsSockSubr.c - easy-to-use socket routines
*
* DESCRIPTION
*	This set of routines supports using Internet sockets for transferring
*	text, with a bias toward use by server/client programs.
*
*	All of these routines use the sockets as TCP/IP sockets, with the
*	sockets specified as SOCK_STREAM.
*
* QUICK REFERENCE
* #include <ezsSockSubr.h>
*     int  ezsCheckFdRead(          fd					    )
*     int  ezsCheckFpRead(          fp					    )
*     int  ezsConnectToServer(     >pServerSock,  portNum, hostName	    )
*     int  ezsCreateListenSocket(  >pListenSock,  portNum		    )
*     int  ezsFopenToFd(            pFp,          pFd			    )
*     int  ezsFreopenToFd(          fp,           pFd,     >pOldFd	    )
*     int  ezsListenExclusiveUse(   pClientSock,  pListenSock, pInUseFlag,
*                                            >clientName,  greetingMsg	    )
*    void  ezsSleep(                seconds,      usec			    )
*
* BUGS
* o	doesn't provide a way for client to distinguish between server output
*	to stdout and stderr, since if server redirects those streams to the
*	same socket, their individual identity is lost
* o	needs examples
*-***************************************************************************/

#include <genDefs.h>
#include <ezsSockSubr.h>
#ifdef vxWorks
#   include <vxWorks.h>
#   include <stdioLib.h>
#   include <ioLib.h>
#   include <taskLib.h>
#   include <types.h>
#   include <systime.h>
#   include <socket.h>
#   include <inetLib.h>
#   include <in.h>
#   include <ctype.h>
#else
#   include <stdio.h>
#   include <sys/types.h>
#   include <sys/time.h>
#   include <sys/socket.h>
#   include <netinet/in.h>
#   include <arpa/inet.h>
#   include <netdb.h>
#   include <ctype.h>
#endif

/*+/subr**********************************************************************
* NAME	ezsCheckFpRead - check fp to see if read is possible
*
* DESCRIPTION
*	Checks the specified stream to see if a read operation is possible.
*
*	ezsCheckFdRead(fd) performs a similar function for fd's.
*
* RETURNS
*	>0	if a read can be done
*	0	if a read can't presently be done
*
* BUGS
* o	due to restrictions in VxWorks 4.0.2, cannot be used for stdin,
*	stdout, or stderr
*
*-*/
int
ezsCheckFpRead(fp)
FILE	*fp;		/* I pointer to FILE to check for "input available" */
{
#ifdef vxWorks
    if (fp == stdin)		assertAlways(0);
    else if (fp == stdout)	assertAlways(0);
    else if (fp == stderr)	assertAlways(0);
#endif

    return(ezsCheckFdRead(fileno(fp)));
}
ezsCheckFdRead(fd)
int	fd;
{
    fd_set readbits, other;
    struct timeval timer;
    int ret;

    timer.tv_sec = 0;
    timer.tv_usec = 0;
    FD_ZERO(&readbits);
    FD_ZERO(&other);
    FD_SET(fd, &readbits);

    ret = select(fd+1, &readbits, &other, &other, &timer);
#ifdef vxWorks
    return ret;
#else
    if (FD_ISSET(fd, &readbits))
	return 1;
    return 0;
#endif
}

/*+/subr**********************************************************************
* NAME	ezsConnectToServer - attempt to connect via socket to server
*
* DESCRIPTION
*
* RETURNS
*	0, or
*	-1 if an error occurs, or
*	-2 if the host name can't be found or the server rejects the
*	    connection.  The message buffer will contain a relevant message.
*
* BUGS
* o	under VxWorks 4.0.2, if the server is on the same machine as the
*	client, then the connect will fail unless the machine's name is
*	in the host table
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
int
ezsConnectToServer(pServerSock, portNum, hostName, message)
int	*pServerSock;	/* O ptr to socket connected to server */
int	portNum;	/* I port number of server */
char	*hostName;	/* I name (or Internet address) of host on which
				the server runs */
char	*message;	/* O message from server (dimension of 80 assumed) */
{
    struct sockaddr_in server;
    struct hostent *hp;
#ifndef vxWorks
    struct hostent *gethostbyname();
#endif
    int		i;
    int optval=1;

    assert(pServerSock != NULL);
    assert(portNum > 0);
    assert(hostName != NULL);
    assert(message != NULL);

/*-----------------------------------------------------------------------------
*    set up to create the socket and connect it to the server
*----------------------------------------------------------------------------*/
    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(portNum);

    if (isdigit(hostName[0]))
	server.sin_addr.s_addr = inet_addr(hostName);
    else {
#ifdef vxWorks
	if ((server.sin_addr.s_addr = hostGetByName(hostName)) == ERROR) {
	    sprintf(message, "host not in VxWorks host table: %s", hostName);
	    return -2;
	}
#else
	if ((hp = gethostbyname(hostName)) == NULL) {
	    sprintf(message, "host unknown: %s", hostName);
	    return -2;
	}
	bcopy(hp->h_addr, &server.sin_addr, hp->h_length);
#endif
    }

    *pServerSock = socket(AF_INET, SOCK_STREAM, 0);
    if (*pServerSock < 0)
	return -1;
    if (setsockopt(*pServerSock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
	close(*pServerSock);
	*pServerSock = -1;
	return -1;
    }
    if (connect(*pServerSock, &server, sizeof(server)) < 0) {
	close(*pServerSock);
	*pServerSock = -1;
	return -1;
    }

/*-----------------------------------------------------------------------------
*    now, with connection completed to server, wait for the message from
*    server--the message will either identify the server, or else indicate
*    that the connection is rejected.
*----------------------------------------------------------------------------*/
    if ((i=read(*pServerSock, message, 79)) <= 0) {
	close(*pServerSock);
	*pServerSock = -1;
	sprintf(message, "connection rejected");
	return -2;
    }
    message[i] = '\0';
    if (strncmp(message, "***", 3) == 0) {
	close(*pServerSock);
	*pServerSock = -1;
	return -2;
    }

    return 0;
}

/*+/subr**********************************************************************
* NAME	ezsCreateListenSocket - create socket to listen for connections
*
* DESCRIPTION
*	Create a socket to be used by a server to listen for connections
*	by potential clients.  The socket is bound to the caller-specified
*	port number.
*
*	The port number specified, in conjunction with the name of the
*	host, constitutes a network "address" at which the server can be
*	reached.
*
*	Ideally, the port number and service name would be registered with
*	the system manager, who would place the information in the
*	/etc/services file.  If this is done, then getservbyname(3N) can be
*	used by a potential client to determine the proper port number to
*	use.  In actual practice, potential clients would "know" what port
*	number to use by means of an include file provided by the service
*	for compiling clients.
*
* RETURNS
*	0, or
*	-1 if an error occurs (errno will have error code)
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
int
ezsCreateListenSocket(pListenSock, portNum)
int	*pListenSock;	/* O ptr to socket for listening for connects */
int	portNum;	/* I number of service's port */
{
    struct sockaddr_in server;	/* temporary Internet socket structure */

    assert(pListenSock != NULL);
    assert(portNum > 0);

    if ((*pListenSock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1;

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;		/* internet */
    server.sin_addr.s_addr = INADDR_ANY;	/* anybody can connect */
    server.sin_port = htons(portNum);		/* server's port */
    if (bind(*pListenSock, (struct sockaddr *)&server, sizeof(server)) < 0) {
	close(*pListenSock);
	return -1;
    }

    if (listen(*pListenSock, 1) < 0)
	return -1;

    return 0;
}

/*+/subr**********************************************************************
* NAME	ezsFopenToFd - open a specified stream to an fd
*
* DESCRIPTION
*	Open a stream to an fd.  Typically, this is used to open a stream
*	to a socket.
*
*	This routine should not be used for stdin, stdout, or stderr.  For
*	these streams, ezsFreopenToFd() should be used.
*
* RETURNS
*	0, or
*	-1 if errors are encountered (errno has error code)
*
* BUGS
* o	the stream is always opened in "r+" (update) mode
*
* SEE ALSO
*	ezsFreopenToFd(), ezsFrestoreToOldFd()
*
* NOTES	
* 1.	For input streams in a client monitoring the output of a server,
*	it will often be useful to set the stream to unbuffered:
*		FILE	*myIn;
*		ezsFopenToFd(&myIn, socket);
*		setbuf(myIn, NULL);
*
*-*/
int
ezsFopenToFd(pFp, pFd)
FILE	**pFp;		/* O pointer to pointer to new FILE */
int	*pFd;		/* I pointer tofd to be used by file pointer */
{
    assert(pFp != NULL);
    assert(pFd != NULL);

    if ((*pFp=fdopen(*pFd, "r+")) == NULL)
	return -1;
    return 0;
}

/*+/subr**********************************************************************
* NAME	ezsFreopenToFd - reopen a specified stream to an fd
*
* DESCRIPTION
*	Reopens a stream (assumed to presently be open) to an fd.  Typically,
*	this is used in a server program to redirect stdin, stdout, or stderr
*	to use a socket.
*
*	Under VxWorks, this is a pseudo re-open, which applies only to
*	the current task.  If a "program" consists of several tasks, each
*	task must re-direct its I/O.  Typically, one task will supply a
*	non-NULL pOldFd argument, and other tasks will use NULL.
*
* RETURNS
*	0, or
*	-1 if errors are encountered (errno has error code)
*	-2 if illegal stream is used
*
* BUGS
* o	the only streams which are handled are stdin, stdout, and stderr
* o	stdout and stderr are line buffered under VxWorks, and output won't
*	get sent (even if fflush() is used, as of 4.0.2) until '\n' is sent.
*
* SEE ALSO
*	ezsFopenToFd(), ezsFrestoreToOldFd()
*
* EXAMPLE
*
*-*/
int
ezsFreopenToFd(fp, pFd, pOldFd)
FILE	*fp;		/* IO pointer to FILE, such as stdin or stdout */
int	*pFd;		/* I pointer to fd to be used by file pointer */
int	*pOldFd;	/* O NULL, or pointer to place to store fd
			     previously used by file pointer */
{
    int		fdNumber;

    assert(fp != NULL);
    assert(pFd != NULL);

    if (fp == stdin)		fdNumber = 0;
    else if (fp == stdout)	fdNumber = 1;
    else if (fp == stderr)	fdNumber = 2;
    else			return -2;

#ifdef vxWorks
    if (pOldFd != NULL)
	*pOldFd = ioTaskStdGet(0, fdNumber);
    ioTaskStdSet(0, fdNumber, *pFd);
#else
    if (pOldFd != NULL) {
	if ((*pOldFd = dup(fileno(fp))) < 0)
	    return -1;
    }
    if (dup2(*pFd, fileno(fp)) < 0)
	return -1;
#endif

    return 0;
}

/*+/subr**********************************************************************
* NAME	ezsListenExclusiveUse - listen for client connect; allow only 1
*
* DESCRIPTION
*	Checks to see if a client connect request is queued.  If so, the
*	action depends on whether a client is presently connected:
*
*	o   if no client is presently connected, then the connection is made:
*	    -   a socket is created to converse with the client,
*	    -   a greeting message is sent to the new client,
*	    -   the inUseFlag is set to 1, and
*	    -   the new socket is returned to the caller
*	o   if a client is presently connected, then the connection is
*	    refused, with a rejection message sent to the hopeful new client.
*	    The rejection message has the form "*** server inuse by host",
*	    where "host" is the name of the host from which the present client
*	    is connected.
*
*	This routine simply polls the listen socket.  It must be called
*	periodically.  Only one queued request is processed on each call.
*
* RETURNS
*	-1 if an error occurs
*
* BUGS
* o	text
*
* SEE ALSO
*
* EXAMPLE
*
*-*/
int
ezsListenExclusiveUse(pClientSock, pListenSock, pInUseFlag, name, greetMsg)
int	*pClientSock;	/* IO ptr to socket (to be) connected to client */
int	*pListenSock;	/* I socket for listening for connects */
int	*pInUseFlag;	/* I ptr to flag; 1 indicates server already engaged */
char	*name;		/* O ptr to buf[30] where client name will go */
char	*greetMsg;	/* I greeting message to sent to new client */
{
    int		i;
    static char	inUseText[80];
    int		connSock;	/* socket connected to potential client */
    struct sockaddr_in clientAddr;/* address of client socket */
    struct hostent *clientHost;
    char	*clientHostAddr;
    char	clientHostAddrText[30];
    char	clientBuf[80];

    if (ezsCheckFdRead(*pListenSock)) {
	i = sizeof(clientAddr);
	connSock = accept(*pListenSock, (struct sockaddr *)&clientAddr, &i);
	if (connSock < 0)
	    return -1;
	if (*pInUseFlag) {
/*-----------------------------------------------------------------------------
*	server is already engaged; send rejection message to hopeful client
*----------------------------------------------------------------------------*/
	    write(connSock, inUseText, strlen(inUseText));
	    close(connSock);
	}
	else {
/*-----------------------------------------------------------------------------
*	server available; consummate connection:
*	o   set inUseFlag to 1
*	o   build a rejection message to send if other clients attempt connect
*	o   send greeting message to new client
*----------------------------------------------------------------------------*/
#ifndef vxWorks
	    clientHost =
		    gethostbyaddr((char *)&clientAddr.sin_addr, i, AF_INET);
	    if (clientHost != NULL)
		sprintf(clientHostAddrText, "%s", clientHost->h_name);
	    else {
#endif
		clientHostAddr = inet_ntoa(clientAddr.sin_addr);
		sprintf(clientHostAddrText, "%s", clientHostAddr);
#ifndef vxWorks
	    }
#endif
	    (void)strcpy(name, clientHostAddrText);
	    sprintf(inUseText, "*** server in use by %s", clientHostAddrText);
	    write(connSock, greetMsg, strlen(greetMsg));
	    *pClientSock = connSock;
	    *pInUseFlag = 1;
	}
    }
    return 0;
}

/*+/subr**********************************************************************
* NAME	ezsSleep - sleep
*
* DESCRIPTION
*	The current task sleeps for the specified time.
*
*	This routine should not be used by programs implemented with the
*	UNIX lwp (light-weight process) library--taskSleep() in genTaskSubr.c
*	should be used, instead.
*
* RETURNS
*	void
*
* SEE ALSO
*	taskSleep()
*
*-*/
void
ezsSleep(seconds, usec)
int	seconds;	/* I number of seconds (added to usec) to sleep */
int	usec;		/* I number of micro-sec (added to sec) to sleep */
{
#ifndef vxWorks
    sleep((unsigned int)seconds);
/* MDA - usleep isn't POSIX 
    usleep((unsigned)(seconds*1000000 + usec));
*/
#else
    int		ticks;
    static int ticksPerSec=0;
    static int usecPerTick;

    if (ticksPerSec == 0) {
	ticksPerSec = sysClkRateGet();
	usecPerTick = 1000000 / ticksPerSec;
    }

    ticks = seconds*ticksPerSec + usec/usecPerTick + 1;
    taskDelay(ticks);
#endif
}
