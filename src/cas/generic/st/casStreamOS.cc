//
// casStreamOS.cc
// $Id$
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
    casStreamOS &os;
    void callBack ();
	casStreamReadReg ( const casStreamReadReg & );
	casStreamReadReg & operator = ( const casStreamReadReg & );
};

//
// casStreamReadReg::casStreamReadReg()
//
inline casStreamReadReg::casStreamReadReg (casStreamOS &osIn) :
	fdReg (osIn.getFD(), fdrRead), os (osIn)
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
    os.pRdReg = NULL; 
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
    casStreamOS &os;
    void callBack ();
	casStreamWriteReg ( const casStreamWriteReg & );
	casStreamWriteReg & operator = ( const casStreamWriteReg & );
};

//
// casStreamWriteReg::casStreamWriteReg()
//
inline casStreamWriteReg::casStreamWriteReg (casStreamOS &osIn) :
	fdReg (osIn.getFD(), fdrWrite, TRUE), os (osIn)
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
    os.pWtReg = NULL; 
#	if defined(DEBUG) 
		printf ("Write off %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.inBuf::bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.outBuf::bytesPresent());
#	endif
}

//
// casStreamEvWakeup()
//
casStreamEvWakeup::casStreamEvWakeup () : 
    timer ( fileDescriptorManager.createTimer() ), pOS ( 0 ) 
{
}

//
// casStreamEvWakeup::~casStreamEvWakeup()
//
casStreamEvWakeup::~casStreamEvWakeup()
{
    this->timer.destroy ();
}

//
// casStreamEvWakeup::show()
//
void casStreamEvWakeup::show(unsigned level) const
{
	printf ( "casStreamEvWakeup at %p {\n", 
        static_cast <const void *> ( this ) );
	this->timer.show ( level );
	printf ( "}\n" );
}

//
// casStreamEvWakeup::expire()
//
epicsTimerNotify::expireStatus casStreamEvWakeup::expire( const epicsTime & /* currentTime */ )
{
    casStreamOS &os = *this->pOS;
    this->pOS = 0;
	casProcCond cond = os.casEventSys::process();
	if ( cond != casProcOk ) {
		//
		// ok to delete the client here
		// because casStreamEvWakeup::expire()
		// is called by the timer queue system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		os.destroy();	

		//
		// must not touch the "this" pointer
		// from this point on however
		//
	}
    return noRestart;
}

//
// casStreamEvWakeup::start()
//
void casStreamEvWakeup::start( casStreamOS &os )
{    
    if ( this->pOS ) {
        assert ( this->pOS == &os );
    }
    else {
        this->pOS = &os;
        this->timer.start ( *this, 0.0 );
    }
}

//
// casStreamIOWakeup::casStreamIOWakeup()
//
casStreamIOWakeup::casStreamIOWakeup () : 
	timer ( fileDescriptorManager.createTimer() ), pOS ( 0 )
{
}

//
// casStreamIOWakeup::~casStreamIOWakeup()
//
casStreamIOWakeup::~casStreamIOWakeup()
{
    this->timer.destroy ();
}

//
// casStreamIOWakeup::show()
//
void casStreamIOWakeup::show ( unsigned level ) const
{
	printf ( "casStreamIOWakeup at %p {\n", 
        static_cast <const void *> ( this ) );
	this->timer.show ( level );
	printf ( "}\n" );
}

//
// casStreamIOWakeup::expire()
//
// Running this indirectly off of the timer queue
// guarantees that we will not call processInput()
// recursively
//
epicsTimerNotify::expireStatus casStreamIOWakeup::expire ( const epicsTime & /* currentTime */ )
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
	this->pOS->processInput();
    this->pOS = 0;
    return noRestart;
}

//
// casStreamIOWakeup::start()
//
void casStreamIOWakeup::start ( casStreamOS &os  )
{
    if ( this->pOS ) {
        assert ( this->pOS == &os );
    }
    else {
        this->pOS = &os;
        this->timer.start ( *this, 0.0 );
    }
}

