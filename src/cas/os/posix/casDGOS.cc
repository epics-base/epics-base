
/*
 *
 * casDGOS.c
 * $Id$
 *
 *
 * $Log$
 *
 */

//
// CA server
// 
#include <server.h>
#include <casClientIL.h> // casClient inline func

class casDGEvWakeup : public osiTimer {
public:
        casDGEvWakeup(casDGOS &osIn) :
                osiTimer(osiTime(0.0)), os(osIn) {}
        ~casDGEvWakeup()
        {
                os.pEvWk = NULL;
        }
        void expire();
	void show (unsigned level);

	const char *name()
	{
		return "casDGEvWakeup";
	}
private:
        casDGOS     &os;
};

//
// casDGEvWakeup::show()
//
void casDGEvWakeup::show(unsigned level)
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
        cond = this->os.casEventSys::process();
        if (cond != casProcOk) {
                //
                // if "this" is being used above this
                // routine on the stack then problems
                // will result if we delete "this" here
                //
                // delete &this->os;
                printf("DG event sys process failed\n");
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
	casDGClient(cas),
	pRdReg(NULL),
	pEvWk(NULL)
{
}

//
// casDGOS::init()
//
caStatus casDGOS::init()
{
	caStatus status;

	//
	// init the base classes
	//
	status = this->casDGClient::init();

	return status;
}


//
// casDGOS::~casDGOS()
//
casDGOS::~casDGOS()
{
	if (this->pRdReg) {
		delete this->pRdReg;
	}
	if (this->pEvWk) {
		delete this->pEvWk;
	}
}

//
// casDGOS::show()
//
void casDGOS::show(unsigned level)
{
	this->casDGClient::show(level);
	printf ("casDGOS at %x\n", (unsigned) this);
	if (this->pRdReg) {
		this->pRdReg->show(level);
	}
	if (this->pEvWk) {
		this->pEvWk->show(level);
	}
}


/*
 * casClientStart ()
 */
caStatus casDGOS::start()
{
	this->pRdReg = new casDGReadReg (*this);
	if (!this->pRdReg) {
		return S_cas_noMemory;
	}
	return S_cas_success;
}


/*
 * casDGOS::processInput ()
 * - a noop
 */
casProcCond casDGOS::processInput ()
{
	return casProcOk;
}

//
// casDGReadReg::callBack()
//
void casDGReadReg::callBack()
{
        caStatus 		status;
	casFlushCondition	flushCond;
	casFillCondition	fillCond;

	assert (os.pRdReg);

        //
        // force all replies to be sent to the client
        // that made the request
        //
	os.clear();

	//
	// read in new input
	//
	fillCond = os.fill();
	if (fillCond == casFillDisconnect) {
		casVerify(0);
		return;
	}
	//
	// verify that we have a message to process
	//
	if (os.inBuf::bytesPresent()>0u) {
		//
		// process the message
		//
		status = os.processMsg();
		if (status) {
			errMessage (status, 
		"unexpected error processing stateless protocol");
			//
			// clear the input buffer so this will 
			// not effect future input
			//
			os.clear();
		}
		else {
			//
			// force all replies to go to the sender
			//
			flushCond = os.flush();
			if (flushCond!=casFlushCompleted) {
				os.clear();
				casVerify(0);
			}
		}
	}
}

//
// casDGReadReg::~casDGReadReg()
//
casDGReadReg::~casDGReadReg()
{
	this->os.pRdReg = NULL;
}

//
// casDGReadReg::show()
//
void casDGReadReg::show(unsigned level)
{
	this->fdReg::show(level);
	printf("casDGReadReg at %x\n", (unsigned) this);
}

