
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

#ifndef tcpKillTimerh  
#define tcpKillTimerh

#include "epicsTimer.h"
#include "epicsOnce.h"

class epicsTime;
class tcpiiu;
class cac;

class tcpKillTimer : private epicsTimerNotify, private epicsOnceNotify {
public:
    tcpKillTimer ( cac &, tcpiiu &, epicsTimerQueue & );
    virtual ~tcpKillTimer ();
    void start ();
    void show ( unsigned level ) const;
private:
    epicsOnce & once;
    epicsTimer & timer;
    cac & clientCtx;
    tcpiiu & iiu;
    static epicsSingleton < tsFreeList < class tcpKillTimer, 16 > > pFreeList;
    expireStatus expire ( const epicsTime & currentTime );
    void initialize ();
	tcpKillTimer ( const tcpKillTimer & );
	tcpKillTimer & operator = ( const tcpKillTimer & );
};

#endif // #ifndef tcpKillTimerh

