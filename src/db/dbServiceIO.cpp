
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

#include "osiMutex.h"
#include "tsFreeList.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"

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

cacLocalChannelIO *dbServiceIO::createChannelIO ( cacChannel &chan, const char *pName )
{
    struct dbAddr addr;

    int status = db_name_to_addr ( pName, &addr );
    if (status) {
        return 0;
    }
    else {
        return new dbChannelIO ( chan, addr, *this ); 
    }
}

void dbServiceIO::subscriptionUpdate ( struct dbAddr &addr, unsigned type, unsigned long count, 
        const struct db_field_log *pfl, cacNotifyIO &notify )
{
    unsigned long size = dbr_size_n ( type, count );

    this->mutex.lock ();
    if ( this->eventCallbackCacheSize < size) {
        if ( this->pEventCallbackCache ) {
            delete [] this->pEventCallbackCache;
        }
        this->pEventCallbackCache = new char [size];
        if ( ! this->pEventCallbackCache ) {
            this->eventCallbackCacheSize = 0ul;
            this->mutex.unlock ();
            notify.exceptionNotify ( ECA_ALLOCMEM, "unable to allocate callback cache" );
            return;
        }
        this->eventCallbackCacheSize = size;
    }
    void *pvfl = (void *) pfl;
    int status = db_get_field ( &addr, static_cast <int> ( type ), 
                    this->pEventCallbackCache, static_cast <int> ( count ), pvfl );
    if ( status ) {
        notify.exceptionNotify ( ECA_GETFAIL, "subscription update db_get_field () completed unsuccessfuly" );
    }
    else { 
        notify.completionNotify ( type, count, this->pEventCallbackCache );
    }
    this->mutex.unlock ();
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
    static const int slightlyHigherPriority = -1;
    int status;
    caClientCtx clientCtx;

    status = ca_current_context ( &clientCtx );
    if ( status != ECA_NORMAL ) {
        return 0;
    }

    this->mutex.lock ();
    if ( ! this->ctx ) {
        this->ctx = db_init_events ();
        if ( ! this->ctx ) {
            this->mutex.unlock ();
            return 0;
        }
        status = db_start_events ( this->ctx, "CAC event", 
            cacAttachClientCtx, clientCtx, slightlyHigherPriority );
        if ( status ) {
            db_close_events (this->ctx);
            this->ctx = 0;
            return 0;
        }
    }
    this->mutex.unlock ();

    return db_add_event ( this->ctx, &addr,
        dbSubscriptionEventCallback, (void *) &subscr, mask );
}
