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
// casStreamOS.cc
// $Id$
//
//
//
// TO DO:
// o armRecv() and armSend() should return bad status when
// 	there isnt enough memory
//

#include "fdManager.h"

#define epicsExportSharedFunc
#include "casStreamOS.h"

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
	        this->os.in.bytesPresent());
        printf ("Send backlog %u\n", 
	        this->os.out.bytesPresent());
#	endif		
}

//
// casStreamReadReg::~casStreamReadReg
//
inline casStreamReadReg::~casStreamReadReg ()
{
#	if defined(DEBUG) 
		printf ("Read off %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.in.bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.out.bytesPresent());
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
	fdReg (osIn.getFD(), fdrWrite, true), os (osIn)
{
#	if defined(DEBUG) 
		printf ("Write on %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.in.bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.out.bytesPresent());
#	endif
}

//
// casStreamWriteReg::~casStreamWriteReg ()
//
inline casStreamWriteReg::~casStreamWriteReg ()
{
#	if defined(DEBUG) 
		printf ("Write off %d\n", this->os.getFD());
		printf ("Recv backlog %u\n", 
			this->os.in.bytesPresent());
		printf ("Send backlog %u\n", 
			this->os.out.bytesPresent());
#	endif
}

//
// casStreamEvWakeup()
//
casStreamEvWakeup::casStreamEvWakeup ( casStreamOS & osIn ) : 
    timer ( fileDescriptorManager.createTimer() ), os ( osIn ) 
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
epicsTimerNotify::expireStatus casStreamEvWakeup::expire ( const epicsTime & /* currentTime */ )
{
    casEventSys::processStatus ps = os.eventSysProcess ();
    if ( ps.nAccepted > 0u ) {
        this->os.eventFlush ();
    }
	if ( ps.cond != casProcOk ) {
		//
		// ok to delete the client here
		// because casStreamEvWakeup::expire()
		// is called by the timer queue system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
		delete & this->os;	

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
    // care is needed here because this is called
    // asynchronously by postEvent
    //
    // there is some overhead here but care is taken
    // in the caller of this routine to call this
    // only when its the 2nd event on the queue
    this->timer.start ( *this, 0.0 );
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
    casStreamOS	& tmpOS = *this->pOS;
    this->pOS = 0;
	tmpOS.processInput();
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
		if ( ! this->inBufFull() ) {
			this->pRdReg = new casStreamReadReg ( *this );
		}
	}
}

//
// casStreamOS::disarmRecv ()
//
inline void casStreamOS::disarmRecv ()
{
	delete this->pRdReg;
    this->pRdReg = 0;
}

//
// casStreamOS::armSend()
//
inline void casStreamOS::armSend()
{
	if ( this->outBufBytesPresent() == 0u ) {
		return;
	}

	if ( ! this->pWtReg ) {
		this->pWtReg = new casStreamWriteReg(*this);
	}
}

//
// casStreamOS::disarmSend()
//
inline void casStreamOS::disarmSend ()
{
	delete this->pWtReg;
    this->pWtReg = 0;
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
	if ( this->inBufBytesAvailable() == 0u ) {
		this->armSend ();
	}
}

//
// casStreamOS::casStreamOS()
//
casStreamOS::casStreamOS ( 
        caServerI & cas, clientBufMemoryManager & bufMgrIn,
        const ioArgsToNewStreamIO & ioArgs ) : 
    casStreamIO ( cas, bufMgrIn, ioArgs ),
    evWk ( *this ), pWtReg ( 0 ), pRdReg ( 0 ), 
    sendBlocked ( false )
{
	this->xSetNonBlocking ();
	this->armRecv ();
}

//
// casStreamOS::~casStreamOS()
//
casStreamOS::~casStreamOS()
{
	//
	// attempt to flush out any remaining messages
	//
	this->flush ();

	this->disarmSend ();
	this->disarmRecv ();
}

//
// casStreamOS::show()
//
void casStreamOS::show ( unsigned level ) const
{
	this->casStrmClient::show ( level );
	printf ( "casStreamOS at %p\n", 
        static_cast <const void *> ( this ) );
	printf ( "\tsendBlocked = %d\n", this->sendBlocked );
	if ( this->pWtReg ) {
		this->pWtReg->show ( level );
	}
	if ( this->pRdReg ) {
		this->pRdReg->show ( level );
	}
	this->evWk.show ( level );
	this->ioWk.show ( level );
}

//
// casStreamReadReg::show()
//
void casStreamReadReg::show ( unsigned level ) const
{
	this->fdReg::show ( level );
	printf ( "casStreamReadReg at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casStreamReadReg::callBack ()
//
void casStreamReadReg::callBack ()
{
	this->os.recvCB ();
	//
	// NO CODE HERE
	// (casStreamOS::recvCB() may up indirectly deleting this object)
	//
}

//
// casStreamOS::recvCB()
//
void casStreamOS::recvCB ()
{
	assert ( this->pRdReg );

    //
    // copy in new messages 
    //
    inBufClient::fillCondition fillCond = this->inBufFill ();
	if ( fillCond == casFillDisconnect ) {
        this->getCAS().destroyClient ( *this );
	}
    else {
	   casProcCond procCond = this->processInput ();
	    if ( procCond == casProcDisconnect ) {
            this->getCAS().destroyClient ( *this );
	    }	
	    else if ( this->inBufFull() ) {
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
		    this->disarmRecv (); // this deletes the casStreamReadReg object
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
void casStreamOS::sendBlockSignal ()
{
	this->sendBlocked = true;
	this->armSend ();
}

//
// casStreamWriteReg::show()
//
void casStreamWriteReg::show ( unsigned level ) const
{
	this->fdReg::show ( level );
	printf ( "casStreamWriteReg at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casStreamWriteReg::callBack ()
//
void casStreamWriteReg::callBack()
{
	casStreamOS * pSOS = & this->os;
	pSOS->sendCB ();
	//
	// NO CODE HERE - sendCB deletes this object
	//
}

//
// casStreamOS::sendCB ()
//
void casStreamOS::sendCB ()
{
    this->disarmSend ();

	//
	// attempt to flush the output buffer 
	//
	outBufClient::flushCondition flushCond = this->flush ();
	if ( flushCond == flushProgress ) {
		if ( this->sendBlocked ) {
			this->sendBlocked = false;
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
        this->getCAS().destroyClient ( *this );
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
    casEventSys::processStatus ps = this->eventSysProcess ();
	if ( ps.cond != casProcOk ) {
		//
		// ok to delete the client here
		// because casStreamWriteReg::callBack()
		// is called by the fdManager system
		// and therefore we are not being
		// called from a client member function
		// higher up on the stack
		//
        this->getCAS().destroyClient ( *this );
		//
		// must _not_ touch "this" pointer
		// after the destroy 
		//
		return;
	}

#	if defined(DEBUG)
		printf ( "write attempted on %d result was %d\n", 
				this->getFD(), flushCond );
		printf ( "Recv backlog %u\n", this->in.bytesPresent() );
		printf ( "Send backlog %u\n", this->out.bytesPresent() );
#	endif

	//
	// If we were able to send something then we need
	// to process the input queue in case we were send
	// blocked.
	//
	casProcCond procCond = this->processInput ();
	if ( procCond == casProcDisconnect ) {
        this->getCAS().destroyClient ( *this );
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
		if ( this->outBufBytesPresent() > 0u &&
			this->inBufBytesAvailable() == 0u ) {
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
			this->out.bytesPresent(),
			this->in.bytesPresent());
#	endif

	status = this->processMsg();
	if (status==S_cas_success ||
		status==S_cas_sendBlocked ||
		status==S_casApp_postponeAsyncIO) {

		//
		// if there is nothing pending in the input
		// queue, then flush the output queue
		//
		if ( this->inBufBytesAvailable() == 0u ) {
			this->armSend ();
		}
		this->armRecv ();

		return casProcOk;
	}
	else {
		errMessage ( status,
	"- unexpected problem with client's input - forcing disconnect");
		return casProcDisconnect;
	}
}

