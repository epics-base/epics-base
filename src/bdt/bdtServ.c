/*
*
* Author:		John Winans
* Date:			95-05-22
*
* $Log$
*/

/*****************************************************************************
*
* IOC listener task blocks on accept calls waiting for binders.
*	If a bind arrives, a receiver task is spawned.
*
* IOC receiver task blocks on read calls waiting for transactions.
*	When a transaction arrives it is serviced.
*	At the end of a transaction service, a response is sent back.
*	After the response is sent, a chack is made to see if a delta transmission
*	was blocked by the transaction's use of the socket... if so, it is sent.
*
******************************************************************************/

#include <selectLib.h>
#include <stdio.h>
#include <signal.h>
#include <vxWorks.h>
#include <sys/socket.h>
#include <in.h>
#include <inetLib.h>
#include <taskLib.h>
#include <sysSymTbl.h>
#include <string.h>

#include "bdt.h"

#define BDT_TASK_PRIO		200
#define BDT_TASK_OPTIONS	VX_FP_TASK
#define BDT_TASK_STACK		5000
#define STATIC				static

/* used for debugging */
STATIC char *BdtNames[] = { 
    "BDT_Ok",
    "BDT_Connect",
    "BDT_Error",
    "BDT_Get",
    "BDT_Put",
    "BDT_Close",
    "BDT_Monitor",
    "BDT_Value",
    "BDT_Delta",
    "BDT_Add",
    "BDT_Delete",
    "BDT_Ping"
};

STATIC int HexDump(char *ReadBuffer, int rbytes);

/*****************************************************************************
*
* A debugging routine that hex-dumps a message to the console.
*
*****************************************************************************/
void BDT_DumpMessage(BDT *Bdt)
{
	char	Buf[16*4];
	int		RecvLen;

	while(Bdt->remaining_recv)
	{
		RecvLen = (Bdt->remaining_recv > sizeof(Buf)) ? sizeof(Buf): Bdt->remaining_recv;
		if (BdtReceiveData(Bdt, Buf, RecvLen) != RecvLen)
			return;				/* Got EOF, (EOM handled by the while() */

		HexDump(Buf, RecvLen);
	}
}
/*****************************************************************************
*
* Throw away a message.
*
****************************************************************************/
void BDT_DiscardMessage(BDT *Bdt)
{
	char	Buf[16*4];
	int		RecvLen;

	while(Bdt->remaining_recv)
	{
		RecvLen = (Bdt->remaining_recv > sizeof(Buf)) ? sizeof(Buf): Bdt->remaining_recv;
		if (BdtReceiveData(Bdt, Buf, RecvLen) != RecvLen)
			return;				/* Got EOF, (EOM handled by the while() */
	}
}

