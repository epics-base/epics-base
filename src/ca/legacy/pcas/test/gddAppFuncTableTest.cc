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
// gddAppFuncTable.h
//
// Author: Jeff Hill
//
//
//
//

#include <stdio.h>
#include "gddAppFuncTable.h"

class casPV {
public:
	casPV(const char * const pNameIn) : pName(pNameIn) {};
	gddAppFuncTableStatus cb(gdd &val);	
protected:
	const char * const pName;
};

class casPVII : public casPV {
public:
	casPVII(const char * const pNameIn) : casPV(pNameIn) {};
	gddAppFuncTableStatus cb0(gdd &val);	
	gddAppFuncTableStatus cb1(gdd &val);	
};

main ()
{
	gddAppFuncTableTable<casPVII> pvAttrDB;
	unsigned appPrecision= pvAttrDB.RegisterApplicationType ("precision");
	unsigned appValue = pvAttrDB.RegisterApplicationType ("value");
	gddScalar *pPrec = new gddScalar (appPrecision, aitEnumFloat32);
	gddScalar *pVal = new gddScalar (appValue, aitEnumFloat32);
	casPV pv ("jane");
	casPVII pvII ("fred");
	casPVII pvIIa ("albert");

	pvAttrDB.installReadFunc (appPrecision, casPVII::cb0);
	pvAttrDB.installReadFunc (appValue, casPVII::cb1);
	
	pvAttrDB.read (pvII, *pPrec);
	pvAttrDB.read (pvII, *pVal);
	pvAttrDB.read (pvIIa, *pPrec);
}

unsigned casPV::cb(gdd &value)
{
	printf("Here for %s\n", pName);
	return 0;
}

unsigned casPVII::cb0(gdd &value)
{
	printf("Here in cb0 for %s\n", pName);
	return 0;
}

unsigned casPVII::cb1(gdd &value)
{
	printf("Here in cb1 for %s\n", pName);
	return 0;
}


