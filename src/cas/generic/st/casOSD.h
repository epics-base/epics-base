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
public:
	caServerOS (caServerI &casIn) : 
		cas (casIn), pBTmr (NULL) {}
	caStatus init ();
	virtual ~caServerOS ();

	caStatus start ();

	inline caServerI * operator -> ();
private:
	caServerI	&cas;
	casBeaconTimer	*pBTmr;
};

class casDGClient;
class casDGReadReg;

//
// casDGIntfOS
//
class casDGIntfOS : public casDGIntfIO {
	friend class casDGReadReg;
public:
	casDGIntfOS(casDGClient &client);
	virtual ~casDGIntfOS();

	caStatus start();

	void show(unsigned level) const;

	void recvCB();
	void sendCB();
private:
	casDGReadReg    *pRdReg;
};

//
// casIntfOS
//
class casIntfOS : public casIntfIO, public tsDLNode<casIntfOS> 
{
	friend class casServerReg;
public:
	casIntfOS (caServerI &casIn) : 
		cas (casIn), pRdReg (NULL) {}
	caStatus init(const caNetAddr &addr, casDGClient &dgClientIn,
			int autoBeaconAddr, int addConfigBeaconAddr);
	virtual ~casIntfOS();

	void recvCB ();
	void sendCB () {}; // NOOP satifies template

	casDGIntfIO *newDGIntfIO (casDGClient &dgClientIn) const;
private:
	caServerI	&cas;
	casServerReg	*pRdReg;
};

class casStreamWriteReg;
class casStreamReadReg;
class casStreamEvWakeup;
class casStreamIOWakeup;

//
// casStreamOS
//
class casStreamOS : public casStreamIO {
	friend class casStreamReadReg;
	friend class casStreamWriteReg;
	friend class casStreamEvWakeup;
	friend class casStreamIOWakeup;
public:
	casStreamOS(caServerI &, const ioArgsToNewStreamIO &ioArgs);
	caStatus init();
	~casStreamOS();

	//
	// process any incomming messages
	//
	casProcCond processInput();
	caStatus start();

	void recvCB();
	void sendCB();

	void sendBlockSignal();
	
	void ioBlockedSignal();

	void eventSignal();
	void eventFlush();

	void show (unsigned level) const;
private:
	casStreamWriteReg	*pWtReg;
	casStreamReadReg	*pRdReg;
	casStreamEvWakeup	*pEvWk;
	casStreamIOWakeup	*pIOWk;
	unsigned		sendBlocked:1;
	//
	//
	//
	inline void armSend ();
	inline void armRecv ();
	inline void disarmSend();
	inline void disarmRecv();
};

class casDGEvWakeup;

//
// casDGOS
//
class casDGOS : public casDGIO {
	friend class casDGEvWakeup;
public:
	casDGOS(caServerI &cas);
	~casDGOS();

	//
	// process any incomming messages
	//
	casProcCond processInput();

	void sendBlockSignal() {}

	void eventSignal();
	void eventFlush();

	void show(unsigned level) const;

	caStatus start();
private:
	casDGEvWakeup	*pEvWk;
};

// no additions below this line
#endif // includeCASOSDH 

