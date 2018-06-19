/*************************************************************************\
* Copyright (c) 2011 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* epicsMutex.c */
/*	Author: Jeff Hill */

#include <new>
#include <exception>

#define epicsExportSharedSymbols
#include "epicsEvent.h"
#include "epicsStdio.h"
#include "cantProceed.h"

// vxWorks 5.4 gcc fails during compile when I use std::exception
using namespace std;

// exception payload
class epicsEvent::invalidSemaphore : public exception
{
    const char * what () const throw ();
}; 

const char * epicsEvent::invalidSemaphore::what () const throw () 
{
    return "epicsEvent::invalidSemaphore()";
}

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

void epicsEvent::trigger ()
{
    epicsEventStatus status = epicsEventTrigger (this->id);

    if (status != epicsEventOK) {
        throw invalidSemaphore ();
    }
}

void epicsEvent::wait ()
{
    epicsEventStatus status = epicsEventWait (this->id);

    if (status != epicsEventOK) {
        throw invalidSemaphore ();
    }
}

bool epicsEvent::wait (double timeOut)
{
    epicsEventStatus status = epicsEventWaitWithTimeout (this->id, timeOut);

    if (status == epicsEventOK) {
        return true;
    } else if (status == epicsEventWaitTimeout) {
        return false;
    }
    throw invalidSemaphore ();
}

bool epicsEvent::tryWait ()
{
    epicsEventStatus status = epicsEventTryWait (this->id);

    if (status == epicsEventOK) {
        return true;
    } else if (status == epicsEventWaitTimeout) {
        return false;
    }
    throw invalidSemaphore ();
}

void epicsEvent::show ( unsigned level ) const
{
    epicsEventShow ( this->id, level );
}


// epicsEventMust... convenience routines for C code

extern "C" {

epicsShareFunc epicsEventId epicsEventMustCreate (
    epicsEventInitialState initialState)
{
    epicsEventId id = epicsEventCreate (initialState);

    if (!id)
        cantProceed ("epicsEventMustCreate");
    return id;
}

epicsShareFunc void epicsEventMustTrigger (epicsEventId id) {
    epicsEventStatus status = epicsEventTrigger (id);

    if (status != epicsEventOK)
        cantProceed ("epicsEventMustTrigger");
}

epicsShareFunc void epicsEventMustWait (epicsEventId id) {
    epicsEventStatus status = epicsEventWait (id);

    if (status != epicsEventOK)
        cantProceed ("epicsEventMustWait");
}

} // extern "C"