//
// casStreamOS::armRecv ()
//
inline void casStreamOS::armRecv()
{
	if ( ! this->pRdReg ) {
		if ( ! this->in.full() ) {
			this->pRdReg = new casStreamReadReg ( *this );
			if ( ! this->pRdReg ) {
				errMessage ( S_cas_noMemory, "armRecv()" );
                throw S_cas_noMemory;
			}
		}
	}
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
	if ( this->out.bytesPresent () == 0u ) {
		return;
	}

	if ( ! this->pWtReg ) {
		this->pWtReg = new casStreamWriteReg(*this);
		if (!this->pWtReg) {
			errMessage(S_cas_noMemory, "armSend() failed");
            throw S_cas_noMemory;
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
    this->ioWk.start ( *this );
}

//
// casStreamOS::eventSignal()
//
void casStreamOS::eventSignal()
{
    this->evWk.start ( *this );
}

//
// casStreamOS::eventFlush()
//
void casStreamOS::eventFlush()
{
	//
	// if there is nothing pending in the input
	// queue, then flush the output queue
	//
	if ( this->in.bytesAvailable() == 0u ) {
		this->armSend ();
	}
}

//
// casStreamOS::casStreamOS()
//
casStreamOS::casStreamOS(caServerI &cas, const ioArgsToNewStreamIO &ioArgs) : 
	casStreamIO (cas, ioArgs),
	pWtReg (NULL),
	pRdReg (NULL),
	sendBlocked (FALSE)
{
	this->xSetNonBlocking();
	this->armRecv();
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
}

//
// casStreamOS::show()
//
void casStreamOS::show(unsigned level) const
{
	this->casStrmClient::show(level);
	printf("casStreamOS at %p\n", 
        static_cast <const void *> ( this ) );
	printf("\tsendBlocked = %d\n", this->sendBlocked);
	if (this->pWtReg) {
		this->pWtReg->show(level);
	}
	if (this->pRdReg) {
		this->pRdReg->show(level);
	}
	this->evWk.show ( level );
	this->ioWk.show ( level );
}

//
// casStreamReadReg::show()
//
void casStreamReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf ( "casStreamReadReg at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casStreamReadReg::callBack ()
//
void casStreamReadReg::callBack ()
{
	this->os.recvCB();
	//
	// NO CODE HERE
	// (casStreamOS::recvCB() may up indirectly deleting this object)
	//
}

//
// casStreamOS::recvCB()
//
void casStreamOS::recvCB()
{
	inBufClient::fillCondition fillCond;
	casProcCond procCond;

	assert ( this->pRdReg );

    //
    // copy in new messages 
    //
    fillCond = this->in.fill();
	if ( fillCond == casFillDisconnect ) {
		delete this;
	}
    else {
	    procCond = this->processInput();
	    if (procCond == casProcDisconnect) {
		    delete this;
	    }	
	    else if ( this->in.full() ) {
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
		    this->disarmRecv(); // this deletes the casStreamReadReg object
	    }
    }
	//
	// NO CODE HERE
	// (see delete above)
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
	printf ( "casStreamWriteReg at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casStreamWriteReg::callBack()
//
void casStreamWriteReg::callBack()
{
	casStreamOS *pSOS = &this->os;
	delete this; // allows rearm to occur if required
	pSOS->sendCB();
	//
	// NO CODE HERE - see delete above
	//
}

//
// casStreamOS::sendCB()
//
void casStreamOS::sendCB()
{
    outBufClient::flushCondition flushCond;
	casProcCond procCond; 

	//
	// attempt to flush the output buffer 
	//
	flushCond = this->out.flush ();
	if (flushCond==flushProgress) {
		if (this->sendBlocked) {
			this->sendBlocked = FALSE;
		}
	}
	else if ( flushCond == outBufClient::flushDisconnect ) {
		//
		// ok to delete the client here
		// because casStreamWriteReg::callBack()
		// is called by the fdManager system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		this->destroy();	
		//
		// must _not_ touch "this" pointer
		// after the destroy 
		//
		return;
	}

	//
	// If we are unable to flush out all of the events 
	// in casStreamEvWakeup::expire() because the
	// client is slow then we must check again here when
	// we _are_ able to write to see if additional events 
	// can be sent to the slow client.
	//
	procCond = this->casEventSys::process();
	if (procCond != casProcOk) {
		//
		// ok to delete the client here
		// because casStreamWriteReg::callBack()
		// is called by the fdManager system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		this->destroy();	
		//
		// must _not_ touch "this" pointer
		// after the destroy 
		//
		return;
	}

#	if defined(DEBUG)
		printf ("write attempted on %d result was %d\n", 
				this->getFD(), flushCond);
		printf ("Recv backlog %u\n", this->inBuf::bytesPresent());
		printf ("Send backlog %u\n", this->outBuf::bytesPresent());
#	endif

	//
	// If we were able to send something then we need
	// to process the input queue in case we were send
	// blocked.
	//
	procCond = this->processInput();
	if (procCond == casProcDisconnect) {
		delete this;	
	}
	else {
		//
		// if anything is left in the send buffer that
		// still needs to be sent and there are not
		// requests pending in the input buffer then
		// keep sending the output buffer until it is
		// empty
		//
		// do not test for this with flushCond since
		// additional bytes may have been added since
		// we flushed the out buffer
		//
		if ( this->out.bytesPresent() > 0u &&
			this->in.bytesAvailable() == 0u ) {
			this->armSend();
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
casProcCond casStreamOS::processInput() // X aCC 361
{
	caStatus status;

#	ifdef DEBUG
		printf(
			"Resp bytes to send=%d, Req bytes pending %d\n", 
			this->outBuf::bytesPresent(),
			this->inBuf::bytesPresent());
#	endif

	status = this->processMsg();
	if (status==S_cas_success ||
		status==S_cas_sendBlocked ||
		status==S_casApp_postponeAsyncIO) {

		//
		// if there is nothing pending in the input
		// queue, then flush the output queue
		//
		if ( this->in.bytesAvailable() == 0u ) {
			this->armSend ();
		}
		this->armRecv ();

		return casProcOk;
	}
	else {
		errMessage (status,
	"unexpected problem with client's input - forcing disconnect");
		return casProcDisconnect;
	}
}

