/*
 *	$Id$
 *      Author: Jeffrey O. Hill, Chris Timossi
 *              hill@luke.lanl.gov
 *		CATimossi@lbl.gov
 *              (505) 665 1831
 *      Date:  9-93
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
 * $Log$
 * Revision 1.34  1998/03/24 20:55:06  jhill
 * fixed console title/correct repeater spawn/correct winsock II URL
 *
 * Revision 1.33  1998/03/12 20:39:11  jhill
 * fixed problem where 3.13.beta11 unable to connect to 3.11 with correct native type
 *
 * Revision 1.32  1998/02/27 01:05:04  jhill
 * integrated Timossi's win sock II changes
 *
 * Revision 1.1.1.3  1996/11/15  17:45:01  timossi
 * 	Interim release from jeff hill
 *
 * Revision 1.23  1996/11/02 00:51:12  jhill
 * many pc port, const in API, and other changes
 *
 * Revision 1.22  1996/09/16 16:40:13  jhill
 * make EPICS version be the console title
 *
 * Revision 1.21  1996/08/05 19:20:29  jhill
 * removed incorrect ver number
 *
 * Revision 1.20  1995/12/19  19:36:20  jhill
 * function prototype changes
 *
 * Revision 1.19  1995/11/29  19:15:42  jhill
 * added $Log$
 * added Revision 1.34  1998/03/24 20:55:06  jhill
 * added fixed console title/correct repeater spawn/correct winsock II URL
 * added
 * added Revision 1.33  1998/03/12 20:39:11  jhill
 * added fixed problem where 3.13.beta11 unable to connect to 3.11 with correct native type
 * added
 * added Revision 1.32  1998/02/27 01:05:04  jhill
 * added integrated Timossi's win sock II changes
 * added
 * Revision 1.1.1.3  1996/11/15  17:45:01  timossi
 * 	Interim release from jeff hill
 *
 * added Revision 1.23  1996/11/02 00:51:12  jhill
 * added many pc port, const in API, and other changes
 * added
 * added Revision 1.22  1996/09/16 16:40:13  jhill
 * added make EPICS version be the console title
 * added
 * added Revision 1.21  1996/08/05 19:20:29  jhill
 * added removed incorrect ver number
 * added
 * Revision 1.20  1995/12/19  19:36:20  jhill
 * function prototype changes
 * to the header
 *
 */

/*
 * Windows includes
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <mmsystem.h>

#include "epicsVersion.h"
#include "iocinf.h"

#ifndef _WIN32
#error This source is specific to WIN32
#endif

long offset_time;  /* time diff (sec) between 1970 and when windows started */
DWORD prev_time;
static WSADATA WsaData; /* version of winsock */

static void init_timers();


/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
    /**
	The multi-media timers used here should be good to a millisecond
	resolution. However, since the timer rolls back to 0 every 49.7
	days (2^32 ms, 4,294,967.296 sec), it's not very good for
	time stamping over long periods (if Windows is restarted more
	often than 49 days, it wont be a problem).  An attempt is made
	to keep the time returned increasing, but there is no guarantee
	the UTC time is right after 49 days.
	**/

	DWORD win_sys_time;	/* time (ms) since windows started */

	win_sys_time = timeGetTime();
	if (prev_time > win_sys_time)	{	/* must have been a timer roll-over */
		offset_time += 4294967;		/* add number of seconds in 49.7 days */
	}
	pt->tv_sec = (long)win_sys_time/1000 + offset_time;	/* time (sec) since 1970 */
	pt->tv_usec = (long)((win_sys_time % 1000) * 1000);
	prev_time = win_sys_time;
}


/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	cac_mux_io(pTV);
}


/*
 * os_specific_sg_io_complete()
 */
void os_specific_sg_io_complete(CASG *pcasg)
{
}


/*
 * does nothing but satisfy undefined
 */
void os_specific_sg_create(CASG *pcasg)
{
}
void os_specific_sg_delete(CASG *pcasg)
{
}


void cac_block_for_sg_completion(CASG *pcasg, struct timeval *pTV)
{
	cac_mux_io(pTV);
}


/*
 * cac_os_depen_init()
 */
