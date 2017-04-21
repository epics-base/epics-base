/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

#include <limits.h>
#include <float.h>
#include <stddef.h>

#define epicsExportSharedSymbols
#include "aitTypes.h"

epicsShareDef const size_t aitSize[aitTotal] = {
	0,
	sizeof(aitInt8),
	sizeof(aitUint8),
	sizeof(aitInt16),
	sizeof(aitUint16),
	sizeof(aitEnum16),
	sizeof(aitInt32),
	sizeof(aitUint32),
	sizeof(aitFloat32),
	sizeof(aitFloat64),
	sizeof(aitFixedString),
	sizeof(aitString),
	0
};

epicsShareDef const char* aitName[aitTotal] = {
	"aitInvalid",
	"aitInt8",
	"aitUint8",
	"aitInt16",
	"aitUint16",
	"aitEnum16",
	"aitInt32",
	"aitUint32",
	"aitFloat32",
	"aitFloat64",
	"aitFixedString",
	"aitString",
	"aitContainer"
};

/*
 * conversion characters used with stdio lib
 */
epicsShareDef const char* aitPrintf[aitTotal] = {
	0,
	"c",
	"c",
	"hd",
	"hu",
	"hu",
	"d",
	"u",
	"g",
	"g",
	"s",
	0, /* printf doesnt know about aitString */
	0
};
epicsShareDef const char* aitScanf[aitTotal] = {
	0,
	"c",
	"c",
	"hd",
	"hu",
	"hu",
	"d",
	"u",
	"g",
	"lg",
	"s",
	0, /* scanf doesnt know about aitString */
	0
};



