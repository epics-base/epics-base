#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#ifdef linux
#include <sys/resource.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>

#ifdef vxWorks
#include <vxWorks.h>
#include <in.h>
#include <inetLib.h>
#include <taskLib.h>
#include <ioLib.h>
#include <selectLib.h>
#else
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

#include "bdt.h"

/* ---------------------------------------------------------------------- */
/* server mode functions */

#ifndef vxWorks /* server mode functions */
static char* filename=(char*)NULL;

/* ----------------------------- */
/* signal catcher for the server */
/* ----------------------------- */
static void catch_sig(int sig)
{
	fprintf(stderr,"\nbdt server exiting\n");
	unlink(filename);
	exit(0);
}

/* -------------------------------- */
/* child reaper for the server mode */
/* -------------------------------- */
static void get_child(int sig)
{
    while(wait3((int *)NULL,WNOHANG,(struct rusage *)NULL)>0);
#ifdef linux
	signal(SIGCHLD,get_child); /* for reaping children */
#endif
}

/* ------------------------------- */
/* Clear the signals for a process */
/* ------------------------------- */
int BdtServerClearSignals()
{
	signal(SIGCHLD,SIG_DFL);
	signal(SIGHUP,SIG_DFL);
	signal(SIGINT,SIG_DFL);
	signal(SIGTERM,SIG_DFL);
	signal(SIGQUIT,SIG_DFL);
	return 0;
}

/* ----------------------------------------------------- */
/* Make a unix process into a generic background process */
/* ----------------------------------------------------- */
int BdtMakeServer(char** argv)
{
	FILE* fd;

	if(filename) return -1;

	/* set up signal handling for the server */
	signal(SIGCHLD,get_child); /* for reaping children */
	signal(SIGHUP,catch_sig);
	signal(SIGINT,catch_sig);
	signal(SIGTERM,catch_sig);
	signal(SIGQUIT,catch_sig);

	/* disconnect from parent */
    switch(fork())
    {
        case -1: /* error */
			perror("Cannot fork");
			return -1;
        case 0: /* child */
#ifdef linux
			setpgrp();
#else
            setpgrp(0,0);
#endif
            setsid();
            break;
        default: /* parent goes away */
        	exit(0);
    }

	/* save process ID */
	filename=(char*)malloc(strlen(argv[0])+10);
	sprintf(filename,"%s.%d",argv[0],getpid());
	fd=fopen(filename,"w");
	fprintf(fd,"%d",getpid());
	fprintf(stderr,"\npv server pid: %d\n",getpid());
	fclose(fd);

	return 0;
}
#endif /* server mode functions */

/* ------------------------------------------ */
/* unimplemented channel access open function */
/* ------------------------------------------ */
BDT* BdtPvOpen(char *IocName, char* PvName)
{
	BDT	*bdt;

    if((bdt = BdtIpOpen(IocName, BDT_TCP_PORT)) == NULL)
    {
        fprintf(stderr,"open of address %s failed\n", IocName);
        return(NULL);
    }

    if(BdtServiceConnect(bdt, BDT_SERVICE_PV, PvName) < 0)
    {
        fprintf(stderr,"connect to PV %s failed\n", PvName);
        BdtClose(bdt);
        return(NULL);
    }
	return(bdt);
}

