
/* only runable on work station now */

#include "PVS.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

struct PVSnode
{
	BSDATA info;
	int alive;
	struct PVSnode* next;
};
typedef struct PVSnode PVSNODE;

static PVSNODE* ioc_list = (PVSNODE*)NULL;

static int read_pvs(BSDATA* info);

int main(int argc,char** argv)
{
	int soc,mlen;
	unsigned short buf,in_buf,ping;
	BSDATA info;
	PVSNODE* node;

	if(BSmakeServer(argv)<0)
	{
		fprintf(stderr,"Cannot make into a server\n");
		return -1;
	}

	if((soc=BSopenListenerUDP(PVS_UDP_PORT))<0)
	{
		fprintf(stderr,"Open of UDP listener socket failed\n");
		return -1;
	}

	buf=htons(BS_Ok); /* always sends this out */
	ping=htons(BS_Ping); /* always sends this out */

	while(1)
	{
		/* wait forever until a message comes in */

		mlen=BSreadUDP(soc,&info,7,&in_buf,sizeof(in_buf));

		/* check for errors */
		switch(mlen)
		{
		case 0: /* timeout */
			printf("Why did a timeout occur?\n");
			/* send out a ping to each of the IOCs in the ioc_list */
			for(node=ioc_list;node;node=node->next)
			{
				mlen=BStransUDP(soc,&(node->info),&ping,sizeof(ping),
					&in_buf,sizeof(in_buf));

				/* check for errors */
				switch(mlen)
				{
				case 0: /* timeout */
					printf("IOC dead\n");
					node->alive=0;
					break;
				case -1: /* error */
					printf("Communications failed\n");
					break;
				default: /* ok */
					if(node->alive==0)
					{
						switch(fork())
						{
						case -1: /* error */
							perror("fork failure");
							break;
						case 0: /* child */
							close(soc);
							BSserverClearSignals();
							sleep(1);
							if(read_pvs(&(node->info))==0)
								node->alive=1;
						default: /* parent */
							break;
						}
					}
					break;
				}
			}
			break;
		case -1: /* error */
			fprintf(stderr,"Communications failure\n");
			break;
		default: /* ok */
			if(BSwriteUDP(soc,&info,&buf,sizeof(buf))<0)
				fprintf(stderr,"respone send failed\n");
			else
			{
				node=(PVSNODE*)malloc(sizeof(PVSNODE));
				node->alive=1;
				node->info=info;
				BSsetPort(&(node->info),PVS_UDP_CPORT);
				node->next=ioc_list;
				ioc_list=node;

				switch(fork())
				{
				case -1: /* error */
					perror("fork failure");
					break;
				case 0: /* child */
					close(soc);
					BSserverClearSignals();
					sleep(1);
					return read_pvs(&info);
				default: /* parent */
					break;
				}
			}
			break;
		}
	}
	close(soc);
	return 0;
}

static int read_pvs(BSDATA* info)
{
	BS* bs;
	int verb,size,done,len,i,port;
	char* buffer;
	char ip_from[40];
	FILE* fd;

	BSgetAddressPort(info,ip_from,&port);

	printf("IOC %s starting\n",ip_from);

	/* verify ioc not already added */
	if(access(ip_from,F_OK)==0)
	{
		/* delete the existing file for this IOC */
		unlink(ip_from);
	}

	buffer=(char*)malloc(PVS_TRANSFER_SIZE+2);
	done=0;
	BSsetPort(info,PVS_TCP_PORT);

	if((bs=BSipOpenData(info))==NULL)
	{
		fprintf(stderr,"Open of socket to IOC failed\n");
	}
	else
	{
		if((fd=fopen(ip_from,"w"))==(FILE*)NULL)
		{
			fprintf(stderr,"Open of name file failed\n");
		}
		else
		{
			while(done==0)
			{
				/* read messages until close received */
				if(BSreceiveHeader(bs,&verb,&size)<0)
				{
					fprintf(stderr,"Receive header failed\n");
					done=-1;
				}
				else
				{
					switch(verb)
					{
					case PVS_Data: /* read a block of names */
						if((len=BSreceiveData(bs,buffer,size))<0)
						{
							fprintf(stderr,"Receive data failed\n");
						}
						else
						{
							for(i=0;i<len;i++)
							{
								if(buffer[i]==' ') buffer[i]='\n';
							}
							buffer[len]='\n';
							buffer[len+1]='\0';

							fputs(buffer,fd);
						}
						break;
					case BS_Close: /* transfers complete */
						if(BSsendHeader(bs,BS_Ok,0)<0)
						{
							fprintf(stderr,"ACK Header send failed\n");
						}
						done=-1;
						break;
					default:
						if(size>0) done=-1;
						break;
					}
				}
			}
			fclose(fd);
		}
		BSfreeBS(bs);
	}
	free(buffer);
	return 0;
}

