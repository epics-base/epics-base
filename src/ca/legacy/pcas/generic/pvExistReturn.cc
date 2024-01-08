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
//      Author: Jeffrey O. Hill
//              johill@lanl.gov
//

#define epicsExportSharedSymbols
#include "casdef.h"

pvExistReturn::pvExistReturn ( pvExistReturnEnum s ) :
	status ( s ) {}

pvExistReturn::pvExistReturn ( const caNetAddr & addrIn ) :
	address ( addrIn ), status ( pverExistsHere ) {}

pvExistReturn::pvExistReturn ( const caNetAddr & addrIn, ca_uint16_t minorProtoIn ) :
	address ( addrIn ), status ( pverExistsHere ),
	minorProtocolVersion(minorProtoIn) {}

pvExistReturn::~pvExistReturn ()
{ 
}

const pvExistReturn & pvExistReturn::operator = ( pvExistReturnEnum rhs )
{
	this->status = rhs;
	this->address.clear ();
	this->minorProtocolVersion= 0;
	return * this;
}

const pvExistReturn & pvExistReturn::operator = ( const caNetAddr & rhs )
{
	this->status = pverExistsHere;
	this->address = rhs;
	this->minorProtocolVersion = 0;
	return * this;
}

pvExistReturnEnum pvExistReturn::getStatus () const 
{ 
    return this->status; 
}

caNetAddr pvExistReturn::getAddr () const 
{ 
    return this->address; 
}

bool pvExistReturn::addrIsValid () const 
{ 
    return this->address.isValid (); 
}

unsigned pvExistReturn::getMinorProtocol () const 
{ 
    return this->minorProtocolVersion; 
}

