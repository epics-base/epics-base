#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "BSlib.h"

#ifdef linux
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef vxWorks
#include <vxWorks.h>
#include <in.h>
#include <ioctl.h>
#include <inetLib.h>
#include <taskLib.h>
#include <ioLib.h>
#include <sockLib.h>
#include <selectLib.h>
#else
#ifdef _OSF_SOURCE
#include <sys/ioctl.h>
#else
#ifdef HP_UX
#include <sys/ioctl.h>
#else
#ifdef linux
#include <sys/socketio.h>
#else
#include <sys/sockio.h>
#endif
#endif
#endif
#include <sys/time.h>
#include <netdb.h>
#include <fcntl.h>
#endif

#ifdef SOLARIS
#define BSD_COMP
#include <sys/ioctl.h>
#endif

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>


static long BSgetBroadcastAddr(int soc, struct sockaddr* sin);

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
#ifdef SOLARIS
	while(waitpid(-1,(int*)NULL,WNOHANG)>0);
#else
    while(wait3((int *)NULL,WNOHANG,(struct rusage *)NULL)>0);
#endif

#if defined linux || defined SOLARIS
	signal(SIGCHLD,get_child); /* for reaping children */
#endif
}

/* ------------------------------- */
/* Clear the signals for a process */
/* ------------------------------- */
int BSserverClearSignals()
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
int BSmakeServer(char** argv)
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
#if defined linux || defined SOLARIS || defined SGI || defined _OSF_SOURCE || defined HP_UX
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

/* --------------------------------------------------------------- */
/* open a bulk data socket to a server given the server IP address */
/* --------------------------------------------------------------- */

BS* BSipOpen(char* address, int Port)
{
	unsigned long addr;
	BSDATA info;
	struct sockaddr_in* tsin;

	tsin=(struct sockaddr_in*)&(info.sin);

#ifndef vxWorks
	/* Deal with the name -vs- IP number issue. */
	if (isdigit(address[0]))
	{
#endif
		addr=inet_addr(address);
#ifndef vxWorks
	}
	else
	{
		struct hostent* pHostent;

		if((pHostent=gethostbyname(address))==NULL) return NULL;
		memcpy((char*)&addr,pHostent->h_addr,sizeof(addr));
	}
#endif

	tsin->sin_port=htons(Port);
	tsin->sin_family=AF_INET;
	memcpy((char*)&(tsin->sin_addr),(char*)&addr,sizeof(addr));

	return BSipOpenData(&info);
}

BS* BSipOpenData(BSDATA* info)
{
	struct sockaddr_in tsin;
	int osoc;
	BS* bdt;

	tsin.sin_port=0;
	tsin.sin_family=AF_INET;
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((osoc=socket(AF_INET,SOCK_STREAM,BS_TCP))<0)
	{
		perror("BSipOpen: create socket failed");
		return (BS*)NULL;
	}

	if((bind(osoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BSipOpen: local address bind failed");
		return (BS*)NULL;
	}

	if(connect(osoc,(struct sockaddr*)&(info->sin),sizeof(info->sin))<0)
	{
		perror("BSipOpen: connect failed");
		close(osoc);
		return (BS*)NULL;
	}

	bdt=(BS*)malloc(sizeof(BS));
	bdt->soc=osoc;
	bdt->remaining_send=0;
	bdt->remaining_recv=0;
	bdt->state=BSidle;

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
int BSwrite(int soc,void* buffer,int size)
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
		if(select(FD_SETSIZE,NULL,&fds,NULL,&to) != 1)
		{
			printf("BSwrite: timeout waiting to write data\n");
			return -1;
		}
		/* send block of data */
		if((rc=send(soc,(char*)&data[size-total],total,0))<0)
		{
			if(errno == EINTR)
				rc = 0;
			else
				perror("BSwrite: Send to remote failed"); 
		}
		else
			total-=rc;
	}
	while(rc>0 && total>0);

	return (rc<=0)?-1:0;
}

/* --------------------------------------- */
/* send a message header down a BS socket */
/* --------------------------------------- */
int BSsendHeader(BS* bdt,unsigned short verb,int size)
{
	BSmsgHead buf;

	if(bdt->state!=BSidle)
	{
		fprintf(stderr,"BSsendHeader: Interface not idle\n");
		bdt->state=BSbad;
		return -1;
	}

	buf.verb=htons(verb);
	buf.size=htonl((unsigned long)size);

	if(BSwrite(bdt->soc,&buf.verb, sizeof(buf.verb))<0)
	{
		fprintf(stderr,"BSsendHeader: write to remote failed");
		return -1;
	}
	if(BSwrite(bdt->soc,&buf.size, sizeof(buf.size))<0)
	{
		fprintf(stderr,"BSsendHeader: write to remote failed");
		return -1;
	}

	/* don't wait for response if data must go out */
	if(size)
	{
		bdt->remaining_send=size;
		bdt->state=BSsData;
	}

	return 0;
}

/* ------------------------------------------- */
/* send a message data chunk down a BS socket */
/* ------------------------------------------- */
int BSsendData(BS* bdt,void* buffer,int size)
{
	int		len;
	int		remaining;

	if(bdt->state!=BSsData)
	{
		fprintf(stderr,"BSsendData: Interface not in send data mode\n");
		bdt->state=BSbad;
		return -1;
	}

	remaining=bdt->remaining_send-size;

	if(remaining<0)
	{
		fprintf(stderr,"WARNING -- BSsendData: To much data to send\n");
		len=bdt->remaining_send;
	}
	else
		len=size;

	if (BSwrite(bdt->soc, buffer, len) < 0)
		return -1;

	bdt->remaining_send-=len;

	if(bdt->remaining_send<0)
	{
		fprintf(stderr,"BSsendData: To much data Sent\n");
		bdt->remaining_send=0;
	}

	if(bdt->remaining_send==0)
		bdt->state=BSidle;

	return len;
}

int BSflushOutput(BS* bdt)
{
#ifdef vxWorks
	ioctl(bdt->soc, FIOWFLUSH, 0);
#endif
	return 0;
}

/* ------------------------------------- */
/* Read exactly size bytes from remote   */
/* ------------------------------------- */
int BSread(int soc,void* buffer,int size)
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
			printf("BSread: timeout waiting for data\n");
			return(-1);
		}