int cac_os_depen_init(struct CA_STATIC *pcas)
{
    int status;

	ca_static = pcas;

	/* DllMain does most OS dependent init & cleanup */
	status = ca_os_independent_init ();
	if (status!=ECA_NORMAL) {
		return status;
	}

	/*
	 * select() under WIN32 gives us grief
	 * if we delay with out interest in at
 	 * least one fd (so we create the UDP
	 * fd during init before it is used for 
	 * the first time)
	 */
	cac_create_udp_fd();
	if(!ca_static->ca_piiuCast){
		return ECA_NOCAST;
	}
    return ECA_NORMAL;
}


/*
 * cac_os_depen_exit ()
 */
void cac_os_depen_exit (struct CA_STATIC *pcas)
{
	ca_static = pcas;
        ca_process_exit();
	ca_static = NULL;

	free ((char *)pcas);
}


/*
 *
 * This should work on any POSIX compliant OS
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
    	int     length;
    	char    *pName;
    	char    *pTmp;
	char    Uname[] = "";

    	pName = getenv("USERNAME");
	if (!pName) {
		pName = Uname;
	}
    	length = strlen(pName)+1;

	pTmp = malloc(length);
	if(!pTmp){
		return pTmp;
	}
	strncpy(pTmp, pName, length-1);
	pTmp[length-1] = '\0';

	return pTmp;
}



/*
 * ca_spawn_repeater()
 */
void ca_spawn_repeater()
{
	int status;
	char *pImageName;
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

	/*
 	 * running in the repeater process
	 * if here
	 */
	pImageName = "caRepeater.exe";

	//
	// This is not called if the repeater is known to be 
	// already running. (in the event of a race condition 
	// the 2nd repeater exits when unable to attach to the 
	// repeater's port)
	//
	GetStartupInfo (&startupInfo); 
	startupInfo.lpReserved = NULL;
	startupInfo.lpTitle = "EPICS CA Repeater";
	startupInfo.dwFlags = STARTF_USESHOWWINDOW;
	startupInfo.wShowWindow = SW_SHOWMINNOACTIVE;
	
	status =  CreateProcess( 
		pImageName, // pointer to name of executable module 
		NULL, // pointer to command line string (defaults to just the executable name above)
		NULL, // pointer to process security attributes 
		NULL, // pointer to thread security attributes 
		FALSE, // handle inheritance flag 
		CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, // creation flags 
		NULL, // pointer to new environment block (defaults to caller's environement)
		NULL, // pointer to current directory name  (defaults to caller's current directory)
		&startupInfo, // pointer to STARTUPINFO 
		&processInfo // pointer to PROCESS_INFORMATION 
	); 
	if (!status) {
		DWORD W32status;
		LPVOID errStrMsgBuf;
		LPVOID complteMsgBuf;

		W32status = FormatMessage( 
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			GetLastError (),
			MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
				(LPTSTR) &errStrMsgBuf,
			0,
			NULL 
		);

		if (W32status) {
			char *pFmtArgs[] = {
					"Failed to start the EPICS CA Repeater -",
					pImageName, 
					errStrMsgBuf,
					"Changes may be required in your \"path\" environement variable."};

			W32status = FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | 
					FORMAT_MESSAGE_ARGUMENT_ARRAY | 80,
				"%1 \"%2\". %3 %4",
				0,
				MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
					(LPTSTR) &complteMsgBuf,
				0,
				pFmtArgs 
			);
			if (W32status) {
				// Display the string.
				MessageBox (NULL, complteMsgBuf, "EPICS Channel Access Configuration Problem", 
					MB_OK|MB_ICONINFORMATION);
				LocalFree (complteMsgBuf);
			}
			else {
				// Display the string.
				MessageBox (NULL, errStrMsgBuf, "Failed to start EPICS caRepeater.exe", 
					MB_OK|MB_ICONINFORMATION);
			}

			// Free the buffer.
			LocalFree (errStrMsgBuf);
		}
		else {
			ca_printf ("!!WARNING!!\n");
			ca_printf ("Unable to locate the EPICS executable \"%s\".\n", pImageName);
			ca_printf ("You may need to modify your environment.\n");
		}
	}

	//
	// use of spawn here causes problems when the ca repeater
	// inheits open files (and sockets) from the spawning
	// process
	//
	//status = _spawnlp (_P_DETACH, pImageName, pImageName, NULL);
	//if (status<0) {
	//	ca_printf ("!!WARNING!!\n");
	//	ca_printf ("Unable to locate the EPICS executable \"%s\".\n",
	//		pImageName);
	//	ca_printf ("You may need to modify your environment.\n");
	//}
}


