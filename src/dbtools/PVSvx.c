
/* This file not really set up to run under Unix yet, just under vxWorks. */

#include "PVS.h"

#include <string.h>
#include <stdio.h>

#ifdef vxWorks
#include <vxWorks.h>
#include <iv.h>
#include <taskLib.h>
#include <sysSymTbl.h>
#include <sysLib.h>
#include <symLib.h>
#include <dbStaticLib.h>

extern struct dbBase *pdbbase;

#else
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#endif

static void PVSserver(int want_annouce,char* name);
static int PVSannouce(int want_annouce,char* name);
static void handle_requests(BS* bs);
static void handle_reclist(BS* bs);
static void handle_applist(BS* bs);
static void handle_recdump(BS* bs);
static void handle_spylist(BS* bs);
static void handle_tasklist(BS* bs);
void PVS_test_server(BS* bs);

static char* names = (char*)NULL;
static char* buffer = (char*)NULL;

#ifdef vxWorks
int PVSstart(int want_annouce, char* name)
#else
int main(int argc,char** argv)
#endif
{
#ifndef vxWorks
	char* name;
	int want_annouce;
#endif

#ifndef vxWorks
	if(argc<3)
	{
		fprintf(stderr,"bad args\n");
		fprintf(stderr," usage: %s a_flag host_name\n",
			argv[0]);
		fprintf(stderr,"  where\n");
		fprintf(stderr,"   a_flag=0(want),1(don't want) to annouce boot\n");
		fprintf(stderr,"   host_name=PV master host (if one exists)\n");
		return -1;
	}
	name=argv[2];
	if(sscanf(argv[1],"%d",&want_annouce)<1)
	{
		fprintf(stderr,"bad a_flag\n");
		return -1;
	}
#endif

   	taskSpawn("PVS",150,VX_FP_TASK|VX_STDIO,5000,
	  (FUNCPTR)PVSserver,want_annouce,(int)name,0,0,0,0,0,0,0,0);

	return 0;
}

static int PVSannouce(int want_annouce,char* name)
{
	int soc,mlen;
	PVS_INFO_PACKET buf,in_buf;
	BSDATA info,in_info;

	if(want_annouce==0 && name)
	{
		if((soc=BSopenListenerUDP(0))<0)
		{
			printf("Open of UDP socket failed\n");
			return -1;
		}

		if(BSsetAddressPort(&info,name,PVS_UDP_PORT)<0)
		{
			printf("Set send port failed\n");
			return -1;
		}

		PVS_SET_CMD(&buf,PVS_Alive);
		mlen=BStransUDP(soc,&info,&buf,sizeof(buf),&in_buf,sizeof(in_buf));

		/* check for errors */
		switch(mlen)
		{
		case 0: /* timeout */
			printf("No server running on host\n");
			break;
		case -1: /* error */
			printf("Communications failed\n");
			break;
		default: /* ok */
			break;
		}

		close(soc);
	}
	return 0;
}

static void PVSserver(int want_annouce,char* name)
{
	fd_set fds,rfds;
	int tsoc,usoc,nsoc,len,s;
	struct sockaddr stemp;
	int stemp_len;
	PVS_INFO_PACKET buf;
	BSDATA info;
	BS* bs;

	bs=(BS*)NULL;
	buffer=(char*)malloc(100); /* just make the buffer */
	names=(char*)malloc(PVS_TRANSFER_SIZE);

	if((tsoc=BSopenListenerTCP(PVS_TCP_PORT))<0)
	{
		printf("PVSserver: Open of TCP listener socket failed\n");
		return;
	}

	if((usoc=BSopenListenerUDP(PVS_UDP_CPORT))<0)
	{
		printf("PVSserver: Open of UDP listener socket failed\n");
		return;
	}

	FD_ZERO(&fds);
	FD_SET(tsoc,&fds);
	FD_SET(usoc,&fds);

	PVSannouce(want_annouce,name);

	while(1)
	{
		rfds=fds;
		if(select(FD_SETSIZE,&rfds,(fd_set*)NULL,(fd_set*)NULL,
			(struct timeval*)NULL)<0)
		{
			printf("PVSserver: Select failure\n");
		}
		else
		{
			if(FD_ISSET(tsoc,&rfds))
			{
				/* handle the request here - single threaded server */

				stemp_len=sizeof(stemp);
				if((nsoc=accept(tsoc,&stemp,&stemp_len))<0)
					printf("PVSserver: Bad accept\n");
				else
				{
					bs=BSmakeBS(nsoc);
					handle_requests(bs);
					BSfreeBS(bs);
				}
			}
			if(FD_ISSET(usoc,&rfds))
			{
				/* only pings will come in here for now */
				len=BSreadUDP(usoc,&info,0,&buf,sizeof(buf));
				if(len<=0)
					printf("PVSserver: UDP listener read failure\n");
				else
				{
					if(BSwriteUDP(usoc,&info,&buf,sizeof(buf))<0)
						printf("PVSserver: UDP listener ping write failure\n");
				}
			}
		}
	}
}

