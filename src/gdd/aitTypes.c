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
 *
 * $Id$
 *
 * $Log$
 * Revision 1.8  2000/10/12 21:52:48  jhill
 * changes to support compilation by borland
 *
 * Revision 1.7  1999/05/13 22:09:23  jhill
 * added new line at EOF
 *
 * Revision 1.6  1999/05/13 20:59:39  jhill
 * removed redundant includes
 *
 * Revision 1.5  1998/04/17 17:49:25  jhill
 * fixed range problem in string to fp convert
 *
 * Revision 1.4  1997/08/05 00:51:07  jhill
 * fixed problems in aitString and the conversion matrix
 *
 * Revision 1.3  1997/05/01 19:54:51  jhill
 * updated dll keywords
 *
 * Revision 1.2  1997/04/10 19:59:24  jhill
 * api changes
 *
 * Revision 1.1  1996/06/25 19:11:32  jbk
 * new in EPICS base
 *
 * Revision 1.1  1996/05/31 13:15:21  jbk
 * add new stuff
 *
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



