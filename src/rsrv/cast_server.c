/*		The IOC connection request server  */
/*
*******************************************************************************
**                              GTA PROJECT
**      Copyright 1988, The Regents of the University of California.
**                      Los Alamos National Laboratory
**                      Los Alamos New Mexico 87845
**      cast_server.c - GTA request server main loop
**      Sun UNIX 4.2 Release 3.4
**	First Release- Jeff Hill May 88
**
**
**
**	FIXES NEEDED: 
**
**	Dont send channel found message unless there is memory, a task slot,
**	and a TCP socket available. Send a diagnostic instead. 
**	Or ... make the timeout shorter? This is only a problem if
**	they persist in trying to make a connection after getting no
**	response.
**
*******************************************************************************
*/

#include <vxWorks.h>
#include <lstLib.h>
#include <taskLib.h>
#include <types.h>
#include <socket.h>
#include <ioLib.h>
#include <in.h>
#include <db_access.h>
#include <task_params.h>
#include <server.h>


	
STATUS 				
cast_server()
{
  struct sockaddr_in		sin;	
  FAST int			status;
  int				i;
  FAST struct client		*client;
  FAST struct client		*tmpclient = NULL;
  struct sockaddr_in		recv_addr;
  int  				recv_addr_size = sizeof(recv_addr);
  unsigned			nchars;
  static struct message_buffer  udp_msg;
# define			TIMEOUT	60 /* sec */
  unsigned long			timeout = TIMEOUT * sysClkRateGet();

  struct client 		*existing_client();
  struct client 		*create_udp_client();
  void		 		rsrv_online_notify_task();

  if( IOC_cast_sock!=0 && IOC_cast_sock!=ERROR )
    if( (status = close(IOC_cast_sock)) == ERROR )
      logMsg("Unable to close open master socket\n");

  /* 
   *  Open the socket.
   *  Use ARPA Internet address format and datagram socket.
   *  Format described in <sys/socket.h>.
   */

  if((IOC_cast_sock = socket (AF_INET, SOCK_DGRAM, 0)) == ERROR){
    logMsg("Socket creation error\n");
    printErrno (errnoGet ());
    taskSuspend(0);
  }


  /*  Zero the sock_addr structure */
  bfill(&sin, sizeof(sin), 0);
  sin.sin_family 	= AF_INET;
  sin.sin_addr.s_addr 	= INADDR_ANY;
  sin.sin_port 		= CA_SERVER_PORT;

	
  /* get server's Internet address */
  if (bind (IOC_cast_sock, &sin, sizeof (sin)) == ERROR){
    logMsg("Bind error\n");
    printErrno (errnoGet ());
    close (IOC_cast_sock);
    taskSuspend(0);
  }


  bfill(&udp_msg, sizeof(udp_msg), NULL);

  /* tell clients we are on line again */
  status = taskSpawn(
	CA_ONLINE_NAME,
	CA_ONLINE_PRI,
	CA_ONLINE_OPT,
	CA_ONLINE_STACK,
	rsrv_online_notify_task);
  if(status<0){
    logMsg("Cast_server: couldnt start up online notify task\n");
    printErrno(errnoGet ());
  }


  while(TRUE){

    status = recvfrom(	IOC_cast_sock,
			&udp_msg.cnt,
			sizeof(udp_msg.cnt)+sizeof(udp_msg.buf),
			NULL,
			&recv_addr, 
			&recv_addr_size);
    if(status<0){
      logMsg("Cast_server: UDP recv error\n");
      printErrno(errnoGet ());
      taskSuspend(0);
    }

    if(status != udp_msg.cnt){
      logMsg("Cast_server: Recieved a corrupt broadcast\n");
      continue;
    }



    if(MPDEBUG==2){
       logMsg(	"cast_server(): recieved a broadcast of %d bytes\n", status);
       logMsg(	"from addr %x, port %x \n", 
		recv_addr.sin_addr, 
		recv_addr.sin_port);
    }

    /* subtract header size */
    udp_msg.cnt -= sizeof(udp_msg.cnt);

    /* setup new client structure but, reuse old structure if possible */
    LOCK_CLIENTQ;
    client = existing_client(&recv_addr);
    if(!client){
      if(tmpclient){
        client = tmpclient;
	tmpclient = NULL;
      }
      else
        client = create_udp_client(IOC_cast_sock);
      if(!client){
        UNLOCK_CLIENTQ;
        continue;
      }

      client->addr = recv_addr;
      client->ticks_at_creation = tickGet();
      lstAdd(&clientQ, client);
    }

    if(MPDEBUG==2)
      i = client->addrq.count;

    message_process(client, &udp_msg);

    /* remove client data structure from list if channel not found */
    if(client->addrq.count){
      if(MPDEBUG==2)
        logMsg(	"Found %d new channel name matches (%d cumulative)\n",
		client->addrq.count-i,
		client->addrq.count);
    }
    else{
      lstDelete(&clientQ, client);
      if(!tmpclient)
  	tmpclient = client;
      else
        free(client);
    }

    /* 
    allow message to batch up if more are comming 
    */
    status = ioctl(IOC_cast_sock, FIONREAD, &nchars);
    if(status == ERROR){
      printErrno(errnoGet(0));
      taskSuspend(0);
    }
    if(nchars == 0){
      client = (struct client *) &clientQ;
      while(client = (struct client *) client->node.next)
        send_msg(client);

      clean_clientq(timeout);
    }
    UNLOCK_CLIENTQ;

  }
}




struct client
*create_udp_client(sock)
unsigned sock;
{
  struct client *client;

      client = (struct client *)malloc(sizeof(struct client));
      if(!client){
        printErrno(errnoGet ());
        return NULL;
      }

      if(MPDEBUG==2)
        logMsg(	"cast_server(): Creating data structures for new udp client\n");

/*    
      The following inits to zero done instead of a bfill since
      the send and recv buffers are large and don't need initialization.

      bfill(client, sizeof(*client), NULL);
*/     
      lstInit(&client->addrq);
      client->tid = 0;
      client->send.stk = 0;
      client->send.cnt = 0;
      client->recv.stk = 0;
      client->recv.cnt = 0;
      client->evuser = NULL;
      client->eventsoff = FALSE;

      client->proto = IPPROTO_UDP;
      client->send.maxstk = MAX_UDP-sizeof(client->recv.cnt);
      FASTLOCKINIT(&client->send.lock);

      client->recv.maxstk = MAX_UDP;
      FASTLOCKINIT(&client->recv.lock);

      client->sock = sock;

      return client;
}


/*
	send lock must be applied

*/
udp_to_tcp(client,sock)
struct client 	*client;
unsigned	sock;
{

  if(MPDEBUG==2)
    logMsg("cast_server(): converting udp client to tcp\n");

  client->proto 	= IPPROTO_TCP;
  client->send.maxstk 	= MAX_TCP;
  client->recv.maxstk 	= MAX_TCP;
  client->sock 		= sock;

  return OK;
}


