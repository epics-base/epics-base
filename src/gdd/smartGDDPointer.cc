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
// Smart Pointer Class for GDD
// ( automatically takes care of referencing / unreferening a gdd each time that it is used )
//
// Author: Jeff Hill
//

#define epicsExportSharedSymbols
#include "smartGDDPointer.h"

//
// smartGDDPointer::set()
//
void smartGDDPointer::set (gdd *pNewValue) 
{
	int gddStatus;
	//
	// dont change the ref count (and
	// potentially unref a gdd that we are 
	// still using if the pointer isnt changing
	//
	if (this->pValue==pNewValue) {
		return;
	}
	if (this->pValue!=NULL) {
		gddStatus = this->pValue->unreference();
		assert (!gddStatus);
	}
	this->pValue = pNewValue;
	if (this->pValue!=NULL) {
		gddStatus = this->pValue->reference();
		assert (!gddStatus);
	}
}
