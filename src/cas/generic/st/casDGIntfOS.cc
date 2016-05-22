/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// casDGIntfOS.cc
//

#include "fdManager.h"
#include "errlog.h"

#define epicsExportSharedFunc
#include "casDGIntfOS.h"

//
// casDGReadReg
//
class casDGReadReg : public fdReg {
public:
	casDGReadReg ( casDGIntfOS & osIn ) :
		fdReg (osIn.getFD(), fdrRead), os (osIn) {}
	~casDGReadReg ();
	void show (unsigned level) const;
private:
	casDGIntfOS &os;
	void callBack ();
	casDGReadReg ( const casDGReadReg & );
	casDGReadReg & operator = ( const casDGReadReg & );
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
	casDGBCastReadReg ( const casDGBCastReadReg & );
	casDGBCastReadReg & operator = ( const casDGBCastReadReg & );
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
	casDGWriteReg ( const casDGWriteReg & );
	casDGWriteReg & operator = ( const casDGWriteReg & );
};

//
// casDGIntfOS::casDGIntfOS()
//
casDGIntfOS::casDGIntfOS (
        caServerI & serverIn, clientBufMemoryManager & memMgrIn,
        const caNetAddr & addr, bool autoBeaconAddr, 
        bool addConfigBeaconAddr ) : 
	casDGIntfIO ( serverIn, memMgrIn, addr, 
            autoBeaconAddr, addConfigBeaconAddr ),
	pRdReg ( 0 ),
    pBCastRdReg ( 0 ),
    pWtReg ( 0 )
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
}

//
// casDGEvWakeup::casDGEvWakeup()
//
casDGEvWakeup::casDGEvWakeup () : 
    timer ( fileDescriptorManager.createTimer() ), pOS ( 0 ) 
{
}

//
// casDGEvWakeup::~casDGEvWakeup()
//
casDGEvWakeup::~casDGEvWakeup()
{
    this->timer.destroy ();
}

void casDGEvWakeup::start ( casDGIntfOS &os )
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
// casDGEvWakeup::show()
//
void casDGEvWakeup::show ( unsigned level ) const
{
	printf ( "casDGEvWakeup at %p {\n", 
        static_cast <const void *> ( this ) );
	this->timer.show ( level );
	printf ("}\n");
}

//
// casDGEvWakeup::expire()
//
epicsTimerNotify::expireStatus casDGEvWakeup::expire ( const epicsTime & /* currentTime */ )
{
    this->pOS->eventSysProcess ();
    // We do not wait for any impartial, or complete, 
    // messages in the input queue to be processed
    // because.
    // A) IO postponement might be preventing the 
    // input queue processing from proceeding.
    // B) Since both reads and events get processed
    // before going back to select to find out if we
    // can do a write then we naturally tend to
    // combine get responses and subscription responses
    // into one write.
    this->pOS->armSend ();
    this->pOS = 0;
    return noRestart;
}

//
// casDGIOWakeup::casDGIOWakeup()
//
casDGIOWakeup::casDGIOWakeup () : 
	timer ( fileDescriptorManager.createTimer() ), pOS ( 0 ) 
{
}

//
// casDGIOWakeup::~casDGIOWakeup()
//
casDGIOWakeup::~casDGIOWakeup()
{
    this->timer.destroy ();
}

//
// casDGIOWakeup::expire()
//
// Running this indirectly off of the timer queue
// guarantees that we will not call processDG()
// recursively
//
epicsTimerNotify :: expireStatus 
    casDGIOWakeup :: expire( const epicsTime & /* currentTime */ )
{
	caStatus status = this->pOS->processDG ();
	if ( status != S_cas_success &&
		 status != S_cas_sendBlocked ) {
        char pName[64u];
        this->pOS->hostName (pName, sizeof (pName));
		errPrintf (status, __FILE__, __LINE__,
	        "unexpected problem with UDP input from \"%s\"", pName);
	}
    this->pOS->armRecv ();
    this->pOS->armSend ();
    this->pOS = 0;
    return noRestart;
}

//
// casDGIOWakeup::show()
//
void casDGIOWakeup::start ( casDGIntfOS &os ) 
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
// casDGIOWakeup::show()
//
void casDGIOWakeup::show(unsigned level) const
{
	printf ( "casDGIOWakeup at %p {\n", 
        static_cast <const void *> ( this ) );
	this->timer.show ( level );
	printf ( "}\n" );
}

//
// casDGIntfOS::armRecv ()
//
void casDGIntfOS::armRecv()
{
	if ( ! this->inBufFull () ) {
	    if ( ! this->pRdReg ) {
			this->pRdReg = new casDGReadReg ( *this );
		}
	    if ( this->validBCastFD() && this->pBCastRdReg == NULL ) {
			this->pBCastRdReg = new casDGBCastReadReg ( *this );
	    }
    }
}

//
// casDGIntfOS::disarmRecv()
//
void casDGIntfOS::disarmRecv()
{
	delete this->pRdReg;
    this->pRdReg = 0;
	delete this->pBCastRdReg;
    this->pBCastRdReg = 0;
}

