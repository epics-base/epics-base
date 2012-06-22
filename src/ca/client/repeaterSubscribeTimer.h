/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*  
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

#ifndef repeaterSubscribeTimerh  
#define repeaterSubscribeTimerh

#include "epicsTimer.h"

#ifdef epicsExportSharedSymbols
#   define repeaterSubscribeTimerh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "epicsTimer.h"

#ifdef repeaterSubscribeTimerh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class epicsMutex;
class cacContextNotify;

class repeaterTimerNotify {
public:
    virtual ~repeaterTimerNotify () = 0;
    virtual void repeaterRegistrationMessage ( 
        unsigned attemptNumber ) = 0;
    virtual int printFormated ( 
        epicsGuard < epicsMutex > & callbackControl, 
        const char * pformat, ... ) = 0;
};

class repeaterSubscribeTimer : private epicsTimerNotify {
public:
    repeaterSubscribeTimer ( 
        repeaterTimerNotify &, epicsTimerQueue &, 
        epicsMutex & cbMutex, cacContextNotify & ctxNotify );
    virtual ~repeaterSubscribeTimer ();
    void start ();
    void shutdown (
        epicsGuard < epicsMutex > & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void confirmNotify ();
	void show ( unsigned level ) const;
private:
    epicsTimer & timer;
    repeaterTimerNotify & iiu;
    epicsMutex & cbMutex;
    cacContextNotify & ctxNotify;
    unsigned attempts;
    bool registered;
    bool once;
	expireStatus expire ( const epicsTime & currentTime );
	repeaterSubscribeTimer ( const repeaterSubscribeTimer & );
	repeaterSubscribeTimer & operator = ( const repeaterSubscribeTimer & );
};

#endif // ifdef repeaterSubscribeTimerh
