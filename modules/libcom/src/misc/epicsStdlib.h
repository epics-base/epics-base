/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsStdlib.h */
/* Author: Eric Norum */

#ifndef INC_epicsStdlib_H
#define INC_epicsStdlib_H

/** 
 * \file epicsStdlib.h
 * \brief Functions to convert strings to primitive types
 *
 * These routines convert a string into an integer of the indicated type and 
 * number base, or into a floating point type. The units pointer argument may 
 * be NULL, but if not it will be left pointing to the first non-whitespace 
 * character following the numeric string, or to the terminating zero byte. 
 * 
 * The return value from these routines is a status code, zero meaning OK.
 * For the macro functions beginning with `epicsScan` the return code is 0 
 * or 1 (0=failure or 1=success, similar to the sscanf() function).
 */

#include <stdlib.h>
#include <limits.h>

#include "libComAPI.h"
#include "osdStrtod.h"
#include "epicsTypes.h"
#include "errMdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Return code for `No digits to convert` */
#define S_stdlib_noConversion (M_stdlib | 1) /* No digits to convert */
/** Return code for `Extraneous characters` */
#define S_stdlib_extraneous   (M_stdlib | 2) /* Extraneous characters */
/** Return code for `Too small to represent` */
#define S_stdlib_underflow    (M_stdlib | 3) /* Too small to represent */
/** Return code for `Too large to represent` */
#define S_stdlib_overflow     (M_stdlib | 4) /* Too large to represent */
/** Return code for `Number base not supported` */
#define S_stdlib_badBase      (M_stdlib | 5) /* Number base not supported */

/**
 * \brief Convert a string to a long type
 *
 * \param str Pointer to a constant character array
 * \param to Pointer to the specified type (this will be set during the conversion)
 * \param base The number base to use
 * \param units Pointer to a char * (this will be set with the units string)
 * \return Status code (0=OK, see macro definitions for possible errors)
 */
LIBCOM_API int 
    epicsParseLong(const char *str, long *to, int base, char **units);

/** 
 * \brief Convert a string to a unsigned long type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseULong(const char *str, unsigned long *to, int base, char **units);

/** 
 * \brief Convert a string to a long long type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseLLong(const char *str, long long *to, int base, char **units);

/** 
 * \brief Convert a string to a unsigned long long type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseULLong(const char *str, unsigned long long *to, int base, char **units);

/** 
 * \brief Convert a string to a double type
 *
 * \param str Pointer to a constant character array
 * \param to Pointer to the specified type (this will be set only upon successful conversion)
 * \param units Pointer to a char * (this will be set with the units string only upon successful conversion)
 * \return Status code (0=OK, see macro definitions for possible errors)
 */
LIBCOM_API int
    epicsParseDouble(const char *str, double *to, char **units);

/** 
 * \brief Convert a string to a float type
 * \copydetails epicsParseDouble
 */
LIBCOM_API int
    epicsParseFloat(const char *str, float *to, char **units);

/** 
 * \brief Convert a string to an epicsInt8 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseInt8(const char *str, epicsInt8 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsUInt8 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseUInt8(const char *str, epicsUInt8 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsInt16 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseInt16(const char *str, epicsInt16 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsUInt16 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseUInt16(const char *str, epicsUInt16 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsInt32 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseInt32(const char *str, epicsInt32 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsUInt32 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseUInt32(const char *str, epicsUInt32 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsInt64 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseInt64(const char *str, epicsInt64 *to, int base, char **units);

/** 
 * \brief Convert a string to an epicsUInt64 type
 * \copydetails epicsParseLong 
 */
LIBCOM_API int
    epicsParseUInt64(const char *str, epicsUInt64 *to, int base, char **units);

/**  Macro utilizing ::epicsParseFloat to convert */
#define epicsParseFloat32(str, to, units) epicsParseFloat(str, to, units)
/**  Macro utilizing ::epicsParseDouble to convert */
#define epicsParseFloat64(str, to, units) epicsParseDouble(str, to, units)

/* These macros return 1 if successful, 0 on failure.
 * This is analogous to the return value from sscanf()
 */

/**  
 * Macro utilizing ::epicsParseLong to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanLong(str, to, base) (!epicsParseLong(str, to, base, NULL))

/**  
 * Macro utilizing ::epicsParseULong to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanULong(str, to, base) (!epicsParseULong(str, to, base, NULL))

/**  
 * Macro utilizing ::epicsParseLLong to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanLLong(str, to, base) (!epicsParseLLong(str, to, base, NULL))

/**  
 * Macro utilizing ::epicsParseULLong to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanULLong(str, to, base) (!epicsParseULLong(str, to, base, NULL))

/**  
 * Macro utilizing ::epicsParseFloat to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanFloat(str, to) (!epicsParseFloat(str, to, NULL))

/**  
 * Macro utilizing ::epicsParseDouble to convert 
 * \return 0=failure, 1=success
 */
#define epicsScanDouble(str, to) (!epicsParseDouble(str, to, NULL))

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsStdlib_H */
