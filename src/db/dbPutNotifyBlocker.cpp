
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

dbPutNotifyBlocker::dbPutNotifyBlocker ( dbChannelIO &chanIn ) :
    pPN (0), chan ( chanIn )
{
}

dbPutNotifyBlocker::~dbPutNotifyBlocker ()
{
    this->lock ();
    if ( this->pPN ) {
        this->pPN->destroy ();
    }
    this->unlock ();
}

void dbPutNotifyBlocker::putNotifyDestroyNotify ()
{
    this->lock ();
    this->pPN = 0;
    this->unlock ();
}


int dbPutNotifyBlocker::initiatePutNotify ( cacNotify &notify, 
        struct dbAddr &addr, unsigned type, unsigned long count, 
        const void *pValue )
{
    dbPutNotifyIO *pIO = new dbPutNotifyIO ( notify, *this );
    if ( ! pIO ) {
        return ECA_ALLOCMEM;
    }

    // wait for current put notify to complete
    this->lock ();
    if ( this->pPN ) {
        epicsTime begin = epicsTime::getCurrent ();
        do {
            this->unlock ();
            this->block.wait ( 1.0 );
            if ( epicsTime::getCurrent () - begin > 30.0 ) {
                pIO->destroy ();
                return ECA_PUTCBINPROG;
            }
            this->lock ();
        } while ( this->pPN );
    }
    this->pPN = pIO;
    int status = this->pPN->initiate ( addr, type, count, pValue );
    if ( status != ECA_NORMAL ) {
        this->pPN->destroy ();
        this->pPN = 0;
    }
    this->unlock ();
    return status;
}

extern "C" void putNotifyCompletion ( putNotify *ppn )
{
    dbPutNotifyBlocker *pBlocker = static_cast < dbPutNotifyBlocker * > ( ppn->usrPvt );
    pBlocker->lock ();
    pBlocker->pPN->completion ();
    pBlocker->pPN->destroy ();
    pBlocker->pPN = 0;
    pBlocker->unlock ();
    pBlocker->block.signal ();
}

void dbPutNotifyBlocker::show ( unsigned level ) const
{
    printf ( "put notify blocker at %p\n", this );
    if ( level > 0u ) {
        printf ( "\tdbPutNotifyIO at %p\n", this->pPN );
        printf ( "\tdbChannelIO at %p\n", &this->chan );
    }
    if ( level > 1u ) {
        this->block.show ( level - 2u );
    }
}



