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
// Author: Jeff Hill
// This file implements a IO blocked list NOOP for multi-threaded systems
//

#include <stdio.h>

#define epicsExportSharedSymbols
#include "casdef.h"

//
// ioBlocked::~ioBlocked()
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
// (NOOP on MT system)
//
ioBlockedList::~ioBlockedList ()
{
}
 
//
// ioBlockedList::signal()
// (NOOP on MT system)
//
void ioBlockedList::signal()
{
}

//
// ioBlockedList::addItemToIOBLockedList()
// (NOOP on MT system)
//
void ioBlockedList::addItemToIOBLockedList(ioBlocked &)
{
}
 

