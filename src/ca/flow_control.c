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
/*	06xx89	joh	First Release					*/
/*	060591	joh	delinting					*/
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

static char	*sccsId = "$Id$\t$Date$";

#if defined(vxWorks)
#	include		<vxWorks.h>
#	include		<ioLib.h>
#	include		<socket.h>
#	include		<ioctl.h>
#	ifdef V5_vxWorks
#		include	<vxTypes.h>
#	else
#		include	<types.h>
#	endif
#elif defined(VMS)
#	include		<sys/types.h>
#	include		<sys/socket.h>
#	if defined(UCX)			/* GeG 09-DEC-1992 */
#     		include	<sys/ucx$inetdef.h>
#     		include	<ucx.h>
#	else
#		include	<sys/ioctl.h>
#	endif
#elif defined(UNIX)
#	include		<sys/types.h>
#	include		<sys/socket.h>
#	include		<sys/ioctl.h>
#else
	@@@@ dont compile @@@@
#endif

#include		<os_depen.h>
#include		<cadef.h>
#include		<iocmsg.h>
#include		<iocinf.h>


/*
 * FLOW CONTROL
 * 
 * Keep track of how many times messages have 
 * come with out a break in between and 
 * suppress monitors if we are behind
 * (an update is sent when we catch up)
 */
void
flow_control(piiu)
	struct ioc_in_use *piiu;
{
	unsigned        nbytes;
	register int    status;
	register int    busy = piiu->client_busy;

	/*
	 * use of the additional system call here does not
	 * seem to slow things down appreciably
	 */
	status = socket_ioctl(piiu->sock_chan,
			      FIONREAD,
			      &nbytes);
	if (status < 0) {
		close_ioc(piiu);
		return;
	}

	/*	
	 * I wish to avoid going into flow control however 
	 * as this impacts the performance of batched fetches
	 */
	if (nbytes) {
		piiu->contiguous_msg_count++;
		if (!busy)
			if (piiu->contiguous_msg_count >
			    MAX_CONTIGUOUS_MSG_COUNT) {
				piiu->client_busy = TRUE;
				ca_busy_message(piiu);
#				if DEBUG	
					printf("fc on\n");
#				endif
			}
	} else {
		piiu->contiguous_msg_count = 0;
		if (busy) {
#			ifdef DEBUG
				printf("fc off\n");
#			endif
			ca_ready_message(piiu);
			piiu->client_busy = FALSE;
		}
	}

	return;
}