/*
 * caSetDefaultPrintfHandler ()
 * use the normal default here
 * ( see access.c )
 */
void caSetDefaultPrintfHandler ()
{
        ca_static->ca_printf_func = epicsVprintf;
}



/*
 *
 * Network interface routines
 *
 */

/*
 * local_addr()
 *
 * return 127.0.0.1
 * (the loop back address)
 */
int local_addr (SOCKET s, struct sockaddr_in *plcladdr)
{
	ca_uint32_t	loopBackAddress = 0x7f000001;

	plcladdr->sin_family = AF_INET;
	plcladdr->sin_port = 0;
	plcladdr->sin_addr.s_addr = htonl (loopBackAddress);
	return OK;
}


/*
 *  	caDiscoverInterfaces()
 *
 *      This routine is provided with the address of an ELLLIST, a socket
 *      a destination port number, and a match address. When the
 *      routine returns there will be one additional inet address
 *      (a caAddrNode) in the list for each inet interface found that
 *      is up and isnt a loop back interface (match addr is INADDR_ANY)
 *      or it matches the specified interface (match addr isnt INADDR_ANY).
 *      If the interface supports broadcast then I add its broadcast
 *      address to the list. If the interface is a point to
 *      point link then I add the destination address of the point to
 *      point link to the list. In either case I set the port number
 *      in the address node to the port supplied in the argument
 *      list.
 *
 * 		LOCK should be applied here for (pList)
 * 		(this is also called from the server)
 */
