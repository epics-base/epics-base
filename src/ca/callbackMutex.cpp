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
 *  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "cac.h"

callbackMutex::callbackMutex ( bool threadsMayBeBlockingForRecvThreadsToFinishIn ) :
    recvThreadsPendingCount ( 0u ), 
    threadsMayBeBlockingForRecvThreadsToFinish ( threadsMayBeBlockingForRecvThreadsToFinishIn )
{
}

callbackMutex::~callbackMutex ()
{
}

void callbackMutex::lock ()
{
    if ( ! this->primaryMutex.tryLock () ) {
        // the count must be incremented prior to blocking for the lock
        {
            epicsGuard < epicsMutex > autoMutex ( this->countMutex );
            assert ( this->recvThreadsPendingCount < UINT_MAX );
            this->recvThreadsPendingCount++;
        }
        
        this->primaryMutex.lock ();
        
        bool signalRequired;
        {
            epicsGuard < epicsMutex > autoMutex ( this->countMutex );
            assert ( this->recvThreadsPendingCount > 0 );
            this->recvThreadsPendingCount--;
            if ( this->recvThreadsPendingCount == 0u &&
                this->threadsMayBeBlockingForRecvThreadsToFinish ) {
                signalRequired = true;
            }
            else {
                signalRequired = false;
            }
        }
        
        if ( signalRequired ) {
            this->noRecvThreadsPending.signal ();
        }
    }
}

void callbackMutex::unlock ()
{
    this->primaryMutex.unlock ();
}

void callbackMutex::waitUntilNoRecvThreadsPending () 
{
    while ( this->recvThreadsPendingCount > 0 ) {
        this->noRecvThreadsPending.wait ();
    }
}
