/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// casStreamOS.cc
//
// TO DO:
// o armRecv() and armSend() should return bad status when
// 	there isnt enough memory
//

#include "fdManager.h"
#include "errlog.h"

#define epicsExportSharedFunc
#include "casStreamOS.h"

#if 0
#define DEBUG
#endif

//
// printStatus ()
//
#if defined(DEBUG) 
void casStreamOS :: printStatus ( const char * pCtx ) const
{
    static epicsTime beginTime = epicsTime :: getCurrent ();
    epicsTime current = epicsTime :: getCurrent ();
    printf (
        "%03.3f, "
        "Sock %d, %s, "
        "RecvBuf %u, "
        "SendBuf %u\n",
        current - beginTime,
        this->getFD(),
        pCtx,
        this->inBufBytesPending (),
        this->outBufBytesPending () );
    fflush ( stdout );
}
#else
inline void casStreamOS :: printStatus ( const char * ) const {}
#endif

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
    this->os.printStatus ( "read schedualed" );
}

//
// casStreamReadReg::~casStreamReadReg
//
inline casStreamReadReg::~casStreamReadReg ()
{
    this->os.printStatus ( "read unschedualed" );
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
    this->os.printStatus ( "write schedualed" );
}

//
// casStreamWriteReg::~casStreamWriteReg ()
//
inline casStreamWriteReg::~casStreamWriteReg ()
{
    this->os.printStatus ( "write unschedualed" );
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
epicsTimerNotify::expireStatus casStreamEvWakeup::
    expire ( const epicsTime & /* currentTime */ )
{
    this->os.printStatus ( "casStreamEvWakeup tmr expire" );
    casProcCond pc = os.eventSysProcess ();
 	if ( pc == casProcOk ) {
        // We do not wait for any impartial, or complete, 
        // messages in the input queue to be processed
        // because.
        // A) IO postponement might be preventing the 
        // input queue processing from proceeding.
        // B) We dont want to interrupt subscription 
        // updates while waiting for very large arrays 
        // to be read in a packet at a time.
        // C) Since both reads and events get processed
        // before going back to select to find out if we
        // can do a write then we naturally tend to
        // combine get responses and subscription responses
        // into one write.
        // D) Its probably questionable to hold up event 
        // traffic (introduce latency) because a partial
        // message is pending in the input queue.
        this->os.armSend ();
    }
    else {
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
// care is needed here because this is called
// asynchronously by postEvent
//
// there is some overhead here but care is taken
// in the caller of this routine to call this
// only when its the 2nd event on the queue
//
void casStreamEvWakeup::start( casStreamOS & )
{    
    this->os.printStatus ( "casStreamEvWakeup tmr start" );
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
// casStreamIOWakeup::expire()
//
// This is called whenever asynchronous IO completes
//
// Running this indirectly off of the timer queue
// guarantees that we will not call processMsg()
// recursively
//
epicsTimerNotify::expireStatus casStreamIOWakeup ::
    expire ( const epicsTime & /* currentTime */ )
{
    assert ( this->pOS );
    this->pOS->printStatus ( "casStreamIOWakeup tmr expire" );
    casStreamOS	& tmpOS = *this->pOS;
    this->pOS = 0;
    caStatus status = tmpOS.processMsg ();
    if ( status == S_cas_success ) {
        tmpOS.armRecv ();
        if ( tmpOS._sendNeeded () ) {
            tmpOS.armSend ();
        }
    }
    else if ( status == S_cas_sendBlocked ) {
        tmpOS.armSend ();
        // always activate receives if space is available
        // in the in buf
        tmpOS.armRecv ();
    }
    else if ( status == S_casApp_postponeAsyncIO ) {
        // we should be back on the IO blocked list
        // if S_casApp_postponeAsyncIO was returned
        // so this function will be called again when
        // another asynchronous request completes
        tmpOS.armSend ();
        // always activate receives if space is available
        // in the in buf
        tmpOS.armRecv ();
    }
    else {
        errMessage ( status,
    "- unexpected problem with client's input - forcing disconnect");
        tmpOS.getCAS().destroyClient ( tmpOS );
        //
        // must _not_ touch "tmpOS" ref
        // after the destroy 
        //
        return noRestart;
    }
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
    this->pOS->printStatus ( "casStreamIOWakeup tmr start" );
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
void casStreamOS::armSend()
{
	if ( this->outBufBytesPending() == 0u ) {
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
// (called by main thread when lock is applied)
//
void casStreamOS::ioBlockedSignal()
{
    this->ioWk.start ( *this );
}

//
// casStreamOS::eventSignal()
// (called by any thread asynchronously
// when an event is posted)
//
void casStreamOS::eventSignal()
{
    this->evWk.start ( *this );
}

//
// casStreamOS::casStreamOS()
//
casStreamOS::casStreamOS ( 
        caServerI & cas, clientBufMemoryManager & bufMgrIn,
        const ioArgsToNewStreamIO & ioArgs ) : 
    casStreamIO ( cas, bufMgrIn, ioArgs ),
    evWk ( *this ), 
    pWtReg ( 0 ), 
    pRdReg ( 0 ), 
    _sendBacklogThresh ( osSendBufferSize () / 2u )
{
 	if ( _sendBacklogThresh < MAX_TCP / 2 ) {
	    _sendBacklogThresh = MAX_TCP / 2;
	}
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
void casStreamOS :: recvCB ()
{
	assert ( this->pRdReg );
	
    printStatus ( "receiving" );

    //
    // copy in new messages 
    //
    inBufClient::fillCondition fillCond = this->inBufFill ();
	if ( fillCond == casFillDisconnect ) {
        this->getCAS().destroyClient ( *this );
        //
        // must _not_ touch "this" pointer
        // after the destroy 
        //
        return;
	}
    else if ( fillCond == casFillNone ) {
        if ( this->inBufFull() ) {
            this->disarmRecv ();
        }
    }
    else {
	    printStatus ( "recv CB req proc" );
	    caStatus status = this->processMsg ();
	    if ( status == S_cas_success ) {
	        this->armRecv ();
            if ( _sendNeeded () ) {
	            this->armSend ();
            }
	    }
	    else if ( status == S_cas_sendBlocked ) {
	        this->armSend ();
	    }
	    else if ( status == S_casApp_postponeAsyncIO ) {
            this->armSend ();
        }
        else {
	        errMessage ( status,
        "- unexpected problem with client's input - forcing disconnect");
            this->getCAS().destroyClient ( *this );
            //
            // must _not_ touch "this" pointer
            // after the destroy 
            //
            return;
        }
    }
}

//
// casStreamOS :: sendBlockSignal()
//
void casStreamOS :: sendBlockSignal ()
{
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
    // we know that the fdManager will destroy the write 
    // registration after this function returns, and that
    // it is robust in situations where the callback
    // deletes its fdReg derived object so delete it now,
    // because we can now reschedule a send as needed
    //
    this->disarmSend ();

    printStatus ( "writing" );
    
	//
	// attempt to flush the output buffer 
	//
	outBufClient::flushCondition flushCond = this->flush ();
	if ( flushCond == outBufClient::flushDisconnect ) {
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
    casProcCond pc = this->eventSysProcess ();
	if ( pc != casProcOk ) {
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
	
	printStatus ( ppFlushCondText [ flushCond ] );

	//
	// If we were able to send something then we need
	// to process the input queue in case we were send
	// blocked.
	//

    bufSizeT inBufBytesPend = this->inBufBytesPending ();
	if ( flushCond == flushProgress && inBufBytesPend ) {
	    printStatus ( "send CB req proc" );
	    caStatus status = this->processMsg ();
	    if ( status == S_cas_success ) {
	        this->armRecv ();
	    }
        else if ( status == S_cas_sendBlocked 
            || status == S_casApp_postponeAsyncIO ) {
            bufSizeT inBufBytesPendNew = this->inBufBytesPending ();
            if ( inBufBytesPendNew < inBufBytesPend ) {
                this->armRecv ();
            }
        }
        else {
	        errMessage ( status,
        "- unexpected problem with client's input - forcing disconnect");
            this->getCAS().destroyClient ( *this );
		    //
		    // must _not_ touch "this" pointer
		    // after the destroy 
		    //
            return;
        }
    }

    // Once a send starts we will keep going with it until
    // it flushes all of the way out. Its important to 
    // perform this step only after processMsg so that we 
    // flush out any send blocks detected by processMsg.
    this->armSend ();
}

//
// casStreamOS :: sendNeeded ()
//
bool casStreamOS :: 
    _sendNeeded () const 
{
    bool sn = this->outBufBytesPending() >= this->_sendBacklogThresh;
    bufSizeT inBytesPending = this->inBufBytesPending ();
    return sn || ( inBytesPending == 0u );
}   
    

