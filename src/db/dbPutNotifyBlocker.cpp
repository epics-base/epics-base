
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

#include "limits.h"
#include "string.h"

#include "epicsMutex.h"
#include "epicsTime.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"
#include "dbChannelIOIL.h"
#include "dbNotifyBlockerIL.h"
#include "dbPutNotifyIOIL.h"

#define S_db_Blocked 	(M_dbAccess|39)
#define S_db_Pending 	(M_dbAccess|37)

tsFreeList <dbPutNotifyBlocker> dbPutNotifyBlocker::freeList;
epicsMutex dbPutNotifyBlocker::freeListMutex;

dbPutNotifyBlocker::dbPutNotifyBlocker ( dbChannelIO &chanIn ) :
    pPN (0), chan ( chanIn )
{
}

dbPutNotifyBlocker::~dbPutNotifyBlocker ()
{
    if ( this->pPN ) {
        this->pPN->destroy ();
    }
}

dbPutNotifyBlocker::uninstallPutNotifyIO ( dbPutNotifyIO &io )
{
    dbAutoScanLock ( this->chan );
    if ( &io == this->pPN ) {
        this->pPN = 0;
    }
}

dbChannelIO & dbPutNotifyBlocker::channel () const
{
    return this->chan;
}

int dbPutNotifyBlocker::initiatePutNotify ( cacNotify &notify, 
        struct dbAddr &addr, unsigned type, unsigned long count, 
        const void *pValue )
{
    dbPutNotifyIO *pIO = new dbPutNotifyIO ( notify, *this );
    if ( ! pIO ) {
        return ECA_ALLOCMEM;
    }

    epicsTime begin;
    bool beginTimeInit = false;
    while ( true ) {
        {
            dbAutoScanLock ( this->chan );
            if ( this->pPN == 0 ) {
                this->pPN = pIO;
                break;
            }
        }
        if ( beginTimeInit ) {
            if ( epicsTime::getCurrent () - begin > 30.0 ) {
                pIO->destroy ();
                return ECA_PUTCBINPROG;
            }
        }
        else {
            begin = epicsTime::getCurrent ();
            beginTimeInit = true;
        }
        this->block.wait ( 1.0 );
    }

    int status = pIO->initiate ( addr, type, count, pValue );
    if ( status != ECA_NORMAL ) {
        pIO->destroy ();
        dbAutoScanLock ( this->chan );
        this->pPN = 0;
    }

    return status;
}

extern "C" void putNotifyCompletion ( putNotify *ppn )
{
    dbPutNotifyBlocker *pBlocker = static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    {
        pBlocker->pPN->completion ();
        pBlocker->pPN->destroy ();
        dbAutoScanLock ( pBlocker->chan );
        pBlocker->pPN = 0;
    }
    pBlocker->block.signal ();
}

void dbPutNotifyBlocker::show ( unsigned level ) const
{
    printf ( "put notify blocker at %p\n", 
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        printf ( "\tdbPutNotifyIO at %p\n", 
            static_cast <void *> ( this->pPN ) );
        printf ( "\tdbChannelIO at %p\n", 
            static_cast <void *> ( &this->chan ) );
    }
    if ( level > 1u ) {
        this->block.show ( level - 2u );
    }
}



