#ifndef __BDT_H
#define __BDT_H

/*
 * $Log$
 * Revision 1.4  1995/07/27  14:21:58  winans
 * General mods for first release.
 *
 * Revision 1.3  1995/05/30  14:47:17  winans
 * Added BDT_Ping and a port parm to BdtIpOpen.
 *
 * Revision 1.2  1995/05/16  15:38:00  winans
 * Added BdtEof to BdtState enum list.
 * Added WriteLock and DeltaFlagLock to vxWorks-side BDTs.
 * Added port number args to BdtOpenListenerTCP() and BdtOpenListenerUDP().
 *
 * Revision 1.1  1995/05/11  02:08:44  jbk
 * new file for the bulk data transfer stuff
 *
 */

/*
	Author:	Jim Kowalkowski
	Date:	5/1/95
*/

/* got from protocols(5) (cheated) or /etc/protocols */
#define BDT_UDP				17
#define BDT_TCP				6

/* server ports - in the user reserved area */
#define BDT_UDP_PORT		50296
#define BDT_TCP_PORT		50297

/* Well known service names */
#define BDT_SERVICE_PV		"pv"
#define BDT_SERVICE_NAME	"name"
#define BDT_SERVICE_DRV		"drv"

#define PV_TRANSFER_SIZE 512

/* How often some type of message is expected to arrive at each end-point */
#define BDT_PING_INTERVAL		10			/* Specified in seconds */
#define BDT_CONENCTION_TIMEOUT	25			/* Specified in seconds */

/* message types */
#define BDT_Ok			0
#define BDT_Connect		1
#define BDT_Error		2
#define BDT_Get			3
#define BDT_Put			4
#define BDT_Close		5
#define BDT_Monitor		6
#define BDT_Value		7
#define BDT_Delta		8
#define BDT_Add			9
#define BDT_Delete		10
#define BDT_Ping		11

#define	BDT_LAST_VERB	11

/* protocol states */
typedef enum
	{ BdtIdle,BdtUnbound,BdtSData,BdtRData,BdtBad,BdtEof } BdtState;

/******************************************************************************
*
* The format of a BDT_Connect message is:
*
* BdtMsgHead.verb			(unsigned 16 binary = BDT_Connect)
* BdtMsgHead.size			(unsigned 32 bit binary)
* service name length		(unsigned 8 bit binary)
* service name string		(service name length characters)
* arg1 string length		(unsigned 8 bit binary)
* arg1 string 				(arg1 string length characters)
* ...
* 
******************************************************************************/

/******************************************************************************
*
* NOTICE!!!!!!!!  MONITORS ARE NOT CURRENTLY SUPPORTED!!!!!!!
*
* The BdtMonitor structure is created on the server-side when ever a client
* registers a monitor.  It contains a tag string that is given by the client
* when the monitor is established.  This tag string is an arbitrary binary
* string of bytes... presumably used by the client as a pointer or index 
* to a status structure that represents the resource being monitored.
*
* The Message portion of the BdtMonitor represents the body of the message
* that is to be sent to the client to indicate the status of the event that
* happened.  (It may not be present in the message.)
*
* The BdtMonitor structures are only hooked into the bdt.pMonitor list if they
* are waiting to be sent to the client.
*
* The format of a BDT_Monitor message is:
*
* BdtMsgHead.verb			(unsigned 16 binary = BDT_Monitor)
* BdtMsgHead.size			(unsigned 32 bit binary)
* TagLength					(unsigned 32 bit binary)
* Tag						(TagLength characters)
* service name string		(BdtMsgHead.size-TagLength-4 characters)
*
* The <service name string> for a monitor message should include the object
* name(s) that are to be monitored.
*
*
* The format of a BDT_Delta message is:
*
* BdtMsgHead.verb			(unsigned 16 binary = BDT_Delta)
* BdtMsgHead.size			(unsigned 32 bit binary)
* BdtMonitor.TagLength		(unsigned 32 bit binary)
* BdtMonitor.Tag			(BdtMonitor.TagLength characters)
* BdtMonitor.MessageLength	(unsigned 32 bit binary)
* BdtMonitor.Message		(BdtMonitor.MessageLength characters)
*
******************************************************************************/

struct bdt;
typedef struct BdtMonitor
{
	struct bdt			*pBdt;			/* Connection to send notices back to */
	char				*Tag;			/* Client-generated tag */
	int					TagLength;		/* Length of tag */
	char				*Message;		/* Server-gen'd status change message */
	int					MessageLength;	/* Length of Message */
	struct BdtMonitor	*pNext;			/* Allow queue of N outbound on BDT */
} BdtMonitor;

typedef int (*BdthandlerFunc)(struct bdt *Bdt);
typedef struct BdtHandlers
{
	BdthandlerFunc	Ok;
	BdthandlerFunc	Connect;
	BdthandlerFunc	Error;
	BdthandlerFunc	Get;
	BdthandlerFunc	Put;
	BdthandlerFunc	Close;
	BdthandlerFunc	Monitor;
	BdthandlerFunc	Value;
	BdthandlerFunc	Delta;
	BdthandlerFunc	Add;
	BdthandlerFunc	Delete;
	BdthandlerFunc	Ping;
} BdtHandlers;

