
/* only runable on work station now */

#include "PVS.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static int read_pvs(BSDATA* info,int serv,char* sname);

int main(int argc,char** argv)
{
	BSDATA info;
	int rc;

	if(argc<2)
	{
		fprintf(stderr,"usage: %s IOC-ip-address\n",argv[0]);
		return -1;
	}

	if(BSsetAddress(&info,argv[1])<0)
	{
		fprintf(stderr,"Cannot determine address for %s\n",argv[1]);
		return -1;
	}

	rc=read_pvs(&info,PVS_RecList,(char*)NULL);

	if(rc<0) fprintf(stderr,"read of data failed horribly\n");
	return 0;
}

static int read_pvs(BSDATA* info,int serv,char* sname)
{
	BS* bs;
	int verb,size,done,len,i,port,rsize;
	char* buffer;
	char ip_from[40];
	FILE* fd;

	BSgetAddressPort(info,ip_from,&port);

	/* printf("IOC %s starting\n",ip_from); */

	/* verify ioc not already added */
	if(access(ip_from,F_OK)==0)
	{
		/* delete the existing file for this IOC */
		unlink(ip_from);
	}

	done=0;
	BSsetPort(info,PVS_TCP_PORT);

	if((bs=BSipOpenData(info))==NULL)
	{
		fprintf(stderr,"Open of socket to IOC failed\n");
		return -1;
	}

	if(serv>PVS_LAST_VERB)
		rsize=strlen(sname)+1;
	else
		rsize=0;

	if(BSsendHeader(bs,serv,rsize)<0)
	{
		fprintf(stderr,"Command send failed\n");
		return -1;
	}

	if(rsize>0)
	{
		if(BSsendData(bs,sname,rsize)<0)
		{
			fprintf(stderr,"send of command name failed\n");
			return -1;
		}
	}

	fd=stdout;
	buffer=(char*)malloc(PVS_TRANSFER_SIZE+2);

	while(done==0)
	{
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
			case BS_Done: /* transfers complete */
				BSclose(bs);
				done=-1;
				break;
			default:
				if(size>0) done=-1;
				break;
			}
		}
	}
	free(buffer);
	return 0;
}

