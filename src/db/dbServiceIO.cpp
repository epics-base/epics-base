
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

#include "cacIO.h"
#include "cadef.h" // this can be eliminated when the callbacks use the new interface
#include "db_access.h" // should be eliminated here in the future
#include "caerr.h" // should be eliminated here in the future
#include "epicsEvent.h"
#include "epicsThread.h"

#define epicsExportSharedSymbols
#include "db_access_routines.h"
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbNotifyBlockerIL.h"

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
    ioTable ( 1024 ), eventCallbackCacheSize ( 0ul ),
        ctx ( 0 ), pEventCallbackCache ( 0 ) 
{
}

dbServiceIO::~dbServiceIO ()
{
    if ( this->pEventCallbackCache ) {
        delete [] this->pEventCallbackCache;
    }
    if ( this->ctx ) {
        db_close_events ( this->ctx );
    }
}

cacChannel *dbServiceIO::createChannel (
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
        cacDataNotify &notify )
{
    unsigned long size = dbr_size_n ( type, count );

    if ( type > INT_MAX ) {
        notify.exception ( ECA_BADTYPE, 
            "type code out of range (high side)", 
            type, count );
        return;
    }

    if ( count > INT_MAX ) {
        notify.exception ( ECA_BADCOUNT, 
            "element count out of range (high side)",
            type, count);
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
            notify.exception ( ECA_ALLOCMEM, 
                "unable to allocate callback cache",
                type, count );
            return;
        }
        this->eventCallbackCacheSize = size;
    }
    void *pvfl = (void *) pfl;
    int status = db_get_field ( &addr, static_cast <int> ( type ), 
                    this->pEventCallbackCache, static_cast <int> ( count ), pvfl );
    if ( status ) {
        notify.exception ( ECA_GETFAIL, 
            "db_get_field() completed unsuccessfuly",
            type, count);
    }
    else { 
        notify.completion ( type, count, this->pEventCallbackCache );
    }
}

extern "C" void cacAttachClientCtx ( void * pPrivate )
{
    int status;
    caClientCtx clientCtx = pPrivate;
    status = ca_attach_context ( clientCtx );
    assert ( status == ECA_NORMAL );
}

dbEventSubscription dbServiceIO::subscribe ( struct dbAddr &addr, dbChannelIO &chan,
             dbSubscriptionIO &subscr, unsigned mask, cacChannel::ioid *pId )
{
    dbEventSubscription es;
    int status;

    caClientCtx clientCtx;

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
                0/*cacAttachClientCtx*/, 0/*clientCtx*/, above );
            if ( status ) {
                db_close_events ( this->ctx );
                this->ctx = 0;
                return 0;
            }
        }
        chan.dbServicePrivateListOfIO::eventq.add ( subscr );
        this->ioTable.add ( subscr );
    }

    es = db_add_event ( this->ctx, &addr,
        dbSubscriptionEventCallback, (void *) &subscr, mask );
    if ( es ) {
        db_post_single_event ( es );
    }
    else {
        epicsAutoMutex locker ( this->mutex );
        chan.dbServicePrivateListOfIO::eventq.remove ( subscr );
        this->ioTable.remove ( subscr );
    }

    if ( pId ) {
        *pId = subscr.getId ();
    }

    return es;
}

void dbServiceIO::initiatePutNotify ( dbChannelIO &chan, struct dbAddr &addr, 
    unsigned type, unsigned long count, const void *pValue, 
    cacNotify &notify, cacChannel::ioid *pId )
{
    epicsAutoMutex locker ( this->mutex );
    if ( ! chan.dbServicePrivateListOfIO::pBlocker ) {
        chan.dbServicePrivateListOfIO::pBlocker = new dbPutNotifyBlocker ( chan );
        if ( ! chan.dbServicePrivateListOfIO::pBlocker ) {
            throw cacChannel::noMemory();
        }
        this->ioTable.add ( *chan.dbServicePrivateListOfIO::pBlocker );
    }
    chan.dbServicePrivateListOfIO::pBlocker->initiatePutNotify ( 
        this->mutex, notify, addr, type, count, pValue );
    if ( pId ) {
        *pId = chan.dbServicePrivateListOfIO::pBlocker->getId ();
    }
}

void dbServiceIO::putNotifyCompletion ( dbPutNotifyBlocker &blocker )
{
    epicsAutoMutex locker ( this->mutex );
    blocker.completion ();
}

void dbServiceIO::destroyAllIO ( dbChannelIO & chan )
{
    dbSubscriptionIO *pIO;
    tsDLList < dbSubscriptionIO > tmp;
    {
        epicsAutoMutex locker ( this->mutex );
        while ( ( pIO = chan.dbServicePrivateListOfIO::eventq.get() ) ) {
            this->ioTable.remove ( *pIO );
            tmp.add ( *pIO );
        }
        if ( chan.dbServicePrivateListOfIO::pBlocker ) {
            this->ioTable.remove ( *chan.dbServicePrivateListOfIO::pBlocker );
        }
    }
    while ( ( pIO = tmp.get() ) ) {
        pIO->destroy ();
    }
    chan.dbServicePrivateListOfIO::pBlocker->destroy ();
}

void dbServiceIO::ioCancel ( dbChannelIO & chan, const cacChannel::ioid &id )
{
    epicsAutoMutex locker ( this->mutex );
    dbBaseIO *pIO = this->ioTable.remove ( id );
    if ( pIO ) {
        dbSubscriptionIO *pSIO = pIO->isSubscription ();
        if ( pSIO ) {
            chan.dbServicePrivateListOfIO::eventq.remove ( *pSIO );
            pIO->destroy ();
        }
        else if ( pIO == chan.dbServicePrivateListOfIO::pBlocker ) {
            chan.dbServicePrivateListOfIO::pBlocker->cancel ();
        }
        else {
            errlogPrintf ( "dbServiceIO::ioCancel() unrecognized IO was probably leaked\n" );
        }
    }
}

void dbServiceIO::ioShow ( const cacChannel::ioid &id, unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    const dbBaseIO *pIO = this->ioTable.lookup ( id );
    if ( pIO ) {
        pIO->show ( level );
    }
}

void dbServiceIO::showAllIO ( const dbChannelIO &chan, unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    tsDLIterConstBD < dbSubscriptionIO > pItem = 
        chan.dbServicePrivateListOfIO::eventq.firstIter ();
    while ( pItem.valid () ) {
        pItem->show ( level );
        pItem++;
    }
    if ( chan.dbServicePrivateListOfIO::pBlocker ) {
        chan.dbServicePrivateListOfIO::pBlocker->show ( level );
    }
}

void dbServiceIO::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    printf ( "dbServiceIO at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        printf ( "\tevent call back cache location %p, and its size %lu\n", 
            this->pEventCallbackCache, this->eventCallbackCacheSize );
    }
    if ( level > 1u ) {
        this->mutex.show ( level - 2u );
    }
}

