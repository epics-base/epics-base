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
// 
// $Id$
// 

#define epicsExportSharedSymbols
#include "gddNewDel.h"
#include <stdio.h>

gddCleanUp gddBufferCleanUp;

gddCleanUp::gddCleanUp(void) { }

gddCleanUp::~gddCleanUp(void) { gddCleanUp::CleanUp(); }

gddCleanUpNode* gddCleanUp::bufs = NULL;

gddSemaphore gddCleanUp::lock;

void gddCleanUp::CleanUp(void)
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
	lock.take();
	p->next=gddCleanUp::bufs;
	gddCleanUp::bufs=p;
	lock.give();
}

