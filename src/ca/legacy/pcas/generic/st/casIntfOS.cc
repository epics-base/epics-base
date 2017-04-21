/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * casIntfOS.cc
 */

#include "fdManager.h"

#define epicsExportSharedSymbols
#include "casIntfOS.h"

class casServerReg : public fdReg {
public:
    casServerReg (casIntfOS &osIn) :
    fdReg (osIn.casIntfIO::getFD(), fdrRead), os (osIn) {}
    ~casServerReg ();
private:
    casIntfOS &os;
    void callBack ();
	casServerReg ( const casServerReg & );
	casServerReg & operator = ( const casServerReg & );
};

casIntfOS::casIntfOS ( caServerI & casIn, clientBufMemoryManager & memMgrIn,
    const caNetAddr & addrIn, bool autoBeaconAddr, bool addConfigBeaconAddr ) : 
    casIntfIO ( addrIn ),
    casDGIntfOS ( casIn, memMgrIn, addrIn, autoBeaconAddr, 
        addConfigBeaconAddr ),
    cas ( casIn )
{    
    this->setNonBlocking();
    
    this->pRdReg = new casServerReg ( *this );
}

casIntfOS::~casIntfOS()
{
	delete this->pRdReg;
}

void casServerReg::callBack()
{
	assert ( this->os.pRdReg );
	this->os.cas.connectCB ( this->os );	
}

casServerReg::~casServerReg()
{
}

void casIntfOS::show ( unsigned level ) const
{
    printf ( "casIntfOS at %p\n", 
        static_cast < const void * > ( this ) );
    this->casDGIntfOS::show ( level );
}

caNetAddr casIntfOS::serverAddress () const
{
    return this->casIntfIO::serverAddress();
}



