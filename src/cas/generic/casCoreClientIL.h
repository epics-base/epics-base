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


#ifndef casCoreClientIL_h
#define casCoreClientIL_h

#include "caServerIIL.h" // caServerI in line func 
#include "casCtxIL.h" // casEventSys in line func 

//
// casCoreClient::getCAS()
//
inline caServerI &casCoreClient::getCAS() const
{
	return *this->ctx.getServer();
}

//
// casCoreClient::installAsyncIO()
//
inline void casCoreClient::installAsyncIO(casAsyncIOI &ioIn)
{
	this->lock();
	this->ioInProgList.add(ioIn);
	this->unlock();
}

//
// casCoreClient::removeAsyncIO()
//
inline void casCoreClient::removeAsyncIO(casAsyncIOI &ioIn)
{
	this->lock();
	this->ioInProgList.remove(ioIn);
	this->ctx.getServer()->ioBlockedList::signal();
	this->unlock();
}

#endif // casCoreClientIL_h

