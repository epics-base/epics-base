
//
// $Id$
// Author Jeff Hill
//
// IO Blocked list class
// (for single threaded version of the server)
//
// $Log$
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

#include "casdef.h"
#include "osiMutexNOOP.h"


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
 

