
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

cacPrivate::cacPrivate ( cac &cacIn ) :
    cacCtx ( cacIn )
{
}

// Destroy all IO blocks attached.
// Care is taken here not to hold the lock while 
// sending a subscription delete message (which
// would result in deadlocks)
void cacPrivate::destroyAllIO ()
{
    while ( true ) {
        unsigned id;
        bool done;

        this->cacCtx.defaultMutex.lock ();
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
        this->cacCtx.defaultMutex.unlock ();

        if ( done ) {
            break;
        }
        // care is taken to not hold a lock when
        // executing this
        this->cacCtx.ioDestroy ( id );
    }
}

// resubscribe for monitors from this channel
void cacPrivate::subscribeAllIO ()
{
    this->cacCtx.defaultMutex.lock ();
    tsDLIterBD < baseNMIU > iter = this->eventq.first ();
    while ( iter.valid () ) {
        iter->subscriptionMsg ();
        iter++;
    }
    this->cacCtx.defaultMutex.unlock ();
}

// cancel IO operations and monitor subscriptions
void cacPrivate::disconnectAllIO ( const char *pHostName )
{
    this->cacCtx.defaultMutex.lock ();
    tsDLIterBD < baseNMIU > iter = this->eventq.first ();
    while ( iter.valid () ) {
        tsDLIterBD < baseNMIU > next = iter.itemAfter ();
        iter->disconnect ( pHostName );
        iter = next;
    }
    this->cacCtx.defaultMutex.unlock ();
}
