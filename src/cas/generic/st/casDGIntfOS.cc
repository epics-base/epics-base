
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
		fdReg (osIn.getFD(), fdrRead), os (osIn) {}
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
	printf ("casDGIntfOS at %p\n", this);
	if (this->pRdReg) {
		this->pRdReg->show(level);
	}
}

/*
 * casDGIntfOS::start()
 */
caStatus casDGIntfOS::start()
{
	if (this->pRdReg==NULL) {
		this->pRdReg = new casDGReadReg (*this);
		if (this->pRdReg==NULL) {
			return S_cas_noMemory;
		}
	}
	return S_cas_success;
}

//
// casDGReadReg::callBack()
//
void casDGReadReg::callBack()
{
	this->os.recvCB();
}

//
// casDGIntfOS::recvCB()
//
void casDGIntfOS::recvCB()
{
	assert (this->pRdReg);
	this->processDG();
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
	printf("casDGReadReg at %p\n", this);
}

