
/*
 *
 * casDGOS.cc
 * $Id$
 *
 *
 */

#if 0

//
// CA server
// 
#include "server.h"

class casDGEvWakeup : public epicsTimerNotify {
public:
	casDGEvWakeup (casDGOS &osIn);
	~casDGEvWakeup();
	void show (unsigned level) const;
private:
    epicsTimer  &timer;
	casDGOS     &os;
	expireStatus expire ();
};

//
// casDGEvWakeup::casDGEvWakeup()
//
casDGEvWakeup::asDGEvWakeup ( casDGOS &osIn ) :
	    timer ( fileDescriptorManager.timerQueRef().createTimer(*this) ), os ( osIn ) 
{
    this->timer.start ( 0.0 );
}

//
// casDGEvWakeup::~casDGEvWakeup()
//
casDGEvWakeup::~casDGEvWakeup()
{
	os.pEvWk = NULL;
    delete & this->timer;
}

//
// casDGEvWakeup::show()
//
void casDGEvWakeup::show(unsigned level) const
{
	printf ( "casDGEvWakeup at %p\n", this );
	this->timer.show ( level );
}

//
// casDGEvWakeup::expire()
//
epicsTimerNotify::expireStatus casDGEvWakeup::expire()
{
	casProcCond cond = this->os.eventSysProcess();
	if ( cond != casProcOk ) {
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
	}
    return noRestart;
}

//
// casDGOS::sendBlockSignal()
// (not inline because its virtual)
//
void casDGOS::sendBlockSignal() 
{
	this->sendBlocked=TRUE;
	this->armSend();
}

//
// casDGOS::eventSignal()
//
void casDGOS::eventSignal()
{
	if (!this->pEvWk) {
		this->pEvWk = new casDGEvWakeup(*this);
		if (!this->pEvWk) {
			errMessage (S_cas_noMemory,
				"casDGOS::eventSignal()");
            throw S_cas_noMemory;
		}
	}
}

//
// casDGOS::eventFlush()
//
void casDGOS::eventFlush()
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
// casDGOS::casDGOS()
//
casDGOS::casDGOS(caServerI &cas) : 
	casDGIO(cas),
	pEvWk(NULL)
{
}

//
// casDGOS::~casDGOS()
//
casDGOS::~casDGOS()
{
	if (this->pEvWk) {
		delete this->pEvWk;
	}
}

//
// casDGOS::show()
//
void casDGOS::show(unsigned level) const
{
	printf ("casDGOS at %p\n", this);
	this->casDGClient::show(level);
	if (this->pEvWk) {
		this->pEvWk->show(level);
	}
}

//
// casDGOS::processInput ()
// - a noop
//
casProcCond casDGOS::processInput ()
{
	return casProcOk;
}

#endif
