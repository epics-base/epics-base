
//
//
// casDGIntfOS.cc
// $Id$
//
//
//

//
// CA server
// 
#include "server.h"
#include "inBufIL.h" // inBuf in line func
#include "outBufIL.h" // outBuf in line func
#include "casIODIL.h" // IO Depen in line func

//
// casDGReadReg
//
class casDGReadReg : public fdReg {
public:
	casDGReadReg (casDGIntfOS &osIn) :
		fdReg (osIn.getFD(), fdrRead), os (osIn) {}
	~casDGReadReg ();

	void show (unsigned level) const;
private:
	casDGIntfOS &os;

	void callBack ();
};

//
// casDGBCastReadReg
//
class casDGBCastReadReg : public fdReg {
public:
	casDGBCastReadReg (casDGIntfOS &osIn) :
		fdReg (osIn.getBCastFD(), fdrRead), os (osIn) {}
	~casDGBCastReadReg ();

	void show (unsigned level) const;
private:
	casDGIntfOS &os;

	void callBack ();
};

//
// casDGWriteReg
//
class casDGWriteReg : public fdReg {
public:
    casDGWriteReg (casDGIntfOS &osIn) :
        fdReg (osIn.getFD(), fdrWrite), os (osIn) {}
    ~casDGWriteReg ();

    void show (unsigned level) const;
private:
    casDGIntfOS &os;

    void callBack ();
};

//
// class casDGEvWakeup
//
class casDGEvWakeup : public osiTimer {
public:

	casDGEvWakeup(casDGIntfOS &osIn) : 
		osiTimer(0.0), os(osIn) {}

	~casDGEvWakeup();

	void expire();

	void show(unsigned level) const;

	const char *name() const;

private:
	casDGIntfOS	&os;
};

//
// class casDGIOWakeup
//
class casDGIOWakeup : public osiTimer {
public:
	casDGIOWakeup (casDGIntfOS &osIn) : 
		osiTimer (0.0), os(osIn) {}
	~casDGIOWakeup ();

	void expire();

	void show(unsigned level) const;

	const char *name() const;
private:
	casDGIntfOS	&os;
};

//
// casDGIntfOS::casDGIntfOS()
//
casDGIntfOS::casDGIntfOS (caServerI &serverIn, const caNetAddr &addr, 
    bool autoBeaconAddr, bool addConfigBeaconAddr) : 
	casDGIntfIO (serverIn, addr, autoBeaconAddr, addConfigBeaconAddr),
	pRdReg (NULL),
    pBCastRdReg (NULL),
    pWtReg (NULL),
    pEvWk (NULL),
    pIOWk (NULL),
    sendBlocked (false)
{
	this->xSetNonBlocking();
    this->armRecv();
}

//
// casDGIntfOS::~casDGIntfOS()
//
casDGIntfOS::~casDGIntfOS()
{
	this->disarmSend();

	this->disarmRecv();

	if (this->pEvWk) {
		delete this->pEvWk;
	}

	if (this->pIOWk) {
		delete this->pIOWk;
	}
}


//
// casDGEvWakeup::~casDGEvWakeup()
//
casDGEvWakeup::~casDGEvWakeup()
{
	this->os.pEvWk = NULL;
}

//
// casDGEvWakeup::name()
//
const char *casDGEvWakeup::name() const
{
	return "casDGEvWakeup";
}

//
// casDGEvWakeup::show()
//
void casDGEvWakeup::show(unsigned level) const
{
	printf ("casDGEvWakeup at %p {\n", this);
	this->osiTimer::show(level);
	printf ("}\n");
}

//
// casDGEvWakeup::expire()
//
void casDGEvWakeup::expire()
{
	this->os.casEventSys::process();
}

//
// casDGIOWakeup::~casDGIOWakeup()
//
casDGIOWakeup::~casDGIOWakeup()
{
	this->os.pIOWk = NULL;
}

