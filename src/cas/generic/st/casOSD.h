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
#include "osiTimer.h"
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
class casDGEvWakeup;
class casDGIOWakeup;

//
// casDGIntfOS
//
class casDGIntfOS : public casDGIntfIO {
    friend class casDGReadReg;
    friend class casDGBCastReadReg;
    friend class casDGWriteReg;
    friend class casDGEvWakeup;
    friend class casDGIOWakeup;
public:

    casDGIntfOS (caServerI &serverIn, const caNetAddr &addr, 
        bool autoBeaconAddr=TRUE, bool addConfigBeaconAddr=FALSE);

	virtual ~casDGIntfOS ();

	virtual void show (unsigned level) const;

private:
	casDGReadReg        *pRdReg;
	casDGBCastReadReg   *pBCastRdReg; // fix for solaris bug
	casDGWriteReg       *pWtReg;
	casDGEvWakeup       *pEvWk;
	casDGIOWakeup       *pIOWk;
	unsigned char		sendBlocked;

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

    void processInput();
};

//
// casIntfOS
//
class casIntfOS : public casIntfIO, public tsDLNode<casIntfOS>, 
    public casDGIntfOS
{
	friend class casDGEvWakeup;
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
};

class casStreamWriteReg;
class casStreamReadReg;
class casStreamEvWakeup;
class casStreamIOWakeup;

//
// casStreamOS
//
class casStreamOS : public casStreamIO {
    friend class casStreamWriteReg;
    friend class casStreamReadReg;
	friend class casStreamEvWakeup;
	friend class casStreamIOWakeup;
public:
	casStreamOS (caServerI &, const ioArgsToNewStreamIO &ioArgs);
	~casStreamOS ();

	void show (unsigned level) const;
private:
	casStreamWriteReg	*pWtReg;
	casStreamReadReg	*pRdReg;
	casStreamEvWakeup	*pEvWk;
	casStreamIOWakeup	*pIOWk;
	unsigned char		sendBlocked;
	//
	//
	//
	inline void armSend ();
	inline void armRecv ();
	inline void disarmSend();
	inline void disarmRecv();

	//
	// process any incomming messages
	//
	casProcCond processInput ();

	void recvCB ();
	void sendCB ();

	void sendBlockSignal ();
	
	void ioBlockedSignal ();

	void eventSignal ();
	void eventFlush ();
};

// no additions below this line
#endif // includeCASOSDH 

