/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
// Example EPICS CA server
// (asynchrronous process variable)
//

#include "exServer.h"

//
// exAsyncPV::read()
//
caStatus exAsyncPV::read (const casCtx &ctx, gdd &valueIn)
{
	exAsyncReadIO	*pIO;
	
	if ( this->simultAsychIOCount >= this->cas.maxSimultAsyncIO () ) {
		return S_casApp_postponeAsyncIO;
	}

	pIO = new exAsyncReadIO ( this->cas, ctx, 
	                *this, valueIn, this->asyncDelay );
	if ( ! pIO ) {
		return S_casApp_noMemory;
	}
	this->simultAsychIOCount++;
    return S_casApp_asyncCompletion;
}

//
// exAsyncPV::writeNotify()
//
caStatus exAsyncPV::writeNotify ( const casCtx &ctx, const gdd &valueIn )
{	
	if ( this->simultAsychIOCount >= this->cas.maxSimultAsyncIO() ) {
		return S_casApp_postponeAsyncIO;
	}

	exAsyncWriteIO * pIO = new 
        exAsyncWriteIO ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
		return S_casApp_noMemory;
	}
	this->simultAsychIOCount++;
	return S_casApp_asyncCompletion;
}

//
// exAsyncPV::write()
//
caStatus exAsyncPV::write ( const casCtx &ctx, const gdd &valueIn )
{
	// implement the discard intermediate values, but last value
    // sent always applied behavior that IOCs provide excepting
    // that we will alow N requests to pend instead of a limit
    // of only one imposed in the IOC
	if ( this->simultAsychIOCount >= this->cas.maxSimultAsyncIO() ) {
        pStandbyValue.set ( & valueIn );
		return S_casApp_success;
	}

	exAsyncWriteIO * pIO = new 
        exAsyncWriteIO ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
		return S_casApp_noMemory;
	}
	this->simultAsychIOCount++;
	return S_casApp_asyncCompletion;
}

void exAsyncPV::removeIO ()
{
    if ( this->simultAsychIOCount > 0u ) {
        this->simultAsychIOCount--;
        if ( this->simultAsychIOCount == 0 && 
            pStandbyValue.valid () ) {
            this->update ( *this->pStandbyValue );
            this->pStandbyValue.set ( 0 );
        }
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychIOCount?\n" );
    }
}

//
// exAsyncWriteIO::exAsyncWriteIO()
//
exAsyncWriteIO::exAsyncWriteIO ( exServer & cas,
        const casCtx & ctxIn, exAsyncPV & pvIn, 
        const gdd & valueIn, double asyncDelay ) :
	casAsyncWriteIO ( ctxIn ), pv ( pvIn ), 
        timer ( cas.createTimer () ), pValue(valueIn)
{
    this->timer.start ( *this, asyncDelay );
}

//
// exAsyncWriteIO::~exAsyncWriteIO()
//
exAsyncWriteIO::~exAsyncWriteIO()
{
    this->timer.destroy ();
    // if the timer hasnt expired, and the value 
    // hasnt been written then force it to happen
    // now so that regression testing works
    if ( this->pValue.valid () ) {
        this->pv.update ( *this->pValue );
    }
	this->pv.removeIO();
}

//
// exAsyncWriteIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncWriteIO::
    expire ( const epicsTime & /* currentTime */ ) 
{
	caStatus status = this->pv.update ( *this->pValue );
	this->pValue.set ( 0 );
	this->postIOCompletion ( status );
    return noRestart;
}

//
// exAsyncReadIO::exAsyncReadIO()
//
exAsyncReadIO::exAsyncReadIO ( exServer & cas, const casCtx & ctxIn, 
                              exAsyncPV & pvIn, gdd & protoIn,
                              double asyncDelay ) :
	casAsyncReadIO ( ctxIn ), pv ( pvIn ), 
        timer ( cas.createTimer() ), pProto ( protoIn )
{
    this->timer.start ( *this, asyncDelay );
}

//
// exAsyncReadIO::~exAsyncReadIO()
//
exAsyncReadIO::~exAsyncReadIO()
{
	this->pv.removeIO ();
    this->timer.destroy ();
}


//
// exAsyncReadIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncReadIO::expire ( const epicsTime & /* currentTime */ )
{
	//
	// map between the prototype in and the
	// current value
	//
	caStatus status = this->pv.exPV::readNoCtx ( this->pProto );

	//
	// post IO completion
	//
	this->postIOCompletion ( status, *this->pProto );

    return noRestart;
}

