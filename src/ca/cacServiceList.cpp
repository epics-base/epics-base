
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

cacServiceList::cacServiceList ()
{
}

void cacServiceList::registerService ( cacServiceIO &service )
{
    this->lock ();
    this->services.add ( service );
    this->unlock ();
}

cacLocalChannelIO * cacServiceList::createChannelIO (const char *pName, cacChannel &chan)
{
    cacLocalChannelIO *pChanIO = 0;

    this->lock ();
    tsDLIterBD <cacServiceIO> iter ( this->services.first () );
    while ( iter.valid () ) {
        pChanIO = iter->createChannelIO ( chan, pName );
        if ( pChanIO ) {
            break;
        }
        iter++;
    }
    this->unlock ();

    return pChanIO;
}

void cacServiceList::show ( unsigned level ) const
{
    cacLocalChannelIO *pChanIO = 0;

    this->lock ();
    tsDLIterConstBD < cacServiceIO > iter ( this->services.first () );
    while ( iter.valid () ) {
        iter->show ( level );
        iter++;
    }
    this->unlock ();
}

