/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 *
 * $Id$
 *
 * $Log$
 * Revision 1.1  1996/05/31 13:15:21  jbk
 * add new stuff
 *
 */

#define AIT_TYPES_SOURCE 1
#include <sys/types.h>
#include "aitTypes.h"

const size_t aitSize[aitTotal] = {
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

const char* aitName[aitTotal] = {
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

const char* aitStringType[aitTotal] = {
	"%8.8x",
	"%2.2x",
	"%2.2x",
	"%hd",
	"%hu",
	"%hu",
	"%d",
	"%u",
	"%f",
	"%lf",
	"%s",
	"%s",
	"%8.8x"
};

