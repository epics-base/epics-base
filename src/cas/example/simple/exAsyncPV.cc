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

	pIO = new exAsyncReadIO(ctx, *this, valueIn);
	if (!pIO) {
		return S_casApp_noMemory;
	}

    return S_casApp_asyncCompletion;
}

//
// exAsyncPV::write()
// (virtual replacement for the default)
//
caStatus exAsyncPV::write (const casCtx &ctx, gdd &valueIn)
{
	exAsyncWriteIO	*pIO;
	
	if (this->simultAsychIOCount>=maxSimultAsyncIO) {
		return S_casApp_postponeAsyncIO;
	}

	this->simultAsychIOCount++;

	pIO = new exAsyncWriteIO(ctx, *this, valueIn);
	if (!pIO) {
		return S_casApp_noMemory;
	}

	return S_casApp_asyncCompletion;
}

//
// exAsyncWriteIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncWriteIO::expire() 
{
	caStatus status;
	status = this->pv.update(this->value);
	this->postIOCompletion (status);
}

//
// exAsyncWriteIO::name()
//
const char *exAsyncWriteIO::name() const
{
	return "exAsyncWriteIO";
}

//
// exAsyncReadIO::expire()
// (a virtual function that runs when the base timer expires)
//
void exAsyncReadIO::expire ()
{
	caStatus status;

	//
	// map between the prototype in and the
	// current value
	//
	status = this->pv.exPV::readNoCtx (this->proto);

	//
	// post IO completion
	//
	this->postIOCompletion (status, this->proto);
}

//
// exAsyncReadIO::name()
//
const char *exAsyncReadIO::name() const
{
	return "exAsyncReadIO";
}

