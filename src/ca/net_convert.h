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
 *
 *	Author: J. Hill 
 *
 */

#ifndef _NET_CONVERT_H
#define _NET_CONVERT_H

#include "db_access.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Here are the definitions for architecture dependent byte ordering 
 * and floating point format
 */
#if defined (_M_IX86) || defined (_X86_) || defined (__i386__)
#	define CA_FLOAT_IEEE
#	define CA_LITTLE_ENDIAN
#elif defined (VAX) 
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif ( defined (__ALPHA) || defined (__alpha) ) && ( defined (VMS) || defined (__VMS) )
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif ( defined (__ALPHA) || defined (__alpha) ) && defined (UNIX)
#	define CA_FLOAT_IEEE
#	define CA_LITTLE_ENDIAN
#else
#	define CA_FLOAT_IEEE
#	define CA_BIG_ENDIAN
#endif

/*
 * some architecture sanity checks
 */
#if defined(CA_BIG_ENDIAN) && defined(CA_LITTLE_ENDIAN)
#   error defined(CA_BIG_ENDIAN) && defined(CA_LITTLE_ENDIAN)
#endif
#if !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#   error !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#endif
#if defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#   error defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#endif
#if !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT)
#   error !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT) 
#endif

/*
 * CONVERSION_REQUIRED is set if either the byte order
 * or the floating point does not match
 */
#if !defined(CA_FLOAT_IEEE) || !defined(CA_BIG_ENDIAN)
#   define CONVERSION_REQUIRED
#endif

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 */

typedef void CACVRTFUNC (const void *pSrc, void *pDest, 
                         int hton, arrayElementCount count);

#ifdef CONVERSION_REQUIRED
/*  cvrt is (array of) (pointer to) (function returning) int */
epicsShareExtern CACVRTFUNC *cac_dbr_cvrt[LAST_BUFFER_TYPE+1];
#endif

#if defined(CA_FLOAT_IEEE) && !defined(CA_LITTLE_ENDIAN)
#   ifdef _cplusplus
        inline void dbr_htond ( dbr_double_t *IEEEhost, dbr_double_t *IEEEnet )
        {
            *IEEEnet = *IEEEhost;
        }
        inline void dbr_ntohd ( dbr_double_t *IEEEnet, dbr_double_t *IEEEhost )
        {
		    *IEEEhost = *IEEEnet;
        }
        inline void dbr_htonf ( dbr_float_t *IEEEhost, dbr_float_t *IEEEnet )
        {
		    *IEEEnet = *IEEEhost;
        }
        inline void dbr_ntohf ( dbr_float_t *IEEEnet, dbr_float_t *IEEEhost )
        {
		    *IEEEhost = *IEEEnet;
        }
#   else
        /*
         * for rsrv
         */
        #define dbr_htond(IEEEhost, IEEEnet) (*IEEEnet = *IEEEhost)
        #define dbr_ntohd(IEEEnet, IEEEhost) (*IEEEhost = *IEEEnet)
        #define dbr_htonf(IEEEhost, IEEEnet) (*IEEEnet = *IEEEhost)
        #define dbr_ntohf(IEEEnet, IEEEhost) (*IEEEhost = *IEEEnet)
#   endif
#else
	epicsShareFunc void dbr_htond ( dbr_double_t *pHost, dbr_double_t *pNet );
	epicsShareFunc void dbr_ntohd ( dbr_double_t *pNet, dbr_double_t *pHost );
	epicsShareFunc void dbr_htonf ( dbr_float_t *pHost, dbr_float_t *pNet );
	epicsShareFunc void dbr_ntohf ( dbr_float_t *pNet, dbr_float_t *pHost );
#endif

#ifdef __cplusplus
}
#endif

#endif	/* define _NET_CONVERT_H */
