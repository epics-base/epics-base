
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

#include "osiMutex.h"
#include "tsFreeList.h"
#include "errMdef.h"

#include "cadef.h"
#include "cacIO.h"

#define epicsExportSharedSymbols
#include "dbCAC.h"

#define S_db_Blocked 	(M_dbAccess|39)
#define S_db_Pending 	(M_dbAccess|37)

tsFreeList <dbPutNotifyBlocker> dbPutNotifyBlocker::freeList;

dbPutNotifyBlocker::dbPutNotifyBlocker ( dbChannelIO &chanIn ) :
    chan ( chanIn ), pPN (0)
{
}

dbPutNotifyBlocker::~dbPutNotifyBlocker ()
{
    if ( this->pPN ) {
        this->pPN->destroy ();
    }
}

void dbPutNotifyBlocker::destroy ()
{
    delete this;
}

void * dbPutNotifyBlocker::operator new ( size_t size )
{
    return dbPutNotifyBlocker::freeList.allocate ( size );
}

void dbPutNotifyBlocker::operator delete ( void *pCadaver, size_t size )
{
    dbPutNotifyBlocker::freeList.release ( pCadaver, size );
}
