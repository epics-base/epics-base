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

#include "cac.h"
#include "tcpKillTimer.h"

tcpKillTimer::tcpKillTimer ( cac & cacIn, tcpiiu & iiuIn, epicsTimerQueue & queueIn ) :
    once ( epicsOnce::create ( * this ) ), timer ( queueIn.createTimer () ), 
        clientCtx ( cacIn ), iiu ( iiuIn )
{
}

tcpKillTimer::~tcpKillTimer ()
{
    this->once.destroy ();
    this->timer.destroy ();
}

void tcpKillTimer::start ()
{
    this->once.once ();
}

void tcpKillTimer::initialize ()
{
    this->timer.start ( *this, 0.0 );
}

epicsTimerNotify::expireStatus tcpKillTimer::expire ( const epicsTime & /* currentTime */ )
{
    this->clientCtx.uninstallIIU ( this->iiu );
    return noRestart;
}

void tcpKillTimer::show ( unsigned level ) const
{
    ::printf ( "TCP kill timer %p\n",
        static_cast <const void *> ( this ) );
    if ( level > 0u ) {
        this->timer.show ( level - 1u );
    }
}

