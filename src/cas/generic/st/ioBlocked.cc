/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// $Id$
// Author Jeff Hill
//
// IO Blocked list class
// (for single threaded version of the server)
//
// $Log$
// Revision 1.4.8.1  2000/07/11 01:30:10  jhill
// fixed DLL export for the Borlund build
//
// Revision 1.4  1998/02/05 23:03:39  jhill
// hp comiler workaround changes
//
// Revision 1.3  1997/06/13 09:16:10  jhill
// connect proto changes
//
// Revision 1.2  1997/04/10 19:34:32  jhill
// API changes
//
// Revision 1.1  1996/11/02 01:01:34  jhill
// installed
//
//

#include <stdio.h>

#include "server.h"


//
// ioBlocked::ioBlocked()
//
ioBlocked::ioBlocked() :
	pList(NULL)
{
}

//
// ioBlocked::~ioBlocked()
//
ioBlocked::~ioBlocked()
{
}

//
// ioBlocked::ioBlockedSignal()
//
void ioBlocked::ioBlockedSignal()
{
	//
	// NOOP
	//
}

//
// ioBlockedList::ioBlockedList ()
//
ioBlockedList::ioBlockedList () 
{
}
 
//
// ioBlockedList::~ioBlockedList ()
//
ioBlockedList::~ioBlockedList ()
{
        ioBlocked *pB;
 
        while ( (pB = this->get ()) ) {
                pB->pList = NULL;
        }
}
 
//
// ioBlockedList::signal()
//
// works from a temporary list to avoid problems
// where the virtual function adds items to the
// list
//
void ioBlockedList::signal()
{
	tsDLList<ioBlocked> tmp;
	ioBlocked *pB;

	//
	// move all of the items onto tmp
	//
	tmp.add(*this);

	while ( (pB = tmp.get ()) ) {
		pB->pList = NULL;
		pB->ioBlockedSignal ();
	}
}

//
// ioBlockedList::removeItemFromIOBLockedList()
//
void ioBlockedList::removeItemFromIOBLockedList(ioBlocked &item)
{
	if (item.pList==this) {
		this->remove(item);
		item.pList = NULL;
	}
}
 
//
// ioBlockedList::addItemToIOBLockedList()
//
void ioBlockedList::addItemToIOBLockedList(ioBlocked &item)
{
        if (item.pList==NULL) {
                this->add(item);
                item.pList = this;
        }
        else {
                assert(item.pList == this);
        }
}
 

