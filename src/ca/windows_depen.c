/*
 *	$Id$
 *      Author: Jeffrey O. Hill, Chris Timossi
 *              hill@luke.lanl.gov (505) 665 1831
 *              CATimossi@lbl.gov
 *             
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
 */

#include <math.h>

#ifndef _WIN32
#error This source is specific to WIN32
#endif

/*
 * Windows includes
 */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>

#include "epicsVersion.h"
#include "bsdSocketResource.h"
#include "iocinf.h"

static long offset_time;  /* time diff (sec) between 1970 and when windows started */
static LARGE_INTEGER time_prev, time_freq;

/*
 * cac_gettimeval
 */
void cac_gettimeval(struct timeval  *pt)
{
	LARGE_INTEGER time_cur, time_sec, time_remainder;

	/*
	 * dont need to check status since it was checked once
	 * during initialization to see if the CPU has HR
	 * counters (Intel and Mips processors do)
	 */
	QueryPerformanceCounter (&time_cur);
	if (time_prev.QuadPart > time_cur.QuadPart)	{	/* must have been a timer roll-over */
		double offset;
		/*
		 * must have been a timer roll-over
		 * It takes 9.223372036855e+18/time_freq sec
		 * to roll over this counter (time_freq is 1193182
		 * sec on my system). This is currently about 245118 years.
		 *
		 * attempt to add number of seconds in a 64 bit integer
		 * in case the timer resolution improves
		 */
		offset = pow(2.0, 63.0)-1.0/time_freq.QuadPart;
		if (offset<=LONG_MAX) {
			offset_time += (long) offset;
		}
		else {
			/*
			 * this problem cant be fixed, but hopefully will never occurr
			 */
			fprintf (stderr, "%s.%d Timer overflowed\n", __FILE__, __LINE__);
		}
	}
	time_sec.QuadPart = time_cur.QuadPart / time_freq.QuadPart;
	time_remainder.QuadPart = time_cur.QuadPart % time_freq.QuadPart;
	if (time_sec.QuadPart > LONG_MAX-offset_time) {
		/*
		 * this problem cant be fixed, but hopefully will never occurr
		 */
		fprintf (stderr, "%s.%d Timer value larger than storage\n", __FILE__, __LINE__);
		pt->tv_sec = 0;
		pt->tv_usec = 0;
	}
	else {
		/* add time (sec) since 1970 */
		pt->tv_sec = offset_time + (long)time_sec.QuadPart;	
		pt->tv_usec = (long)((time_remainder.QuadPart*1000000)/time_freq.QuadPart);
	}
	time_prev = time_cur;
}

/*
 * cac_block_for_io_completion()
 */
void cac_block_for_io_completion(struct timeval *pTV)
{
	cac_mux_io(pTV, TRUE);
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
	cac_mux_io(pTV, TRUE);
}

/*
 *	ca_task_initialize()
 */
int epicsShareAPI ca_task_initialize(void)
{
	int status;

	if (ca_static) {
		return ECA_NORMAL;
	}

	ca_static = (struct CA_STATIC *) 
		calloc(1, sizeof(*ca_static));
	if (!ca_static) {
		return ECA_ALLOCMEM;
	}

	/*
	 * initialize elapsed time counters
	 *
	 * All CPUs running win32 currently have HR
	 * counters (Intel and Mips processors do)
	 */
	if (QueryPerformanceCounter (&time_prev)==0) {
		return ECA_INTERNAL;
	}
	if (QueryPerformanceFrequency (&time_freq)==0) {
		return ECA_INTERNAL;
	}
	offset_time = (long)time(NULL) - (long)(time_prev.QuadPart/time_freq.QuadPart);

	/*
	 * this code moved here from dllMain() so that the code will also run
	 * in object libraries
	 */
	if (!bsdSockAttach()) {
		free (ca_static);
		ca_static = NULL;
		return ECA_INTERNAL;
	}

	status = ca_os_independent_init ();
	if (status != ECA_NORMAL) {
		bsdSockRelease ();
		free (ca_static);
		ca_static = NULL;
		return status;
	}

    return ECA_NORMAL;
}

/*
 * ca_task_exit ()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
 */
