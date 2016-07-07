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
// Author Jeff Hill
//
// IO Blocked list class
// (for single threaded version of the server)
//

#include <stdio.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"

#define epicsExportSharedSymbols
#include "ioBlocked.h"

//
// ioBlocked::ioBlocked ()
//
ioBlocked::ioBlocked () :
	pList(NULL)
{
}

//
// ioBlocked::~ioBlocked ()
//
ioBlocked::~ioBlocked ()
{
    if (this->pList) {
        this->pList->remove (*this);
        this->pList = NULL;
    }
}

//
// ioBlocked::ioBlockedSignal ()
//
void ioBlocked::ioBlockedSignal ()
{
    //
    // this must _not_ be pure virtual because
    // there are situations where this is called
    // inbetween the derived class's and this base 
    // class's destructors, and therefore a
    // NOOP is required
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
    for ( ioBlocked * pB = this->get (); pB; pB = this->get () ) {
        pB->pList = NULL;
    }
}
 
//
// ioBlockedList::signal ()
//
// works from a temporary list to avoid problems
// where the virtual function adds items to the
// list
//
void ioBlockedList::signal ()
{
    tsDLList<ioBlocked> tmp;
    
    //
    // move all of the items onto tmp
    //
    tmp.add(*this);
    
    for ( ioBlocked * pB = tmp.get (); pB; pB = tmp.get () ) {
        pB->pList = NULL;
        pB->ioBlockedSignal ();
    }
}

//
// ioBlockedList::addItemToIOBLockedList ()
//
void ioBlockedList::addItemToIOBLockedList (ioBlocked &item)
{
    if (item.pList==NULL) {
        this->add (item);
        item.pList = this;
    }
    else {
        assert (item.pList == this);
    }
}
 