/* --------------------------------------------------------------- */
/* open a bulk data socket to a server given the server IP address */
/* --------------------------------------------------------------- */
BDT* BdtIpOpen(char* address, int Port)
{
	struct hostent		*pHostent;
	struct sockaddr_in	tsin;
	unsigned long		addr;
	int					osoc;
	BDT					*bdt;

#ifndef vxWorks
	/* Deal with the name -vs- IP number issue. */
	if (isdigit(address[0]))
#endif
		addr=inet_addr(address);
#ifndef vxWorks
	else
	{
		if ((pHostent = gethostbyname (address)) == NULL)
			return(NULL);
		bcopy (pHostent->h_addr, (char *) &addr, sizeof(addr));
		printf("Converting name >%s< to IP number %08.8X\n", address, addr);
	}
#endif

	tsin.sin_port=0;
	tsin.sin_family=AF_INET;
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((osoc=socket(AF_INET,SOCK_STREAM,BDT_TCP))<0)
	{
		perror("BdtIpOpen: create socket failed");
		return (BDT*)NULL;
	}

	if((bind(osoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BdtIpOpen: local address bind failed");
		return (BDT*)NULL;
	}

	tsin.sin_port=htons(Port);
	memcpy((char*)&tsin.sin_addr,(char*)&addr,sizeof(addr));

	if(connect(osoc,(struct sockaddr*)&tsin,sizeof(tsin))<0)
	{
		perror("BdtIpOpen: connect failed");
		close(osoc);
		return (BDT*)NULL;
	}

	bdt=(BDT*)malloc(sizeof(BDT));
	bdt->soc=osoc;
	bdt->remaining_send=0;
	bdt->remaining_recv=0;
	bdt->state=BdtUnbound;

#ifndef vxWorks
	{
		int	j;
		j = fcntl(bdt->soc, F_GETFL, 0);
		fcntl(bdt->soc, F_SETFL, j|O_NDELAY);
	}
#endif
	/* now connected to the bulk data socket on the IOC */
	return bdt;
}

/* -------------------------------------- */
/* write size bytes from buffer to socket */
/* -------------------------------------- */
int BdtWrite(int soc,void* buffer,int size)
{
	int				rc;
	int				total;
	unsigned char*	data;
	fd_set			fds;
	struct timeval	to;

	data=(unsigned char*)buffer;
	total=size;

	to.tv_sec = 5;
	to.tv_usec = 0;

	do
	{
		FD_ZERO(&fds);
		FD_SET(soc, &fds);
		if (select(soc+1, NULL, &fds, NULL, &to) != 1)
		{
			printf("BdtWrite: timeout waiting to write data\n");
			return(-1);
		}
		/* send block of data */
		if((rc=send(soc,&data[size-total],total,0))<0)
		{
			if(errno == EINTR)
				rc = 0;
			else
				perror("BdtWrite: Send to remote failed"); 
		}
		else
			total-=rc;
	}
	while(rc>0 && total>0);

	return (rc<=0)?-1:0;
}

/* --------------------------------------- */
/* send a message header down a BDT socket */
/* --------------------------------------- */
int BdtSendHeader(BDT* bdt,unsigned short verb,int size)
{
	BdtMsgHead buf;

	if(bdt->state!=BdtIdle)
	{
		fprintf(stderr,"BdtSendHeader: Interface not idle\n");
		bdt->state=BdtBad;
		return -1;
	}

	buf.verb=htons(verb);
	buf.size=htonl((unsigned long)size);

	if(BdtWrite(bdt->soc,&buf.verb, sizeof(buf.verb))<0)
	{
		fprintf(stderr,"BdtSendHeader: write to remote failed");
		return -1;
	}
	if(BdtWrite(bdt->soc,&buf.size, sizeof(buf.size))<0)
	{
		fprintf(stderr,"BdtSendHeader: write to remote failed");
		return -1;
	}

	/* don't wait for response if data must go out */
	if(size)
	{
		bdt->remaining_send=size;
		bdt->state=BdtSData;
	}

	return 0;
}

/* ------------------------------------------- */
/* send a message data chunk down a BDT socket */
/* ------------------------------------------- */
int BdtSendData(BDT* bdt,void* buffer,int size)
{
	int		len;
	int		remaining;
	int		rc;

	if(bdt->state!=BdtSData)
	{
		fprintf(stderr,"BdtSendData: Interface not in send data mode\n");
		bdt->state=BdtBad;
		return -1;
	}

	remaining=bdt->remaining_send-size;

	if(remaining<0)
	{
		fprintf(stderr,"WARNING -- BdtSendData: To much data to send\n");
		len=bdt->remaining_send;
	}
	else
		len=size;

	if (BdtWrite(bdt->soc, buffer, len) < 0)
		return -1;

	bdt->remaining_send-=len;

	if(bdt->remaining_send<0)
	{
		fprintf(stderr,"BdtSendData: To much data Sent\n");
		bdt->remaining_send=0;
	}

	if(bdt->remaining_send==0)
		bdt->state=BdtIdle;

	return len;
}

int BdtFlushOutput(BDT* bdt)
{
#ifdef vxWorks
	ioctl(bdt->soc, FIOWFLUSH, 0);
#endif
}

/* ------------------------------------- */
/* Read exactly size bytes from remote   */
/* ------------------------------------- */
int BdtRead(int soc,void* buffer,int size)
{
	int rc,total;
	unsigned char* data;
	fd_set			fds;
	struct timeval	to;

	to.tv_sec = 5;
	to.tv_usec = 0;

	data=(unsigned char*)buffer;
	total=size;

	do
	{
#if 1
		/* wait for data chunk */
		FD_ZERO(&fds);
		FD_SET(soc, &fds);
		if (select(soc+1, &fds, NULL, NULL, &to) != 1)
		{
			printf("BdtRead: timeout waiting for data\n");
			return(-1);
		}
#endif
		if((rc=recv(soc,&data[size-total],total,0))<0)
		{
			if(errno==EINTR)
			{
				printf("BdtRead: EINTR");
				rc=0;
			}
			else
			{ 
				perror("BdtRead: Receive data chunk failed"); 
			}
		}
		else
			total-=rc;
	}
	while(rc>0 && total>0);

	return (rc<=0)?-1:0;
}

/* ------------------------------------- */
/* wait for a message header from remote */
/* ------------------------------------- */
int BdtReceiveHeader(BDT* bdt,int* verb,int* size)
{
	BdtMsgHead buf;

	/* can only receive header when in the idle state */
	if (bdt->state == BdtEof)
		return -1;

	if(bdt->state != BdtIdle)
	{
		fprintf(stderr,"BdtReceiveHeader: Interface not idle\n");
		bdt->state=BdtBad;
		return -1;
	}

	if(BdtRead(bdt->soc,&buf.verb,sizeof(buf.verb))<0)
	{
		fprintf(stderr,"BdtReceiveHeader: Read failed\n");
		return -1;
	}
	if(BdtRead(bdt->soc,&buf.size,sizeof(buf.size))<0)
	{
		fprintf(stderr,"BdtReceiveHeader: Read failed\n");
		return -1;
	}

	/* copy message data to user */
	*verb=ntohs(buf.verb);
	*size=ntohl(buf.size);

	if(*size)
		bdt->state=BdtRData;

	bdt->remaining_recv=*size;

	return 0;
}

/* ------------------------------------------------------------------------
	Wait for a chunk of data from remote.
	User can continually call this with a maximum size until it return 0.
   ------------------------------------------------------------------------ */
int BdtReceiveData(BDT* bdt,void* buffer,int size)
{
	int rc;

	/* can only receive data when in the receive data state */
	switch(bdt->state)
	{
	case BdtRData: break;
	case BdtIdle: return 0;
	default:
		fprintf(stderr,"BdtReceiveData: bad receive state\n");
		bdt->state=BdtBad;
		break;
	}

	if (bdt->remaining_recv < size)
		size = bdt->remaining_recv;

	if(BdtRead(bdt->soc,buffer,size)<0)
	{
		fprintf(stderr,"BdtReceiveData: Read failed\n");
		bdt->state = BdtEof;
		return -1;
	}

	bdt->remaining_recv-=size;

	if(bdt->remaining_recv<0)
	{
		fprintf(stderr,"BdtReceiveData: To much data received\n");
		bdt->remaining_recv=0;
	}

	if(bdt->remaining_recv==0)
		bdt->state=BdtIdle;

	return size;
}

/* ------------------------------------------------------ */
/* connect to a process variable, useful if raw open used */
/* ------------------------------------------------------ */
int BdtServiceConnect(BDT* bdt, char* Name, char *Args)
{
	int				len;
	int				rc;
	int				size;
	int				verb;
	unsigned char	NameLen;
	unsigned char	ArgLen;

	if(bdt->state!=BdtUnbound)
	{
		fprintf(stderr,"BdtServiceConnect: can only bind to one service\n");
		return -1;
	}
	bdt->state=BdtIdle;
	NameLen = strlen(Name)+1;
	if (Args != NULL)
		ArgLen = strlen(Args)+1;
	else
		ArgLen = 0;

	/* send out connect message */
	if(BdtSendHeader(bdt, BDT_Connect, NameLen+ArgLen) < 0)
	{
		fprintf(stderr,"BdtServiceConnect: send of connect header failed\n");
		bdt->state=BdtUnbound;
		return -1;
	}

	NameLen--;
	ArgLen--;
	/* send out the process variable to connect to */
	if((BdtSendData(bdt, &NameLen, 1) < 0) || (BdtSendData(bdt, Name, NameLen) < 0))

	{
		fprintf(stderr,"BdtServiceConnect: send of connect body failed\n");
		bdt->state=BdtUnbound;
		return -1;
	}
	if (ArgLen > 0)
	{
		if ((BdtSendData(bdt, &ArgLen, 1) < 0) || (BdtSendData(bdt, Args, ArgLen) < 0))
		{
			fprintf(stderr,"BdtServiceConnect: send of connect body failed\n");
			bdt->state=BdtUnbound;
			return -1;
		}
	}
	rc=0;

	/* wait for response from connect to process variable */
	if(BdtReceiveHeader(bdt,&verb,&size)<0)
	{
		fprintf(stderr,"BdtServiceConnect: receive reponse failed\n");
		bdt->state=BdtUnbound;
		return -1;
	}

	/* check response */
	switch(verb)
	{
	case BDT_Ok:
		rc=0;
		break;
	case BDT_Error:
		fprintf(stderr,"BdtServiceConnect: connection rejected\n");
		bdt->state=BdtUnbound;
		rc=-1;
		break;
	default:
		fprintf(stderr,"BdtServiceConnect: unknown response from remote\n");
		bdt->state=BdtUnbound;
		rc=-1;
		break;
	}

	return rc;
}

/* -------------------- */
/* close the connection */
/* -------------------- */
int BdtClose(BDT* bdt)
{
	int verb,size,done;

	/* send a close message out */
	if(BdtSendHeader(bdt,BDT_Close,0)<0)
	{
		fprintf(stderr,"BdtClose: Cannot send close message\n");
		return -1;
	}

	done=0;

	do
	{
		/* check response */
		if(BdtReceiveHeader(bdt,&verb,&size)<0)
		{
			fprintf(stderr,"BdtClose: Close message response error\n");
			return -1;
		}

		switch(verb)
		{
		case BDT_Ok:
			done=1;
			break;
		case BDT_Error:
			fprintf(stderr,"BdtClose: Close rejected\n");
			return -1;
			break;
		default: break;
		}
	}
	while(done==0);

	bdt->state=BdtUnbound;
	free(bdt);
	return 0;
}

/* --------------------------------------- */
/* make a listener socket for UDP - simple */
/* --------------------------------------- */
int BdtOpenListenerUDP(int Port)
{
	int nsoc;
	struct sockaddr_in tsin;

	tsin.sin_port=htons(Port);
	tsin.sin_family=AF_INET;
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((nsoc=socket(AF_INET,SOCK_DGRAM,BDT_UDP))<0)
	{
		perror("BdtOpenListenerUDP: open socket failed");
		return -1;
	}

	if((bind(nsoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BdtOpenListenerUDP: local bind failed");
		close(nsoc);
		return -1;
	}

	return nsoc;
}

/* --------------------------------------- */
/* make a listener socket for TCP - simple */
/* --------------------------------------- */
int BdtOpenListenerTCP(int Port)
{
	int nsoc;
	struct sockaddr_in tsin;

	memset (&tsin, 0, sizeof(struct  sockaddr_in));
	tsin.sin_port=htons(Port);
	tsin.sin_family=htons(AF_INET);
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((nsoc=socket(AF_INET,SOCK_STREAM,BDT_TCP))<0)
	{
		perror("BdtOpenListenerTCP: open socket failed");
		return -1;
	}

	if((bind(nsoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BdtOpenListenerTCP: local bind failed");
		close(nsoc);
		return -1;
	}

	listen(nsoc,5);
	return nsoc;
}

/* ------------------------------- */
/* make BDT from a socket - simple */
/* ------------------------------- */
BDT* BdtMakeBDT(int soc)
{
	BDT* bdt;
	bdt=(BDT*)malloc(sizeof(BDT));
	bdt->soc=soc;
	bdt->remaining_send=0;
	bdt->remaining_recv=0;
	bdt->state=BdtIdle;
	return bdt;
}

/* --------------------------- */
/* free a BDT and close socket */
/* --------------------------- */
int BdtFreeBDT(BDT* bdt)
{
	close(bdt->soc);
	free(bdt);
	return 0;
}

int BdtPvPutArray(BDT *bdt, short DbrType, void *Buf, unsigned long NumElements,
 unsigned long ElementSize)
{
    int     Verb;
    int     Size;
    unsigned long BufSize;

    BufSize = NumElements * ElementSize;
    if (BdtSendHeader(bdt, BDT_Put, 6 + BufSize) != 0)
        return(-1);
    if (BdtSendData(bdt, &DbrType, 2) < 0)
        return(-1);
    if (BdtSendData(bdt, &NumElements, 4) < 0)
        return(-1);
    if (BdtSendData(bdt, Buf, BufSize) < 0)
        return(-1);
    if (BdtReceiveHeader(bdt, &Verb, &Size) != 0)
        return(-1);
    if (Verb != BDT_Ok)
        return(-1);

    return(0);
}
