/*
 *	$Id$	
 *
 *	N E T _ C O N V E R T . H
 *	MACROS for rapid conversion between HOST data formats and those used
 *	by the IOCs (NETWORK).
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

#include <db_access.h>

#ifdef CA_LITTLE_ENDIAN 
#  	ifndef ntohs
#    		define ntohs(SHORT)\
   (	((SHORT) & 0x00ff) << 8 | ((SHORT) & 0xff00) >> 8	)
#  	endif
#  	ifndef htons
#    		define htons(SHORT)\
   (	((SHORT) & 0x00ff) << 8 | ((SHORT) & 0xff00) >> 8 	)
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
  	(\
   		((LONG) & 0xff000000) >> 24 |\
          	((LONG) & 0x000000ff) << 24 |\
         	((LONG) & 0x0000ff00) << 8  |\
         	((LONG) & 0x00ff0000) >> 8\
  	)
#  	endif
#  	ifndef htonl
#  	define htonl(LONG)\
  	(\
          	((LONG) & 0x000000ff) << 24 |\
   		((LONG) & 0xff000000) >> 24 |\
         	((LONG) & 0x00ff0000) >> 8  |\
         	((LONG) & 0x0000ff00) << 8\
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
#	define dbr_ntohd(NET,HOST) \
		{ ((dbr_long_t *)(HOST))[1] = ntohl(((dbr_long_t *)(NET))[0]) ; \
		((dbr_long_t *)(HOST))[0] = ntohl(((dbr_long_t *)(NET))[1]) ;}
#	define dbr_htond(HOST,NET) \
		{ ((dbr_long_t *)(NET))[1] = htonl(((dbr_long_t *)(HOST))[0]) ; \
		((dbr_long_t *)(NET))[0] = htonl(((dbr_long_t *)(HOST))[1]) ;}
#else
	void dbr_htond(dbr_double_t *pHost, dbr_double_t *pNet);
	void dbr_ntohd(dbr_double_t *pNet, dbr_double_t *pHost);
	void dbr_htonf(dbr_float_t *pHost, dbr_float_t *pNet);
	void dbr_ntohf(dbr_float_t *pNet, dbr_float_t *pHost);
#endif

