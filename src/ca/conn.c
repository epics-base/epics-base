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
/*	Title:	IOC connection automation				*/
/*	File:	atcs:[ca]conn.c						*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*									*/
/************************************************************************/
/*_end									*/
#ifdef UNIX
#include		<stdio.h>
#endif

#include		<vxWorks.h>
#include 		<cadef.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>


/*
 *
 *	CHID_RETRY
 *
 *	retry disconnected channels
 *
 *
 *	NOTES:
 *	Lock must be applied while in this routine
 */
chid_retry(silent)
char			silent;
{
  	register chid		chix;
  	register unsigned int	retry_cnt = 0;
  	unsigned int		retry_cnt_no_handler = 0;
  	char			string[100];
  	int			i;
  	int			search_type;

	/*
	 * CASTTMO+pndrecvcnt*LKUPTMO)/DELAYVAL + 1
	 */
#define CASTTMO		0.150		/* 150 mS	*/
#define LKUPTMO		0.015		/* 15 mS 	*/

  	for(i=0; i< nxtiiu; i++){
    		if(i != BROADCAST_IIU && iiu[i].conn_up)
      			continue;

    		if(iiu[i].nconn_tries++ > MAXCONNTRIES)
      			continue;

    		search_type = (i==BROADCAST_IIU? DONTREPLY: DOREPLY);

    		chix = (chid) &iiu[i].chidlist;
    		while(chix = (chid) chix->node.next){
      			if(chix->type != TYPENOTCONN)
				continue;
      			build_msg(chix,search_type);
     			retry_cnt++;

      			if(!(silent || chix->connection_func)){
       				 ca_signal(ECA_CHIDNOTFND, chix+1);
				retry_cnt_no_handler++;
			}
    		}
  	}

  	if(retry_cnt){
    		send_msg();
    		printf("<Trying> ");
#ifdef UNIX
    		fflush(stdout);
#endif

    		if(!silent && retry_cnt_no_handler){
      			sprintf(string, "%d channels outstanding", retry_cnt);
      			ca_signal(ECA_CHIDRETRY, string);
    		}
  	}

  	return ECA_NORMAL;
}



/*
 *
 *
 *	MARK_SERVER_AVAILABLE
 *
 *
 *	NOTES:
 *	Lock must be applied while in this routine
 *
 */
void
mark_server_available(net_addr)
struct in_addr	net_addr;
{
  	int i;
  	void noop_msg();

 	for(i=0;i<nxtiiu;i++)
    		if(	(net_addr.s_addr == 
			iiu[i].sock_addr.sin_addr.s_addr)){

			/*
			 *	reset the retry count out
			 *
			 */
      			iiu[i].nconn_tries = 0;

			/*
			 * Check if the conn is down but TCP 
			 * has not informed me by sending a NULL msg
			 */
      			if(iiu[i].conn_up){
        			noop_msg(&iiu[i]);
        			send_msg();
      			}
      			return;
    		}

	/*
	 *	Not on a known IOC so try a directed UDP
	 *
	 */
/*
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	This connects when the client starts before the server
	1) uses a broadcast- should use a directed UDP messaage
	2) many clients with nonexsistent channels could
	cause a flood here

	



  	iiu[BROADCAST_IIU].nconn_tries = MAXCONNTRIES-1;
@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/
}
