#ifndef ___BS_H
#define ___BS_H

/*
 * $Log$
 */

/*
	Author:	Jim Kowalkowski
	Date:	9/1/95
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* got from protocols(5) (cheated) or /etc/protocols */
#define BS_UDP				17
#define BS_TCP				6

/* server ports - in the user reserved area - above 50000 */
#define BS_UDP_PORT		50296
#define BS_TCP_PORT		50297

/* message types */
#define BS_Ok		0
#define BS_Error	1
#define BS_Close	2
#define BS_Ping		3

#define	BS_LAST_VERB	3
#define	BS_RETRY_COUNT	3

/* protocol states */
typedef enum { BSidle,BSunbound,BSsData,BSrData,BSbad,BSeof } BSstate;

struct bs
{
	int soc;
	int remaining_send;
	int remaining_recv;
	BSstate state;
};
typedef struct bs BS;

struct bs_udp_data
{
	struct sockaddr sin;
	int len;
};
typedef struct bs_udp_data BSDATA;

struct BSmsgHead
{
	unsigned short verb;
	unsigned long size;
};
typedef struct BSmsgHead BSmsgHead;

typedef unsigned long BS_ULONG;

#define BSgetSocket(BS)			(BS->soc)
#define BSgetResidualWrite(BS)	(BS->remaining_send)
#define BSgetResidualRead(BS)	(BS->remaining_recv)
#define BSgetProtoState(BS)		(BS->state)

/* ------------------------------------------------------------------------
	Server functions:

	BSopenListenerTCP:
	 Open a socket locally bound to the bulk data transfer TCP server port.
	 Set the socket up as a listener.  Return the open socket.

	BSopenListenerUDP:
	 Open a socket locally bound to the bulk data transfer UDP server port.
	 Return the open socket.

   ------------------------------------------------------------------------ */

int BSopenListenerTCP(int Port);
int BSopenListenerUDP(int Port);
int BSopenTCP(BSDATA*);

/* ------------------------------------------------------------------------
	Utilities functions:

	BSmakeServer:
	 Available under unix only.  Put process in the background, disassociate
	 process from controlling terminal and parent process, and prepare
	 signals for reaping children spawned by the process.

	BSserverClearSignals:
	 Clear the signal handlers for a process, set them to default.

	BSmakeBS:
	 Allocate and initialize a BS from a socket.

	BSfreeBS:
	 Close the open socket and free the memory for the BS.
   ------------------------------------------------------------------------ */

#ifndef vxWorks
int BSmakeServer(char** argv);	
int BSserverClearSignals();
#endif

BS* BSmakeBS(int socket);	/* make a BS from a socket */
int BSfreeBS(BS* bdt);		/* free a BS */

int BSgetBroadcastSocket(int port, struct sockaddr_in* sin);
int BSipBroadcastOpen(BSDATA* info, int default_dest_port);
int BSgetAddressPort(BSDATA* info,char* ip_addr, int* dest_port);
int BSsetAddressPort(BSDATA* info,char* ip_addr, int dest_port);
int BSsetAddress(BSDATA* info,char* ip_addr);
int BSsetPort(BSDATA* info,int dest_port);

/* ------------------------------------------------------------------------
	UDP functions:
   ------------------------------------------------------------------------ */
int BStransUDP(int soc,BSDATA* info,void* obuf,int osize,void* ibuf,int isize);
int BSwriteUDP(int soc,BSDATA* info,void* obuf,int osize);
int BSwriteDataUDP(int soc,int dest_port, char* ip_addr,void* obuf,int osize);
int BSreadUDP(int soc,BSDATA* info,BS_ULONG tout,void* buf,int size);
int BSbroadcast(int soc,BSDATA* info,void* buf,int size);
int BSbroadcastTrans(int soc,int trys,BSDATA* o_info,BSDATA* i_info,
	void* obuf,int osize,void* ibuf,int isize);

/* ------------------------------------------------------------------------
	Client functions:

	BSipOpen:
	 Open a connection to an bulk data transfer given the IP address of the
	 machine where the server exists.  The returned BS is returned unbound,
	 a connect must be issued before data transactions can take place.

	BSclose:
	 Completely close a connection to a server and free the BS.

   ------------------------------------------------------------------------ */

BS* BSipOpen(char* address, int port);
BS* BSipOpenData(BSDATA* info);
int BSclose(BS* bdt);	

/* ------------------------------------------------------------------------
	Client and Server shared functions:

	BSsendHeader:
	 Send a message header out to a connect BS with command and message body
	 size information.

	BSsendData:
	 Send a portion or all the message body out a connected BS.  A header
	 must have previously been sent with length information.  The interface
	 will only allow the amount of data specified in the header to be sent.

	BSwrite:
	 This call will block until all the data specified in the size parameter
	 are sent down the socket.

	BSread:
	 This call will block until all the data specified in the size parameter
	 is read from the socket.

	BSreceiveHeader:
	 Wait until a message header appears at the BS, return the action and
	 remaining message body size.

	BSreceiveData:
	 Wait for a chunk or the entire body of a message to appear at the BS.
	 Put the data into the buffer for a maximum size.  Return the amount of
	 data actually received, or zero if there is no data remaining for the
	 current message.

   ------------------------------------------------------------------------ */

int BSsendHeader(BS* bdt, unsigned short verb, int size);
int BSsendData(BS* bdt, void* buffer, int size);
int BSreceiveHeader(BS* bdt, int* verb, int* size);
int BSreceiveData(BS* bdt, void* buffer, int size);
int BSread(int socket, void* buffer, int size);
int BSwrite(int socket, void* buffer, int size);
int BSflushOutput(BS* bdt);

#endif