/*****************************************************************************
*
* Process a single Connect message.  And return a response.
*
******************************************************************************/
STATIC int BDT_ProcessConnect(BDT *Bdt)
{
	SYM_TYPE			Type;
	unsigned char		length;
	char				Buf[50];
	char				HandlerName[70];

	if (Bdt->remaining_recv > sizeof(Buf))
	{
		printf("BDT_ProcessConnect Connect Message too long %d\n", Bdt->remaining_recv);
		BDT_DiscardMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	if (Bdt->remaining_recv < 2)
	{
		printf("BDT_ProcessConnect Connect Message w/missing service name\n");
		BDT_DiscardMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	BdtReceiveData(Bdt, &length, 1);
	if (length > sizeof(Buf))
	{
		printf("BDT_ProcessConnect Connect Message service name too long %d\n", length);
		BDT_DiscardMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}
	BdtReceiveData(Bdt, Buf, length);
	Buf[length] = '\0';

	sprintf(HandlerName, "_BDT_ServiceHandler_%s", Buf);
	printf("BDT_ProcessConnect NAME service (%s)\n", HandlerName);

	/*Bdt->pHandlers = (BdthandlerFunc *)(&BDT_NameServicehandlers);*/
	if (symFindByName(sysSymTbl, HandlerName, (char **)&(Bdt->pHandlers), &Type) != OK)
	{
		printf("BDT_ProcessConnect Connect to unknown service (%s)\n", Buf);
		BdtSendHeader(Bdt, BDT_Error, 0);
	}
	else
	{
		Bdt->Name = (char *)malloc(strlen(Buf)+1);
		strcpy(Bdt->Name, Buf);
		if (Bdt->pHandlers[BDT_Connect] != NULL)
			return((*(Bdt->pHandlers[BDT_Connect]))(Bdt));
		else
		{
			BDT_DiscardMessage(Bdt);
			BdtSendHeader(Bdt, BDT_Ok, 0);
		}
	}
	return(0);
}
/*****************************************************************************
*
* Process a single message.  And return a response.
*
******************************************************************************/
STATIC int BDT_ProcMessage(BDT *Bdt, unsigned short Command)
{
	int		RecvLen;

	if (Command > BDT_LAST_VERB)
	{
		printf("BDT: %s Invalid command %d, length = %d\n", Bdt->Name, Command, Bdt->remaining_recv);
		BDT_DumpMessage(Bdt);
		BdtSendHeader(Bdt, BDT_Error, 0);
		return(0);
	}

	if (Bdt->pHandlers == NULL)
	{
		if (Command == BDT_Connect)
			BDT_ProcessConnect(Bdt);
		else
		{
			printf("BDT_ProcMessage: %s got %s before connect\n", Bdt->Name, BdtNames[Command]);
			BDT_DiscardMessage(Bdt);
			BdtSendHeader(Bdt, BDT_Error, 0);
		}
		return(0);
	}
	if (Bdt->pHandlers[Command] == NULL)
	{
		printf("BDT_ProcMessage: service %s got %s... invalid\n", Bdt->Name, BdtNames[Command]);
	}
	return((*(Bdt->pHandlers[Command]))(Bdt));
}

/*****************************************************************************
*
* Wait on a socket read for a message.  When one arrives, read the header,
* decode it, and call the message handler routine to process and respond to it.
*
******************************************************************************/
STATIC void BDT_ReceiverTask(int Sock)
{
	int			Verb;
	int			Size;
	BDT			Bdt;
	int			MonitorLockTimeout = (BDT_PING_INTERVAL*sysClkRateGet())/2;
	static char *NoBdtName = "(No Name)";
	fd_set		FdSet;
	struct timeval	TimeVal;
	int			PollStatus;
	int			SocketState;

	Bdt.soc = Sock;
	Bdt.pMonitor = NULL;
	Bdt.WriteLock = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
	Bdt.MonitorLock = semBCreate(SEM_Q_PRIORITY, SEM_FULL);
	Bdt.state = BdtIdle;
	Bdt.pHandlers = NULL;
	Bdt.Name = NoBdtName;

	printf("BDT_ReceiverTask(%d) started\n", Sock);

	TimeVal.tv_sec = BDT_CONENCTION_TIMEOUT;
	TimeVal.tv_usec = 0;
	FD_ZERO(&FdSet);
	FD_SET(Bdt.soc, &FdSet);
	
	SocketState = 0;
	while (((PollStatus = select(FD_SETSIZE, &FdSet, NULL, NULL, &TimeVal)) > 0) && (BdtReceiveHeader(&Bdt, &Verb, &Size) == 0))
	{
		semTake(Bdt.WriteLock, WAIT_FOREVER);

		SocketState = BDT_ProcMessage(&Bdt, Verb);

		if (SocketState != 0)
			break;

#if 0
		if (semTake(Bdt.MonitorLock, MonitorLockTimeout) == OK)
		{
			/* Check for delta flag and send if so */
	
			/* Change this to run thru a delta-message linked list */
			if (Bdt.pMonitor != NULL)
			{
				/* Send delta notifier */
			}
			semGive(Bdt.WriteLock);	/* Order important for BDT_SendDelta */
			semGive(Bdt.MonitorLock);
		}
		else
		{
			printf("BDT_ReceiverTask timeout on monitor semaphore.  Monitors are stuck!\n");
			semGive(Bdt.WriteLock);
		}
#else
		semGive(Bdt.WriteLock);
#endif
		BdtFlushOutput(&Bdt);

		FD_ZERO(&FdSet);
		FD_SET(Bdt.soc, &FdSet);
	}
	if (SocketState == 0)
	{
		if (PollStatus == 0)
			printf("BDT_ReceiverTask(%d) exiting on client timeout\n", Sock);
		else 
			printf("BDT_ReceiverTask(%d) exiting on I/O error talking to Client\n", Sock);
	}
	else
		printf("BDT_ReceiverTask(%d) received close from client\n", Sock);

	/* Free up resources */
	if (Bdt.Name != NoBdtName)
		free(Bdt.Name);

	close(Sock);
	semDelete(Bdt.WriteLock);
	semDelete(Bdt.MonitorLock);
	return;
}

/*****************************************************************************
*
******************************************************************************/
#if 0
int BDT_SendDelta(int Socket, char *Message)
{
	semTake (DeltaFlagLock, WAIT_FOREVER);
	if (if (semTake(SocketWriteLock, no wait) == failed)
	{
		/* Reader task is busy... Post message for future transmission */
		Bdt.pending_delta = 1;
	}
	else
	{
		write(Message);					/* This COULD block */
		semGive(SocketWriteLock);
	}
	semGive(DeltaFlagLock);
	return (0);
}
#endif

/*****************************************************************************
*
* This task listens on a port for new connections.  When one is made, it
* spawns a task to manage it.
*
******************************************************************************/
void BDT_ListenerTask(int Port)
{
	/* Open a socket to listen on */
	struct	sockaddr_in		ListenerAddr;
	struct	sockaddr_in		ClientAddr;
	int						ListenerSock;
	int						ClientSock;
	int						ClientAddrLen;
	int						SockAddrSize = sizeof(struct  sockaddr_in);

	if (Port == 0)
		Port = BDT_TCP_PORT;

	printf("BDT_Listener(%d) started\n", Port);

	if ((ListenerSock = BdtOpenListenerTCP(Port)) < 0)
	{
		printf("BDT_ListenerTask(%d) can't start listener\n", Port);
		return;
	}

	while (1)
	{
		ClientAddrLen = sizeof(ClientAddr);
		if((ClientSock = accept(ListenerSock, (struct sockaddr*)&ClientAddr, &ClientAddrLen)) < 0)
		{
			if(errno!=EINTR) 
			{
				printf("BDT_ListenerTask(%d) accept() failed\n", Port);
			}
		}
		else
		{
			/* Spawn a task to handle the new connection */
			printf("Accepted a connection\n");
			taskSpawn("BDT", BDT_TASK_PRIO, BDT_TASK_OPTIONS, BDT_TASK_STACK, (FUNCPTR)BDT_ReceiverTask, ClientSock, 2,3,4,5,6,7,8,9,0);
		}
	}
	/* Never reached */
}

/*****************************************************************************
*
* A handy routine to assist in debugging.
*
******************************************************************************/
STATIC int HexDump(char *ReadBuffer, int rbytes)
{
	int   c = 0;
	int   i = 0;
	int   firsttime;
	char  ascii[20];  /* To hold printable portion of string */

	if (!rbytes)
		return(0);

	firsttime = 1;
	while(c < rbytes)
	{
		if ((c % 16) == 0)
		{
			if (!firsttime)
			{
				ascii[i] = '\0';
				printf(" *%s*\n", ascii);
			}
			firsttime=0;
			i = 0;
		}
		printf(" %02.2X", ReadBuffer[c] & 0xff);
		ascii[i] = ReadBuffer[c];
		if (!isprint(ascii[i]))
			ascii[i] = '.';
		++i;
		++c;
	}
	while (c%16)
	{
		fputs("   ", stdout);
		++c;
	}
	ascii[i] = '\0';
	printf(" *%s*\n", ascii);
	return(0);
}
