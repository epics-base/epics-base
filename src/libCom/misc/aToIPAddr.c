/*
 * rational replacement for inet_addr()
 * 
 * author: Jeff Hill
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#define LOCAL

#define epicsExportSharedSymbols
#include "bsdSocketResource.h"

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif /*NELEMENTS*/

LOCAL int addrArrayToUL (const long *pAddr, unsigned nElements, unsigned long *pIpAddr);
LOCAL int initIPAddr (unsigned long ipAddr, unsigned short port, struct sockaddr_in *pIP);

/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255, allows the user
 * to specify a port number, and allows also a
 * named host to be specified.
 *
 * Sets the port number to "defaultPort" only if 
 * "pAddrString" does not contain an addres of the form
 * "n.n.n.n:p"
 */
epicsShareFunc int epicsShareAPI 
	aToIPAddr(const char *pAddrString, unsigned short defaultPort, struct sockaddr_in *pIP)
{
	int status;
	long addr[4];
	char hostName[512]; /* !! change n elements here requires change in format below !! */
	int port;

	unsigned long ipAddr;

	/*
	 * traditional dotted ip addres
	 */
	status = sscanf (pAddrString, "%li.%li.%li.%li:%i", 
			addr, addr+1u, addr+2u, addr+3u, &port);
	if (status>=4) {
		if (addrArrayToUL (addr, NELEMENTS(addr), &ipAddr)<0) {
			return -1;
		}
		if (status==4) {
			port = defaultPort;
		}
		if (port<0 || port>USHRT_MAX) {
			return -1;
		}
		return initIPAddr (ipAddr, (unsigned short) port, pIP);
	}
	
	/*
	 * IP address as a raw number
	 */
	status = sscanf (pAddrString, "%li:%i", addr, &port);
	if (status>=1) {
		if (*addr<0x0 && *addr>0xffffffff) {
			return -1;
		}
		if (status==1) {
			port = defaultPort;
		}
		if (port<0 || port>USHRT_MAX) {
			return -1;
		}
		return initIPAddr ((unsigned long)*addr, (unsigned short)port, pIP);
	}

	/*
	 * check for a valid host name before giving up
	 */
	status = sscanf (pAddrString, "%511s:%i", hostName, &port);
	if (status>=1) {
		struct in_addr ina;

		if (status==1) {
			port = defaultPort;
		}
		if (port<0 || port>USHRT_MAX) {
			return -1;
		}
		status = hostToIPAddr (hostName, &ina);
		if (status==0) {
			return initIPAddr (ina.s_addr, (unsigned short)port, pIP);
		}
	}

	/*
	 * none of the above - return indicating failure
	 */
	return -1;
}

/*
 * initIPAddr()
 */
LOCAL int initIPAddr (unsigned long ipAddr, unsigned short port, struct sockaddr_in *pIP)
{
	memset (pIP, '\0', sizeof(*pIP));
	pIP->sin_family = AF_INET;
	pIP->sin_port = htons(port);
	pIP->sin_addr.s_addr = htonl(ipAddr);
	return 0;
}

/*
 * addrArrayToUL()
 */
LOCAL int addrArrayToUL (const long *pAddr, unsigned nElements, unsigned long *pIpAddr)
{
	unsigned i;
	
	for (i=0u; i<nElements; i++) {
		if (pAddr[i]<0x0 || pAddr[i]>0xff) {
			return -1;
		}
	}
	*pIpAddr = (unsigned long) 
		(pAddr[3u] | (pAddr[2u]<<8u) | 
		(pAddr[1u]<<16u) | (pAddr[0u]<<24u));
	return 0;
}
