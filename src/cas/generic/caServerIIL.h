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
 * Revision 1.6  1999/08/04 23:52:10  jhill
 * chronIntIdResTable name change
 *
 * Revision 1.5  1997/06/13 09:15:52  jhill
 * connect proto changes
 *
 * Revision 1.4  1997/04/10 19:33:54  jhill
 * API changes
 *
 * Revision 1.3  1996/11/02 00:53:56  jhill
 * many improvements
 *
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
    chronIntId      id (idIn);
    casRes          *pRes;    

	this->lock();
    pRes = this->chronIntIdResTable<casRes>::lookup (id);
	if (pRes) {
		if (pRes->resourceType()!=type) {
			pRes = NULL;
		}
	}
	this->unlock();
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
// caServerI::installItem()
//
inline void caServerI::installItem(casRes &res)
{
    this->chronIntIdResTable<casRes>::add(res);
}

//
// caServerI::removeItem()
//
inline casRes *caServerI::removeItem(casRes &res)
{
	return this->chronIntIdResTable<casRes>::remove(res);
}

//
// caServerI::setDebugLevel()
//
inline void caServerI::setDebugLevel(unsigned debugLevelIn)
{
	this->debugLevel = debugLevelIn;
}

inline casEventMask caServerI::valueEventMask() const
{
    return this->valueEvent;
}

inline casEventMask caServerI::logEventMask() const
{
    return this->logEvent;
}

inline casEventMask caServerI::alarmEventMask() const
{
    return this->alarmEvent;
}

#endif // caServerIIL_h

