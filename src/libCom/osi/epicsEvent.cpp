
/* epicsMutex.c */
/*	Author: Jeff Hill */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.

This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

#include <new>

#define epicsExportSharedSymbols
#include "epicsEvent.h"

//
// Its probably preferable to not make these inline because they are in 
// the sharable library interface. The use of inline or not here is probably
// not an issue because all of this ends up in the operating system in system 
// calls
//

epicsEvent::epicsEvent ( epicsEventInitialState initial ) :
    id ( epicsEventCreate ( initial ) )
{
    if ( this->id == 0 ) {
        throw std::bad_alloc ();
    }
}

epicsEvent::~epicsEvent ()
{
    epicsEventDestroy ( this->id );
}

void epicsEvent::signal ()
{
    epicsEventSignal ( this->id );
}

void epicsEvent::wait ()
{
    epicsEventWaitStatus status;
    status = epicsEventWait (this->id);
    if (status!=epicsEventWaitOK) {
        throwWithLocation ( invalidSemaphore () );
    }
}

bool epicsEvent::wait (double timeOut)
{
    epicsEventWaitStatus status;
    status = epicsEventWaitWithTimeout (this->id, timeOut);
    if (status==epicsEventWaitOK) {
        return true;
    } else if (status==epicsEventWaitTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false;
}

bool epicsEvent::tryWait ()
{
    epicsEventWaitStatus status;
    status = epicsEventTryWait (this->id);
    if (status==epicsEventWaitOK) {
        return true;
    } else if (status==epicsEventWaitTimeout) {
        return false;
    } else {
        throwWithLocation ( invalidSemaphore () );
    }
    return false;
}

void epicsEvent::show ( unsigned level ) const
{
    epicsEventShow ( this->id, level );
}
