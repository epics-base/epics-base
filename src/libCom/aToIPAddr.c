/*
 * rational replacement for inet_addr()
 * 
 * author: Jeff Hill
 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

#include "osiSock.h"
#define epicsExportSharedSymbols
#include "ipAddrToA.h"

#ifndef NELEMENTS
#define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif /*NELEMENTS*/

static int addrArrayToUL (const long *pAddr, unsigned nElements, unsigned long *pIpAddr);
static int initIPAddr (unsigned long ipAddr, unsigned short port, struct sockaddr_in *pIP);

/*
 * rational replacement for inet_addr()
 * which allows the limited broadcast address
 * 255.255.255.255 and also allows the user
 * to specify a port number
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
	int port;

	unsigned long ipAddr;

	status = sscanf (pAddrString, "%li.%li.%li.%li:%i", 
			addr, addr+1u, addr+2u, addr+3u, &port);
	if (status==5) {
		status = addrArrayToUL (addr, NELEMENTS(addr), &ipAddr);
		if (status<0) {
			return -1;
		}
		if (port<0 || port>USHRT_MAX) {
			return -1;
		}
		return initIPAddr (ipAddr, (unsigned short) port, pIP);
	}

	status = sscanf (pAddrString, "%li.%li.%li.%li", 
		addr, addr+1u, addr+2u, addr+3u);
	if (status==4) {
		status = addrArrayToUL (addr, NELEMENTS(addr), &ipAddr);
		if (status<0) {
			return -1;
		}
		return initIPAddr (ipAddr, defaultPort, pIP);
	}
	
	status = sscanf (pAddrString, "%li", addr);
	if (status==1) {
		if (*addr<0x0 && *addr>0xffffffff) {
			return -1;
		}
		return initIPAddr ((unsigned long)*addr, defaultPort, pIP);
	}

	return -1;
}

/*
 * initIPAddr()
 */
static int initIPAddr (unsigned long ipAddr, unsigned short port, struct sockaddr_in *pIP)
{
	if (port<=IPPORT_USERRESERVED) {
		return -1;
	}

	memset (pIP, '\0', sizeof(*pIP));
	pIP->sin_family = AF_INET;
	pIP->sin_port = htons(port);
	pIP->sin_addr.s_addr = htonl(ipAddr);
	return 0;
}

/*
 * addrArrayToUL()
 */
static int addrArrayToUL (const long *pAddr, unsigned nElements, unsigned long *pIpAddr)
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
