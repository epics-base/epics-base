/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */


#ifndef casPVIIL_h
#define casPVIIL_h

#include "dbMapper.h"
#include "caServerIIL.h"

//
// casPVI::getPCAS() 
//
inline caServerI *casPVI::getPCAS() const
{
	return this->pCAS;
}

//
// casPVI::lock()
//
inline void casPVI::lock() const
{
	//
	// NOTE:
	// if this lock becomes something else besides the
	// server's lock then look carefully at the 
	// comment in casPVI::deleteSignal()
	//
	if (this->pCAS) {
		this->pCAS->lock();
	}
	else {
		fprintf (stderr, "PV lock call when not attached to server?\n");
	}
}

//
// casPVI::unlock()
//
inline void casPVI::unlock() const
{
	if (this->pCAS) {
		this->pCAS->unlock();
	}
	else {
		fprintf (stderr, "PV unlock call when not attached to server?\n");
	}
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
// casPVI::unregisterIO()
//
inline void casPVI::unregisterIO()
{
	this->ioBlockedList::signal();
}

//
// casPVI::bestDBRType()
//
inline caStatus  casPVI::bestDBRType (unsigned &dbrType) // X aCC 361
{
	unsigned bestAIT = this->bestExternalType();

	if (bestAIT<NELEMENTS(gddAitToDbr)&&bestAIT!=aitEnumInvalid) {
		dbrType = gddAitToDbr[bestAIT];
		return S_cas_success;
	}
	else {
		return S_cas_badType;
	}
}

#include "casChannelIIL.h" // inline func for casChannelI

//
// functions that use casChannelIIL.h below here
//

//
// casPVI::postEvent()
//
inline void casPVI::postEvent (const casEventMask &select, const gdd &event)
{
	if (this->nMonAttached==0u) {
		return;
	}

	this->lock();
	tsDLIterBD<casPVListChan> iter = this->chanList.firstIter ();
    while ( iter.valid () ) {
		iter->postEvent ( select, event );
		++iter;
	}
	this->unlock();
}

//
// CA only does 1D arrays for now 
//
inline aitIndex casPVI::nativeCount () 
{
	if (this->maxDimension()==0u) {
		return 1u; // scalar
	}
	return this->maxBound(0u);
}

//
// casPVI::enumStringTable ()
//
inline const gddEnumStringTable & casPVI::enumStringTable () const
{
    return this->enumStrTbl;
}

#endif // casPVIIL_h



