//
// $Id$
// Author: Jeff Hill
// This file implements a IO blocked list NOOP for multi-threaded systems
//
// $Log$
// Revision 1.1  1996/11/02 01:01:24  jhill
// installed
//
//

#include <stdio.h>
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
// ioBlocked::ioBlockedSignal()
//
void ioBlocked::ioBlockedSignal()
{
	printf("in virtual base ioBlocked::ioBlockedSignal() ?\n");
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
// ioBlockedList::removeItemFromIOBLockedList()
// (NOOP on MT system)
//
void ioBlockedList::removeItemFromIOBLockedList(ioBlocked &)
{
}
 
//
// ioBlockedList::addItemToIOBLockedList()
// (NOOP on MT system)
//
void ioBlockedList::addItemToIOBLockedList(ioBlocked &)
{
}
 