//
// casDGIOWakeup::expire()
//
// Running this indirectly off of the timer queue
// guarantees that we will not call processInput()
// recursively
//
void casDGIOWakeup::expire()
{
	//
	// in case there is something in the input buffer
	// and currently nothing to be read from UDP 
	//
	// processInput() does an armRecv() so
	// if recv is not armed, there is space in
	// the input buffer, and there eventually will
	// be something to read from TCP this works
	//
	this->os.processInput ();
}

//
// casDGIOWakeup::name()
//
const char *casDGIOWakeup::name() const
{
	return "casDGIOWakeup";
}

//
// casDGIOWakeup::show()
//
void casDGIOWakeup::show(unsigned level) const
{
	printf ("casDGIOWakeup at %p {\n", this);
	this->osiTimer::show(level);
	printf ("}\n");
}

//
// casDGIntfOS::armRecv ()
//
inline void casDGIntfOS::armRecv()
{
	if (!this->inBuf::full()) {
	    if (!this->pRdReg) {
			this->pRdReg = new casDGReadReg(*this);
			if (!this->pRdReg) {
				errMessage (S_cas_noMemory, "armRecv()");
                throw S_cas_noMemory;
			}
		}
	    if (this->validBCastFD() && this->pBCastRdReg==NULL) {
			this->pBCastRdReg = new casDGBCastReadReg(*this);
			if (!this->pBCastRdReg) {
				errMessage (S_cas_noMemory, "armRecv()");
                throw S_cas_noMemory;
			}
	    }
    }
}

//
// casDGIntfOS::disarmRecv()
//
inline void casDGIntfOS::disarmRecv()
{
	if (this->pRdReg) {
		delete this->pRdReg;
	}
	if (this->pBCastRdReg) {
		delete this->pBCastRdReg;
	}
}

//
// casDGIntfOS::armSend()
//
inline void casDGIntfOS::armSend()
{
	if (this->outBuf::bytesPresent()==0u) {
		return;
	}

	if (!this->pWtReg) {
		this->pWtReg = new casDGWriteReg(*this);
		if (!this->pWtReg) {
			errMessage (S_cas_noMemory, "armSend()");
            throw S_cas_noMemory;
		}
	}
}

//
// casDGIntfOS::disarmSend()
//
inline void casDGIntfOS::disarmSend ()
{
	if (this->pWtReg) {
		delete this->pWtReg;
	}
}

//
// casDGIntfOS::ioBlockedSignal()
//
void casDGIntfOS::ioBlockedSignal()
{
	if (!this->pIOWk) {
		this->pIOWk = new casDGIOWakeup(*this);
		if (!this->pIOWk) {
			errMessage (S_cas_noMemory,
				"casDGIntfOS::ioBlockedSignal()");
            throw S_cas_noMemory;
		}			
	}
}

//
// casDGIntfOS::eventSignal()
//
void casDGIntfOS::eventSignal()
{
	if (!this->pEvWk) {
		this->pEvWk = new casDGEvWakeup (*this);
		if (!this->pEvWk) {
			errMessage (S_cas_noMemory, 
				"casDGIntfOS::eventSignal ()");
            throw S_cas_noMemory;
		}
	}
}

//
// casDGIntfOS::eventFlush()
//
void casDGIntfOS::eventFlush()
{
	//
	// if there is nothing pending in the input
	// queue, then flush the output queue
	//
	if (this->inBuf::bytesAvailable()==0u) {
		this->armSend ();
	}
}

//
// casDGIntfOS::show()
//
void casDGIntfOS::show(unsigned level) const
{
	printf ("casDGIntfOS at %p\n", this);
	if (this->pRdReg) {
		this->pRdReg->show (level);
	}
	if (this->pWtReg) {
		this->pWtReg->show (level);
	}
    if (this->pBCastRdReg) {
		this->pBCastRdReg->show (level);
    }
    if (this->pEvWk) {
		this->pEvWk->show (level);
    }
    if (this->pIOWk) {
		this->pIOWk->show (level);
    }
}

//
// casDGReadReg::callBack()
//
void casDGReadReg::callBack()
{
    this->os.recvCB (inBuf::fpNone);
}

//
// casDGReadReg::~casDGReadReg()
//
casDGReadReg::~casDGReadReg()
{
    os.pRdReg = NULL;
}

