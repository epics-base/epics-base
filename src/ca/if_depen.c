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


/*
 * local_addr()
 * 
 * Perhaps it is sufficient for this to return 127.0.0.1
 * (the loop back address)
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
		ca_printf("CAC: ioctl failed %s\n", strerror(MYERRNO));
		ifconf.ifc_len = 0;
	}

#ifdef DEBUG
	ca_printf("CAC: %d if fnd\n", ifconf.ifc_len/sizeof(*pifreq));
#endif

	for (	pifreq = ifconf.ifc_req;
	    	ifconf.ifc_len >= sizeof(*pifreq);
	     	pifreq++, ifconf.ifc_len -= sizeof(*pifreq)) {

		status = socket_ioctl(s, SIOCGIFFLAGS, pifreq);
		if (status == ERROR){
			ca_printf("CAC: could not obtain if flags\n");
			continue;
		}

#ifdef DEBUG
		ca_printf("CAC: if fnd %s\n", pifreq->ifr_name);
#endif

		if (!(pifreq->ifr_flags & IFF_UP)) {
#ifdef DEBUG
			ca_printf("CAC: if was down\n");
#endif
			continue;
		}

		/*
		 * o Dont require broadcast capabilities.
		 * o Loopback addresss is ok - preferable?
		 */	
		status = socket_ioctl(s, SIOCGIFADDR, pifreq);
		if (status == ERROR){
#ifdef DEBUG
			ca_printf("CAC: could not obtain addr\n");
#endif
			continue;
		}

		if (pifreq->ifr_addr.sa_family != AF_INET){
#ifdef DEBUG
			ca_printf("CAC: interface was not AF_INET\n");
#endif
			continue;
		}

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
 * caDiscoverInterfaces()
 *
 * Load the list with the broadcast address for all
 * interfaces found that support broadcast.
 */
void caDiscoverInterfaces(ELLLIST *pList, int socket, int port)
{
	struct sockaddr_in 	localAddr;
	struct sockaddr_in 	*pInetAddr;
	caAddrNode		*pNode;
	int             	status;
	struct ifconf   	ifconf;
	struct ifreq    	*pIfreqList;
	struct ifreq   		*pifreq;
	unsigned		nelem;

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
	for (pifreq = pIfreqList; pifreq<(pIfreqList+nelem); pifreq++){
		status = socket_ioctl(socket, SIOCGIFFLAGS, pifreq);
		if (status){
			continue;
		}

		/*
		 * dont bother with interfaces that have been disabled
		 */
		if (!(pifreq->ifr_flags & IFF_UP)) {
			continue;
		}

		/*
		 * dont use the loop back interface
		 */
		if (pifreq->ifr_flags & IFF_LOOPBACK) {
			continue;
		}

		/*
		 * Fetch the local address for this interface
		 */
		status = socket_ioctl(socket, SIOCGIFADDR, pifreq);
		if (status){
			continue;
		}
		/*
		 * If its not an internet inteface 
		 * then dont use it.
		 */
		if (pifreq->ifr_addr.sa_family != AF_INET) {
			continue;
		}
		pInetAddr = (struct sockaddr_in *)&pifreq->ifr_addr;
		localAddr = *pInetAddr;

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
			if (status){
				continue;
			}
		}
		else if(pifreq->ifr_flags & IFF_POINTOPOINT){
			status = socket_ioctl(
					socket, 
					SIOCGIFDSTADDR, 
					pifreq);
			if (status){
				continue;
			}
		}
		else{
			continue;
		}




		pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
		if(!pNode){
			continue;
		}

		pNode->destAddr.inetAddr = *pInetAddr;
		pNode->destAddr.inetAddr.sin_port = htons(port);
		pNode->srcAddr.inetAddr = localAddr;

		LOCK;
		ellAdd(pList, &pNode->node);
		UNLOCK;

	}

	free(pIfreqList);
}


