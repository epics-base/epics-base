/*
 *	%W% %G%
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

#ifndef MIT_FLOAT
#define htond(IEEEhost, IEEEnet) (*(IEEEnet) = *(IEEEhost))
#define ntohd(IEEEnet, IEEEhost) (*(IEEEhost) = *(IEEEnet))
#define htonf(IEEEhost, IEEEnet) (*(IEEEnet) = *(IEEEhost))
#define ntohf(IEEEnet, IEEEhost) (*(IEEEhost) = *(IEEEnet))
#endif
