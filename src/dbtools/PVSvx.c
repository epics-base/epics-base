
#include "PVS.h"

#include <string.h>
#include <stdio.h>

#ifdef vxWorks
#include <vxWorks.h>
#include <iv.h>
#include <taskLib.h>
#include <dbStaticLib.h>

extern struct dbBase *pdbBase;

#else
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#endif

static void PVSserver(int want_annouce,char* name);
static int PVSannouce(int want_annouce,char* name);

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

#ifdef vxWorks
   	taskSpawn("PVS",150,VX_FP_TASK|VX_STDIO,5000,
	  (FUNCPTR)PVSserver,want_annouce,(int)name,0,0,0,0,0,0,0,0);
#else
	PVSserver(want_annouce,name);
#endif

	return 0;
}

static int PVSannouce(int want_annouce,char* name)
{
	int soc,mlen;
	unsigned short buf,in_buf;
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

		buf=htons(PVS_Alive);
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
	unsigned short buf;
	unsigned long names_len,nl;
	struct sockaddr stemp;
	int stemp_len;
	char* names;
	char* n;
	BSDATA info;
	BS* bs;
	long rc;
#ifdef vxWorks
	DBENTRY db;
#endif

	bs=(BS*)NULL;
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
				/* only requests for PVs will come in here for now */
				/* handle the request here - single threaded server */

				stemp_len=sizeof(stemp);
				if((nsoc=accept(tsoc,&stemp,&stemp_len))<0)
					printf("PVSserver: Bad accept\n");
				else
				{
					bs=BSmakeBS(nsoc);
#ifdef vxWorks
					dbInitEntry(pdbBase,&db);
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
#else
					strcpy(names,"jim Kowalkowski");
					names_len=strlen("jim kowalkowski")+1;
					if(BSsendHeader(bs,PVS_Data,names_len)<0)
						printf("BSsendHeader failed\n");
					else
					{
						if(BSsendData(bs,names,names_len)<0)
							printf("BSsendData failed\n");
					}
#endif
					BSclose(bs);
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


