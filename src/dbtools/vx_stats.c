
/*
	You must run PVSstart() in your vxWorks startup script before these
	functions can be used.

	Just ld() the object file for this code in your vxWorks startup script.

	Services added in this file:
		PVS_MemStats - Send the bytes free and allocated to the requestor.
		PVS_ClientStats - Send a CA client statistics summary report to
			the requestor.  Includes host name, user id, number of channels
			in use.
		PVS_TaskList - Send a report of all the tasks in the IOC, along
			with there state and priority.
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <vxWorks.h>
#include <iv.h>
#include <taskLib.h>
#include <memLib.h>
#include <private/memPartLibP.h> /* sucks, don't it */

/* required for client stat section */
#include <sockLib.h>
#include <socket.h>
#include <in.h>
#include <dbStaticLib.h>

#include "PVS.h"

#define MAX_TASK 100

extern struct dbBase *pdbbase;
static int* task_list=NULL;

void PVS_MemStats(BS* bs)
{
	int len;
	unsigned long b_free, b_alloc;
	char line[100];

	b_free=2*(memSysPartId->totalWords-memSysPartId->curWordsAllocated);
	b_alloc=2*memSysPartId->curWordsAllocated;

	/* report columns:
		bytes_free bytes allocated
	*/

	sprintf(line,"%lu %lu\n",b_free,b_alloc);
	len=strlen(line)+1;

	if(BSsendHeader(bs,PVS_Data,len)<0)
		printf("PVSserver: data cmd failed\n");
	else
	{
		if(BSsendData(bs,line,len)<0)
			printf("PVSserver: data send failed\n");
	}
	BSsendHeader(bs,BS_Done,0);
}

void PVS_TaskList(BS* bs)
{
	int len,i,tot,pri;
	char line[100];
	char state[30];
	char* name;

	if(task_list==NULL)
		task_list=(int*)malloc(sizeof(int)*MAX_TASK);

	if((tot=taskIdListGet(task_list,MAX_TASK))==0)
	{
		BSsendHeader(bs,BS_Done,0);
		return;
	}

	/* report columns:
		name_of_task state_of_task priority of task
	*/

	for(i=0;i<tot;i++)
	{
		if((name=taskName(task_list[i]))==NULL) continue;
		if((taskPriorityGet(task_list[i],&pri))==ERROR) continue;
		if((taskStatusString(task_list[i],state))==ERROR) continue;

		sprintf(line,"%s %s %d\n",name,state,pri);
		len=strlen(line)+1;

		if(BSsendHeader(bs,PVS_Data,len)<0)
			printf("PVSserver: data cmd failed\n");
		else
		{
			if(BSsendData(bs,line,len)<0)
				printf("PVSserver: data send failed\n");
		}
	}
	BSsendHeader(bs,BS_Done,0);
}

#if 0
void PVS_FieldList(BS* bs)
{
	DBENTRY dbe;
	char buffer[50]; /* max size of a PV */
	char *name,*value;
	int size,verb;
	long rc,len;

	if(BSreceiveHeader(bs,&verb,&size)<0)
	{
		printf("PVS_FieldList(): Receive header failed\n");
	}
	else
	{
		switch(verb)
		{
		case PVS_Data:
			/* read the PV name */
			if((len=BSreceiveData(bs,buffer,size))<0)
				fprintf(stderr,"PVS_FieldList(): Receive data failed\n");
			else
			{
				if(ptr=strchr(buffer,'.')) *ptr='\0';

				dbInitEntry(pdbbase,&dbe);
				dbFindRecord(&dbe,buffer);

				for(rc=dbFirstFielddes(&dbe,FALSE); rc==0;
					rc=dbNextFieldDes(&dbe,FALSE))
				{
					name=dbGetFieldName(&dbe);
					value=dbGetString(&dbe);
					printf("name=<%s>, value=<%s>\n",name,value);
				}
			}
			break;
		default:
			fprintf(stderr,"PVS_FieldList(): Unknown packet received\n");
			break;
		default:
		}
	}
	BSsendHeader(bs,BS_Done,0);
}
#endif
