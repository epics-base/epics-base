//
// $Id$
//
// casOSD.h - Channel Access Server OS Dependent for posix
// 
//
// Some BSD calls have crept in here
//
// $Log$
// Revision 1.2  1997/04/10 19:34:31  jhill
// API changes
//
// Revision 1.1  1996/11/02 01:01:32  jhill
// installed
//
// Revision 1.3  1996/09/04 22:04:07  jhill
// moved netdb.h include here
//
// Revision 1.2  1996/08/13 22:58:15  jhill
// fdMgr.h => fdmanager.h
//
// Revision 1.1.1.1  1996/06/20 00:28:06  jhill
// ca server installation
//
//

#ifndef includeCASOSDH 
#define includeCASOSDH 

#include "osiTimer.h"
#include "fdManager.h"

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
	~caServerOS ();

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

