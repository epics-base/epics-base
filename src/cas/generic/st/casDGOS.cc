
/*
 *
 * casDGOS.cc
 * $Id$
 *
 *
 * $Log$
 * Revision 1.6  1998/10/23 00:27:14  jhill
 * fixed problem where send was not always rearmed if this
 * was indirectly necessary in the send callback because
 * in this callback the code considered sends to be still armed
 * until the send callback completed
 *
 * Revision 1.5  1998/07/08 15:38:10  jhill
 * fixed lost monitors during flow control problem
 *
 * Revision 1.4  1997/08/05 00:47:19  jhill
 * fixed warnings
 *
 * Revision 1.3  1997/06/30 22:54:33  jhill
 * use %p with pointers
 *
 * Revision 1.2  1997/04/10 19:34:30  jhill
 * API changes
 *
 * Revision 1.1  1996/11/02 01:01:29  jhill
 * installed
 *
 * Revision 1.3  1996/09/16 18:27:50  jhill
 * vxWorks port changes
 *
 * Revision 1.2  1996/08/05 19:29:25  jhill
 * os depen code now smaller
 *
 * Revision 1.1.1.1  1996/06/20 00:28:06  jhill
 * ca server installation
 *
 *
 */

//
// CA server
// 
#include "server.h"

class casDGEvWakeup : public osiTimer {
public:
	casDGEvWakeup (casDGOS &osIn) :
	osiTimer (0.0), os(osIn) {}
	~casDGEvWakeup();
	void expire();
	void show (unsigned level) const;

	const char *name() const;
private:
	casDGOS     &os;
};

//
// casDGEvWakeup::~casDGEvWakeup()
//
casDGEvWakeup::~casDGEvWakeup()
{
	os.pEvWk = NULL;
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
	this->osiTimer::show(level);
	printf("casDGEvWakeup at %p\n", this);
}

//
// casDGEvWakeup::expire()
//
void casDGEvWakeup::expire()
{
	casProcCond cond;
	cond = this->os.eventSysProcess();
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
// casDGOS::sendBlockSignal()
// (not inline because its virtual)
//
void casDGOS::sendBlockSignal() 
{
}

//
// casDGOS::eventSignal()
//
void casDGOS::eventSignal()
{
	if (!this->pEvWk) {
		this->pEvWk = new casDGEvWakeup(*this);
		if (!this->pEvWk) {
			errMessage(S_cas_noMemory,
				"casDGOS::eventSignal()");
		}
	}
}

//
// casDGOS::eventFlush()
//
void casDGOS::eventFlush()
{
	this->flush();
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

//
// casDGOS::start()
// - a noop
//
caStatus casDGOS::start()
{
	return S_cas_success;
}

