
/*
 *
 * casDGIntfOS.cc
 * $Id$
 *
 *
 */

//
// CA server
// 
#include "server.h"
#include "casIODIL.h" // IO Depen in line func

//
// casDGReadReg
//
class casDGReadReg : public fdReg {
public:
        casDGReadReg (casDGIntfOS &osIn) :
                os (osIn), fdReg (osIn.getFD(), fdrRead) {}
        ~casDGReadReg ();
 
        void show (unsigned level) const;
private:
        casDGIntfOS &os;
 
        void callBack ();
};


//
// casDGIntfOS::casDGIntfOS()
//
casDGIntfOS::casDGIntfOS(casDGClient &clientIn) : 
	casDGIntfIO(clientIn),
	pRdReg(NULL)
{
}


//
// casDGIntfOS::~casDGIntfOS()
//
casDGIntfOS::~casDGIntfOS()
{
	if (this->pRdReg) {
		delete this->pRdReg;
	}
}

//
// casDGIntfOS::show()
//
void casDGIntfOS::show(unsigned level) const
{
	printf ("casDGIntfOS at %x\n", (unsigned) this);
	if (this->pRdReg) {
		this->pRdReg->show(level);
	}
}


/*
 * casDGIntfOS::start()
 */
caStatus casDGIntfOS::start()
{
	this->pRdReg = new casDGReadReg (*this);
	if (!this->pRdReg) {
		return S_cas_noMemory;
	}
	return S_cas_success;
}

//
// casDGReadReg::callBack()
//
void casDGReadReg::callBack()
{
	assert (os.pRdReg);
	os.processDG();
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
void casDGReadReg::show(unsigned level) const
{
	this->fdReg::show(level);
	printf("casDGReadReg at %x\n", (unsigned) this);
}

