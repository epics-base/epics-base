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
// $Log$
// Revision 1.2  1997/06/25 06:17:37  jhill
// fixed warnings
//
// Revision 1.1  1996/06/25 19:11:45  jbk
// new in EPICS base
//
//

// *Revision 1.2  1996/06/24 03:15:37  jbk
// *name changes and fixes for aitString and fixed string functions
// *Revision 1.1  1996/05/31 13:15:31  jbk
// *add new stuff

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