void epicsShareAPI caDiscoverInterfaces(ELLLIST *pList, SOCKET socket, 
			unsigned short port, struct in_addr matchAddr)
{
	struct sockaddr_in 	localAddr;
	struct sockaddr_in 	*pInetAddr;
	struct sockaddr_in 	*pInetNetMask;
	caAddrNode			*pNode;
	int             	status;
	LPINTERFACE_INFO    pIfinfo;
	LPINTERFACE_INFO    pIfinfoList;
	unsigned			nelem;
	int					numifs;
	DWORD				cbBytesReturned;
	char				wsaInBuffer[1024];

	/*
	 * use pool so that we avoid using to much stack space
	 * under vxWorks
	 *
	 * nelem is set to the maximum interfaces 
	 * on one machine here
	 */

	/* only valid for winsock 2 and above */
	if ( LOBYTE( WsaData.wVersion ) < 2 ) {
		fprintf(stderr, "Need to set EPICS_CA_AUTO_ADDR_LIST=NO for winsock 1\n");
		return;
	}

	nelem = 10;
	pIfinfoList = (LPINTERFACE_INFO)calloc(nelem, sizeof(INTERFACE_INFO));
	if(!pIfinfoList){
		return;
	}

	status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST, 
						wsaInBuffer, sizeof(wsaInBuffer),
						(LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
						&cbBytesReturned, NULL, NULL);

	if (status != 0 || cbBytesReturned == 0) {
		fprintf(stderr, "WSAIoctl failed %d\n",WSAGetLastError());
		free(pIfinfoList);		
		return;
	}

	numifs = cbBytesReturned/sizeof(INTERFACE_INFO);
	for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

		/*
		 * dont bother with interfaces that have been disabled
		 */
		if (!(pIfinfo->iiFlags & IFF_UP)) {
			continue;
		}

		/*
		 * dont use the loop back interface
		 */
		if (pIfinfo->iiFlags & IFF_LOOPBACK) {
			continue;
		}

		/*
		 * If its not an internet inteface 
		 * then dont use it. But for MS Winsock2
		 * assume 0 means internet.
		 */
		if (pIfinfo->iiAddress.sa_family != AF_INET) {
			if (pIfinfo->iiAddress.sa_family == 0)
				pIfinfo->iiAddress.sa_family = AF_INET;
			else
				continue;
		}

		/*
		 * save the interface's IP address
		 */
		pInetAddr = (struct sockaddr_in *)&pIfinfo->iiAddress;
		pInetNetMask = (struct sockaddr_in *)&pIfinfo->iiNetmask;
		localAddr = *pInetAddr;

		/*
		 * if it isnt a wildcarded interface then look for
		 * an exact match
		 */
		if (matchAddr.s_addr != INADDR_ANY) {
			if (pInetAddr->sin_addr.s_addr != matchAddr.s_addr) {
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
		if (pIfinfo->iiFlags & IFF_BROADCAST) {
			//pInetAddr = (struct sockaddr_in *)&pIfinfo->iiBroadcastAddress;
			pInetAddr->sin_addr.s_addr = 
				(localAddr.sin_addr.s_addr & pInetNetMask->sin_addr.s_addr) | ~pInetNetMask->sin_addr.s_addr;
		}
		else if(pIfinfo->iiFlags & IFF_POINTTOPOINT){
			//pInetAddr = (struct sockaddr_in *)&pIfinfo->iiBroadcastAddress;
		}
		else{
			continue;
		}

		pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
		if(!pNode){
			continue;
		}

		pNode->destAddr.in = *pInetAddr;
		pNode->destAddr.in.sin_port = htons(port);
		pNode->srcAddr.in = localAddr;
		

		/*
		 * LOCK applied externally
		 */
		ellAdd(pList, &pNode->node);
	}

	free(pIfinfoList);
}

BOOL epicsShareAPI DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	int status;
	TIMECAPS tc;
	static UINT     wTimerRes;

	switch (dwReason)  {

	case DLL_PROCESS_ATTACH:

#if _DEBUG			  /* for gui applications, setup console for error messages */
		if (AllocConsole())	{
			char title[256];
			DWORD titleLength = GetConsoleTitle(title, sizeof(title));
			if (titleLength) {
				titleLength = strlen (title);
				strncat (title, " " BASE_VERSION_STRING, sizeof(title));
			}
			else {
				strncpy(title, BASE_VERSION_STRING, sizeof(title));
			}
			title[sizeof(title)-1]= '\0';
			SetConsoleTitle(title);
    		freopen( "CONOUT$", "a", stderr );
		}
		fprintf(stderr, "Process attached to ca.dll version %s\n", EPICS_VERSION_STRING);
#endif	 			  /* init. winsock */
		if ((status = WSAStartup(MAKEWORD(/*major*/2,/*minor*/2), &WsaData)) != 0) {
			WSACleanup();
			fprintf(stderr,
	"Unable to attach to windows sockets version 2. error=%d\n", status);
			fprintf(stderr,
	"A Windows Sockets II update for windows 95 is available at\n");
			fprintf(stderr,
	"http://www.microsoft.com/windows95/info/ws2.htm");
			return FALSE;
		}
		
#if _DEBUG			  
		fprintf(stderr, "EPICS ca.dll attached to winsock version %s\n", WsaData.szDescription);
#endif	 	
					   /* setup multi-media timer */
		if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
			fprintf(stderr,"cant get timer info \n");
			return FALSE;
		}
					  /* set for 1 ms resoulution */
		wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);
		status = timeBeginPeriod(wTimerRes);
		if (status != TIMERR_NOERROR)
			fprintf(stderr,"timer setup failed\n");
		offset_time = (long)time(NULL) - (long)timeGetTime()/1000;
		prev_time =  timeGetTime();

		break;

	case DLL_PROCESS_DETACH:

		timeEndPeriod(wTimerRes);

		if ((status = WSACleanup()) !=0)
			return FALSE;
		break;

	case DLL_THREAD_ATTACH:
#if _DEBUG
		fprintf(stderr, "Thread attached to ca.dll\n");
#endif
		break;

	case DLL_THREAD_DETACH:
#if _DEBUG
		fprintf(stderr, "Thread detached from ca.dll\n");
#endif
		break;

	default:
		break;
	}

return TRUE;


}



