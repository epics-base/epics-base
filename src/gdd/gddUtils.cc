/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

// Author:	Jim Kowalkowski
// Date:	3/97
//
// $Id$
// $Log$
// Revision 1.2  1997/04/23 17:13:06  jhill
// fixed export of symbols from WIN32 DLL
//
// Revision 1.1  1997/03/21 01:56:11  jbk
// *** empty log message ***
//

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#define epicsExportSharedSymbols
#include "gddNewDel.h"
#include "gddUtils.h"

gdd_NEWDEL_NEW(gddBounds1D)
gdd_NEWDEL_DEL(gddBounds1D)
gdd_NEWDEL_STAT(gddBounds1D)

gdd_NEWDEL_NEW(gddBounds2D)
gdd_NEWDEL_DEL(gddBounds2D)
gdd_NEWDEL_STAT(gddBounds2D)

gdd_NEWDEL_NEW(gddBounds3D)
gdd_NEWDEL_DEL(gddBounds3D)
gdd_NEWDEL_STAT(gddBounds3D)

gdd_NEWDEL_NEW(gddDestructor)
gdd_NEWDEL_DEL(gddDestructor)
gdd_NEWDEL_STAT(gddDestructor)

// --------------------------The gddBounds functions-------------------

// gddBounds::gddBounds(void) { first=0; count=0; }

// --------------------------The gddDestructor functions-------------------

gddStatus gddDestructor::destroy(void* thing)
{
	if(ref_cnt==0 || --ref_cnt==0)
	{
		run(thing);
		delete this;
	}
	return 0;
}

void gddDestructor::run(void* thing)
{
	aitInt8* pd = (aitInt8*)thing;
	delete [] pd;
}