#endif
		if((rc=recv(soc,(char*)&data[size-total],total,0))<0)
		{
			if(errno==EINTR)
			{
				printf("BSread: EINTR");
				rc=0;
			}
			else
			{ 
				perror("BSread: Receive data chunk failed"); 
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
int BSreceiveHeader(BS* bdt,int* verb,int* size)
{
	BSmsgHead buf;

	/* can only receive header when in the idle state */
	if (bdt->state == BSeof)
		return -1;

	if(bdt->state != BSidle)
	{
		fprintf(stderr,"BSreceiveHeader: Interface not idle\n");
		bdt->state=BSbad;
		return -1;
	}

	if(BSread(bdt->soc,&buf.verb,sizeof(buf.verb))<0)
	{
		fprintf(stderr,"BSreceiveHeader: Read failed\n");
		return -1;
	}
	if(BSread(bdt->soc,&buf.size,sizeof(buf.size))<0)
	{
		fprintf(stderr,"BSreceiveHeader: Read failed\n");
		return -1;
	}

	/* copy message data to user */
	*verb=ntohs(buf.verb);
	*size=ntohl(buf.size);

	if(*size)
		bdt->state=BSrData;

	bdt->remaining_recv=*size;

	return 0;
}

/* ------------------------------------------------------------------------
	Wait for a chunk of data from remote.
	User can continually call this with a maximum size until it return 0.
   ------------------------------------------------------------------------ */
int BSreceiveData(BS* bdt,void* buffer,int size)
{

	/* can only receive data when in the receive data state */
	switch(bdt->state)
	{
	case BSrData: break;
	case BSidle: return 0;
	default:
		fprintf(stderr,"BSreceiveData: bad receive state\n");
		bdt->state=BSbad;
		break;
	}

	if (bdt->remaining_recv < size)
		size = bdt->remaining_recv;

	if(BSread(bdt->soc,buffer,size)<0)
	{
		fprintf(stderr,"BSreceiveData: Read failed\n");
		bdt->state = BSeof;
		return -1;
	}

	bdt->remaining_recv-=size;

	if(bdt->remaining_recv<0)
	{
		fprintf(stderr,"BSreceiveData: To much data received\n");
		bdt->remaining_recv=0;
	}

	if(bdt->remaining_recv==0)
		bdt->state=BSidle;

	return size;
}

/* -------------------- */
/* close the connection */
/* -------------------- */
int BSclose(BS* bdt)
{
	int verb,size,done;

	/* send a close message out */
	if(BSsendHeader(bdt,BS_Close,0)<0)
	{
		fprintf(stderr,"BSclose: Cannot send close message\n");
		return -1;
	}

	done=0;

	do
	{
		/* check response */
		if(BSreceiveHeader(bdt,&verb,&size)<0)
		{
			fprintf(stderr,"BSclose: Close message response error\n");
			return -1;
		}

		switch(verb)
		{
		case BS_Ok:
			done=1;
			break;
		case BS_Error:
			fprintf(stderr,"BSclose: Close rejected\n");
			return -1;
		default: break;
		}
	}
	while(done==0);

	BSfreeBS(bdt);
	return 0;
}

/* --------------------------------------- */
/* make a listener socket for UDP - simple */
/* --------------------------------------- */
int BSopenListenerUDP(int Port)
{
	int nsoc;
	struct sockaddr_in tsin;

	tsin.sin_port=htons(Port);
	tsin.sin_family=AF_INET;
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((nsoc=socket(AF_INET,SOCK_DGRAM,BS_UDP))<0)
	{
		perror("BSopenListenerUDP: open socket failed");
		return -1;
	}

	if((bind(nsoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BSopenListenerUDP: local bind failed");
		close(nsoc);
		return -1;
	}

	return nsoc;
}

/* --------------------------------------- */
/* make a listener socket for TCP - simple */
/* --------------------------------------- */
int BSopenListenerTCP(int Port)
{
	int nsoc;
	struct sockaddr_in tsin;

	memset ((void *)&tsin, 0, sizeof(struct  sockaddr_in));
	tsin.sin_port=htons(Port);
	tsin.sin_family=htons(AF_INET);
	tsin.sin_addr.s_addr=htonl(INADDR_ANY);

	if((nsoc=socket(AF_INET,SOCK_STREAM,BS_TCP))<0)
	{
		perror("BSopenListenerTCP: open socket failed");
		return -1;
	}

	if((bind(nsoc,(struct sockaddr*)&tsin,sizeof(tsin)))<0)
	{
		perror("BSopenListenerTCP: local bind failed");
		close(nsoc);
		return -1;
	}

	listen(nsoc,5);
	return nsoc;
}

/* ------------------------------- */
/* make BS from a socket - simple */
/* ------------------------------- */
BS* BSmakeBS(int soc)
{
	BS* bdt;
	bdt=(BS*)malloc(sizeof(BS));
	bdt->soc=soc;
	bdt->remaining_send=0;
	bdt->remaining_recv=0;
	bdt->state=BSidle;
	return bdt;
}

/* --------------------------- */
/* free a BS and close socket */
/* --------------------------- */
int BSfreeBS(BS* bdt)
{
	close(bdt->soc);
	free(bdt);
	return 0;
}

/*-----------------------------------------------------------------------*/
/*
	Next three function adjust the fields of the BSDATA structure.
*/
/*-----------------------------------------------------------------------*/

int BSgetAddressPort(BSDATA* info, char* ip_addr, int* dest_port)
{
	char* name;
	struct sockaddr_in* sin = (struct sockaddr_in*)&(info->sin);

	*dest_port=sin->sin_port;
	name=inet_ntoa(sin->sin_addr);
	strcpy(ip_addr,name);

	return 0;
}

int BSsetAddressPort(BSDATA* info, char* ip_addr, int dest_port)
{
	BSsetPort(info, dest_port);
	return BSsetAddress(info, ip_addr);
}

int BSsetPort(BSDATA* info, int dest_port)
{
	struct sockaddr_in* sin = (struct sockaddr_in*)&(info->sin);
	sin->sin_family=AF_INET;
	sin->sin_port=htons(dest_port);
	return 0;
}

int BSsetAddress(BSDATA* info, char* ip_addr)
{
	struct sockaddr_in* sin = (struct sockaddr_in*)&(info->sin);
	unsigned long addr;

#ifndef vxWorks
	/* Deal with the name -vs- IP number issue. */
	if (isdigit(ip_addr[0]))
	{
#endif
		if((addr=inet_addr(ip_addr))==-1) return -1;
#ifndef vxWorks
	}
	else
	{
	        struct hostent *pHostent;

		if((pHostent=gethostbyname(ip_addr))==NULL) return -1;
		memcpy((char*)&addr,pHostent->h_addr,sizeof(addr));
	}
#endif

	sin->sin_family=AF_INET;
	memcpy((char*)&(sin->sin_addr),(char*)&addr,sizeof(addr));

	return 0;
}

/* ----------------------------------------------------------------- */
/*
   All this function does is send a broadcast message and return the
   addressing info to the caller of a responder to the broadcast.

   arguments:
   soc - broadcast socket to use for sending data
   trys - number of times to send if no response
   o_info - outgoing BSDATA, address/port info
   i_info - incoming BSDATA, address/port where response came from
   omsg/osize - outgoing message and size
   imsg/isize - incoming message and buffer size

   Returns the number of bytes read.
*/
/* ----------------------------------------------------------------- */

int BSbroadcastTrans(int soc,int trys,BSDATA* o_info,BSDATA* i_info,
	void* omsg,int osize,void* imsg,int isize)
{
	int i;
	int rc=0;

	for(i=0;rc==0 && i<trys;i++)
	{
		/* send out the message */
		rc=BSwriteUDP(soc,o_info,omsg,osize);
		if(rc<0) return rc;

		/* wait for a response */
		rc=BSreadUDP(soc,i_info,1,imsg,isize);
	}

	return rc;
}

int BSbroadcast(int soc,BSDATA* o_info, void* omsg,int osize)
{
	int rc;

	/* send out the message */
	rc=BSwriteUDP(soc,o_info,omsg,osize);
	return rc;
}

/* ----------------------------------------------------------------------- */
/*
   This function waits for a message and sends out an ACK when it gets one.
   addressing info to the caller of a responder to the broadcast.

   Lots of arguments:

	soc - The socket to send the message down.
	info - The socket information telling where the data read came from
   			(returned to the user).
	tout - Timeout for read in seconds. 0=no wait, -1=wait forever.
	buf - Buffer to populate with read data.
	size - Size of the buf.

	returns the length the read message, 0 is timeout, -1 is error
*/
/* ----------------------------------------------------------------------- */

int BSreadUDP(int soc,BSDATA* info,BS_ULONG tout,void* buf,int size)
{
	int mlen,rc;
	fd_set fds;
	struct timeval to;
	int error=0;

	do
	{
		FD_ZERO(&fds);
		FD_SET(soc, &fds);

		if(tout==-1)
			rc=select(FD_SETSIZE,&fds,NULL,NULL,NULL);
		else
		{
			to.tv_sec=tout;
			to.tv_usec=0;
			rc=select(FD_SETSIZE,&fds,NULL,NULL,&to);
		}

		switch(rc)
		{
		case -1: /* bad */
			switch(errno)
			{
			case EINTR: break;
			default: error=-1; break;
			}
			break;
		case 0: /* timeout */
			break;
		default: /* data ready */
			break;
		}
	}
	while(rc<0 && error==0);

	error=0;

	if(rc>0)
	{
		error=0;
		do
		{
			info->len=sizeof(info->sin);
			mlen=recvfrom(soc,(char*)buf,size,0, &info->sin,&info->len);

			if(mlen<0)
			{
				switch(errno)
				{
				case EINTR: break;
				default: error=-1; break;
				}
			}
		}
		while(mlen<0 && error==0);

		if(mlen<0)
			rc=-1;
		else
			rc=mlen;
	}

	return rc;
}

/*-----------------------------------------------------------------------*/
/*
	Write a chuck of data to a UDP socket at an internet address
*/
/*-----------------------------------------------------------------------*/
int BSwriteDataUDP(int soc,int dest_port,char* ip_addr,void* buf,int size)
{
	BSDATA data;

	BSsetAddress(&data,ip_addr);
	BSsetPort(&data,dest_port);

	return BSwriteUDP(soc,&data,buf,size);
}

/*-----------------------------------------------------------------------*/
/*
	Write a chunk of data to a UDP socket using BSDATA.
	Arguments:
*/
/*-----------------------------------------------------------------------*/
int BSwriteUDP(int soc,BSDATA* info,void* obuf,int osize)
{
	int mlen;

	mlen=sendto(soc,(char*)obuf,osize,0,&info->sin,sizeof(info->sin));

	return mlen;
}

/*-----------------------------------------------------------------------*/
/*
	Write/Read a chuck of data to a UDP socket using BSDATA.
	Arguments:
		soc - socket to send message down and read from
		info - address/port to send message to
		obuf/osize - outgoing message and size of it.
		ibuf/isize - incoming message and size of the buffer.

	Returns the number of bytes read, -1 for error.
*/
/*-----------------------------------------------------------------------*/
int BStransUDP(int soc,BSDATA* info,void* obuf,int osize,void* ibuf,int isize)
{
	int done,i,mlen,flen;
	struct sockaddr fromsin;
	fd_set fds;
	struct timeval tout;

	done=0;
	mlen=0;

	for(i=0;i<BS_RETRY_COUNT && done==0;i++)
	{
		mlen=sendto(soc,(char*)obuf,osize,0,&(info->sin),sizeof(info->sin));
		if(mlen<0)
		{
			printf("send failed\n");
			return -1;
		}

		tout.tv_sec=0;
		tout.tv_usec=200000;

		FD_ZERO(&fds);
		FD_SET(soc,&fds);

        switch(select(FD_SETSIZE,&fds,(fd_set*)0,(fd_set*)0,&tout))
		{
		case 0: /* timeout */ break;
		case -1: /* error */
			printf("select failed\n");
			return -1;
		default: /* data ready */
			flen=sizeof(fromsin);
			mlen=recvfrom(soc,(char*)ibuf,isize,0,&fromsin,&flen);
			if(mlen<0) return -1;
			done=1;
			break;
		}
	}

	if(i>=BS_RETRY_COUNT) return 0;

	return mlen;
}

/*-----------------------------------------------------------------------*/
/*
	Open a broadcast socket and set port to a default.
*/
/*-----------------------------------------------------------------------*/
int BSipBroadcastOpen(BSDATA* info, int default_dest_port)
{
	struct sockaddr_in* sin;
	int soc;

	sin=(struct sockaddr_in*)&(info->sin);

	if( (soc=BSgetBroadcastSocket(0,sin)) <0) return -1;

	sin->sin_port=htons(default_dest_port);

	return soc;
}

/*-----------------------------------------------------------------------*/
/*	
	BSgetBroadcastSocket() - return a broadcast socket for a port, return
	a sockaddr also.
*/
/*-----------------------------------------------------------------------*/
int BSgetBroadcastSocket(int port, struct sockaddr_in* sin)
{
	int on=1;
	int soc;

	sin->sin_port=htons(port);
	sin->sin_family=AF_INET;
	sin->sin_addr.s_addr=htonl(INADDR_ANY);

	if( (soc=socket(AF_INET,SOCK_DGRAM,BS_UDP)) < 0 )
	{
		perror("socket create failed");
		return -1;
	}

	setsockopt(soc,SOL_SOCKET,SO_BROADCAST,(char*)&on,sizeof(on));

	if( bind(soc,(struct sockaddr*)sin,sizeof(struct sockaddr_in)) < 0 )
	{
		perror("socket bind failed");
		close(soc);
		return -1;
	}

	if( BSgetBroadcastAddr(soc,(struct sockaddr*)sin) < 0 )
		return -1;

	return soc;
}

/*-----------------------------------------------------------------------*/
/*	
	BSgetBroadcastAddr() - Determine the broadcast address, this is 
	directly from the Sun Network Programmer's guide.
*/
/*-----------------------------------------------------------------------*/
static long BSgetBroadcastAddr(int soc, struct sockaddr* sin)
{
	struct ifconf ifc;
	struct ifreq* ifr;
	struct ifreq* save;
	char buf[BUFSIZ];
	int tot,i;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if(ioctl(soc,SIOCGIFCONF,(int)&ifc) < 0)
		{ perror("ioctl SIOCGIFCONF failed"); return -1; }

	ifr = ifc.ifc_req;
	tot = ifc.ifc_len/sizeof(struct ifreq);
	save=(struct ifreq*)NULL;
	i=0;

	do
	{
		if(ifr[i].ifr_addr.sa_family==AF_INET)
		{
			if(ioctl(soc,SIOCGIFFLAGS,(int)&ifr[i])<0)
				{ perror("ioctl SIOCGIFFLAGS failed"); return -1; }

			if( (ifr[i].ifr_flags&IFF_UP) && 
				!(ifr[i].ifr_flags&IFF_LOOPBACK) &&
				(ifr[i].ifr_flags&IFF_BROADCAST))
			{ save=&ifr[i]; }
		}
	} while( !save && ++i<tot );

	if(save)
	{
		if(ioctl(soc,SIOCGIFBRDADDR,(int)save)<0)
			{ perror("ioctl SIOCGIFBRDADDR failed"); return -1; }

		memcpy((char*)sin,(char*)&save->ifr_broadaddr,
				sizeof(save->ifr_broadaddr));
	}
	else
		return -1;

	return 0;
}

