
/*
 *	    $Id$
 *
 *      socket support library API def
 *
 *      7-1-97  -joh-
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 */
#ifndef osiSockh
#define osiSockh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * convert IP address to ASCII in this order
 * 1) look for matching host name
 * 2) convert to raw dotted IP address with trailing port
 */
epicsShareFunc void epicsShareAPI ipAddrToA
	(const struct sockaddr_in *pInetAddr, char *pBuf, unsigned bufSize);

/*
 * convert socket address to ASCII in this order
 * 1) look for matching host name
 * 2) convert to raw ascii address (typically this is a 
 *      dotted IP address with trailing port)
 */
epicsShareFunc void epicsShareAPI sockAddrToA 
			(const struct sockaddr *paddr, char *pBuf, unsigned bufSize);

/*
 * convert inet address to a host name string
 *
 * returns the number of bytes stored in buffer not counting the terminating 
 * null character, or zero on failure
 *
 * OS specific
 */
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize);

/*
 * attempt to convert ASCII string to an IP address in this order
 * 1) look for traditional doted ip with optional port
 * 2) look for raw number form of ip address with optional port
 * 3) look for valid host name with optional port
 */
epicsShareFunc int epicsShareAPI aToIPAddr
	(const char *pAddrString, unsigned short defaultPort, struct sockaddr_in *pIP);

/*
 * attempt to convert ASCII host name string with optional port to an IP address
 */
epicsShareFunc int epicsShareAPI hostToIPAddr 
				(const char *pHostName, struct in_addr *pIPA);
/*
 * attach to BSD socket library
 */
epicsShareFunc int epicsShareAPI bsdSockAttach(void); /* returns T if success, else F */

/*
 * release BSD socket library
 */
epicsShareFunc void epicsShareAPI bsdSockRelease(void);

/*
 * convert socket error number to a string
 */
epicsShareFunc const char * epicsShareAPI convertSocketErrorToString (int errnoIn);

#ifdef _WIN32
epicsShareFunc unsigned epicsShareAPI wsaMajorVersion(void);
#endif

#ifdef __cplusplus
}
#endif

#include "osdSock.h"

#endif /* ifndef osiSockh */
