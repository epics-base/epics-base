
/*
 * $Id$
 * $Log$
 * Revision 1.1  1997/04/07 20:16:41  jbk
 * Added a simple library for doing malloc/free from a buffer pool
 *
 *
 * Author: Jim Kowalkowski
 * Date:   4/97
 *
 * Intended for applications that create small strings over and over again
 *
 * I use a 1 byte indentifier in the front of the allocated chunk to tell
 * if it is managed by me or not.  This limited the applications that can
 * use this library to just strings because the block I return will not
 * be aligned on a 4 or 8 byte boundary.  Increasing it to 4 or 8 bytes
 * seems wasteful.  I really need to identify the chunks as mine or malloced.
 *
 */

#include <stdlib.h>

#ifdef vxWorks
#include <semLib.h>
#else
#define SEM_ID int
#define semGive(x) ;
#define semTake(x,y) ;
#define semBCreate(x,y) 0
#endif

#include "dbmf.h"

#define MAKE_TEST_PROGRAM 0
#define DBMF_OWNED	0x01
#define DBMF_SIZE	55
#define DBMF_ALIGN	1
#define DBMF_POOL	4

struct _chunk
{
	size_t chunk_size;	/* size of my chunk */
	size_t user_size;	/* size the user wants managed (this or less) */
	size_t group_size;	/* bytes in group */
	size_t total;		/* allocate chunks in groups of this number */
	size_t align;
	char* free_list;
	SEM_ID sem;
};
typedef struct _chunk chunk;

static chunk* def=NULL; /* default buffer pool - when handle to malloc() NULL */

/* returns handle to user */
void* epicsShareAPI dbmfInit(size_t size, size_t alignment, int init_num_free_list_items)
{
	chunk* c = (chunk*)malloc(sizeof(chunk));

	if(c)
	{
		c->user_size=size;
		c->chunk_size=((c->user_size/alignment)*alignment)+alignment;
		c->total=init_num_free_list_items;
		c->group_size=c->chunk_size*c->total;
		c->align=alignment;
		c->free_list=NULL;
		c->sem=semBCreate(SEM_Q_PRIORITY,SEM_FULL);
	}

	return c;
}

void* epicsShareAPI dbmfMalloc(void* handle,size_t x)
{
	chunk* c = (chunk*)handle;
	char** addr;
	char *node,*rc;
	int i;

	if(c==NULL)
	{
		if(def==NULL)
			def=dbmfInit(DBMF_SIZE,DBMF_ALIGN,DBMF_POOL);
		c=def;
	}

	if(c->free_list==NULL)
	{
		node=(char*)malloc(c->group_size);
		if(node)
		{
			semTake(c->sem,WAIT_FOREVER);
			for(i=0;i<c->total;i++)
			{
				/* yuck */
				addr=(char**)node;
				*addr=c->free_list;
				c->free_list=node;
				node+=c->chunk_size;
			}
			semGive(c->sem);
		}
		else
			rc=NULL;
	}
	if(x<=c->user_size)
	{
		semTake(c->sem,WAIT_FOREVER);
		node=c->free_list;
		if(node)
		{
			addr=(char**)node;
			c->free_list=*addr;
			node[0]=DBMF_OWNED;
		}
		semGive(c->sem);
	}
	else
	{
		node=(char*)malloc(x+c->align);
		node[0]=0x00;
	}

	return (void*)(&node[c->align]);
}

void epicsShareAPI dbmfFree(void* handle,void* x)
{
	chunk* c = (chunk*)handle;
	char* p = (char*)x;
	char** addr;

	/* kind-of goofy, bad if buffer not malloc'ed using dbmfMalloc() */
	if(c==NULL)
	{
		if(def==NULL)
			def=dbmfInit(DBMF_SIZE,DBMF_ALIGN,DBMF_POOL);
		c=def;
	}

	p-=c->align;
	if(*p!=DBMF_OWNED)
		free(p);
	else
	{
		addr=(char**)p;
		semTake(c->sem,WAIT_FOREVER);
		*addr=c->free_list;
		c->free_list=p;
		semGive(c->sem);
	}
}

#if MAKE_TEST_PROGRAM
#include <stdio.h>

int main()
{
	char* x[10];
	int i;
	void* handle;

	handle=dbStrInit(20,1,5);

	printf("ALLOCATE\n");
	for(i=0;i<10;i++)
	{
		x[i]=(char*)dbStrMalloc(i);
		printf("x[%d]=%8.8x\n",i,(int)x[i]);
	}
	printf("FREE\n");
	for(i=0;i<10;i++) dbStrFree(x[i]);

	printf("ALLOCATE\n");
	for(i=0;i<10;i++)
	{
		x[i]=(char*)dbStrMalloc(i);
		printf("x[%d]=%8.8x\n",i,(int)x[i]);
	}
	printf("FREE\n");
	for(i=0;i<10;i++) dbStrFree(x[i]);

	printf("ALLOCATE BIGGER\n");
	for(i=0;i<10;i++)
	{
		x[i]=(char*)dbStrMalloc(i+15);
		printf("x[%d]=%8.8x\n",i,(int)x[i]);
	}
	printf("FREE\n");
	for(i=0;i<10;i++) dbStrFree(x[i]);

	printf("ALLOCATE BIGGER\n");
	for(i=0;i<10;i++)
	{
		x[i]=(char*)dbStrMalloc(i+15);
		printf("x[%d]=%8.8x\n",i,(int)x[i]);
	}
	printf("FREE\n");
	for(i=0;i<10;i++) dbStrFree(x[i]);

	printf("ALLOCATE BIGGER\n");
	for(i=0;i<10;i++)
	{
		x[i]=(char*)dbStrMalloc(i+15);
		fprintf(stderr,"x[%d]=%8.8x\n",i,(int)x[i]);
	}
	fprintf(stderr,"FREE\n");
	for(i=0;i<10;i++)
		{ fprintf(stderr,"free %8.8x\n",(int)x[i]); dbStrFree(x[i]); }

	return 0;
}
#endif
