//
// $Id$
//
// casOSD.h - Channel Access Server OS dependent wrapper
// 
//
//

#ifndef includeCASOSDH 
#define includeCASOSDH 

#undef epicsExportSharedSymbols
#include "fdManager.h"

#define epicsExportSharedSymbols
#include "shareLib.h" // redefine share lib defines

class caServerI;
class caServerOS;

class casServerReg;
class casBeaconTimer;

//
// caServerOS
//
class caServerOS {
	friend class casServerReg;
    friend class casBeaconTimer;
public:
	caServerOS ();
	virtual ~caServerOS ();

private:
	casBeaconTimer	*pBTmr;

	virtual double getBeaconPeriod() const = 0;
	virtual void sendBeacon() = 0;
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
    casDGIntfOS (caServerI &serverIn, const caNetAddr &addr, 
        bool autoBeaconAddr=TRUE, bool addConfigBeaconAddr=FALSE);

	virtual ~casDGIntfOS ();

	virtual void show (unsigned level) const;

    void processInput();

private:
    casDGIOWakeup       ioWk;
    casDGEvWakeup       evWk;
	casDGReadReg        *pRdReg;
	casDGBCastReadReg   *pBCastRdReg; // fix for solaris bug
	casDGWriteReg       *pWtReg;
	bool		        sendBlocked;

    void armRecv ();
    void armSend ();

    void disarmRecv ();
    void disarmSend ();

    void recvCB (inBuf::fillParameter parm);
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
	casIntfOS (caServerI &casIn, const caNetAddr &addr, 
        bool autoBeaconAddr=true, bool addConfigBeaconAddr=false);
	virtual ~casIntfOS();

    virtual void show (unsigned level) const;

    caNetAddr serverAddress () const;

private:
	caServerI       &cas;
	casServerReg    *pRdReg;

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
    friend class casStreamWriteReg;
    friend class casStreamReadReg;
public:
	casStreamOS (caServerI &, const ioArgsToNewStreamIO &ioArgs);
	~casStreamOS ();

	void show (unsigned level) const;

	casProcCond processInput ();

private:
	casStreamEvWakeup	evWk;
	casStreamIOWakeup	ioWk;
	casStreamWriteReg	*pWtReg;
	casStreamReadReg	*pRdReg;
	bool		        sendBlocked;
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
};

// no additions below this line
#endif // includeCASOSDH 

