/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* epicsMutex.c */
/*	Author: Jeff Hill */

#include <new>
#include <exception>

#define epicsExportSharedSymbols
#include "epicsEvent.h"
#include "epicsStdioRedirect.h"

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

void epicsEvent::signal ()
{
    epicsEventSignal ( this->id );
}

void epicsEvent::wait ()
{
    epicsEventWaitStatus status;
    status = epicsEventWait (this->id);
    if (status!=epicsEventWaitOK) {
        throw invalidSemaphore ();
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
        throw invalidSemaphore ();
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
        throw invalidSemaphore ();
    }
    return false;
}

void epicsEvent::show ( unsigned level ) const
{
    epicsEventShow ( this->id, level );
}
