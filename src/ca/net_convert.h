/*
 *	$Id$	
 *
 *	MACROS for rapid conversion between HOST data formats and those used
 *	by the IOCs (NETWORK).
 *
 *	Author: J. Hill 
 *
 *	The conversion routines are used in both ca lib
 *	and the IOC ca server (base/rsrv).
 *	The latter, however, cannot include os_depen.h so
 *	I extracted the conversion specific code from there
 *	and put it in this "now stand alone" net_convert.h
 *	8-22-96  -kuk-
 *
 *
 *	joh 	09-13-90	force MIT sign to zero if exponent is zero
 *				to prevent a reseved operand fault.
 *
 *	joh	03-16-94	Added double fp
 *	joh	11-02-94	Moved all fp cvrt to functions in
 *				convert.c
 *
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
#if defined(VAX) 
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif defined(_X86_)
#	define CA_FLOAT_IEEE
#	define CA_LITTLE_ENDIAN
#elif (defined(__ALPHA) && defined(VMS) || defined(__alpha)) && defined(VMS)
#	define CA_FLOAT_MIT
#	define CA_LITTLE_ENDIAN
#elif (defined(__ALPHA) && defined(UNIX) || defined(__alpha)) && defined(UNIX)
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
#error defined(CA_BIG_ENDIAN) && defined(CA_LITTLE_ENDIAN)
#endif
#if !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#error !defined(CA_BIG_ENDIAN) && !defined(CA_LITTLE_ENDIAN)
#endif
#if defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#error defined(CA_FLOAT_IEEE) && defined(CA_FLOAT_MIT)
#endif
#if !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT)
#error !defined(CA_FLOAT_IEEE) && !defined(CA_FLOAT_MIT) 
#endif

/*
 * CONVERSION_REQUIRED is set if either the byte order
 * or the floating point does not match
 */
#if !defined(CA_FLOAT_IEEE) || !defined(CA_BIG_ENDIAN)
#define CONVERSION_REQUIRED
#endif

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 */

typedef void CACVRTFUNC(void *pSrc, void *pDest, int hton, unsigned long count);

#ifdef CONVERSION_REQUIRED
/*  cvrt is (array of) (pointer to) (function returning) int */
epicsShareExtern CACVRTFUNC *cac_dbr_cvrt[LAST_BUFFER_TYPE+1];
#endif


/*
 *	Macros ...
 *
 *	This is also used in the ca server on pc486 archs, 
 *	where source and destination buffers are identical,
 *	so we need a tmp variable in e.g. 'double' conversions.
 */

#ifdef CA_LITTLE_ENDIAN 
#  	ifndef ntohs
#    		define ntohs(SHORT)\
   (	(dbr_short_t)\
	(((SHORT) & (dbr_short_t) 0x00ff) << 8 |\
	((SHORT) & (dbr_short_t) 0xff00) >> 8) )
#  	endif
#  	ifndef htons
#    		define htons(SHORT)\
   (	(dbr_short_t)\
	(((SHORT) & (dbr_short_t) 0x00ff) << 8 |\
	((SHORT) & (dbr_short_t) 0xff00) >> 8) )
#  	endif
#else
#  	ifndef ntohs
#    		define ntohs(SHORT)	(SHORT)
#  	endif
#  	ifndef htons 
#    		define htons(SHORT)	(SHORT)
#  	endif
#endif


#ifdef CA_LITTLE_ENDIAN 
#  	ifndef ntohl
#  	define ntohl(LONG)\
  	( (dbr_long_t) (\
   		( ((dbr_ulong_t)LONG) & 0xff000000 ) >> 24u |\
          	( ((dbr_ulong_t)LONG) & 0x000000ff ) << 24u |\
         	( ((dbr_ulong_t)LONG) & 0x0000ff00 ) << 8u  |\
         	( ((dbr_ulong_t)LONG) & 0x00ff0000 ) >> 8u )\
  	)
#  	endif
#  	ifndef htonl
#  	define htonl(LONG)\
  	( (dbr_long_t) (\
          	( ((dbr_ulong_t)(LONG)) & 0x000000ff ) << 24u |\
   		( ((dbr_ulong_t)(LONG)) & 0xff000000 ) >> 24u |\
         	( ((dbr_ulong_t)(LONG)) & 0x00ff0000 ) >> 8u  |\
         	( ((dbr_ulong_t)(LONG)) & 0x0000ff00 ) << 8u )\
  	)
#  	endif
#	else 
#  	ifndef ntohl
#    		define ntohl(LONG)	(LONG)
#  	endif
#  	ifndef htonl
#    		define htonl(LONG)	(LONG)
#  	endif
#endif

#if defined(CA_FLOAT_IEEE) && !defined(CA_LITTLE_ENDIAN)
#	define dbr_htond(IEEEhost, IEEEnet) \
		(*(dbr_double_t *)(IEEEnet) = *(dbr_double_t *)(IEEEhost))
#	define dbr_ntohd(IEEEnet, IEEEhost) \
		(*(dbr_double_t *)(IEEEhost) = *(dbr_double_t *)(IEEEnet))
#	define dbr_htonf(IEEEhost, IEEEnet) \
		(*(dbr_float_t *)(IEEEnet) = *(dbr_float_t *)(IEEEhost))
#	define dbr_ntohf(IEEEnet, IEEEhost) \
		(*(dbr_float_t *)(IEEEhost) = *(dbr_float_t *)(IEEEnet))
#elif defined(CA_FLOAT_IEEE) && defined(CA_LITTLE_ENDIAN)
#	define dbr_ntohf(NET,HOST)  \
		{*((dbr_long_t *)(HOST)) = ntohl(*((dbr_long_t *)(NET )));}
#	define dbr_htonf(HOST,NET) \
		{*((dbr_long_t *)(NET) ) = htonl(*((dbr_long_t *)(HOST)));}
#	define dbr_ntohd(NET,HOST)					\
		{							\
		dbr_long_t  cvrt_tmp;					\
		cvrt_tmp = ntohl(((dbr_long_t *)(NET))[0]);		\
		((dbr_long_t *)(HOST))[0] = ntohl(((dbr_long_t *)(NET))[1]); \
		((dbr_long_t *)(HOST))[1] = cvrt_tmp;			\
		}
#	define dbr_htond(HOST,NET)					\
		{							\
		dbr_long_t  cvrt_tmp;					\
		cvrt_tmp = htonl(((dbr_long_t *)(HOST))[0]);		\
		((dbr_long_t *)(NET))[0] = htonl(((dbr_long_t *)(HOST))[1]); \
		((dbr_long_t *)(NET))[1] = cvrt_tmp;			\
		}
#else
	void dbr_htond(dbr_double_t *pHost, dbr_double_t *pNet);
	void dbr_ntohd(dbr_double_t *pNet, dbr_double_t *pHost);
	void dbr_htonf(dbr_float_t *pHost, dbr_float_t *pNet);
	void dbr_ntohf(dbr_float_t *pNet, dbr_float_t *pHost);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* define _NET_CONVERT_H */
