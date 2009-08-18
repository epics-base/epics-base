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

exAsyncPV::exAsyncPV ( exServer & cas, pvInfo & setup, 
                             bool preCreateFlag, bool scanOnIn,
                             double asyncDelayIn ) :
    exScalarPV ( cas, setup, preCreateFlag, scanOnIn ),
    asyncDelay ( asyncDelayIn ),
    simultAsychReadIOCount ( 0u ),
    simultAsychWriteIOCount ( 0u )
{
}

//
// exAsyncPV::read()
//
caStatus exAsyncPV::read (const casCtx &ctx, gdd &valueIn)
{
	exAsyncReadIO	*pIO;
	
	if ( this->simultAsychReadIOCount >= this->cas.maxSimultAsyncIO () ) {
		return S_casApp_postponeAsyncIO;
	}

	pIO = new exAsyncReadIO ( this->cas, ctx, 
	                *this, valueIn, this->asyncDelay );
	if ( ! pIO ) {
        if ( this->simultAsychReadIOCount > 0 ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
		    return S_casApp_noMemory;
        }
	}
	this->simultAsychReadIOCount++;
    return S_casApp_asyncCompletion;
}

//
// exAsyncPV::writeNotify()
//
caStatus exAsyncPV::writeNotify ( const casCtx &ctx, const gdd &valueIn )
{	
	if ( this->simultAsychWriteIOCount >= this->cas.maxSimultAsyncIO() ) {
		return S_casApp_postponeAsyncIO;
	}

	exAsyncWriteIO * pIO = new 
        exAsyncWriteIO ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
        if ( this->simultAsychReadIOCount > 0 ) {
            return S_casApp_postponeAsyncIO;
        }
        else {
		    return S_casApp_noMemory;
        }
    }
	this->simultAsychWriteIOCount++;
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
	if ( this->simultAsychWriteIOCount >= this->cas.maxSimultAsyncIO() ) {
        pStandbyValue.set ( & valueIn );
		return S_casApp_success;
	}

	exAsyncWriteIO * pIO = new 
        exAsyncWriteIO ( this->cas, ctx, *this, 
	                    valueIn, this->asyncDelay );
	if ( ! pIO ) {
        pStandbyValue.set ( & valueIn );
		return S_casApp_success;
    }
	this->simultAsychWriteIOCount++;
	return S_casApp_asyncCompletion;
}

// Implementing a specialized update for exAsyncPV
// allows standby value to update when we update 
// the PV from an asynchronous write timer expiration
// which is a better time compared to removeIO below
// which, if used, gets the reads and writes out of
// order. This type of reordering can cause the 
// regression tests to fail.
caStatus exAsyncPV :: updateFromAsyncWrite ( const gdd & src )
{
    caStatus stat = this->update ( src );
    if ( this->simultAsychWriteIOCount <=1 && 
            pStandbyValue.valid () ) {
//printf("updateFromAsyncWrite: write standby\n");
        stat = this->update ( *this->pStandbyValue );
        this->pStandbyValue.set ( 0 );
    }
    return stat;
}

void exAsyncPV::removeReadIO ()
{
    if ( this->simultAsychReadIOCount > 0u ) {
        this->simultAsychReadIOCount--;
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychReadIOCount?\n" );
    }
}

void exAsyncPV::removeWriteIO ()
{
    if ( this->simultAsychWriteIOCount > 0u ) {
        this->simultAsychWriteIOCount--;
        if ( this->simultAsychWriteIOCount == 0 && 
            pStandbyValue.valid () ) {
//printf("removeIO: write standby\n");
            this->update ( *this->pStandbyValue );
            this->pStandbyValue.set ( 0 );
        }
    }
    else {
        fprintf ( stderr, "inconsistent simultAsychWriteIOCount?\n" );
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
        this->pv.updateFromAsyncWrite ( *this->pValue );
    }
	this->pv.removeWriteIO();
}

//
// exAsyncWriteIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncWriteIO::
    expire ( const epicsTime & /* currentTime */ ) 
{
    assert ( this->pValue.valid () );
	caStatus status = this->pv.updateFromAsyncWrite ( *this->pValue );
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
	this->pv.removeReadIO ();
    this->timer.destroy ();
}

//
// exAsyncReadIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus 
    exAsyncReadIO::expire ( const epicsTime & /* currentTime */ )
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

