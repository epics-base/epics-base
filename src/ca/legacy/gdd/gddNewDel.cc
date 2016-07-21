/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// Author: Jim Kowalkowski
// Date: 2/96

#define epicsExportSharedSymbols
#include "gddNewDel.h"
#include <stdio.h>

class gddCleanUpNode
{
public:
    void* buffer;
    gddCleanUpNode* next;
};

class gddCleanUp
{
public:
    gddCleanUp();
    ~gddCleanUp();
    void Add(void*);
private:
    gddCleanUpNode * bufs;
    epicsMutex lock;
};

static gddCleanUp * pBufferCleanUpGDD = NULL;

static epicsThreadOnceId gddCleanupOnce = EPICS_THREAD_ONCE_INIT;

extern "C" {
static void gddCleanupInit ( void * )
{
    pBufferCleanUpGDD = new gddCleanUp;
    assert ( pBufferCleanUpGDD );
}
}

void gddGlobalCleanupAdd ( void * pBuf )
{
    epicsThreadOnce ( & gddCleanupOnce, gddCleanupInit, 0 );
    pBufferCleanUpGDD->Add ( pBuf );
}

gddCleanUp::gddCleanUp() : bufs ( NULL ) {}

gddCleanUp::~gddCleanUp() 
{
	gddCleanUpNode *p1,*p2;

	for(p1=gddCleanUp::bufs;p1;)
	{
		p2=p1;
		p1=p1->next;
		free((char*)p2->buffer);
		delete p2;
	}
}

void gddCleanUp::Add(void* v)
{
	gddCleanUpNode* p = new gddCleanUpNode;
	p->buffer=v;
    {
        epicsGuard < epicsMutex > guard ( lock );
	    p->next=gddCleanUp::bufs;
	    gddCleanUp::bufs=p;
    }
}

