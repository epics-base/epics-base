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

pvAttachReturn::pvAttachReturn ()
{ 
	this->pPV = 0; 
	//
	// A pv name is required for success
	//
	this->stat = S_cas_badParameter; 
}

pvAttachReturn::pvAttachReturn ( caStatus statIn )
{ 
	this->pPV = NULL; 
	if ( statIn == S_casApp_success ) {
		//
		// A pv name is required for success
		//
		this->stat = S_cas_badParameter;
	}
	else {
		this->stat = statIn;
	}
}

pvAttachReturn::pvAttachReturn ( casPV & pv ) 
{ 
	this->pPV = & pv; 
	this->stat = S_casApp_success; 
}

const pvAttachReturn & pvAttachReturn::operator = ( caStatus rhs )
{
	this->pPV = NULL;
	if ( rhs == S_casApp_success ) {
		this->stat = S_cas_badParameter;
	}
	else {
		this->stat = rhs;
	}
	return *this;
}

// 
// const pvAttachReturn &operator = (casPV &pvIn)
//
// The above assignment operator is not included 
// because it does not match the strict definition of an 
// assignment operator unless "const casPV &"
// is passed in, and we cant use a const PV
// pointer here because the server library _will_ make
// controlled modification of the PV in the future.
//
const pvAttachReturn & pvAttachReturn::operator = ( casPV *pPVIn )
{
	if ( pPVIn != NULL ) {
		this->stat = S_casApp_success;
	}
	else {
		this->stat = S_casApp_pvNotFound;
	}
	this->pPV = pPVIn;
	return *this;
}	

caStatus pvAttachReturn::getStatus () const 
{ 
    return this->stat; 
}

casPV * pvAttachReturn::getPV () const 
{ 
    return this->pPV; 
}

