/* if_depen.c */
/* share/src/ca/$Id$ */

/*
 *      Author:	Jeff Hill 
 *      Date:  	04-05-94	
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
 * Modification Log:
 * -----------------
 *      8/87            Jeff Hill       Init Release                    
 *      072792          Jeff Hill       better messages                
 *      09-DEC-1992     Gerhard Grygiel (GeG) support  VMS/UCX        
 *      050593          Jeff Hill       now checks all N interfaces  
 *                                      (and not N-1 interfaces)    
 */


static char	*sccsId = "@(#) $Id$";

#include "iocinf.h"

#ifdef DEBUG
#define ifDepenDebufPrintf(argsInParen) printf argsInParen
#else
#define ifDepenDebufPrintf(argsInParen)
#endif

/*
 * Dont use ca_static based lock macros here because this is
 * also called by the server. All locks required are applied at
 * a higher level.
 */

/*
 * local_addr()
 * 
 * A valid non-loopback local address is required in the
 * beacon message in certain situations where
 * there are beacon repeaters and there are addresses
 * in the EPICS_CA_ADDRESS_LIST for which we dont have
 * a strictly correct local server address on a multi-interface
 * system. In this situation we use the first valid non-loopback local
 * address found in the beacon message.
 */
int local_addr(int s, struct sockaddr_in *plcladdr)
{
	int             	status;
	struct ifconf   	ifconf;
	struct ifreq    	ifreq[25];
	struct ifreq    	*pifreq;
	static struct sockaddr_in addr;
	static char     	init = FALSE;
	struct sockaddr_in 	*tmpaddr;

	if (init){
		*plcladdr = addr;
		return OK;
	}

	/* 
	 *	get the addr of the first interface found 
	 * 	(report inconsistent interfaces however)
	 */
	ifconf.ifc_len = sizeof ifreq;
	ifconf.ifc_req = ifreq;
	status = socket_ioctl(s, SIOCGIFCONF, &ifconf);
	if (status < 0 || ifconf.ifc_len == 0) {
		ca_printf(
			"CAC: SIOCGIFCONF ioctl failed because \"%s\"\n", 
			SOCKERRSTR(SOCKERRNO));
		ifconf.ifc_len = 0;
	}

	ifDepenDebufPrintf ( ("local_addr: %ld net intf(s) found\n", 
        (unsigned long) (ifconf.ifc_len/sizeof(*pifreq))) );

	for (	pifreq = ifconf.ifc_req;
	    	((size_t)ifconf.ifc_len) >= sizeof(*pifreq);
	     	pifreq++, ifconf.ifc_len -= sizeof(*pifreq)) {

		status = socket_ioctl(s, SIOCGIFFLAGS, pifreq);
		if (status == ERROR){
			ca_printf("local_addr: net intf flags fetch for %s failed\n", pifreq->ifr_name);
			continue;
		}

		if (!(pifreq->ifr_flags & IFF_UP)) {
			ifDepenDebufPrintf ( ("local_addr: net intf %s was down\n", pifreq->ifr_name) );
			continue;
		}
		
		/*
		 * dont use the loop back interface
		 */
		if (pifreq->ifr_flags & IFF_LOOPBACK) {
			ifDepenDebufPrintf ( ("local_addr: ignoring loopback interface: %s\n", pifreq->ifr_name) );
			continue;
		}

		status = socket_ioctl(s, SIOCGIFADDR, pifreq);
		if (status == ERROR){
			ifDepenDebufPrintf ( ("local_addr: could not obtain addr for %s\n", pifreq->ifr_name) );
			continue;
		}

		if (pifreq->ifr_addr.sa_family != AF_INET){
			ifDepenDebufPrintf ( ("local_addr: interface %s was not AF_INET\n", pifreq->ifr_name) );
			continue;
		}

		ifDepenDebufPrintf ( ("local_addr: net intf %s found\n", pifreq->ifr_name) );

		tmpaddr = (struct sockaddr_in *) &pifreq->ifr_addr;

		init = TRUE;
		addr = *tmpaddr;
		break;
	}

	if(!init){
		return ERROR;
	}

	*plcladdr = addr;
	return OK;
}

/*
 *  	caDiscoverInterfaces()
 *
 *	This routine is provided with the address of an ELLLIST, a socket
 * 	a destination port number, and a match address. When the 
 * 	routine returns there will be one additional inet address 
 *	(a caAddrNode) in the list for each inet interface found that 
 *	is up and isnt a loop back interface (match addr is INADDR_ANY)
 *	or it matches the specified interface (match addr isnt INADDR_ANY). 
 *	If the interface supports broadcast then I add its broadcast 
 *	address to the list. If the interface is a point to 
 *	point link then I add the destination address of the point to
 *	point link to the list. In either case I set the port number
 *	in the address node to the port supplied in the argument
 *	list.
 *
 * 	LOCK should be applied here for (pList)
 * 	(this is also called from the server)
 */