//
// casDGReadReg::show()
//
void casDGReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf("casDGReadReg at %p\n", this);
}

//
// casDGBCastReadReg::callBack()
//
void casDGBCastReadReg::callBack()
{
    this->os.recvCB (inBuf::fpUseBroadcastInterface);
}

//
// casDGBCastReadReg::~casDGBCastReadReg()
//
casDGBCastReadReg::~casDGBCastReadReg()
{
    os.pBCastRdReg = NULL;
}

//
// casDGReadReg::show()
//
void casDGBCastReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf("casDGBCastReadReg at %p\n", this);
}

//
// casDGWriteReg::~casDGWriteReg()
//
casDGWriteReg::~casDGWriteReg()
{
	os.pWtReg = NULL; // allow rearm (send callbacks are one shots)
}

//
// casDGWriteReg::callBack()
//
void casDGWriteReg::callBack()
{
	casDGIntfOS *pDGIOS = &this->os;
	delete this; // allows rearm to occur if required
	pDGIOS->sendCB();
	//
	// NO CODE HERE - see delete above
	//
}

//
// casDGWriteReg::show()
//
void casDGWriteReg::show(unsigned level) const
{
	this->fdReg::show (level);
	printf ("casDGWriteReg: at %p\n", this);
}

//
// casDGIntfOS::sendBlockSignal()
//
void casDGIntfOS::sendBlockSignal()
{
	this->sendBlocked=TRUE;
	this->armSend();
}

//
// casDGIntfOS::sendCB()
//
void casDGIntfOS::sendCB()
{
	flushCondition flushCond;

	//
	// attempt to flush the output buffer 
	//
	flushCond = this->flush();
	if (flushCond==flushProgress) {
		if (this->sendBlocked) {
			this->sendBlocked = FALSE;
		}
        //
        // this reenables receipt of incoming frames once
        // the output has been flushed when the incomming
        // address is different
        //
        this->armRecv ();
	}

#	if defined(DEBUG)
		printf ("write attempted on %d result was %d\n", this->getFD(), flushCond);
		printf ("Recv backlog %u\n", this->inBuf::bytesPresent());
		printf ("Send backlog %u\n", this->outBuf::bytesPresent());
#	endif

	//
	// If we are unable to flush out all of the events 
	// in casDgEvWakeup::expire() because the
	// client is slow then we must check again here when
	// we _are_ able to write to see if additional events 
	// can be sent to the slow client.
	//
	this->casEventSys::process();

	//
	// If we were able to send something then we need
	// to process the input queue in case we were send
	// blocked.
	//
	this->processInput ();
}

//
// casDGIntfOS::recvCB()
//
void casDGIntfOS::recvCB (inBuf::fillParameter parm)
{	
	assert (this->pRdReg);

    //
    // copy in new messages 
    //
    this->inBuf::fill (parm);
    this->processInput ();

	//
	// If there isnt any space then temporarily 
	// stop calling this routine until problem is resolved 
	// either by:
	// (1) sending or
	// (2) a blocked IO op unblocks
	//
	// (casDGReadReg is _not_ a onceOnly fdReg - 
	// therefore an explicit delete is required here)
	//
	if (this->inBuf::full()) {
		this->disarmRecv(); // this deletes the casDGReadReg object
	}
}

//
// casDGIntfOS::processInput()
//
void casDGIntfOS::processInput()
{
	caStatus status;

    //
    // attempt to process the datagram in the input
    // buffer
    //
	status = this->processDG ();
	if (status!=S_cas_success &&
		status!=S_cas_sendBlocked &&
		status!=S_casApp_postponeAsyncIO) {
        char pName[64u];

        this->clientHostName (pName, sizeof (pName));
		errPrintf (status, __FILE__, __LINE__,
	        "unexpected problem with UDP input from \"%s\"", pName);
	}

	//
	// if anything is in the send buffer
	// and there are not requests pending in the 
    // input buffer then keep sending the output 
    // buffer until it is empty
	//
    if (this->outBuf::bytesPresent()>0u) {
        if ( this->bytesAvailable () == 0 ) {
            this->armSend ();
        }
	}
}

