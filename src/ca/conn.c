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
/*	File:	atcs:[ca]access.c					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*									*/
/************************************************************************/
/*_end									*/

#include		<vxWorks.h>
#include 		<cadef.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>


chid_retry(silent)
char			silent;
{
  register chid		chix;
  register unsigned int	retry_cnt = 0;
  char			string[100];
  void			send_msg();

  if(!chidlist_pend.count)
    return ECA_NORMAL;

  LOCK;
  chix = (chid) &chidlist_pend;
  while(chix = (chid) chix->node.next){
    build_msg(chix,TRUE);
    retry_cnt++;
    if(!silent)
      ca_signal(ECA_CHIDNOTFND, chix+1);
  }
  send_msg();
  printf("Sent ");
  UNLOCK;

  if(retry_cnt && !silent){
    sprintf(string, "%d channels outstanding", retry_cnt);
    ca_signal(ECA_CHIDRETRY, string);
  }

  return ECA_NORMAL;
}

/*
Locks must be applied while in this routine
*/
mark_chids_disconnected(iocix)
register unsigned	iocix;
{
  register chid		chix;
  register chid		nextchix;

  nextchix = (chid) &chidlist_conn.node;
  while(chix = nextchix){
    nextchix = (chid) chix->node.next;
    if(chix->iocix == iocix){
      lstDelete(&chidlist_conn, chix); 
      lstAdd(&chidlist_pend, chix);    
      chix->type = TYPENOTCONN;
    }
  }

}
