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
 * Revision 1.1  1996/11/02 01:01:06  jhill
 * installed
 *
 *
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
// casCoreClient::lookupRes()
//
inline casRes *casCoreClient::lookupRes(const caResId &idIn, casResType type)
{
	casRes *pRes;
	pRes = this->ctx.getServer()->lookupRes(idIn, type);
	return pRes;
}

//
// casCoreClient::installAsyncIO()
//
inline void casCoreClient::installAsyncIO(casAsyncIOI &ioIn)
{
	this->osiLock();
	this->ioInProgList.add(ioIn);
	this->osiUnlock();
}

//
// casCoreClient::removeAsyncIO()
//
inline void casCoreClient::removeAsyncIO(casAsyncIOI &ioIn)
{
	this->osiLock();
	this->ioInProgList.remove(ioIn);
	this->ctx.getServer()->ioBlockedList::signal();
	this->osiUnlock();
}

#endif // casCoreClientIL_h