void epicsShareAPI caDiscoverInterfaces
	(ELLLIST *pList, int socket, unsigned short port, struct in_addr matchAddr)
{
    struct sockaddr_in  *pInetAddr;
    caAddrNode          *pNode;
    int                 status;
    struct ifconf       ifconf;
    struct ifreq        *pIfreqList;
    struct ifreq        *pifreq;
    unsigned long       nelem;

	/*
	 * use pool so that we avoid using to much stack space
	 * under vxWorks
	 *
	 * nelem is set to the maximum interfaces 
	 * on one machine here
	 */
	nelem = 100;
	pIfreqList = (struct ifreq *)calloc(nelem, sizeof(*pifreq));
	if(!pIfreqList){
		return;
	}

	ifconf.ifc_len = nelem*sizeof(*pifreq);
	ifconf.ifc_req = pIfreqList;
	status = socket_ioctl(socket, SIOCGIFCONF, &ifconf);
	if (status < 0 || ifconf.ifc_len == 0) {
		free(pIfreqList);
		return;
	}

	nelem = ifconf.ifc_len/sizeof(struct ifreq);
	ifDepenDebufPrintf ( ("caDiscoverInterfaces: %ld net intf(s) found\n", nelem) );

	for (pifreq = pIfreqList; pifreq<(pIfreqList+nelem); pifreq++){
		status = socket_ioctl(socket, SIOCGIFFLAGS, pifreq);
		if (status) {
            ca_printf ("caDiscoverInterfaces: net intf flags fetch for %s failed\n", pifreq->ifr_name);
			continue;
		}

		/*
		 * dont bother with interfaces that have been disabled
		 */
		if (!(pifreq->ifr_flags & IFF_UP)) {
			ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s was down\n", pifreq->ifr_name) );
			continue;
		}

		/*
		 * dont use the loop back interface
		 */
		if (pifreq->ifr_flags & IFF_LOOPBACK) {
			ifDepenDebufPrintf ( ("caDiscoverInterfaces: ignoring loopback interface: %s\n", pifreq->ifr_name) );
			continue;
		}

		/*
		 * Fetch the local address for this interface
		 */
		status = socket_ioctl(socket, SIOCGIFADDR, pifreq);
		if (status){
			ifDepenDebufPrintf ( ("caDiscoverInterfaces: could not obtain addr for %s\n", pifreq->ifr_name) );
			continue;
		}

		/*
		 * If its not an internet inteface 
		 * then dont use it.
		 */
		if (pifreq->ifr_addr.sa_family != AF_INET) {
			ifDepenDebufPrintf ( ("caDiscoverInterfaces: interface %s was not AF_INET\n", pifreq->ifr_name) );
			continue;
		}

		/*
		 * save the interface's IP address
		 */
		pInetAddr = (struct sockaddr_in *)&pifreq->ifr_addr;

		/*
		 * if it isnt a wildcarded interface then look for
		 * an exact match
		 */
		if (matchAddr.s_addr != htonl(INADDR_ANY)) {
			if (pInetAddr->sin_addr.s_addr != matchAddr.s_addr) {
                ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s didnt match\n", pifreq->ifr_name) );
				continue;
			}
		}

		/*
		 * If this is an interface that supports
		 * broadcast fetch the broadcast address.
		 *
		 * Otherwise if this is a point to point 
		 * interface then use the destination address.
		 *
		 * Otherwise CA will not query through the 
		 * interface.
		 */
		if (pifreq->ifr_flags & IFF_BROADCAST) {
			status = socket_ioctl(
					socket, 
					SIOCGIFBRDADDR, 
					pifreq);
			if (status) {
                ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s: bcast addr fetch fail\n", pifreq->ifr_name) );
				continue;
			}
		}
		else if(pifreq->ifr_flags & IFF_POINTOPOINT){
			status = socket_ioctl(
					socket, 
					SIOCGIFDSTADDR, 
					pifreq);
			if (status){
                ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s: pt to pt addr fetch fail\n", pifreq->ifr_name) );
				continue;
			}
		}
		else{
            ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s: not pt to pt or bcast\n", pifreq->ifr_name) );
			continue;
		}

        ifDepenDebufPrintf ( ("caDiscoverInterfaces: net intf %s found\n", pifreq->ifr_name) );

		pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
		if(!pNode){
            ca_printf ("caDiscoverInterfaces: malloc failed for net intf %s: \n", pifreq->ifr_name);
			continue;
		}

		pNode->destAddr.in = *pInetAddr;
		pNode->destAddr.in.sin_port = htons(port);

		/*
		 * LOCK applied externally
		 */
		ellAdd(pList, &pNode->node);
	}

	free(pIfreqList);
}

