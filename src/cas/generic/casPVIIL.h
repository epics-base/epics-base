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
// casPVI::installChannel()
//
inline void casPVI::installChannel ( casPVListChan & chan )
{
    epicsGuard < caServerI > guard ( * this->pCAS );
	this->chanList.add(chan);
}
 
//
// casPVI::removeChannel()
//
inline void casPVI::removeChannel ( casPVListChan & chan )
{
    epicsGuard < caServerI > guard ( * this->pCAS );
	this->chanList.remove(chan);
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
inline caStatus  casPVI::bestDBRType ( unsigned & dbrType ) // X aCC 361
{
	aitEnum bestAIT = this->bestExternalType ();
    if ( bestAIT == aitEnumInvalid || bestAIT < 0 ) {
		return S_cas_badType;
    }
    unsigned aitIndex = static_cast < unsigned > ( bestAIT );
	if ( aitIndex >= NELEMENTS ( gddAitToDbr ) ) {
		return S_cas_badType;
    }
	dbrType = gddAitToDbr[bestAIT];
	return S_cas_success;
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
	if ( this->nMonAttached == 0u ) {
		return;
	}

    epicsGuard < caServerI > guard ( * this->pCAS );
	tsDLIter < casPVListChan > iter = this->chanList.firstIter ();
    while ( iter.valid () ) {
		iter->postEvent ( select, event );
		++iter;
	}
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



