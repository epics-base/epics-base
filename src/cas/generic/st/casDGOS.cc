
/*
 *
 * casDGOS.cc
 * $Id$
 *
 *
 * $Log$
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
#include <server.h>
#include <casClientIL.h>

class casDGEvWakeup : public osiTimer {
public:
        casDGEvWakeup(casDGOS &osIn) :
                osiTimer(osiTime(0.0)), os(osIn) {}
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
	printf("casDGEvWakeup at %x\n", (unsigned) this);
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
                // if "this" is being used above this
                // routine on the stack then problems
                // will result if we delete "this" here
                //
                // delete &this->os;
                ca_printf("DG event sys process failed\n");
        }
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
	printf ("casDGOS at %lx\n", (unsigned long) this);
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

