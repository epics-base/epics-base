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
 * Revision 1.7  1996/11/02 00:54:23  jhill
 * many improvements
 *
 * Revision 1.6  1996/09/16 18:24:05  jhill
 * vxWorks port changes
 *
 * Revision 1.5  1996/09/04 20:23:59  jhill
 * added operator ->
 *
 * Revision 1.4  1996/07/01 19:56:13  jhill
 * one last update prior to first release
 *
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

#include "dbMapper.h"

//
// casPVI::getCAS() 
//
inline caServerI &casPVI::getCAS() 
{
	return this->cas;
}

//
// casPVI::interfaceObjectPointer()
//
// casPVI must always be a base for casPV
// (the constructor assert fails if this isnt the case)
//
inline casPV *casPVI::interfaceObjectPointer() const
{
	return &this->pv;
}

//
// casPVI::operator -> ()
//
casPV * casPVI::operator -> () const
{
	return  interfaceObjectPointer();
}

//
// casPVI::lock()
//
inline void casPVI::lock()
{
	//
	// NOTE:
	// if this lock becomes something else besides the
	// server's lock then look carefully at the 
	// comment in casPVI::deleteSignal()
	//
	this->cas.osiLock();
}

//
// casPVI::unlock()
//
inline void casPVI::unlock()
{
	this->cas.osiUnlock();
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
	if (this->nIOAttached >= (*this)->maxSimultAsyncOps()) {               
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
	caServerI	&localCASRef(this->cas);

	//
	// We dont take the PV lock here because
	// the PV may be destroyed and we must
	// keep the lock unlock pairs consistent
	// (because the PV's lock is really a ref
	// to the server's lock)
	//
	// This is safe to do because we take the PV
	// lock when we add a new channel (and the
	// PV lock is realy the server's lock)
	//
	localCASRef.osiLock();

	if (this->chanList.count()==0u && !this->destroyInProgress) {
		(*this)->destroy();
		//
		// !! dont access self after destroy !!
		//
	}

	localCASRef.osiUnlock();
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
 
	this->lock();
        tsDLFwdIter<casPVListChan> iter(this->chanList);
        while ( (pChan = iter.next()) ) {
                pChan->postEvent(select, event);
        }
	this->unlock();
}

//
// CA only does 1D arrays for now 
//
inline aitIndex casPVI::nativeCount() 
{
	if ((*this)->maxDimension()==0u) {
		return 1u; // scaler
	}
	return (*this)->maxBound(0u);
}

#endif // casPVIIL_h



