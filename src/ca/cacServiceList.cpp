
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

epicsShareDef cacServiceList cacGlobalServiceList;

void cacServiceList::registerService ( cacServiceIO &service )
{
    epicsAutoMutex locker ( this->mutex );
    this->services.add ( service );
}

cacChannelIO * cacServiceList::createChannelIO ( 
    const char *pName, cacChannelNotify &chan )
{
    cacChannelIO *pChanIO = 0;

    epicsAutoMutex locker ( this->mutex );
    tsDLIterBD < cacServiceIO > iter = this->services.firstIter ();
    while ( iter.valid () ) {
        pChanIO = iter->createChannelIO ( pName, chan );
        if ( pChanIO ) {
            break;
        }
        iter++;
    }

    return pChanIO;
}

void cacServiceList::show ( unsigned level ) const
{
    epicsAutoMutex locker ( this->mutex );
    tsDLIterConstBD < cacServiceIO > iter = this->services.firstIter ();
    while ( iter.valid () ) {
        iter->show ( level );
        iter++;
    }
}

