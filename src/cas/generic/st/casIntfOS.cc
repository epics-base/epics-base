
/*
 *
 * casIntfOS.cc
 * $Id$
 *
 *
 */


//
// CA server
// 
#include "server.h"
#include "casIODIL.h"

//
// casServerReg
//
class casServerReg : public fdReg {
public:
        casServerReg (casIntfOS &osIn) :
      fdReg (osIn.casIntfIO::getFD(), fdrRead), os (osIn) {}
        ~casServerReg ();
private:
        casIntfOS &os;
 
        void callBack ();
};

//
// casIntfOS::casIntfOS()
//
casIntfOS::casIntfOS (caServerI &casIn, const caNetAddr &addrIn, 
    bool autoBeaconAddr, bool addConfigBeaconAddr) : 
    casDGIntfOS (casIn, addrIn, autoBeaconAddr, addConfigBeaconAddr),
    cas (casIn), 
    casIntfIO (addrIn)
{    
    this->setNonBlocking();
    
    this->pRdReg = new casServerReg(*this);
    if (this->pRdReg==NULL) {
        errMessage (S_cas_noMemory,
				"casIntfOS::casIntfOS()");
        throw S_cas_noMemory;
    }
}

//
// casIntfOS::~casIntfOS()
//
casIntfOS::~casIntfOS()
{
	if (this->pRdReg) {
		delete this->pRdReg;
	}
}

//
// casServerReg::callBack()
//
void casServerReg::callBack()
{
	assert(this->os.pRdReg);
	this->os.cas.connectCB(this->os);	
}

//
// casServerReg::~casServerReg()
//
casServerReg::~casServerReg()
{
}

//
// casIntfOS::show ()
//
void casIntfOS::show (unsigned level)
{
    printf ("casIntfOS at %p\n", this);
    this->casIntfIO::show (level);
    this->casDGIntfOS::show (level);
}

//
// casIntfOS::serverAddress ()
//
caNetAddr casIntfOS::serverAddress () const
{
    return this->casIntfIO::serverAddress();
}

