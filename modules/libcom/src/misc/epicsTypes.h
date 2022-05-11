/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsTypes.h
 * \author: Jeff Hill
 * 
 * \brief The core data types used by epics
 */

#ifndef INC_epicsTypes_H
#define INC_epicsTypes_H

#include "libComAPI.h"
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

/**
 * \name epicsTypes
 * Architecture Independent Data Types
 * 
 * These are sufficient for all our current archs
 * @{
 */
typedef signed char     epicsInt8;
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
 /** @} */

#define MAX_STRING_SIZE 40

/**
 * \brief !! Don't use this - it may vanish in the future !!
 */
typedef struct {
    unsigned    length;
    char        *pString;
} epicsString;

/**
 * \brief !! Don't use this - it may vanish in the future !!
 *
 * Provided only for backwards compatibility with
 * db_access.h
 */
typedef char            epicsOldString[MAX_STRING_SIZE];

/**
 * \brief Union of all types
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

/**
 * \brief Corresponding Type Codes
 * (this enum must start at zero)
 *
 * \note Update \a epicsTypeToDBR_XXXX[] and \a DBR_XXXXToEpicsType
 *  in db_access.h if you edit this enum
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


/**
 * \brief An array providing the names for each type
 * The enumeration \ref epicsType is an index to this array
 * of type name strings.
 */
#ifdef epicsTypesGLOBAL
const char *epicsTypeNames [lastEpicsType+1] = {
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
LIBCOM_API extern const char *epicsTypeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/**
 * \brief An array providing the names for each type code
 * The enumeration \ref epicsType is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
const char *epicsTypeCodeNames [lastEpicsType+1] = {
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
LIBCOM_API extern const char *epicsTypeCodeNames [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/**
 * \brief An array providing the sizes for each type
 * The enumeration \ref epicsType is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
const unsigned epicsTypeSizes [lastEpicsType+1] = {
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
LIBCOM_API extern const unsigned epicsTypeSizes [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

typedef enum {
    epicsIntC,
    epicsUIntC,
    epicsEnumC,
    epicsFloatC,
    epicsStringC,
    epicsOldStringC
} epicsTypeClass;

/**
 * \brief An array providing the class of each type
 * The enumeration \ref epicsType is an index to this array
 * of type class identifiers.
 */
#ifdef epicsTypesGLOBAL
const epicsTypeClass epicsTypeClasses [lastEpicsType+1] = {
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
LIBCOM_API extern const epicsTypeClass epicsTypeClasses [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

/**
 * \brief An array providing the field name for each type
 * The enumeration \ref epicsType is an index to this array
 * of type code name strings.
 */
#ifdef epicsTypesGLOBAL
const char *epicsTypeAnyFieldName [lastEpicsType+1] = {
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
LIBCOM_API extern const char *epicsTypeAnyFieldName [lastEpicsType+1];
#endif /* epicsTypesGLOBAL */

#endif /* INC_epicsTypes_H */

