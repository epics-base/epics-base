
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include "iocinf.h"

cacPrivateListOfIO::cacPrivateListOfIO ( cac &cacIn ) :
    cacCtx ( cacIn )
{
}

void cacPrivateListOfIO::addIO ( baseNMIU &io )
{
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.lock ();
    this->eventq.add ( io );
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.unlock ();
}

void cacPrivateListOfIO::removeIO ( baseNMIU &io )
{
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.lock ();
    this->eventq.remove ( io );
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.unlock ();
}

// Destroy all IO blocks attached.
// Care is taken here not to hold the lock while 
// sending a subscription delete message (which
// would result in deadlocks)
void cacPrivateListOfIO::destroyAllIO ()
{
    while ( true ) {
        unsigned id;
        bool done;

        this->cacCtx.cacPrivateListOfIOPrivate::mutex.lock ();
        {
            baseNMIU *pNMIU = this->eventq.first ();
            if ( pNMIU ) {
                id = pNMIU->getId ();
                done = false;
            }
            else {
                id = UINT_MAX;
                done = true;
            }
        }
        this->cacCtx.cacPrivateListOfIOPrivate::mutex.unlock ();

        if ( done ) {
            break;
        }
        // care is taken to not hold a lock when
        // executing this
        this->cacCtx.ioDestroy ( id );
    }
}

// resubscribe for monitors from this channel
void cacPrivateListOfIO::subscribeAllIO ()
{
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.lock ();
    tsDLIterBD < baseNMIU > iter = this->eventq.first ();
    while ( iter.valid () ) {
        iter->subscriptionMsg ();
        iter++;
    }
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.unlock ();
}

// cancel IO operations and monitor subscriptions
void cacPrivateListOfIO::disconnectAllIO ( const char *pHostName )
{
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.lock ();
    tsDLIterBD < baseNMIU > iter = this->eventq.first ();
    while ( iter.valid () ) {
        tsDLIterBD < baseNMIU > next = iter.itemAfter ();
        iter->disconnect ( pHostName );
        iter = next;
    }
    this->cacCtx.cacPrivateListOfIOPrivate::mutex.unlock ();
}
