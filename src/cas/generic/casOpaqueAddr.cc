/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#define epicsExportSharedSymbols
#include "casdef.h"

//
// this needs to be here (and not in casOpaqueAddrIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
//
// casOpaqueAddr::casOpaqueAddr()
//
casOpaqueAddr::casOpaqueAddr()
{
	this->clear();
}

//
// this needs to be here (and not in casOpaqueAddrIL.h) if we
// are to avoid undefined symbols under gcc 2.7.x without -O
//
//
// casOpaqueAddr::clear()
//
void casOpaqueAddr::clear()
{
	this->init = false;
}


