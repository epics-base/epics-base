
#include "PVS.h"
 
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static int read_data(BSDATA* info,char* service_name);

int main(int argc,char** argv)
{
	BSDATA info;
	int rc;

	if(argc<3)
	{
		fprintf(stderr,"usage: %s IOC-name service-name\n",argv[0]);
		return -1;
	}

	BSsetAddress(&info,argv[1]);
	rc=read_data(&info,argv[2]);
	if(rc<0) fprintf(stderr,"read of data failed horribly\n");
	return 0;
}

static int read_data(BSDATA* info,char* sname)
{
	BS* bs;
	int verb,size,done,len,i,port,rsize;
	char* buffer;
	char ip_from[40];
	FILE* fd;

	BSgetAddressPort(info,ip_from,&port);
	BSsetPort(info,PVS_TCP_PORT);
	done=0;
	rsize=strlen(sname)+1;
	fd=stdout;

	if((bs=BSipOpenData(info))==NULL)
	{
		fprintf(stderr,"Open of socket to IOC failed\n");
		return -1;
	}

	if(BSsendHeader(bs,9999,rsize)<0)
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
					fprintf(stderr,"Receive data failed\n");
				else
				{
					/* buffer[len]='\n'; */
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


