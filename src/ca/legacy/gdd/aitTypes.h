/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef AIT_TYPES_H
#define AIT_TYPES_H 1

/*
 * Author: Jim Kowalkowski
 * Date: 2/96
 */

/* This is the file the user sets up for a given architecture */

#define AIT_FIXED_STRING_SIZE 40

#include "shareLib.h"

typedef signed char         aitInt8;
typedef unsigned char       aitUint8;
typedef short               aitInt16;
typedef unsigned short      aitUint16;
typedef aitUint16           aitEnum16;
typedef int                 aitInt32;
typedef unsigned int        aitUint32;
typedef float               aitFloat32;
typedef double              aitFloat64;
typedef aitUint32           aitIndex;
typedef void*               aitPointer;

typedef union {
    struct {
        aitUint16 aitStat;
        aitUint16 aitSevr;
    } s;
    aitUint32 u;
} aitStatus;

/* should the bool be added as a conversion type? it currently is not */
typedef enum {
	aitFalse=0,
	aitTrue
} aitBool;

typedef struct {
	char fixed_string[AIT_FIXED_STRING_SIZE];
} aitFixedString;

#ifdef __cplusplus
#include "aitHelpers.h"
#else
/* need time stamp structure different from posix unfortunetly */
typedef struct {
	aitUint32 tv_sec;
	aitUint32 tv_nsec;
} aitTimeStamp;

/* strings are a struct so they are different then aitInt8 */
typedef	struct {
	char* string;
	aitUint32 len;
} aitString;
#endif

/* all normal types */
#define aitTotal 13
#define aitFirst aitEnumInvalid
#define aitLast aitEnumContainer
#define aitValid(x) ((x)<=aitLast && (x)>aitFirst)

/* all conversion types */
#define aitConvertTotal 11
#define aitConvertFirst aitEnumInt8
#define aitConvertLast aitEnumString
#define aitConvertAutoFirst aitEnumInt8
#define aitConvertAutoLast aitEnumFloat64
#define aitConvertValid(x) ((x)>=aitConvertFirst && (x)<=aitConvertLast)

/* currently no 64-bit integer support */
typedef enum {
	aitEnumInvalid=0,
	aitEnumInt8,
	aitEnumUint8,
	aitEnumInt16,
	aitEnumUint16,
	aitEnumEnum16,
	aitEnumInt32,
	aitEnumUint32,
	aitEnumFloat32,
	aitEnumFloat64,
	aitEnumFixedString,
	aitEnumString,
	aitEnumContainer
} aitEnum;

typedef union {
	aitInt8		Int8;
	aitUint8	Uint8;
	aitInt16	Int16;
	aitUint16	Uint16;
	aitEnum16	Enum16;
	aitInt32	Int32;
	aitUint32	Uint32;
	aitFloat32	Float32;
	aitFloat64	Float64;
	aitIndex	Index;
	aitPointer	Pointer;
	aitFixedString* FString;
	aitUint8	Dumb1[sizeof(aitString)];		/* aitString String; */
	aitUint8	Dumb3[sizeof(aitTimeStamp)];	/* aitTimeStamp	Stamp; */
} aitType;

/*
 classes are not allowed in union that required construction/destruction
 so I am insuring that the size of aitType is large enough to hold
 strings and timestamps in an obsure, horrible way with the Dumb variables
*/


#ifdef __cplusplus
extern "C" {
#endif
epicsShareExtern const size_t aitSize[aitTotal];
epicsShareExtern const char*  aitName[aitTotal];
epicsShareExtern const char*  aitPrintf[aitTotal];
epicsShareExtern const char*  aitScanf[aitTotal];
#ifdef __cplusplus
}
#endif


#endif
