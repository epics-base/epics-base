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


#ifndef casMonEventIL_h
#define casMonEventIL_h

//
// casMonEvent::casMonEvent()
//
inline casMonEvent::casMonEvent () : 
		id(0u) {}

//
// casMonEvent::casMonEvent()
//
inline casMonEvent::casMonEvent (casMonitor &monitor, const smartConstGDDPointer &pNewValue) :
        pValue ( pNewValue ),
        id ( monitor.casRes::getId () ) {}

//
// casMonEvent::casMonEvent()
//
inline casMonEvent::casMonEvent (const casMonEvent &initValue) :
        pValue ( initValue.pValue ),
        id ( initValue.id ) {}

//
// casMonEvent::operator =  ()
//
inline void casMonEvent::operator = (const class casMonEvent &monEventIn)
{
	this->pValue = monEventIn.pValue;
	this->id = monEventIn.id;
}

//
//  casMonEvent::clear()
//
inline void casMonEvent::clear()
{
	this->pValue = NULL;
	this->id = 0u;
}

//
// casMonEvent::getValue()
//
inline smartConstGDDPointer casMonEvent::getValue() const
{
	return this->pValue;
}

#endif // casMonEventIL_h

