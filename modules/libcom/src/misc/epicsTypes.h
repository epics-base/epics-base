/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      Author:     Jeff Hill
 *      Date:           5-95
 */

#ifndef INC_epicsTypes_H
#define INC_epicsTypes_H

#include "shareLib.h"
#include "compilerDependencies.h"

#ifndef stringOf
#   if defined (__STDC__ ) || defined (__cplusplus)
#       define stringOf(TOKEN) #TOKEN
#   else
#       define stringOf(TOKEN) "TOKEN"
#   endif
#endif

typedef enum {
    epicsFalse = 0,
    epicsTrue  = 1
} epicsBoolean EPICS_DEPRECATED;

/*
 * Architecture Independent Data Types
 * These are sufficient for all our current archs
 */
typedef char            epicsInt8;
typedef unsigned char   epicsUInt8;
typedef short           epicsInt16;
typedef unsigned short  epicsUInt16;
typedef int             epicsInt32;
typedef unsigned int    epicsUInt32;
typedef long long       epicsInt64;
typedef unsigned long long epicsUInt64;

typedef epicsUInt16     epicsEnum16;
typedef float           epicsFloat32;
typedef double          epicsFloat64;
typedef epicsInt32      epicsStatus;


typedef struct {
    unsigned    length;
    char        *pString;
} epicsString;

/*
 * !! Dont use this - it may vanish in the future !!
 *
 * Provided only for backwards compatibility with
 * db_access.h
 *
 */
#define MAX_STRING_SIZE 40
typedef char            epicsOldString[MAX_STRING_SIZE];

/*
 * union of all types
 *
 * Strings included here as pointers only so that we support
 * large string types.
 *
 * Arrays included here as pointers because large arrays will
 * not fit in this union.
 */
typedef union epics_any {
    epicsInt8       int8;
    epicsUInt8      uInt8;
    epicsInt16      int16;
    epicsUInt16     uInt16;
    epicsEnum16     enum16;
    epicsInt32      int32;
    epicsUInt32     uInt32;
    epicsInt64      int64;
    epicsUInt64     uInt64;
    epicsFloat32    float32;
    epicsFloat64    float64;
    epicsString     string;
} epicsAny;

/*
 * Corresponding Type Codes
 * (this enum must start at zero)
 *
 * !! Update epicsTypeToDBR_XXXX[] and DBR_XXXXToEpicsType
 *  in db_access.h if you edit this enum !!
 */
typedef enum {
    epicsInt8T,
    epicsUInt8T,
    epicsInt16T,
    epicsUInt16T,
    epicsEnum16T,
    epicsInt32T,
    epicsUInt32T,
    epicsFloat32T,
    epicsFloat64T,
    epicsStringT,
    epicsOldStringT
} epicsType;
#define firstEpicsType epicsInt8T
#define lastEpicsType epicsOldStringT
#define validEpicsType(x)   ((x>=firstEpicsType) && (x<=lastEpicsType))
#define invalidEpicsType(x) ((x<firstEpicsType) || (x>lastEpicsType))


/*
 * The enumeration "epicsType" is an index to this array
 * of type name strings.
 */
#ifdef epicsTypesGLOBAL
epicsShareDef const char *epicsTypeNames [lastEpicsType+1] = {
    "epicsInt8",
    "epicsUInt8",
    "epicsInt16",
    "epicsUInt16",
    "epicsEnum16",
    "epicsInt32",
    "epicsUInt32",
    "epicsFloat32",
    "epicsFloat64",
    "epicsString",
    "epicsOldString",
};
#else /* epicsTypesGLOBAL */
epicsShareExtern const char *epicsTypeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/*
 * The enumeration "epicsType" is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
epicsShareDef const char *epicsTypeCodeNames [lastEpicsType+1] = {
    "epicsInt8T",
    "epicsUInt8T",
    "epicsInt16T",
    "epicsUInt16T",
    "epicsEnum16T",
    "epicsInt32T",
    "epicsUInt32T",
    "epicsFloat32T",
    "epicsFloat64T",
    "epicsStringT",
    "epicsOldStringT",
};
#else /* epicsTypesGLOBAL */
epicsShareExtern const char *epicsTypeCodeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#ifdef epicsTypesGLOBAL
epicsShareDef const unsigned epicsTypeSizes [lastEpicsType+1] = {
    sizeof (epicsInt8),
    sizeof (epicsUInt8),
    sizeof (epicsInt16),
    sizeof (epicsUInt16),
    sizeof (epicsEnum16),
    sizeof (epicsInt32),
    sizeof (epicsUInt32),
    sizeof (epicsFloat32),
    sizeof (epicsFloat64),
    sizeof (epicsString),
    sizeof (epicsOldString),
};
#else /* epicsTypesGLOBAL */
epicsShareExtern const unsigned epicsTypeSizes [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/*
 * The enumeration "epicsType" is an index to this array
 * of type class identifiers.
 */
typedef enum {
    epicsIntC,
    epicsUIntC,
    epicsEnumC,
    epicsFloatC,
    epicsStringC,
    epicsOldStringC
} epicsTypeClass;

#ifdef epicsTypesGLOBAL
epicsShareDef const epicsTypeClass epicsTypeClasses [lastEpicsType+1] = {
    epicsIntC,
    epicsUIntC,
    epicsIntC,
    epicsUIntC,
    epicsEnumC,
    epicsIntC,
    epicsUIntC,
    epicsFloatC,
    epicsFloatC,
    epicsStringC,
    epicsOldStringC
};
#else /* epicsTypesGLOBAL */
epicsShareExtern const epicsTypeClass epicsTypeClasses [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */


#ifdef epicsTypesGLOBAL
epicsShareDef const char *epicsTypeAnyFieldName [lastEpicsType+1] = {
    "int8",
    "uInt8",
    "int16",
    "uInt16",
    "enum16",
    "int32",
    "uInt32",
    "float32",
    "float64",
    "string",
    "", /* Old Style Strings will not be in epicsAny type */
};
#else /* epicsTypesGLOBAL */
epicsShareExtern const char *epicsTypeAnyFieldName [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#endif /* INC_epicsTypes_H */

