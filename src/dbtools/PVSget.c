
/* only runable on work station now */

#include "PVS.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc,char** argv)
{
	BS* bs;
	int verb,size,done,len,i;
	char* buffer;
	BSDATA info;

	if(argc<2)
	{
		printf("Enter IOC name on command line\n");
		return -1;
	}

	done=0;
	buffer=(char*)malloc(PVS_TRANSFER_SIZE);

	if(BSsetAddressPort(&info,argv[1],PVS_TCP_PORT)<0)
	{
		printf("Failed to locate IOC host name\n");
		return -1;
	}

	if((bs=BSipOpenData(&info))==NULL)
	{
		printf("open of socket to IOC failed\n");
	}
	else
	{
		while(done==0)
		{
			printf("Begin----------\n");
			/* read messages until close received */
			if(BSreceiveHeader(bs,&verb,&size)<0)
			{
				printf("receive failed from socket\n");
				done=-1;
			}
			else
			{
				switch(verb)
				{
				case PVS_Data: /* read a block of names */
					printf("got PVS_Data msg, size=%d\n",size);

					if((len=BSreceiveData(bs,buffer,size))<0)
					{
						printf("receive data failed\n");
					}
					else
					{
						for(i=0;i<len;i++)
						{
							if(buffer[i]!=' ')
								putchar(buffer[i]);
							else
								putchar('\n');
						}
					}
					putchar('\n');
					break;
				case BS_Close: /* transfers complete */
					printf("got BS_Close\n");
					BSsendHeader(bs,BS_Ok,0);
					done=-1;
					break;
				default:
					printf("unknown message type received from remote\n");
					if(size>0) done=-1;
					break;
				}
			}
		}
		printf("Closing connection\n");
		BSfreeBS(bs);
	}
	free(buffer);
}

