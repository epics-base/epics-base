/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.3  1996/06/26 21:18:58  jhill
 * now matches gdd api revisions
 *
 * Revision 1.2  1996/06/21 02:30:55  jhill
 * solaris port
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casPVIIL_h
#define casPVIIL_h

#include <dbMapper.h>

//
// casPVI::lock()
//
inline void casPVI::lock()
{
	this->cas.lock();
}

//
// casPVI::unlock()
//
inline void casPVI::unlock()
{
	this->cas.unlock();
}

//
// casPVI::installChannel()
//
inline void casPVI::installChannel(casPVListChan &chan)
{
	this->lock();
	this->chanList.add(chan);
	this->unlock();
}
 
//
// casPVI::removeChannel()
//
inline void casPVI::removeChannel(casPVListChan &chan)
{
	this->lock();
	this->chanList.remove(chan);
	this->unlock();
}

//
// okToBeginNewIO()
//
inline aitBool casPVI::okToBeginNewIO() const
{
	if (this->nIOAttached >= (*this)->maxSimultAsyncOps()) 
	{               
		return aitFalse;
	}
	else {
		return aitTrue;
	}
}

//
// casPVI::registerIO()
//
inline void casPVI::registerIO()
{
	this->lock();
	casVerify (this->nIOAttached < (*this)->maxSimultAsyncOps());
	this->nIOAttached++;
	this->unlock();
}

//
// casPVI::unregisterIO()
//
inline void casPVI::unregisterIO()
{
	this->lock();
	assert(this->nIOAttached>0u);
	this->nIOAttached--;
	this->unlock();
	this->ioBlockedList::signal();
}

//
// casPVI::deleteSignal()
// check for none attached and delete self if so
//
inline void casPVI::deleteSignal()
{
	this->lock();
	if (this->chanList.count()==0u) {
		(*this)->destroy();
	}
	else {
		this->unlock();
	}
}

//
// casPVI::bestDBRType()
//
inline caStatus  casPVI::bestDBRType (unsigned &dbrType)
{
	int dbr;
	dbr = gddAitToDbr[(*this)->bestExternalType()];
	if (INVALID_DB_FIELD(dbr)) {
		return S_cas_badType;
	}
	dbrType = dbr;
	return S_cas_success;
}

#include <casChannelIIL.h> // inline func for casChannelI

//
// functions that use casChannelIIL.h below here
//

//
// casPVI::postEvent()
//
inline void casPVI::postEvent (const casEventMask &select, gdd &event)
{
        casPVListChan           *pChan;
 
        if (this->nMonAttached==0u) {
                return;
        }
 
        //
        // the event queue is looking at the DD 
        // now so it must not be changed
        //
        event.markConstant();
 
        tsDLIter<casPVListChan> iter(this->chanList);
        while ( (pChan = iter()) ) {
                pChan->postEvent(select, event);
        }
}


#endif // casPVIIL_h



