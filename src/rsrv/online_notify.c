/*
 *	O N L I N E _ N O T I F Y . C
 *	tell CA clients this a server has joined the network
 *
 *	Author: Jeffrey O. Hill
 *
 *	Hisory
 *	.00 103090 	joh	First release
 */

/*
 *	system includes
 */
#include <vxWorks.h>
#include <types.h>
#include <socket.h>
#include <in.h>

/*
 *	LAACS includes
 */
#include <taskParams.h>
#include <iocmsg.h>

#define abort taskSuspend

struct complete_msg{
	unsigned		length;
	struct extmsg		extmsg;
};

/*
 *	RSRV_ONLINE_NOTIFY_TASK
 *	
 *
 *
 */
void rsrv_online_notify_task()
{
	/*
	 * 1 sec init delay
	 */
  	unsigned long		delay = sysClkRateGet();
	/*
	 * CA_ONLINE_DELAY [sec] max delay
	 */
  	unsigned long		maxdelay = CA_ONLINE_DELAY * sysClkRateGet();
	struct complete_msg	msg;

  	struct sockaddr_in	send_addr;
  	struct sockaddr_in	recv_addr;
	int			status;
	int			sock;
	struct sockaddr_in	lcl;
  	int			true = TRUE;
	int 			i;

	struct sockaddr_in	*local_addr();
  	struct in_addr		broadcast_addr();

  	/* 
  	 *  Open the socket.
  	 *  Use ARPA Internet address format and datagram socket.
  	 *  Format described in <sys/socket.h>.
   	 */
  	if((sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    		logMsg("Socket creation error\n");
    		printErrno (errnoGet ());
    		abort(0);
  	}

	lcl = *(local_addr(sock));

      	status = setsockopt(	sock,
				SOL_SOCKET,
				SO_BROADCAST,
				&true,
				sizeof(true));
      	if(status<0){
    		printErrno (errnoGet ());
    		abort(0);
      	}

      	bfill(&recv_addr, sizeof recv_addr, 0);
      	recv_addr.sin_family = AF_INET;
      	recv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* let slib pick lcl addr */
      	recv_addr.sin_port = htons(0);	 /* let slib pick port */
      	status = bind(sock, &recv_addr, sizeof recv_addr);
      	if(status<0)
		abort(0);

   	bfill(&msg, sizeof msg, NULL);
	msg.length = htonl(sizeof msg);
	msg.extmsg.m_cmmd = htons(IOC_RSRV_IS_UP);
	msg.extmsg.m_available = lcl.sin_addr.s_addr;

 	/*  Zero the sock_addr structure */
  	bfill(&send_addr, sizeof send_addr, 0);
  	send_addr.sin_family 	= AF_INET;
  	send_addr.sin_addr	= broadcast_addr();
  	send_addr.sin_port 	= htons(CA_CLIENT_PORT);

 	while(TRUE){
  

        	status = sendto(
			sock,
        		&msg,
        		sizeof msg,
        		0,
       			&send_addr, 
			sizeof send_addr);
      		if(status != sizeof msg)
			abort();

		taskDelay(delay);
		delay = min(delay << 1, maxdelay);
	}

}









#ifdef JUNKYARD
  	client = create_udp_client(sock);
 	client->addr = send_addr;
  	client->ticks_at_creation = tickGet();

	while(TRUE){
    		reply = (struct extmsg *) ALLOC_MSG(client, 0);
    		if(!reply)
      			abort(0);


  		bfill(reply, sizeof(*reply), NULL);
  		reply->m_cmmd = IOC_RSRV_IS_UP;
		reply->m_available = lcl.sin_addr.s_addr;
  		reply->m_postsize = 0;

  		END_MSG(client);
  		send_msg(client);

		taskDelay(delay);
		delay = delay << 1;
	}
/*
	Should it need to quit....

  	free_one_client(client);
*/
#endif