static void handle_reclist(BS* bs)
{
	DBENTRY db;
	long rc;
	char* n;
	unsigned long names_len;
	int s;

	dbInitEntry(pdbbase,&db);
	names_len=0;

	for(rc=dbFirstRecdes(&db);rc==0;rc=dbNextRecdes(&db))
	{
		for(rc=dbFirstRecord(&db);rc==0;rc=dbNextRecord(&db))
		{
			/* collect the names util we excede the max */
			n=dbGetRecordName(&db);
			s=strlen(n);
			if((names_len+s)>PVS_TRANSFER_SIZE)
			{
				names[names_len++]='\0';
				if(BSsendHeader(bs,PVS_Data,names_len)<0)
					printf("PVSserver: data cmd failed\n");
				else
				{
					if(BSsendData(bs,names,names_len)<0)
						printf("PVSserver: data send failed\n");
				}
				names_len=0;
			}
			memcpy(&names[names_len],n,s);
			names_len+=s;
			names[names_len++]=' ';
		}
	}
	if(names_len>0)
	{
		names[names_len++]='\0';
		if(BSsendHeader(bs,PVS_Data,names_len)<0)
			printf("PVSserver: data cmd failed\n");
		else
		{
			if(BSsendData(bs,names,names_len)<0)
				printf("PVSserver: data send failed\n");
		}
	}
	BSsendHeader(bs,BS_Done,0);
}

static void handle_requests(BS* bs)
{
	int verb,size,notdone,len;
	void (*func)(BS*);
	SYM_TYPE stype;

	notdone=1;
	while(notdone)
	{
		/* at this point I should be getting a command */
		if(BSreceiveHeader(bs,&verb,&size)<0)
		{
			printf("PVSserver: receive header failed\n");
			notdone=0;
		}
		else
		{
			switch(verb)
			{
			case PVS_RecList: handle_reclist(bs); break;
			case PVS_AppList: handle_applist(bs); break;
			case PVS_RecDump: handle_recdump(bs); break;
			case BS_Close:
				BSsendHeader(bs,BS_Ok,0);
				notdone=0;
				break;
			case PVS_Data: break;
			case PVS_Alive: break;
			case BS_Ok: break;
			case BS_Error: break;
			case BS_Ping: break;
			case BS_Done: break;
			default:
				/* custom service */
				if(size>0)
				{
					/* this should be the name of the service */
					/* look up the symbol name in buffer and call as
						subroutine, passing it the BS */

					len=BSreceiveData(bs,&buffer[1],size);
					switch(len)
					{
					case 0: /* timeout */ notdone=0; break;
					case -1: /* error */ notdone=0; break;
					default:
						buffer[0]='_';

						if(strncmp(buffer,"_PVS",4)==0)
						{
							if(symFindByName(sysSymTbl,buffer,
					  		(char**)&func,&stype)==ERROR)
								func=(void (*)(BS*))NULL;

							if(func)
								func(bs);
							else
								BSsendHeader(bs,BS_Done,0);
						}
						else
							BSsendHeader(bs,BS_Done,0);
					}
				}
				else
					printf("PVSserver: unknown command received\n");
			}
		}
	}
}

/* --------------------------------------------------------------- */

struct dbnode
{
	char* name;
	struct dbnode* next;
};
typedef struct dbnode DBNODE;

extern DBNODE* DbApplList;

void handle_applist(BS* bs)
{
	DBNODE* n;
	int size,len;

	len=0;
	for(n=DbApplList;n;n=n->next)
	{
		len=strlen(n->name)+1;

		if(BSsendHeader(bs,PVS_Data,len)<0)
			printf("PVSserver: data cmd failed\n");
		else
		{
			if(BSsendData(bs,n->name,len)<0)
				printf("PVSserver: data send failed\n");
		}
	}
	BSsendHeader(bs,BS_Done,0);
}

/* --------------------------------------------------------------- */

void handle_recdump(BS* bs)
{
	printf("RecDump server invoked\n");
	BSsendHeader(bs,BS_Done,0);
}

void PVS_test_server(BS* bs)
{
	printf("PVS_test_server invoked\n");
	BSsendHeader(bs,BS_Done,0);
}

void handle_spylist(BS* bs)
{
	printf("PVS spy list server invoked\n");
}

void handle_tasklist(BS* bs)
{
	printf("PVS task list server invoked\n");
}
