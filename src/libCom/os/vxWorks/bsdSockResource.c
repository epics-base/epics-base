
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
 * ipAddrToHostName
 */
epicsShareFunc unsigned epicsShareAPI ipAddrToHostName 
            (const struct in_addr *pAddr, char *pBuf, unsigned bufSize)
{
	int				status;
	int				errnoCopy = errno;
    unsigned        len;

	if (bufSize<1) {
		return 0;
	}

    if (bufSize>MAXHOSTNAMELEN) {
	    status = hostGetByAddr ((int)pAddr->s_addr, pBuf);
	    if (status==OK) {
            pBuf[MAXHOSTNAMELEN] = '\0';
            len = strlen (pBuf);
        }
        else {
            len = 0;
        }
    }
    else {
	    char name[MAXHOSTNAMELEN+1];
	    status = hostGetByAddr (pAddr->s_addr, name);
	    if (status==OK) {
            strncpy (pBuf, name, bufSize);
            pBuf[bufSize-1] = '\0';
            len = strlen (pBuf);
	    }
        else {
            len = 0;
        }
    }

	errno = errnoCopy;

    return len;
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
