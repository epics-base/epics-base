
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
#include "ellLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct sockaddr_in;
struct in_addr;

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

#ifdef __cplusplus
}
#endif

#include "osdSock.h"

typedef union osiSockAddr {
    struct sockaddr_in  ia;
    struct sockaddr     sa;
} osiSockAddr;

typedef struct osiSockAddrNode {
    ELLNODE node;
    osiSockAddr addr;
} osiSockAddrNode;

/*
 *  osiSockDiscoverBroadcastAddresses ()
 *  Returns the broadcast addresses of each network interface found.
 *
 *	This routine is provided with the address of an ELLLIST, a socket,
 * 	a destination port number, and a match address. When the 
 * 	routine returns there will be one additional entry 
 *	(an osiSockAddrNode) in the list for each network interface found that 
 *	is up and isnt a loop back interface (match addr is INADDR_ANY),
 *	or only the interfaces that match the specified addresses (match addr 
 *  is other than INADDR_ANY). If the interface supports broadcasting 
 *  then add its broadcast address to the list. If the interface is a 
 *  point to point link then add the destination address of the point to
 *	point link to the list. 
 *
 * 	Any mutex locking required to protect pList is applied externally.
 *
 */
epicsShareFunc void epicsShareAPI osiSockDiscoverBroadcastAddresses
     (ELLLIST *pList, SOCKET socket, const osiSockAddr *pMatchAddr);

/*
 * osiLocalAddr ()
 * Returns the osiSockAddr of the first non-loopback interface found
 * that is operational (up flag is set). If no valid address can be 
 * located then return an osiSockAddr with the address family set to 
 * unspecified (AF_UNSPEC).
 *
 * Unfortunately in EPICS 3.13 beta 11 and before the CA
 * repeater would not always allow the loopback address
 * as a local client address so current clients alternate
 * between the address of the first non-loopback interface
 * found and the loopback addresss when subscribing with 
 * the CA repeater until all CA repeaters have been updated
 * to current code. After all CA repeaters have been restarted 
 * this osi interface can be eliminated.
 */
epicsShareFunc osiSockAddr epicsShareAPI osiLocalAddr (SOCKET socket);

#endif /* ifndef osiSockh */
