/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// $Id$
//
// casOSD.h - Channel Access Server OS dependent wrapper
// 
//
//

#ifndef includeCASOSDH 
#define includeCASOSDH 

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_includeCASOSDH
#   undef epicsExportSharedSymbols
#endif
#include "fdManager.h"
#ifdef epicsExportSharedSymbols_includeCASOSDH
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

class caServerI;
class caServerOS;

class casServerReg;

//
// caServerOS
//
class caServerOS {
public:
	caServerOS ();
	virtual ~caServerOS ();
private:
	friend class casServerReg;
};

class casDGReadReg;
class casDGBCastReadReg;
class casDGWriteReg;

//
// class casDGEvWakeup
//
class casDGEvWakeup : public epicsTimerNotify {
public:
	casDGEvWakeup ();
	virtual ~casDGEvWakeup();
	void show ( unsigned level ) const;
    void start ( class casDGIntfOS &osIn );
private:
    epicsTimer &timer;
	class casDGIntfOS *pOS;
	expireStatus expire( const epicsTime & currentTime );
	casDGEvWakeup ( const casDGEvWakeup & );
	casDGEvWakeup & operator = ( const casDGEvWakeup & );
};

//
// class casDGIOWakeup
//
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

//
// casDGIntfOS
//
class casDGIntfOS : public casDGIntfIO {
    friend class casDGReadReg;
    friend class casDGBCastReadReg;
    friend class casDGWriteReg;
public:
    casDGIntfOS ( caServerI &, clientBufMemoryManager &,
        const caNetAddr & addr, bool autoBeaconAddr = true, 
        bool addConfigBeaconAddr = false);

	virtual ~casDGIntfOS ();

	virtual void show (unsigned level) const;

    void processInput();

private:
    casDGIOWakeup ioWk;
    casDGEvWakeup evWk;
	casDGReadReg *pRdReg;
	casDGBCastReadReg *pBCastRdReg; // fix for solaris bug
	casDGWriteReg *pWtReg;
	bool sendBlocked;

    void armRecv ();
    void armSend ();

    void disarmRecv ();
    void disarmSend ();

    void recvCB ( inBufClient::fillParameter parm );
	void sendCB ();

	void sendBlockSignal ();

	void ioBlockedSignal ();

	void eventSignal ();
	void eventFlush ();

	casDGIntfOS ( const casDGIntfOS & );
	casDGIntfOS & operator = ( const casDGIntfOS & );
};

//
// casIntfOS
//
class casIntfOS : public casIntfIO, public tsDLNode<casIntfOS>, 
    public casDGIntfOS
{
	friend class casServerReg;
public:
	casIntfOS ( caServerI &, clientBufMemoryManager &, const caNetAddr &, 
        bool autoBeaconAddr = true, bool addConfigBeaconAddr = false );
	virtual ~casIntfOS();
    void show ( unsigned level ) const;
    caNetAddr serverAddress () const;
private:
	caServerI       & cas;
	casServerReg    * pRdReg;

	casIntfOS ( const casIntfOS & );
	casIntfOS & operator = ( const casIntfOS & );
};

class casStreamWriteReg;
class casStreamReadReg;

//
// class casStreamIOWakeup
//
class casStreamIOWakeup : public epicsTimerNotify {
public:
	casStreamIOWakeup ();
	virtual ~casStreamIOWakeup ();
	void show ( unsigned level ) const;
    void start ( casStreamOS &osIn );
private:
    epicsTimer &timer;
	casStreamOS	*pOS;
	expireStatus expire ( const epicsTime & currentTime );
	casStreamIOWakeup ( const casStreamIOWakeup & );
	casStreamIOWakeup & operator = ( const casStreamIOWakeup & );
};

//
// class casStreamEvWakeup
//
class casStreamEvWakeup : public epicsTimerNotify {
public:
	casStreamEvWakeup ();
	virtual ~casStreamEvWakeup ();
	void show ( unsigned level ) const;
    void start ( casStreamOS &osIn );
private:
    epicsTimer &timer;
	casStreamOS	*pOS;
	expireStatus expire ( const epicsTime & currentTime );
	casStreamEvWakeup ( const casStreamEvWakeup & );
	casStreamEvWakeup & operator = ( const casStreamEvWakeup & );
};

//
// casStreamOS
//
class casStreamOS : public casStreamIO {
public:
	casStreamOS ( caServerI &, clientBufMemoryManager &,
        const ioArgsToNewStreamIO & );
	~casStreamOS ();

	void show ( unsigned level ) const;

	casProcCond processInput ();

private:
	casStreamEvWakeup evWk;
	casStreamIOWakeup ioWk;
	casStreamWriteReg * pWtReg;
	casStreamReadReg * pRdReg;
	bool sendBlocked;
	//
	//
	//
	inline void armSend ();
	inline void armRecv ();
	inline void disarmSend();
	inline void disarmRecv();

	void recvCB ();
	void sendCB ();

	void sendBlockSignal ();
	
	void ioBlockedSignal ();

	void eventSignal ();
	void eventFlush ();

	casStreamOS ( const casStreamOS & );
	casStreamOS & operator = ( const casStreamOS & );

    friend class casStreamWriteReg;
    friend class casStreamReadReg;
};

// no additions below this line
#endif // includeCASOSDH 

