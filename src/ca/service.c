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
/*	8/87		Jeff Hill	Init Release			*/
/*	1/90		Jeff Hill	Made cmmd for errors which	*/
/*					eliminated the status field	*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	channel access service routines				*/
/*	File:	atcs:[ca]servuice.c					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	channel access service routines					*/
/*									*/
/*									*/
/*	Special comments						*/	
/*	------- --------						*/
/*									*/	
/************************************************************************/
/*_end									*/

#ifdef VMS
#include		<stsdef.h>
#endif

#include		<vxWorks.h>

#include		<types.h>
#include 		<cadef.h>
#include 		<net_convert.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>


void					post_msg(hdrptr,bufcnt,net_addr,piiu)
register struct extmsg			*hdrptr;
register long				bufcnt;
struct in_addr				net_addr;
struct ioc_in_use			*piiu;
{
  evid					monix;
  long					msgcnt;
  long					tmp;
  char 					*name;

  register void *			t_available;
  register unsigned short		t_postsize;
  register unsigned short		t_cmmd;
  register chtype 			t_type;
  register unsigned short 		t_count;
  register unsigned short 		t_size;
  int					status;

# define BUFCHK(SIZE)\
  msgcnt = (SIZE); if(bufcnt-msgcnt < 0)\
  {printf("post_msg(): expected msg size larger than actual msg\n");return;}

# define BUFSTAT\
  printf("expected %d left %d\n",msgcnt,bufcnt);

  post_msg_active++;

