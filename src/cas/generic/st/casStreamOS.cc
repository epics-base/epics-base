//
// casStreamOS.cc
// $Id$
//
//
// $Log$
// Revision 1.4  1996/12/12 21:24:17  jhill
// moved casStreamOS *pStrmOS decl down
//
// Revision 1.3  1996/12/12 19:02:36  jhill
// fixed send does not get armed after complete flush bug
//
// Revision 1.2  1996/12/11 00:55:14  jhill
// better message
//
// Revision 1.1  1996/11/02 01:01:33  jhill
// installed
//
// Revision 1.1.1.1  1996/06/20 00:28:06  jhill
// ca server installation
//
//
//
// TO DO:
// o armRecv() and armSend() should return bad status when
// 	there isnt enough memory
//

//
// CA server
// 
#include "server.h"
#include "casClientIL.h" // casClient inline func
#include "inBufIL.h" // inBuf inline func
#include "outBufIL.h" // outBuf inline func

//
// casStreamReadReg
//
class casStreamReadReg : public fdReg {
public:
        inline casStreamReadReg (casStreamOS &osIn);
	inline ~casStreamReadReg ();
 
        void show (unsigned level) const;
private:
        casStreamOS     &os;
 
        void callBack ();
};

//
// casStreamReadReg::casStreamReadReg()
//
inline casStreamReadReg::casStreamReadReg (casStreamOS &osIn) :
	os (osIn), fdReg (osIn.getFD(), fdrRead) 
{
#	if defined(DEBUG) 
		printf ("Read on %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.outBuf::bytesPresent());
#	endif		
}

//
// casStreamReadReg::~casStreamReadReg
//
inline casStreamReadReg::~casStreamReadReg ()
{
	this->os.pRdReg = NULL;
#	if defined(DEBUG) 
		printf ("Read off %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.outBuf::bytesPresent());
#	endif
}

//
// casStreamWriteReg
//
class casStreamWriteReg : public fdReg {
public:
        inline casStreamWriteReg (casStreamOS &osIn);
        inline ~casStreamWriteReg ();
 
        void show (unsigned level) const;
private:
        casStreamOS     &os;
 
        void callBack ();
};

//
// casStreamWriteReg::casStreamWriteReg()
//
inline casStreamWriteReg::casStreamWriteReg (casStreamOS &osIn) :
	os (osIn), fdReg (osIn.getFD(), fdrWrite, TRUE) 
{
#	if defined(DEBUG) 
		printf ("Write on %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.outBuf::bytesPresent());
#	endif
}

//
// casStreamWriteReg::~casStreamWriteReg ()
//
inline casStreamWriteReg::~casStreamWriteReg ()
{
	this->os.pWtReg = NULL;
#	if defined(DEBUG) 
		printf ("Write off %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.outBuf::bytesPresent());
#	endif
}

//
// class casStreamEvWakeup
//
class casStreamEvWakeup : public osiTimer {
public:
	casStreamEvWakeup(casStreamOS &osIn) : 
		osiTimer(osiTime(0.0)), os(osIn) {}
	~casStreamEvWakeup();

	void expire();

	void show(unsigned level) const;

	const char *name() const;
private:
	casStreamOS	&os;
};

//
// casStreamEvWakeup::~casStreamEvWakeup()
//
casStreamEvWakeup::~casStreamEvWakeup()
{
	this->os.pEvWk = NULL;
}

//
// casStreamEvWakeup::name()
//
const char *casStreamEvWakeup::name() const
{
	return "casStreamEvWakeup";
}

//
// casStreamEvWakeup::show()
//
void casStreamEvWakeup::show(unsigned level) const
{
	printf ("casStreamEvWakeup at %x {\n", (unsigned) this);
	this->osiTimer::show(level);
	printf ("}\n");
}

//
// casStreamEvWakeup::expire()
//
void casStreamEvWakeup::expire()
{
	casProcCond cond;
	cond = this->os.casEventSys::process();
	if (cond != casProcOk) {
		//
		// ok to delete the client here
		// because casStreamEvWakeup::expire()
		// is called by the timer queue system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		this->os.destroy();	

		//
		// must not touch the "this" pointer
		// from this point on however
		//
		return;
	}
}

//
// class casStreamIOWakeup
//
class casStreamIOWakeup : public osiTimer {
public:
	casStreamIOWakeup(casStreamOS &osIn) : 
		osiTimer(osiTime(0.0)), os(osIn) {}
	~casStreamIOWakeup();

	void expire();

	void show(unsigned level) const;

	const char *name() const;
private:
	casStreamOS	&os;
};

//
// casStreamIOWakeup::~casStreamIOWakeup()
//
casStreamIOWakeup::~casStreamIOWakeup()
{
	this->os.pIOWk = NULL;
}

//
// casStreamIOWakeup::name()
//
const char *casStreamIOWakeup::name() const
{
	return "casStreamIOWakeup";
}

//
// casStreamIOWakeup::show()
//
void casStreamIOWakeup::show(unsigned level) const
{
	printf ("casStreamIOWakeup at %x {\n", (unsigned) this);
	this->osiTimer::show(level);
	printf ("}\n");
}

//
// casStreamOS::armRecv ()
//
inline void casStreamOS::armRecv()
{
        if (!this->pRdReg) {
		if (this->inBuf::full()!=aitTrue) {
			this->pRdReg = new casStreamReadReg(*this);
			if (!this->pRdReg) {
				errMessage(S_cas_noMemory, "armRecv()");
			}
		}
        }
}

//
// casStreamIOWakeup::expire()
//
// Running this indirectly off of the timer queue
// guarantees that we will not call processInput()
// recursively
//
void casStreamIOWakeup::expire()
{
	//
	// in case there is something in the input buffer
	// and currently nothing to be read from TCP 
	//
	// processInput() does an armRecv() so
	// if recv is not armed, there is space in
	// the input buffer, and there eventually will
	// be something to read from TCP this works
	//
	this->os.processInput();
}

//
// casStreamOS::disarmRecv()
//
inline void casStreamOS::disarmRecv()
{
        if (this->pRdReg) {
		delete this->pRdReg;
	}
}

//
// casStreamOS::armSend()
//
inline void casStreamOS::armSend()
{
	if (this->outBuf::bytesPresent()==0u) {
		return;
	}

	if (!this->pWtReg) {
		this->pWtReg = new casStreamWriteReg(*this);
		if (!this->pWtReg) {
			errMessage(S_cas_noMemory, "armSend() failed");
		}
	}
}

//
// casStreamOS::disarmSend()
//
inline void casStreamOS::disarmSend ()
{
	if (this->pWtReg) {
		delete this->pWtReg;
	}
}

//
// casStreamOS::ioBlockedSignal()
//
void casStreamOS::ioBlockedSignal()
{
	if (!this->pIOWk) {
		this->pIOWk = new casStreamIOWakeup(*this);
		if (!this->pIOWk) {
			errMessage(S_cas_noMemory,
				"casStreamOS::ioBlockedSignal()");
		}			
	}
}

//
// casStreamOS::eventSignal()
//
void casStreamOS::eventSignal()
{
	if (!this->pEvWk) {
		this->pEvWk = new casStreamEvWakeup(*this);
		if (!this->pEvWk) {
			errMessage(S_cas_noMemory, 
				"casStreamOS::eventSignal()");
		}
	}
}

//
// casStreamOS::eventFlush()
//
void casStreamOS::eventFlush()
{
	this->armSend();
}


//
// casStreamOS::casStreamOS()
//
casStreamOS::casStreamOS(caServerI &cas, const ioArgsToNewStreamIO &ioArgs) : 
	casStreamIO(cas, ioArgs),
	pWtReg(NULL),
	pRdReg(NULL),
	pEvWk(NULL),
	pIOWk(NULL),
	sendBlocked(FALSE)
{
}

//
// casStreamOS::init()
//
caStatus casStreamOS::init()
{
	caStatus status;

	//
	// init the base classes
	//
	status = this->casStreamIO::init();
	if (status) {
		return status;
	}

	this->xSetNonBlocking();

	return S_cas_success;
}


//
// casStreamOS::~casStreamOS()
//
casStreamOS::~casStreamOS()
{
	//
	// attempt to flush out any remaining messages
	//
	this->flush();

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
// casStreamOS::show()
//
void casStreamOS::show(unsigned level) const
{
	this->casStrmClient::show(level);
	printf("casStreamOS at %x\n", (unsigned) this);
	printf("\tsendBlocked = %d\n", this->sendBlocked);
	if (this->pWtReg) {
		this->pWtReg->show(level);
	}
	if (this->pRdReg) {
		this->pRdReg->show(level);
	}
	if (this->pEvWk) {
		this->pEvWk->show(level);
	}
}


//
// casClientStart ()
//
caStatus casStreamOS::start()
{
	this->armRecv();
	return S_cas_success;
}


//
// casStreamReadReg::show()
//
void casStreamReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf ("casStreamReadReg at %x\n", (unsigned) this);
}

//
// casStreamReadReg::callBack ()
//
void casStreamReadReg::callBack ()
{
	casFillCondition fillCond;
	casProcCond procCond;

	assert (this->os.pRdReg);

        //
        // copy in new messages 
        //
        fillCond = os.fill();
	procCond = os.processInput();
	if (fillCond == casFillDisconnect ||
		procCond == casProcDisconnect) {
		delete &this->os;
	}	
	else if (os.inBuf::full()==aitTrue) {
		//
		// If there isnt any space then temporarily 
		// stop calling this routine until problem is resolved 
		// either by:
		// (1) sending or
		// (2) a blocked IO op unblocks
		//
		// (casStreamReadReg is _not_ a onceOnly fdReg - 
		// therefore an explicit delete is required here)
		//
		delete this;
	}
	//
	// NO CODE HERE
	// (see deletes above)
	//
}


//
// casStreamOS::sendBlockSignal()
//
void casStreamOS::sendBlockSignal()
{
	this->sendBlocked=TRUE;
	this->armSend();
}



//
// casStreamWriteReg::show()
//
void casStreamWriteReg::show(unsigned level) const
{
	this->fdReg::show (level);
	printf ("casStreamWriteReg at %x\n", (unsigned) this);
}


//
// casStreamWriteReg::callBack()
//
void casStreamWriteReg::callBack()
{
	casFlushCondition flushCond;
	casProcCond procCond; 

	assert (os.pWtReg);

	//
	// attempt to flush the output buffer 
	//
	flushCond = os.flush();
	if (	flushCond==casFlushCompleted ||
		flushCond==casFlushPartial) {
		if (os.sendBlocked) {
			os.sendBlocked = FALSE;
		}
	}
	else if (flushCond==casFlushDisconnect) {
		return;
	}
#if defined(DEBUG)
	else if (flushCond==casFlushNone) {
	}
	else {
		assert(0);
	}
#endif

	//
	// If we are unable to flush out all of the events 
	// in casStreamEvWakeup::expire() because the
	// client is slow then we must check again here when
	// we _are_ able to write to see if additional events 
	// can be sent to the slow client.
	//
	procCond = this->os.casEventSys::process();
	if (procCond != casProcOk) {
		//
		// ok to delete the client here
		// because casStreamWriteReg::callBack()
		// is called by the fdManager system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		this->os.destroy();	
		//
		// must not touch "this" pointer
		// after the destroy however
		//
		return;
	}

#	if defined(DEBUG)
		printf ("write attempted on %d result was %d\n", 
				os.getFD(), flushCond);
		printf ("Recv backlog %u\n", os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", os.outBuf::bytesPresent());
#	endif

	//
	// If we were able to send something then we need
	// to process the input queue in case we were send
	// blocked.
	//
	procCond = this->os.processInput();
	if (procCond == casProcDisconnect) {
		delete &this->os;	
	}
	else {
		casStreamOS *pStrmOS = &this->os;
		//
		// anything left in the send buffer that
		// still needs to be sent ?
		// (once this starts sending it doesnt stop until
		// the outgoing buf is empty)
		//
		// do not test for this with flushCond since
		// additional bytes may have been added since
		// we flushed the out buffer
		//
		if (pStrmOS->outBuf::bytesPresent()>0u) {
			//
			// delete this object now so that the
			// arm will work 
			//
			delete this;
			pStrmOS->armSend();
		}
	}
	//
	// NO CODE HERE
	// (see deletes above)
	//
}

//
// casStreamOS::processInput()
//
casProcCond casStreamOS::processInput()
{
        caStatus status;

#	ifdef DEBUG
		printf(
		"Resp bytes to send=%d, Req bytes pending %d\n", 
		this->outBuf::bytesPresent(),
		this->inBuf::bytesPresent());
#	endif

        status = this->processMsg();
	if (	status==S_cas_success ||
		status==S_cas_sendBlocked ||
		status==S_casApp_postponeAsyncIO ||
		status==S_cas_partialMessage) {

		if (this->inBuf::bytesAvailable()==0u) {
			this->armSend ();
		}
		this->armRecv();
		return casProcOk;
	}
	else {
                errMessage (status,
	"unexpected problem with client's input - forcing disconnect");
		return casProcDisconnect;
        }
}

