
#include <string.h>
#include <stdio.h>

#ifndef vxWorks
#error this is a vxWorks specific source code
#endif

#include <hostLib.h>
#include <inetLib.h>

#define epicsExportSharedSymbols
#include "bsdSocketResource.h"

int bsdSockAttach()
{
	return 1;
}

void bsdSockRelease()
{
}


/*
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
void epicsShareAPI ipAddrToA 
			(const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize)
{
	const int		maxPortDigits = 5u;
	char			name[max(INET_ADDR_LEN,MAXHOSTNAMELEN)+1];
	int				status;
	int				errnoCopy = errno;

	if (bufSize<1) {
		return;
	}

	if (paddr->sin_family!=AF_INET) {
		strncpy(pBuf, "<Host with Unknown Address Type>", bufSize-1);
		/*
		 * force null termination
		 */
		pBuf[bufSize-1] = '\0';
	}
	else {
		status = hostGetByAddr ((int)paddr->sin_addr.s_addr, name);
		if (status!=OK) {
			inet_ntoa_b (paddr->sin_addr, name);
		}

		/*
		 * allow space for the port number
		 */
		if (bufSize>maxPortDigits+strlen(name)) {
			sprintf (pBuf, "%.*s:%u", ((int)bufSize)-maxPortDigits-1, 
				name, ntohs(paddr->sin_port));
		}
		else {
			sprintf (pBuf, "%.*s", ((int)bufSize)-1, name);
		}
	}

	errno = errnoCopy;
}

/*
 * hostToIPAddr ()
 */
epicsShareFunc int epicsShareAPI hostToIPAddr 
				(const char *pHostName, struct in_addr *pIPA)
{
	int addr;

	addr = hostGetByName ((char *)pHostName);
	if (addr==ERROR) {
		/*
		 * return indicating an error
		 */
		return -1;
	}

	pIPA->s_addr = (unsigned long) addr;

	/*
	 * success
	 */
	return 0;
}