//
// casDGIntfOS::armSend()
//
void casDGIntfOS::armSend()
{
	if ( this->outBufBytesPending () == 0u ) {
		return;
	}

	if ( ! this->pWtReg ) {
		this->pWtReg = new casDGWriteReg ( *this );
	}
}

//
// casDGIntfOS::disarmSend()
//
void casDGIntfOS::disarmSend ()
{
	delete this->pWtReg;
    this->pWtReg = 0;
}

//
// casDGIntfOS::ioBlockedSignal()
//
void casDGIntfOS::ioBlockedSignal()
{
    this->ioWk.start ( *this );
}

//
// casDGIntfOS::eventSignal()
//
void casDGIntfOS::eventSignal()
{
    this->evWk.start ( *this );
}

//
// casDGIntfOS::show()
//
void casDGIntfOS::show(unsigned level) const
{
	printf ( "casDGIntfOS at %p\n", 
        static_cast <const void *> ( this ) );
    if ( this->pRdReg ) { 
		this->pRdReg->show ( level );
	}
	if ( this->pWtReg ) {
		this->pWtReg->show ( level );
	}
    if ( this->pBCastRdReg ) {
		this->pBCastRdReg->show ( level );
    }
	this->evWk.show (level);
	this->ioWk.show (level);
    this->casDGIntfIO::show (level);
}

//
// casDGReadReg::callBack()
//
void casDGReadReg::callBack()
{
    this->os.recvCB ( inBufClient::fpNone );
}

//
// casDGReadReg::~casDGReadReg()
//
casDGReadReg::~casDGReadReg()
{
}

//
// casDGReadReg::show()
//
void casDGReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf("casDGReadReg at %p\n", 
		static_cast <const void *> ( this) );
}

//
// casDGBCastReadReg::callBack()
//
void casDGBCastReadReg::callBack()
{
    this->os.recvCB ( inBufClient::fpUseBroadcastInterface );
}

//
// casDGBCastReadReg::~casDGBCastReadReg()
//
casDGBCastReadReg::~casDGBCastReadReg()
{
}

//
// casDGReadReg::show()
//
void casDGBCastReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf ( "casDGBCastReadReg at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casDGWriteReg::~casDGWriteReg()
//
casDGWriteReg::~casDGWriteReg()
{
}

//
// casDGWriteReg::callBack()
//
void casDGWriteReg::callBack()
{
	casDGIntfOS * pDGIOS = & this->os;
	pDGIOS->sendCB();
	//
	// NO CODE HERE - sendCB deletes this object
	//
}

//
// casDGWriteReg::show()
//
void casDGWriteReg::show(unsigned level) const
{
	this->fdReg::show (level);
	printf ( "casDGWriteReg: at %p\n", 
        static_cast <const void *> ( this ) );
}

//
// casDGIntfOS::sendBlockSignal()
//
void casDGIntfOS::sendBlockSignal()
{
	this->armSend();
}

//
// casDGIntfOS::sendCB()
//
void casDGIntfOS::sendCB()
{
    // allows rearm to occur if required
    this->disarmSend ();

	//
	// attempt to flush the output buffer 
	//
	outBufClient::flushCondition flushCond = this->flush ();
	if ( flushCond == flushProgress ) {
	    //
	    // If we are unable to flush out all of the events 
	    // in casDgEvWakeup::expire() because the
	    // client is slow then we must check again here when
	    // we _are_ able to write to see if additional events 
	    // can be sent to the slow client.
	    //
        this->eventSysProcess ();

	    //
	    // If we were able to send something then we need
	    // to process the input queue in case we were send
	    // blocked.
	    //
	    caStatus status = this->processDG ();
	    if (    status != S_cas_success && 
                status != S_cas_sendBlocked ) {
            char pName[64u];
            this->hostName (pName, sizeof (pName));
		    errPrintf ( status, __FILE__, __LINE__,
	            "unexpected problem with UDP input from \"%s\"", pName);
	    }
    }

#	if defined(DEBUG)
		printf ("write attempted on %d result was %d\n", this->getFD(), flushCond );
		printf ("Recv backlog %u\n", this->inBuf::bytesPresent());
		printf ("Send backlog %u\n", this->outBuf::bytesPresent());
#	endif

    //
    // this reenables receipt of incoming frames once
    // the output has been flushed in case the receive
    // side is blocked due to lack of buffer space
    //
    this->armRecv ();


    // once we start sending we continue until done
    this->armSend ();
}

//
// casDGIntfOS::recvCB()
//
void casDGIntfOS :: recvCB ( inBufClient::fillParameter parm )
{	
	assert ( this->pRdReg );

    //
    // copy in new messages 
    //
    this->inBufFill ( parm );
	caStatus status = this->processDG ();
	if ( status != S_cas_success &&
		 status != S_cas_sendBlocked ) {
        char pName[64u];
        this->hostName (pName, sizeof (pName));
		errPrintf (status, __FILE__, __LINE__,
	        "unexpected problem with UDP input from \"%s\"", pName);
	}

    this->armSend ();

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
	if ( this->inBufFull() ) {
		this->disarmRecv(); // this deletes the casDGReadReg object
	}
}

