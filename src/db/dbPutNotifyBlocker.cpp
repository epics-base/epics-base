
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
 *	Author: 
 *  Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#include <limits.h>
#include <string.h>

#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "cacIO.h"
#include "caerr.h" // this needs to be eliminated
#include "db_access.h" // this needs to be eliminated

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbNotifyBlockerIL.h"

#define S_db_Blocked 	(M_dbAccess|39)
#define S_db_Pending 	(M_dbAccess|37)

tsFreeList < dbPutNotifyBlocker > dbPutNotifyBlocker::freeList;
epicsMutex dbPutNotifyBlocker::freeListMutex;

dbPutNotifyBlocker::dbPutNotifyBlocker ( dbChannelIO &chanIn ) :
    chan ( chanIn ), pNotify ( 0 )
{
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
}

dbPutNotifyBlocker::~dbPutNotifyBlocker () 
{
}

void dbPutNotifyBlocker::destroy ()
{
    delete this;
}

void dbPutNotifyBlocker::cancel ()
{
    if ( this->pn.paddr ) {
        dbNotifyCancel ( &this->pn );
    }
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
    this->pNotify = 0;
    this->block.signal ();
}

extern "C" void putNotifyCompletion ( putNotify *ppn )
{
    dbPutNotifyBlocker *pBlocker = static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    if ( pBlocker->pNotify ) {
        if ( pBlocker->pn.status ) {
            if ( pBlocker->pn.status == S_db_Blocked ) {
                pBlocker->pNotify->exception ( 
                    ECA_PUTCBINPROG, "put notify blocked",
                    static_cast <unsigned> (pBlocker->pn.dbrType), 
                    static_cast <unsigned> (pBlocker->pn.nRequest) );
            }
            else {
                pBlocker->pNotify->exception ( 
                    ECA_PUTFAIL,  "put notify unsuccessful",
                    static_cast <unsigned> (pBlocker->pn.dbrType), 
                    static_cast <unsigned> (pBlocker->pn.nRequest) );
            }
        }
        else {
            pBlocker->pNotify->completion ();
        }
    }
    else {
        errlogPrintf ( "put notify completion pNotify = %p?\n", pBlocker->pNotify );
    }
    memset ( &pBlocker->pn, '\0', sizeof ( pBlocker->pn ) );
    pBlocker->pNotify = 0;
    pBlocker->block.signal ();
    pBlocker->chan.putNotifyCompletion ( *pBlocker );
}

void dbPutNotifyBlocker::completion ()
{
    memset ( &this->pn, '\0', sizeof ( this->pn ) );
    this->pNotify = 0;
    this->block.signal ();
}

void dbPutNotifyBlocker::initiatePutNotify ( epicsMutex &mutex, cacWriteNotify &notify, 
        struct dbAddr &addr, unsigned type, unsigned long count, 
        const void *pValue )
{
    int status;

    epicsTime begin;
    bool beginTimeInit = false;
    while ( true ) {
        if ( this->pNotify ) {
            this->pNotify = &notify;
            break;
        }
        if ( beginTimeInit ) {
            if ( epicsTime::getCurrent () - begin > 30.0 ) {
                throw -1;
            }
        }
        else {
            begin = epicsTime::getCurrent ();
            beginTimeInit = true;
        }
        {
            epicsAutoMutexRelease autoRelease ( mutex );
            this->block.wait ( 1.0 );
        }
    }

    if ( count > LONG_MAX ) {
        throw cacChannel::outOfBounds();
    }

    if ( type > SHRT_MAX ) {
        throw cacChannel::badType();
    }

    status = this->pn.dbrType = dbPutNotifyMapType ( 
                &this->pn, static_cast <short> ( type ) );
    if ( status ) {
        memset ( &this->pn, '\0', sizeof ( this->pn ) );
        this->pNotify = 0;
        throw cacChannel::badType();
    }

    this->pn.pbuffer = const_cast < void * > ( pValue );
    this->pn.nRequest = static_cast < unsigned > ( count );
    this->pn.paddr = &addr;
    this->pn.userCallback = putNotifyCompletion;
    this->pn.usrPvt = this;

    status = ::dbPutNotify ( &this->pn );
    if ( status && status != S_db_Pending ) {
        memset ( &this->pn, '\0', sizeof ( this->pn ) );
        this->pNotify = 0;
        {
            epicsAutoMutexRelease autoRelease ( mutex );
            notify.exception (
                ECA_PUTFAIL, "dbPutNotify() returned failure",
                static_cast <unsigned> (this->pn.dbrType), 
                static_cast <unsigned> (this->pn.nRequest) );
        }
    }
}

void dbPutNotifyBlocker::show ( unsigned level ) const
{
    printf ( "put notify blocker at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        printf ( "\tdbChannelIO at %p\n", 
            static_cast <void *> ( &this->chan ) );
    }
    if ( level > 1u ) {
        this->block.show ( level - 2u );
    }
}

dbSubscriptionIO * dbPutNotifyBlocker::isSubscription () 
{
    return 0;
}

