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


#ifndef casChannelIIL_h
#define casChannelIIL_h

#include "casCoreClientIL.h"
#include "casEventSysIL.h"

//
// casChannelI::postEvent()
//
inline void casChannelI::postEvent ( const casEventMask &select, const gdd &event )
{
    this->pClient->postEvent ( this->monitorList, select, event );
}

//
// casChannelI::destroyNoClientNotify()
//
inline void casChannelI::destroyNoClientNotify() 
{
	this->destroy ();
}

#include "casPVIIL.h"

//
// functions that use casPVIIL.h below here 
//

//
// casChannelI::installAsyncIO()
//
inline void casChannelI::installAsyncIO ( casAsyncIOI & io )
{
    // install through the client so that we can 
    // use its lock to protect the list
    this->pClient->installChannelsAsynchIO ( this->ioInProgList, io );
}

//
// casChannelI::removeAsyncIO()
//
inline void casChannelI::removeAsyncIO ( casAsyncIOI & io )
{
    // uninstall through the client so that we can 
    // use its lock to protect the list
    this->pClient->uninstallChannelsAsynchIO ( this->ioInProgList, io );
    this->pPV->unregisterIO ();
}

//
// casChannelI::getSID()
// fetch the unsigned integer server id for this PV
//
inline const caResId casChannelI::getSID()
{
    return this->casRes::getId();
}

//
// casChannelI::postAccessRightsEvent()
//
inline void casChannelI::postAccessRightsEvent()
{
	if ( ! this->accessRightsEvPending ) {
		this->accessRightsEvPending = true;
		this->pClient->addToEventQueue ( *this );
	}
}

//
// casChannelI::enumStringTable ()
//
inline const gddEnumStringTable & casChannelI::enumStringTable () const
{
    return this->pPV->enumStringTable ();
}

#endif // casChannelIIL_h

