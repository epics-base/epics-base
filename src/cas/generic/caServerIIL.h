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
 * Revision 1.2  1996/09/16 18:23:57  jhill
 * vxWorks port changes
 *
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef caServerIIL_h
#define caServerIIL_h

//
// caServerI::getAdapter()
//
inline caServer *caServerI::getAdapter()
{
	return &this->adapter;
}

//
// call virtual function in the interface class
//
inline caServer * caServerI::operator -> ()
{
	return this->getAdapter();
}

//
// caServerI::lookupRes()
//
inline casRes *caServerI::lookupRes(const caResId &idIn, casResType type)
{
	uintId	id(idIn);
	casRes	*pRes;

	this->osiLock();
	pRes = this->uintResTable<casRes>::lookup(id);
	if (pRes) {
		if (pRes->resourceType()!=type) {
			pRes = NULL;
		}
	}
	this->osiUnlock();
	return pRes;
}

//
// find the channel associated with a resource id
//
inline casChannelI *caServerI::resIdToChannel(const caResId &id)
{
        casRes *pRes;
 
        pRes = this->lookupRes(id, casChanT);
 
        //
        // safe to cast because we have checked the type code above
        // (and we know that casChannelI derived from casRes)
        //
        return (casChannelI *) pRes;
}

//
// caServerI::pvExistTestPossible()
//
inline aitBool caServerI::pvExistTestPossible()
{
	if (this->nExistTestInProg < this->maxSimultaneousIO) {
		return aitTrue;
	}
	return aitFalse;
}

//
// find the channel associated with a resource id
//
inline pvExistReturn caServerI::pvExistTest(
		const casCtx &ctxIn, const char *pPVName)
{
	this->osiLock();
	if (pvExistTestPossible()) {
		this->nExistTestInProg++;
		osiUnlock();
		return (*this)->pvExistTest(ctxIn, pPVName);
	}
	else {
		osiUnlock();
		return pvExistReturn(S_cas_ioBlocked);
	}
}

//
// caServerI::pvExistTestCompletion()
//
inline void caServerI::pvExistTestCompletion()
{
	this->osiLock();
	this->nExistTestInProg--;
	this->osiUnlock();
	this->ioBlockedList::signal();
}


//
// install a PV into the server
//
inline void caServerI::installPV (casPVI &pv)
{
	int resLibStatus;

	this->osiLock ();
	this->pvCount++;
	resLibStatus = this->stringResTbl.add (pv);
	this->osiUnlock ();
	assert (resLibStatus==0);
}

//
// remove  PV from the server
//
inline void caServerI::removePV(casPVI &pv)
{
	casPVI *pPV;

	this->osiLock();
	casVerify (this->pvCount>=1u);
	this->pvCount--;
	pPV = this->stringResTbl.remove (pv);
	this->osiUnlock();
	casVerify (pPV!=0);
	casVerify (pPV==&pv);
}

//
// caServerI::getPVMaxNameLength()
//
inline unsigned caServerI::getPVMaxNameLength() const
{
	return this->pvMaxNameLength;
}

//
// caServerI::installItem()
//
inline void caServerI::installItem(casRes &res)
{
	this->uintResTable<casRes>::installItem(res);
}

//
// caServerI::removeItem()
//
inline casRes *caServerI::removeItem(casRes &res)
{
	return this->uintResTable<casRes>::remove(res);
}

//
// caServerI::ready()
//
inline aitBool caServerI::ready()
{
	if (this->haveBeenInitialized) {
		return aitTrue;
	}
	else {
		return aitFalse;
	}
}

//
// caServerI::setDebugLevel()
//
inline void caServerI::setDebugLevel(unsigned debugLevelIn)
{
	this->debugLevel = debugLevelIn;
}

#endif // caServerIIL_h

