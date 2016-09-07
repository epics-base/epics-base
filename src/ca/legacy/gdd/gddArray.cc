/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Author:	Jim Kowalkowski
// Date:	3/97

#define epicsExportSharedSymbols
#include "gdd.h"

// ----------------------The gddAtomic functions-------------------------

gddAtomic::gddAtomic(int app, aitEnum prim, int dimen, ...):
		gdd(app,prim,dimen)
{
	va_list ap;
	int i;
	aitIndex val;

	va_start(ap,dimen);
	for(i=0;i<dimen;i++)
	{
		val=va_arg(ap,aitUint32);
		bounds[i].set(0,val);
	}
	va_end(ap);
}

gddStatus gddAtomic::getBoundingBoxSize(aitUint32* b) const
{
	unsigned i;
	gddStatus rc=0;

	if(dimension()>0)
		for(i=0;i<dimension();i++) b[i]=bounds[i].size();
	else
	{
		gddAutoPrint("gddAtomic::getBoundingBoxSize()",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}

	return rc;
}

gddStatus gddAtomic::setBoundingBoxSize(const aitUint32* const b)
{
	unsigned i;
	gddStatus rc=0;

	if(dimension()>0)
		for(i=0;i<dimension();i++) bounds[i].setSize(b[i]);
	else
	{
		gddAutoPrint("gddAtomic::setBoundingBoxSize()",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}

	return rc;
}

gddStatus gddAtomic::getBoundingBoxOrigin(aitUint32* b) const
{
	unsigned i;
	gddStatus rc=0;

	if(dimension()>0)
		for(i=0;i<dimension();i++) b[i]=bounds[i].first();
	else
	{
		gddAutoPrint("gddAtomic::getBoundingBoxOrigin()",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}

	return rc;
}

gddStatus gddAtomic::setBoundingBoxOrigin(const aitUint32* const b)
{
	unsigned i;
	gddStatus rc=0;

	if(dimension()>0)
		for(i=0;i<dimension();i++) bounds[i].setFirst(b[i]);
	else
	{
		gddAutoPrint("gddAtomic::setBoundingBoxOrigin",gddErrorOutOfBounds);
		rc=gddErrorOutOfBounds;
	}

	return rc;
}

