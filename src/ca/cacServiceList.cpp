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

#define epicsExportSharedSymbols
#include "cacIO.h"
#undef epicsExportSharedSymbols

epicsShareDef cacServiceList cacGlobalServiceList;

void cacServiceList::registerService ( cacService &service )
{
    epicsAutoMutex locker ( this->mutex );
    this->services.add ( service );
}

cacChannel * cacServiceList::createChannel ( 
    const char * pName, cacChannelNotify & chan, cacChannel::priLev pri )
{
    cacChannel *pChanIO = 0;

    epicsAutoMutex locker ( this->mutex );
    tsDLIterBD < cacService > iter = this->services.firstIter ();
    while ( iter.valid () ) {
        pChanIO = iter->createChannel ( pName, chan, pri );
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
    tsDLIterConstBD < cacService > iter = this->services.firstIter ();
    while ( iter.valid () ) {
        iter->show ( level );
        iter++;
    }
}