  while(bufcnt>0){
#   ifdef DEBUG
      printf(	"processing message- bytes left %d, pending msgcnt %d\n",
		bufcnt,
		pndrecvcnt);
#   endif

    /* byte swap the message up front */
    t_available = (void *) hdrptr->m_available;
    t_postsize 	= ntohs(hdrptr->m_postsize);
    t_cmmd	= ntohs(hdrptr->m_cmmd);
    t_type	= ntohs(hdrptr->m_type);
    t_count 	= ntohs(hdrptr->m_count);
    t_size	= dbr_size[t_type];



    switch(t_cmmd){

    case	IOC_EVENT_ADD:
      /* 	run the user's event handler			*/
      /*	m_available points to event descriptor		*/

      monix = (evid) t_available;

      /*  m_postsize = 0  is a confirmation of a monitor cancel  */
      if( !t_postsize ){
	LOCK;
	lstDelete(&eventlist, monix); 
	UNLOCK;
	if(free(monix)<0)
	  printf("Unable to dealloc VM on IOC monitor cancel confirmation\n");

	BUFCHK(sizeof(*hdrptr));
        break;
      }

      /* BREAK LEFT OUT HERE BY DESIGN -JH */

    case	IOC_READ:
    {
      /* 	update and convert the user's argument if read 	*/
      /*	(m_available points to argument descriptor)	*/
      /*	else update in buffer if a monitor		*/
      register void	*objptr = (void *) (hdrptr+1);
      register void	*dstptr;
      unsigned int	i;

      if(t_cmmd == IOC_READ){
	/* only count get returns if from the current read seq */
        if(!VALID_MSG(piiu))
	  break;
	dstptr = (void *) t_available;
      }
      else
	dstptr = objptr;

      BUFCHK(sizeof(*hdrptr) + t_postsize);

#     ifdef DEBUG
        BUFSTAT
	if(t_cmmd==IOC_READ)
          {static short i;  printf("global reccnt %d\n",++i);}
	else
          {static short i;  printf("global moncnt %d\n",++i);}
#     endif

      for(i=0; i<t_count; i++){

        /* perform 2 and 4 byte transfer in line */
	/* NOTE that if we are on a machine that does not require */
	/* conversions these will be NULL macros */
        switch(t_type){

        /*	long word convert/transfer	*/
        case DBR_INT:
        case DBR_ENUM:
      	  *(short *)dstptr = ntohs(*(short *)objptr);
	  break;	

        case DBR_FLOAT:
	  tmp = *(long *)objptr;
	  ntohf(&tmp, dstptr);
	  break;

        case DBR_STRING:
	  if(dstptr != objptr)
            strcpy(	dstptr,
			objptr);
	  break;	

        /* record transfer and convert */
        case DBR_STS_STRING:
        case DBR_STS_INT:
        case DBR_STS_FLOAT:
        case DBR_STS_ENUM:
        case DBR_GR_INT:
        case DBR_GR_FLOAT:
        case DBR_CTRL_INT:
        case DBR_CTRL_FLOAT:
        case DBR_CTRL_ENUM:
#	  ifdef VAX
            (*cvrt[t_type])(objptr,dstptr,(int)FALSE,(int)t_count);
#	  else
	    if(dstptr != objptr)
              memcpy(	dstptr,
			objptr,
			t_size);
#         endif

	  /* 
	    Conversion Routines handle all elements of an array in one call 
	    for efficiency.
	  */
	  goto array_loop_exit;

          break;
        default:
	  printf("post_msg(): unsupported db fld type in msg ?%d\n",t_type);
	  abort();	
        }

	(char *) objptr += t_size;
	(char *) dstptr += t_size;

      }

array_loop_exit:

      if(t_cmmd == IOC_READ)
        /* decrement the outstanding IO count */
        CLRPENDRECV
      else
        /* call handler */
        (*monix->usr_func)(	
				monix->usr_arg,	/* usr supplied		*/
				monix->chan,	/* channel id		*/
				t_type,		/* type of returned val	*/ 
				t_count,	/* the element count	*/
				hdrptr+1	/* ptr to returned val	*/
				);

      break;
    }
    case	IOC_SEARCH:
    case	IOC_BUILD:
      BUFCHK(sizeof(*hdrptr));
      if( ((chid)t_available)->paddr ){
	int 	iocix = ((chid)t_available)->iocix;

	if(iiu[iocix].sock_addr.sin_addr.s_addr==net_addr.s_addr)
	  printf("burp ");
        else{
	  char	msg[256];
	  char	acc[64];
	  char	rej[64];
	  sprintf(acc,"%s",host_from_addr(iiu[iocix].sock_addr.sin_addr));
	  sprintf(rej,"%s",host_from_addr(net_addr));
	  sprintf(	msg,
			"Channel: %s Accepted: %s Rejected: %s ",
			((chid)t_available)+1,
			acc,
			rej);
          ca_signal(ECA_DBLCHNL, msg);
	}

	/* IOC_BUILD messages allways have a IOC_READ msg following */
	/* (IOC_BUILD messages are sometimes followed by error messages */
  	/* which are also ignored on double replies) */
	/* the following cause this message to be ignored for double replies */ 
	if(t_cmmd == IOC_BUILD)
	  msgcnt += sizeof(struct extmsg) + (hdrptr+1)->m_postsize; 
	
	break;
      }
      LOCK;
      status = alloc_ioc	(
				net_addr,
				IPPROTO_TCP,		
				&((chid)t_available)->iocix
				);
      SEVCHK(status, host_from_addr(net_addr));
      SEVCHK(status, ((chid)t_available)+1);

      /*	Update rmt chid fields from extmsg fields	*/
      ((chid)t_available)->type  = t_type;      
      ((chid)t_available)->count = t_count;      
      ((chid)t_available)->paddr = hdrptr->m_paddr;      
      lstDelete(&chidlist_pend, t_available);
      lstAdd(&chidlist_conn, t_available);
      UNLOCK;

      /* decrement the outstanding IO count */
      CLRPENDRECV;

      break;

    case IOC_READ_SYNC:
      BUFCHK(sizeof(struct extmsg));
      piiu->read_seq++;
      break;

    case IOC_ERROR:
    {
      char context[255];

      BUFCHK(sizeof(struct extmsg) + t_postsize);

      name = (char *) host_from_addr(net_addr);
      if(!name)
	name = "an unregistered IOC";

      if(t_postsize>sizeof(struct extmsg))
        sprintf(context, "detected by: %s for: %s", name, hdrptr+2);   
      else
        sprintf(context, "detected by: %s", name);   

      /* 
	NOTE:
	The orig request is stored as an extmsg structure at
	hdrptr+1 
	I should print additional diagnostic info using this
	info when time permits......
      */

      SEVCHK(ntohl((int)t_available), context);
      break;
    }

    default:
      printf("post_msg(): Corrupt message or unsupported m_cmmd type\n");
      return;
    }

    bufcnt -= msgcnt;
    hdrptr  = (struct extmsg *) (msgcnt + (char *) hdrptr);    

  }


  post_msg_active--;

} 

