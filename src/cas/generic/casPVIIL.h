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
 */


#ifndef casPVIIL_h
#define casPVIIL_h

#include "dbMapper.h"

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
inline caStatus  casPVI::bestDBRType (unsigned &dbrType)
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
inline void casPVI::postEvent (const casEventMask &select, const smartConstGDDPointer &pEvent)
{
	if (this->nMonAttached==0u) {
		return;
	}

	this->lock();
	tsDLIterBD<casPVListChan> iter = this->chanList.firstIter ();
    while ( iter.valid () ) {
		iter->postEvent ( select, pEvent );
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
inline const std::vector< std::string >& casPVI::enumStringTable () const
{
    return this->enumStrTbl;
}

#endif // casPVIIL_h



