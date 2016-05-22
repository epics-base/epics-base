/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casStreamOSh
#define casStreamOSh 

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_casStreamOSh
#   undef epicsExportSharedSymbols
#endif

#undef epicsAssertAuthor
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"
#include "epicsTimer.h"

#ifdef epicsExportSharedSymbols_casStreamOSh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "casStreamIO.h"

class casStreamIOWakeup : public epicsTimerNotify {
public:
	casStreamIOWakeup ();
	virtual ~casStreamIOWakeup ();
	void show ( unsigned level ) const;
    void start ( class casStreamOS & osIn );
private:
    epicsTimer & timer;
	casStreamOS	* pOS;
	expireStatus expire ( const epicsTime & currentTime );
	casStreamIOWakeup ( const casStreamIOWakeup & );
	casStreamIOWakeup & operator = ( const casStreamIOWakeup & );
};

class casStreamEvWakeup : public epicsTimerNotify {
public:
	casStreamEvWakeup ( casStreamOS	& );
	virtual ~casStreamEvWakeup ();
	void show ( unsigned level ) const;
    void start ( class casStreamOS & osIn );
private:
    epicsTimer & timer;
	casStreamOS	& os;
	expireStatus expire ( const epicsTime & currentTime );
	casStreamEvWakeup ( const casStreamEvWakeup & );
	casStreamEvWakeup & operator = ( const casStreamEvWakeup & );
};

class casStreamOS : public casStreamIO {
public:
	casStreamOS ( caServerI &, clientBufMemoryManager &,
        const ioArgsToNewStreamIO & );
	~casStreamOS ();
	void show ( unsigned level ) const;
    void printStatus ( const char * pCtx ) const;
private:
	casStreamEvWakeup evWk;
	casStreamIOWakeup ioWk;
	class casStreamWriteReg * pWtReg;
	class casStreamReadReg * pRdReg;
	bufSizeT _sendBacklogThresh;
	void armSend ();
	void armRecv ();
	void disarmSend();
	void disarmRecv();
	void recvCB ();
	void sendCB ();
	void sendBlockSignal ();
	void ioBlockedSignal ();
	void eventSignal ();
    bool _sendNeeded () const;
	casStreamOS ( const casStreamOS & );
	casStreamOS & operator = ( const casStreamOS & );
    friend class casStreamWriteReg;
    friend class casStreamReadReg;
    friend class casStreamIOWakeup;
    friend class casStreamEvWakeup;
};

#endif // casStreamOSh