int epicsShareAPI ca_task_exit (void)
{
	if (!ca_static) {
		return ECA_NORMAL;
	}
	ca_process_exit();
	free ((char *)ca_static);
	ca_static = NULL;

	/*
	 * this code moved here from dllMain() so that the code will also run
	 * in object libraries
	 */
	bsdSockRelease ();

	return ECA_NORMAL;
}

/*
 *
 * obtain the local user name
 *
 * o Indicates failure by setting ptr to nill
 */
char *localUserName()
{
	TCHAR tmpStr[256];
	DWORD bufsize = sizeof(tmpStr);
	char *pTmp;

	if (!GetUserName(tmpStr, &bufsize)) {
		tmpStr[0] = '\0';
		bufsize = 1;
	}

	pTmp = malloc (bufsize);
	if (pTmp!=NULL) {
		strncpy(pTmp, tmpStr, bufsize-1);
		pTmp[bufsize-1] = '\0';
	}
	return pTmp;
}

/*
 * ca_spawn_repeater()
 */
void ca_spawn_repeater()
{
	BOOL status;
	char *pImageName = "caRepeater.exe";
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInfo;

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
		NULL, // pointer to name of executable module (not required if command line is specified)
		pImageName, // pointer to command line string 
		NULL, // pointer to process security attributes 
		NULL, // pointer to thread security attributes 
		FALSE, // handle inheritance flag 
		CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS, // creation flags 
		NULL, // pointer to new environment block (defaults to caller's environement)
		NULL, // pointer to current directory name  (defaults to caller's current directory)
		&startupInfo, // pointer to STARTUPINFO 
		&processInfo // pointer to PROCESS_INFORMATION 
	); 
	if (status==0) {
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
					"Changes may be required in your \"path\" environment variable.",
					"PATH = ",
					getenv ("path")};

			if (pFmtArgs[5]==NULL) {
				pFmtArgs[5] = "<empty string>";
			}

			W32status = FormatMessage( 
				FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_STRING | 
					FORMAT_MESSAGE_ARGUMENT_ARRAY | 80,
				"%1 \"%2\". %3 %4 %5 \"%6\"",
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
 * A valid non-loopback local address is required in the
 * beacon message in certain situations where
 * there are beacon repeaters and there are addresses
 * in the EPICS_CA_ADDRESS_LIST for which we dont have
 * a strictly correct local server address on a multi-interface
 * system. In this situation we use the first valid non-loopback local
 * address found in the beacon message.
 */
int local_addr (SOCKET socket, struct sockaddr_in *plcladdr)
{
	int             	status;
	INTERFACE_INFO		*pIfinfo;
	INTERFACE_INFO      *pIfinfoList;
	struct sockaddr_in 	*pInetAddr;
	unsigned			nelem;
	DWORD				numifs;
	DWORD				cbBytesReturned;
	static struct sockaddr_in addr;
	static char     	init = FALSE;

	if (init) {
		*plcladdr = addr;
		return 0;
	}

	/*
	 * nelem is set to the maximum interfaces 
	 * on one machine here
	 */

	/* 
	 * only valid for winsock 2 and above 
	 */
	if (wsaMajorVersion() < 2 ) {
		return -1;
	}

	nelem = 10;
	pIfinfoList = (INTERFACE_INFO *) calloc(nelem, sizeof(INTERFACE_INFO));
	if(!pIfinfoList){
		return -1;
	}

	status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST, 
						NULL, 0,
						(LPVOID)pIfinfoList, nelem*sizeof(INTERFACE_INFO),
						&cbBytesReturned, NULL, NULL);

	if (status != 0 || cbBytesReturned == 0) {
		fprintf(stderr, "WSAIoctl failed %d\n",WSAGetLastError());
		free(pIfinfoList);		
		return -1;
	}

	numifs = cbBytesReturned/sizeof(INTERFACE_INFO);
	for (pIfinfo = pIfinfoList; pIfinfo < (pIfinfoList+numifs); pIfinfo++){

		/*
		 * dont use interfaces that have been disabled
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

		pInetAddr = (struct sockaddr_in *) &pIfinfo->iiAddress;

		/*
		 * If its not an internet inteface 
		 * then dont use it. But for MS Winsock2
		 * assume 0 means internet.
		 */
		if (pInetAddr->sin_family != AF_INET) {
			if (pInetAddr->sin_family == 0) {
				pInetAddr->sin_family = AF_INET;
			}
			else {
				continue;
			}
		}

		/*
		 * save the interface's IP address
		 */
		addr = *pInetAddr;

		*plcladdr = addr;
		init = TRUE;
		free (pIfinfoList);
		return 0;

	}

	free (pIfinfoList);
	return -1;
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
	struct sockaddr_in 	*pInetAddr;
	struct sockaddr_in 	*pInetNetMask;
	caAddrNode			*pNode;
	int             	status;
	INTERFACE_INFO      *pIfinfo;
	INTERFACE_INFO      *pIfinfoList;
	unsigned			nelem;
	int					numifs;
	DWORD				cbBytesReturned;

	/*
	 *
	 * nelem is set to the maximum interfaces 
	 * on one machine here
	 */

	/* only valid for winsock 2 and above */
	if (wsaMajorVersion() < 2 ) {
		fprintf(stderr, "Need to set EPICS_CA_AUTO_ADDR_LIST=NO for winsock 1\n");
		return;
	}

	nelem = 10;
	pIfinfoList = (INTERFACE_INFO *) calloc(nelem, sizeof(INTERFACE_INFO));
	if(!pIfinfoList){
		return;
	}

	status = WSAIoctl (socket, SIO_GET_INTERFACE_LIST, 
						NULL, 0,
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

		pInetAddr = (struct sockaddr_in *) &pIfinfo->iiAddress;
		pInetNetMask = (struct sockaddr_in *) &pIfinfo->iiNetmask;

		/*
		 * If its not an internet inteface 
		 * then dont use it. But for MS Winsock2
		 * assume 0 means internet.
		 */
		if (pInetAddr->sin_family != AF_INET) {
			if (pInetAddr->sin_family == 0) {
				pInetAddr->sin_family = AF_INET;
			}
			else {
				continue;
			}
		}

		/*
		 * if it isnt a wildcarded interface then look for
		 * an exact match
		 */
		if (matchAddr.s_addr != htonl(INADDR_ANY)) {
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
				(pInetAddr->sin_addr.s_addr & pInetNetMask->sin_addr.s_addr) | ~pInetNetMask->sin_addr.s_addr;
		}
		else if(pIfinfo->iiFlags & IFF_POINTTOPOINT){
			//pInetAddr = (struct sockaddr_in *)&pIfinfo->iiBroadcastAddress;
		}
		else{
			continue;
		}

		pNode = (caAddrNode *) calloc(1,sizeof(*pNode));
		if (!pNode) {
			continue;
		}

		pNode->destAddr.in = *pInetAddr;
		pNode->destAddr.in.sin_port = htons(port);

		/*
		 * LOCK applied externally
		 */
		ellAdd (pList, &pNode->node);
	}

	free(pIfinfoList);
}

#if !defined(EPICS_DLL_NO)

/*
 * most of the code here was moved to ca_task_initialize and ca_task_exit()
 * so that the code will also run in object libraries
 */
BOOL epicsShareAPI DllMain (HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)  {

	case DLL_PROCESS_ATTACH:
#		if defined(_DEBUG) && 0
		{
			char name[256];
			char mname[80];
			DWORD nchars;

			nchars = GetModuleFileName (hModule, mname, 80);
			if (!nchars) {
				strcpy (mname,"Unknown");
			}
			sprintf(name,"Process Attach\n\nBuild Date: %s\nBuild Time: %s\n"
			  "Module Name: %s", __DATE__, __TIME__, mname);
			MessageBox (NULL, name, "CA.DLL Version", MB_OK);
		}
#		endif	

#		ifdef _DEBUG
			fprintf(stderr, "Process attached to ca.dll version %s\n", EPICS_VERSION_STRING);
#		endif
		break;

	case DLL_PROCESS_DETACH:

#		ifdef _DEBUG
			fprintf(stderr, "Process detached from ca.dll version %s\n", EPICS_VERSION_STRING);
#		endif
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

#endif