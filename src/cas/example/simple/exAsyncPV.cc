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
// (virtual replacement for the default)
//
caStatus exAsyncPV::read (const casCtx &ctx, gdd &valueIn)
{
	exAsyncReadIO	*pIO;
	
	if (this->simultAsychIOCount>=maxSimultAsyncIO) {
		return S_casApp_postponeAsyncIO;
	}

	this->simultAsychIOCount++;

	pIO = new exAsyncReadIO ( this->cas, ctx, *this, valueIn );
	if (!pIO) {
		return S_casApp_noMemory;
	}

    return S_casApp_asyncCompletion;
}

//
// exAsyncPV::write()
// (virtual replacement for the default)
//
caStatus exAsyncPV::write ( const casCtx &ctx, const gdd &valueIn )
{
	exAsyncWriteIO *pIO;
	
	if ( this->simultAsychIOCount >= maxSimultAsyncIO ) {
		return S_casApp_postponeAsyncIO;
	}

	this->simultAsychIOCount++;

	pIO = new exAsyncWriteIO ( this->cas, ctx, *this, valueIn );
	if ( ! pIO ) {
		return S_casApp_noMemory;
	}

	return S_casApp_asyncCompletion;
}

//
// exAsyncWriteIO::exAsyncWriteIO()
//
exAsyncWriteIO::exAsyncWriteIO ( exServer & cas,
        const casCtx & ctxIn, exAsyncPV & pvIn, const gdd & valueIn ) :
	casAsyncWriteIO ( ctxIn ), pv ( pvIn ), 
        timer ( cas.createTimer () ), pValue(valueIn)
{
    this->timer.start ( *this, 0.1 );
}

//
// exAsyncWriteIO::~exAsyncWriteIO()
//
exAsyncWriteIO::~exAsyncWriteIO()
{
	this->pv.removeIO();
    this->timer.destroy ();
}

//
// exAsyncWriteIO::expire()
// (a virtual function that runs when the base timer expires)
//
epicsTimerNotify::expireStatus exAsyncWriteIO::expire ( const epicsTime & /* currentTime */ ) 
{
	caStatus status;
	status = this->pv.update ( *this->pValue );
	this->postIOCompletion ( status );
    return noRestart;
}

//
// exAsyncReadIO::exAsyncReadIO()
//
exAsyncReadIO::exAsyncReadIO ( exServer & cas, const casCtx & ctxIn, 
                              exAsyncPV & pvIn, gdd & protoIn ) :
	casAsyncReadIO ( ctxIn ), pv ( pvIn ), 
        timer ( cas.createTimer() ), pProto ( protoIn )
{
    this->timer.start ( *this, 0.1 );
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
	caStatus status;

	//
	// map between the prototype in and the
	// current value
	//
	status = this->pv.exPV::readNoCtx ( this->pProto );

	//
	// post IO completion
	//
	this->postIOCompletion ( status, *this->pProto );

    return noRestart;
}