struct bdt
{
	int soc;
	int remaining_send;
	int remaining_recv;
	BdtState state;

#ifdef vxWorks
	char		*Name;					/* Service name (used for messages) */
	SEM_ID		WriteLock;
	SEM_ID		MonitorLock;
	BdtMonitor	*pMonitor;				/* List of pending monitor events */
	BdthandlerFunc	*pHandlers;			/* Support routines for messages */
	void		*pService;				/* Provate pointer for service */
#endif
};
typedef struct bdt BDT;

struct BdtMsgHead
{
	unsigned short verb;
	unsigned long size;
};
typedef struct BdtMsgHead BdtMsgHead;

#define BdtGetSocket(BDT)			(BDT->soc)
#define BdtGetResidualWrite(BDT)	(BDT->remaining_send)
#define BdtGetResidualRead(BDT)		(BDT->remaining_recv)
#define BdtGetProtoState(BDT)		(BDT->state)
#ifdef vxWorks
#define BdtGetServiceName(BDT)		(BDT->Name)
#endif

/* ------------------------------------------------------------------------
	Server functions:

	BdtOpenListenerTCP:
	 Open a socket locally bound to the bulk data transfer TCP server port.
	 Set the socket up as a listener.  Return the open socket.

	BdtOpenListenerUDP:
	 Open a socket locally bound to the bulk data transfer UDP server port.
	 Return the open socket.

   ------------------------------------------------------------------------ */

int BdtOpenListenerTCP(int Port);
int BdtOpenListenerUDP(int Port);

/* ------------------------------------------------------------------------
	Utilities functions:

	BdtMakeServer:
	 Available under unix only.  Put process in the background, disassociate
	 process from controlling terminal and parent process, and prepare
	 signals for reaping children spawned by the process.

	BdtServerClearSignals:
	 Clear the signal handlers for a process, set them to default.

	BdtMakeBDT:
	 Allocate and initialize a BDT from a socket.

	BdtFreeBDT:
	 Close the open socket and free the memory for the BDT.
   ------------------------------------------------------------------------ */

#ifndef vxWorks
int BdtMakeServer(char** argv);	
int BdtServerClearSignals();
#endif

BDT* BdtMakeBDT(int socket);	/* make a BDT from a socket */
int BdtFreeBDT(BDT* bdt);		/* free a BDT */

/* ------------------------------------------------------------------------
	Client functions:

	BdtIpOpen:
	 Open a connection to an bulk data transfer given the IP address of the
	 machine where the server exists.  The returned BDT is returned unbound,
	 a connect must be issued before data transactions can take place.

	BdtPvOpen:
	 Open and connect (bind) to a process variable.  Data transfers can
	 take place after this call.

	BdtServiceConnect:
	 Used in conjunction with BdtIpOpen().  Bind an unbound BDT to a
	 generic service provided by the server at the other end of the open
	 connection.

	BdtPvConnect:
	 Used in conjunction with BdtIpOpen().  Bind an unbound BDT to a
	 process variable on the server at the other end of the open
	 connection.

	BdtClose:
	 Completely close a connection to a server and free the BDT.

	BdtDeltaPending:
	 Check if a delta message arrived at an unexpected time.  This function
	 clears the pending condition.

   ------------------------------------------------------------------------ */

BDT* BdtIpOpen(char* address, int port);
BDT* BdtPvOpen(char *IocName, char* PvName);
int BdtServiceConnect(BDT* bdt, char* service_name, char *args);
int BdtPvConnect(BDT* bdt, char* pv_name);
int BdtClose(BDT* bdt);	
int BdtPvDeltaPending(BDT* bdt);

/* ------------------------------------------------------------------------
	Client and Server shared functions:

	BdtSendHeader:
	 Send a message header out to a connect BDT with command and message body
	 size information.

	BdtSendData:
	 Send a portion or all the message body out a connected BDT.  A header
	 must have previously been sent with length information.  The interface
	 will only allow the amount of data specified in the header to be sent.

	BdtWrite:
	 This call will block until all the data specified in the size parameter
	 are sent down the socket.

	BdtRead:
	 This call will block until all the data specified in the size parameter
	 is read from the socket.

	BdtReceiveHeader:
	 Wait until a message header appears at the BDT, return the action and
	 remaining message body size.

	BdtReceiveData:
	 Wait for a chunk or the entire body of a message to appear at the BDT.
	 Put the data into the buffer for a maximum size.  Return the amount of
	 data actually received, or zero if there is no data remaining for the
	 current message.

   ------------------------------------------------------------------------ */

int BdtSendHeader(BDT* bdt, unsigned short verb, int size);
int BdtSendData(BDT* bdt, void* buffer, int size);
int BdtReceiveHeader(BDT* bdt, int* verb, int* size);
int BdtReceiveData(BDT* bdt, void* buffer, int size);
int BdtRead(int socket, void* buffer, int size);
int BdtWrite(int socket, void* buffer, int size);
int BdtFlushOutput(BDT* bdt);
int BdtPvPutArray(BDT *bdt, short DbrType, void *Buf, unsigned long NumElements, unsigned long ElementSize);


#endif

