
/*
 *	    $Id$
 *
 *      socket support library generic code
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
 *		Lawrence Berkley National Laboratory
 *
 *      Modification Log:
 *      -----------------
 */

#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#include <epicsAssert.h>
#include <bsdSocketResource.h>

#define nDigitsDottedIP 4u
#define maxChunkDigit 3u
#define maxDottedIPDigit ((nDigitsDottedIP-1) + nDigitsDottedIP*maxChunkDigit)
#define chunkSize 8u
#define maxPortDigits 5u

#define makeMask(NBITS) ((1u<<((unsigned)NBITS))-1u)

/*
 * ipAddrToA() 
 * (convert IP address to ASCII host name)
 */
void epicsShareAPI ipAddrToA 
			(const struct sockaddr_in *paddr, char *pBuf, unsigned bufSize)
{
    sockAddrToA ( (struct sockaddr *)paddr, pBuf, bufSize);
}


/*
 * sockAddrToA() 
 * (convert socket address to ASCII host name)
 */
void epicsShareAPI sockAddrToA 
			(const struct sockaddr *paddr, char *pBuf, unsigned bufSize)
{
	if (bufSize<1) {
		return;
	}

	if (paddr->sa_family!=AF_INET) {
		strncpy (pBuf, "<Ukn Addr Type>", bufSize-1);
		/*
		 * force null termination
		 */
		pBuf[bufSize-1] = '\0';
	}
	else {
        struct sockaddr_in *paddr_in = (struct sockaddr_in *) paddr;
        unsigned len;

		len = ipAddrToHostName (&paddr_in->sin_addr, pBuf, bufSize);
		if (len==0) {

            if (bufSize>maxDottedIPDigit) {
                unsigned chunk[nDigitsDottedIP];
                unsigned addr = ntohl (paddr_in->sin_addr.s_addr);
                unsigned i;

                for (i=0; i<nDigitsDottedIP; i++) {
                    chunk[i] = addr & makeMask(chunkSize);
                    addr >>= chunkSize;
                }

                /*
                 * inet_ntoa() isnt used because it isnt thread safe
                 * (and the replacements are not standardized)
                 */
                len = (unsigned) sprintf (pBuf, "%u.%u.%u.%u", 
                        chunk[3], chunk[2], chunk[1], chunk[0]);
            }
            else {
                strncpy (pBuf, "<IP addr>", bufSize);
		        pBuf[bufSize-1] = '\0';
                len = strlen (pBuf);
            }
		}

        assert (len<bufSize);
        bufSize -= len;

		/*
		 * allow space for the port number
		 */
		if (bufSize>maxPortDigits+1) {
			sprintf (&pBuf[len], ":%hu", ntohs(paddr_in->sin_port));
		}
	}
}
