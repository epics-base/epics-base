/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casDGIOWakeuph 
#define casDGIOWakeuph

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casDGIOWakeuph
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "epicsTimer.h"

#ifdef epicsExportSharedSymbols_casDGIOWakeuph
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class casDGIOWakeup : public epicsTimerNotify {
public:
	casDGIOWakeup ();
	virtual ~casDGIOWakeup ();
	void show ( unsigned level ) const;
    void start ( class casDGIntfOS &osIn );
private:
    epicsTimer &timer;
	class casDGIntfOS *pOS;
	expireStatus expire( const epicsTime & currentTime );
	casDGIOWakeup ( const casDGIOWakeup & );
	casDGIOWakeup & operator = ( const casDGIOWakeup & );
};

#endif // casDGIOWakeuph
