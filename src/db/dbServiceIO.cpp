
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

#include "limits.h"

#include "epicsMutex.h"
#include "tsFreeList.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIOIL.h"

class dbServiceIOLoadTimeInit {
public:
    dbServiceIOLoadTimeInit ();
private:
    dbServiceIO dbio;
};

// The following is just to force lti to be constructed
extern "C" void dbServiceIOInit()
{
    static dbServiceIOLoadTimeInit lti;
}

dbServiceIOLoadTimeInit::dbServiceIOLoadTimeInit ()
{
    cacGlobalServiceList.registerService ( this->dbio );
}

dbServiceIO::dbServiceIO () :
    ctx (0), pEventCallbackCache (0), eventCallbackCacheSize (0ul)
{
}

dbServiceIO::~dbServiceIO ()
{
    if ( this->pEventCallbackCache ) {
        delete [] this->pEventCallbackCache;
    }
    if (this->ctx) {
        db_close_events (this->ctx);
    }
}

cacChannelIO *dbServiceIO::createChannelIO (
            const char *pName, cacChannelNotify &notify )
{
    struct dbAddr addr;

    int status = db_name_to_addr ( pName, &addr );
    if ( status ) {
        return 0;
    }
    else {
        return new dbChannelIO ( notify, addr, *this ); 
    }
}

void dbServiceIO::callReadNotify ( struct dbAddr &addr, 
        unsigned type, unsigned long count, 
        const struct db_field_log *pfl, 
        cacChannelIO &chan, cacNotify &notify )
{
    unsigned long size = dbr_size_n ( type, count );

    if ( type > INT_MAX ) {
        notify.exceptionNotify ( chan, ECA_BADTYPE, 
            "type code out of range (high side)" );
        return;
    }

    if ( count > INT_MAX ) {
        notify.exceptionNotify ( chan, ECA_BADCOUNT, 
            "element count out of range (high side)" );
        return;
    }

    epicsAutoMutex locker ( this->mutex );

    if ( this->eventCallbackCacheSize < size) {
        if ( this->pEventCallbackCache ) {
            delete [] this->pEventCallbackCache;
        }
        this->pEventCallbackCache = new char [size];
        if ( ! this->pEventCallbackCache ) {
            this->eventCallbackCacheSize = 0ul;
            notify.exceptionNotify ( chan, ECA_ALLOCMEM, 
                "unable to allocate callback cache" );
            return;
        }
        this->eventCallbackCacheSize = size;
    }
    void *pvfl = (void *) pfl;
    int status = db_get_field ( &addr, static_cast <int> ( type ), 
                    this->pEventCallbackCache, static_cast <int> ( count ), pvfl );
    if ( status ) {
        notify.exceptionNotify ( chan, ECA_GETFAIL, 
            "db_get_field() completed unsuccessfuly" );
    }
    else { 
        notify.completionNotify ( chan, type, 
            count, this->pEventCallbackCache );
    }
}

extern "C" void cacAttachClientCtx ( void * pPrivate )
{
    int status;
    caClientCtx clientCtx = pPrivate;
    status = ca_attach_context ( clientCtx );
    assert ( status == ECA_NORMAL );
}

dbEventSubscription dbServiceIO::subscribe ( struct dbAddr &addr, dbSubscriptionIO &subscr, unsigned mask )
{
    caClientCtx clientCtx;
    dbEventSubscription es;
    int status;

    status = ca_current_context ( &clientCtx );
    if ( status != ECA_NORMAL ) {
        return 0;
    }

    {
        epicsAutoMutex locker ( this->mutex );
        if ( ! this->ctx ) {
            this->ctx = db_init_events ();
            if ( ! this->ctx ) {
                return 0;
            }
   
            unsigned selfPriority = epicsThreadGetPrioritySelf ();
            unsigned above;
            epicsThreadBooleanStatus tbs = epicsThreadLowestPriorityLevelAbove (selfPriority, &above);
            if ( tbs != epicsThreadBooleanStatusSuccess ) {
                above = selfPriority;
            }
            status = db_start_events ( this->ctx, "CAC-event", 
                cacAttachClientCtx, clientCtx, above );
            if ( status ) {
                db_close_events ( this->ctx );
                this->ctx = 0;
                return 0;
            }
        }
    }

    es = db_add_event ( this->ctx, &addr,
        dbSubscriptionEventCallback, (void *) &subscr, mask );
    if (es) {
        db_post_single_event ( es );
    }

    return es;
}

void dbServiceIO::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    printf ( "dbServiceIO at %p\n", 
        static_cast <const void *> ( this ) );
    if (level > 0u ) {
        printf ( "\tevent call back cache location %p, and its size %lu\n", 
            this->pEventCallbackCache, this->eventCallbackCacheSize );
    }
    if ( level > 1u ) {
        this->mutex.show ( level - 2u );
    }
}

