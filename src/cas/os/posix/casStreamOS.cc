//
// casStreamOS.cc
// $Id$
//
//
// $Log$
//
//

//
// CA server
// 
#include<server.h>
#include <casClientIL.h> // casClient inline func

class casStreamEvWakeup : public osiTimer {
public:
	casStreamEvWakeup(casStreamOS &osIn) : 
		osiTimer(osiTime(0.0)), os(osIn) {}
	~casStreamEvWakeup()
	{
		os.pEvWk = NULL;
	}
	void expire();

	void show(unsigned level);

	const char *name()
	{
		return "casStreamEvWakeup";
	}
private:
	casStreamOS	&os;
};

//
// casStreamEvWakeup::show()
//
void casStreamEvWakeup::show(unsigned level)
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
		// if "this" is being used above this
		// routine on the stack then problems 
		// will result if we delete "this" here
		//
		// delete &this->os;	
		printf("strm event sys process failed\n");
	}
}

//
// casStreamOS::ioBlockedSignal()
//
void casStreamOS::ioBlockedSignal()
{
	//
	// in case there is something in the input buffer
	// and currently nothing to be read from TCP 
	//
	this->processInput();
	//
	// in case recv is not armed, there is space in 
	// the input buffer, and there eventually will
	// be something to read from TCP
	//
	this->armRecv();
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
casStreamOS::casStreamOS(caServerI &cas, casMsgIO &ioIn) : 
	casStrmClient(cas, ioIn)
{
	this->pRdReg = NULL;
	this->pWtReg = NULL;
	this->pEvWk = NULL;
        this->sendBlocked = FALSE;
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
	status = this->casStrmClient::init();
	if (status) {
		return status;
	}
	this->setNonBlocking();

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
}

//
// casStreamOS::show()
//
void casStreamOS::show(unsigned level)
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
// casStreamOS::disarmRecv()
//
void casStreamOS::disarmRecv()
{
        if (this->pRdReg) {
		delete this->pRdReg;
#		if defined(DEBUG)
			printf ("Read off %d\n", this->getFD());
			printf ("Recv backlog %u\n", 
				this->inBuf::bytesPresent());
			printf ("Send backlog %u\n", 
				this->outBuf::bytesPresent());
#		endif
	}
}


//
// casStreamOS::armRecv ()
//
void casStreamOS::armRecv()
{
        if (!this->pRdReg) {
		if (this->inBuf::full()==aitTrue) {
			return;
		}

		this->pRdReg = new casStreamReadReg(*this);
		if (!this->pRdReg) {
			errMessage(S_cas_noMemory, "armRecv()");
		}

#		if defined(DEBUG)
			printf ("Read on %d\n", this->getFD());
			printf ("Recv backlog %u\n", 
				this->inBuf::bytesPresent());
			printf ("Send backlog %u\n", 
				this->outBuf::bytesPresent());
#		endif		
        }
}

//
// casStreamReadReg::show()
//
void casStreamReadReg::show(unsigned level)
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
		delete this;
	}
	//
	// NO CODE HERE
	// (see deletes above)
	//
}


//
// casStreamReadReg::~casStreamReadReg
//
casStreamReadReg::~casStreamReadReg ()
{
	this->os.pRdReg = NULL;
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
// casStreamOS::armSend()
//
void casStreamOS::armSend()
{
	if (this->outBuf::bytesPresent()==0u) {
		return;
	}

	if (!this->pWtReg) {
		this->pWtReg = new casStreamWriteReg(*this);
		if (!this->pWtReg) {
			errMessage(S_cas_noMemory, "armSend() failed");
		}

#		if defined(DEBUG)
			printf ("Write on %d\n", this->getFD());
			printf ("Recv backlog %u\n", 
				this->inBuf::bytesPresent());
			printf ("Send backlog %u\n", 
				this->outBuf::bytesPresent());
#		endif
	}
}


//
// casStreamOS::disarmSend()
//
void casStreamOS::disarmSend ()
{
	if (this->pWtReg) {
		delete this->pWtReg;
#		if defined(DEBUG)
			printf ("Write off %d\n", this->getFD());
			printf ("Recv backlog %u\n", 
				this->inBuf::bytesPresent());
			printf ("Send backlog %u\n", 
				this->outBuf::bytesPresent());
#		endif
	}
}


//
// casStreamWriteReg::show()
//
void casStreamWriteReg::show(unsigned level)
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
	switch (flushCond) {
	case casFlushCompleted:
	case casFlushPartial:
		if (os.sendBlocked) {
			os.sendBlocked = FALSE;
		}
		break;
	case casFlushNone:
		break;
	case casFlushDisconnect:
		return;
		break;
	default:
		assert(0);
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
	// blocked
	//
	procCond = this->os.processInput();
	if (procCond == casProcDisconnect) {
		delete &this->os;	
	}
	else {
		//
		// anything left in the send buffer that
		// still needs to be sent ?
		// (once this starts sending it doesnt stop until
		// the outgoing buf is empty)
		//
		if (flushCond!=casFlushCompleted) {
			casStreamOS *pStrmOS = &this->os;
			//
			// force the delete now so that the
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
// casStreamWriteReg::~casStreamWriteReg ()
//
casStreamWriteReg::~casStreamWriteReg()
{
	this->os.pWtReg = NULL;
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
        switch (status) {
        case S_cas_sendBlocked:
        case S_cas_partialMessage:
        case S_cas_ioBlocked:
	case S_cas_success:
		if (this->inBuf::bytesAvailable()==0u) {
			this->armSend ();
		}
		this->armRecv();
		return casProcOk;
		break;
        default:
                errMessage (status,
                 	"unexpected error processing client's input");
		return casProcDisconnect;
        }
}

