/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/	
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*									*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date		Programmer	Comments			*/
/*	----		----------	--------			*/
/*	6/89		Jeff Hill	Init Release			*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC network flow control module				*/
/*	File:	atcs:[ca]flow_control.c					*/
/*	Environment: VMS. UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc flow control module						*/
/*									*/
/*									*/
/*	Special comments						*/	
/*	------- --------						*/
/*									*/	
/************************************************************************/
/*_end									*/


#include		<types.h>
#include		<vxWorks.h>
#include		<socket.h>
#include		<ioctl.h>
#ifdef vxWorks
#include		<ioLib.h>
#endif
#include		<cadef.h>
#include		<iocmsg.h>
#include		<iocinf.h>


/*
Keep track of how many times messages have come with out a break in between
*/
flow_control(piiu)
struct ioc_in_use               *piiu; 
{	
  unsigned 			nbytes;
  register int			status;
  register int			busy = piiu->client_busy;

  status = socket_ioctl(	piiu->sock_chan,
				FIONREAD,
				&nbytes);
  if(status < 0)
    return ERROR;

  if(nbytes){
    piiu->contiguous_msg_count++;
    if(!busy)
      if(piiu->contiguous_msg_count > MAX_CONTIGUOUS_MSG_COUNT){
        piiu->client_busy = TRUE;
        ca_busy_message(piiu);
      }                                  
  }
  else{
    piiu->contiguous_msg_count=0;
    if(busy){
      ca_ready_message(piiu);
      piiu->client_busy = FALSE;
    }                                   
  }

  return;
}
