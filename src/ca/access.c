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
/*	1/90		Jeff HIll	switched order in task exit	*/
/*					so events are deleted first	*/
/*					then closed			*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:	IOC high level access routines				*/
/*	File:	atcs:[ca]access.c					*/
/*	Environment: VMS, UNIX, VRTX					*/
/*	Equipment: VAX, SUN, VME					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	ioc high level access routines					*/
/*									*/
/*									*/
/*	Special comments						*/	
/*	------- --------						*/
/*	Things that could be done to improve this code			*/
/*	1)	Check timeouts on the ca_pend for values to large	*/
/*	2)	When a chid is marked disconnected any associated	*/
/*		monitors should be placed in inactive list as well	*/
/*	3)	Fix the monitor cancel protocol				*/
/*	4)	Allow them to recieve channel A when channel B changes	*/
/*									*/
/*									*/
/************************************************************************/
/*_end									*/

/* the following support riu.h */
#ifdef VMS
#include		stsdef.h
#include		ssdef.h
#include		psldef.h
#else
#endif

#include		<vxWorks.h>

#ifdef vxWorks
# include		<taskLib.h>
# include		<semLib.h>
/*# include		<dblib.h>*/
#endif

#include 		<cadef.h>
#include		<net_convert.h>
#include		<db_access.h>
#include		<iocmsg.h>
#include 		<iocinf.h>


/****************************************************************/
/*	Macros for syncing message insertion into send buffer	*/
/****************************************************************/

#define SNDBEG(STRUCTURE) \
SND_BEG(STRUCTURE, &iiu[chix->iocix])

#define SND_BEG(STRUCTURE, PIIU)\
LOCK SND_BEG_NOLOCK(STRUCTURE,PIIU)

#define SND_BEG_NOLOCK(STRUCTURE,PIIU) \
{ \
  void send_msg(); \
  register struct STRUCTURE *mptr; \
  register unsigned short msgsize = sizeof(*mptr); \
  register struct ioc_in_use *psendiiu = (PIIU); \
  if(psendiiu->send->stk + msgsize > psendiiu->max_msg)send_msg(); \
  mptr = (struct STRUCTURE *) (psendiiu->send->buf + psendiiu->send->stk);


#define SNDEND \
SNDEND_NOLOCK  UNLOCK


#define SNDEND_NOLOCK \
psendiiu->send->stk += msgsize;}


#define SNDBEG_VARIABLE(POSTSIZE) \
LOCK SNDBEG_VARIABLE_NOLOCK(POSTSIZE)


#define SNDBEG_VARIABLE_NOLOCK(POSTSIZE) \
{ \
  void send_msg(); \
  register struct extmsg *mptr; \
  register unsigned short msgsize = sizeof(*mptr) + POSTSIZE; \
  register struct ioc_in_use *psendiiu = &iiu[chix->iocix]; \
  if(psendiiu->send->stk+sizeof(*mptr)+POSTSIZE > psendiiu->max_msg) \
    send_msg(); \
  mptr = (struct extmsg *) (psendiiu->send->buf + psendiiu->send->stk);



/****************************************************************/
/*	Macros for error checking				*/
/****************************************************************/
#define CHIXCHK(CHIX) \
{ \
  register unsigned short iocix = CHIX->iocix; \
  if(	iocix == BROADCAST_IIU || \
	(iocix >= nxtiiu && iocix != LOCAL_IIU) || \
	INVALID_DB_REQ(CHIX->type)) \
    return ECA_BADCHID; \
}

#define INITCHK\
if(!ca_static)\
  ca_task_initialize();


static struct extmsg	nullmsg;




ca_task_initialize
#ifdef VAXC
(void)
#else
()
#endif
{
  int 		status;
  char		*taskName();
  char		name[15];
  void		*db_init_events();
  int		ca_import();

  if(!ca_static){                                                 

#   ifdef vxWorks
    {
      int options;

      if(taskOptionsGet(taskIdSelf(),&options)==ERROR)
	abort();
      if(!(options & VX_FP_TASK))
	SEVCHK(ECA_NEEDSFP, NULL);

      if(taskOptionsSet(taskIdSelf(),VX_FP_TASK,VX_FP_TASK)==ERROR)
	abort();

      if(ca_add_task_variable()== ERROR)
        abort();
    }
#   endif

    ca_static = (struct ca_static *) malloc(sizeof(*ca_static));  
    if(!ca_static)
      abort();
    memset(ca_static, NULL, sizeof(*ca_static));                  

#   ifdef VMS
      status = lib$get_ef(&io_done_flag);
      if(status != SS$_NORMAL)
	lib$signal(status);
#   endif

#   ifdef vxWorks

      FASTLOCKINIT(&client_lock);

      evuser = (void *) db_init_events();
      if(!evuser)
        abort();

      name[0]=NULL;
      strncat(name,"EV ", sizeof(name)-1);
      strncat(name, taskName(VXTHISTASKID), sizeof(name)-1);
      status = db_start_events(evuser, name, ca_import, taskIdSelf());
      if(status != OK)
        abort();
#   endif                                                         

  }                                                               

  return ECA_NORMAL;
}

#ifdef vxWorks
ca_import(moms_tid)
int	moms_tid;
{
  int                   status;                                     
                                                                    
  status = taskVarAdd(VXTHISTASKID, &ca_static);                    
  if(status == ERROR)                                               
    abort();                                                        
                                                                    
  ca_static = (struct ca_static *) taskVarGet(moms_tid, &ca_static);
  if(ca_static == (struct ca_static*)ERROR)                         
    abort();          

  return ECA_NORMAL;                                              
}
#endif


#ifdef vxWorks
static
ca_add_task_variable()
{                                                 
  static void 	ca_task_exit_tcbx();
  static char	ca_installed;
  int		status;

  status = taskVarAdd(VXTHISTASKID, &ca_static);                          
  if(status == ERROR)   
    abort();

  /* 
	only one delete hook for all CA tasks 
  */
  if(vxTas(&ca_installed)){  
    status = taskDeleteHookAdd(ca_task_exit_tcbx);                  
    if(status == ERROR){                                        
      logMsg("ca_init_task: could not add CA delete routine\n");
      abort();
    }
  }

  return ECA_NORMAL;
}
#endif



/*
	NOTE: only call this routine if you wish to free resources
	prior to task exit- The following is executed routinely at 
	task exit by the OS.
*/
ca_task_exit
#ifdef VAXC
(void)
#else
()
#endif
{
  void ca_task_exit_tid();

# ifdef vxWorks
  ca_task_exit_tid(VXTHISTASKID);
# else
  ca_task_exit_tid(0);
#endif

  return ECA_NORMAL;
}


#ifdef vxWorks                                                 
static void
ca_task_exit_tcbx(ptcbx)
TCBX *ptcbx;
{
  void ca_task_exit_tid();

  ca_task_exit_tid((int) ptcbx->taskId);
}
#endif


static void
ca_task_exit_tid(tid)
int				tid;
{
  int 				i;
  chid				chix;
  evid				monix;
  struct ca_static		*ca_temp;

# ifdef vxWorks
    ca_temp = (struct ca_static *) taskVarGet(tid, &ca_static);
    if(ca_temp == (struct ca_static *) ERROR)
      return;

# else
    ca_temp = ca_static;
# endif

  /* if allready in the exit state just NOOP */
# ifdef vxWorks
    if(!vxTas(&ca_temp->ca_exit_in_progress))
      return;
# else
    if(ca_temp->ca_exit_in_progress)
      return;
    ca_temp->ca_exit_in_progress = TRUE;
# endif

  if(ca_temp){
#   ifdef vxWorks
      logMsg("ca_task_exit: Removing task %x from CA\n",tid);
#   endif

/**********
    LOCK;
***********/

    for(i=0; i< ca_temp->ca_nxtiiu; i++){
      if(socket_close(ca_temp->ca_iiu[i].sock_chan)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt iiu list- close");
      if(free(ca_temp->ca_iiu[i].send)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt iiu list- send free");
      if(free(ca_temp->ca_iiu[i].recv)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt iiu list- recv free");
#     ifdef vxWorks
        taskDelete(ca_temp->ca_iiu[i].recv_tid);
#     endif
    }

    while(monix = (evid) lstGet(&ca_temp->ca_eventlist)){
#     ifdef vxWorks
      {int status;
	if(monix->chan->iocix == LOCAL_IIU){
          status = db_cancel_event(monix+1);
          if(status==ERROR)
            abort();
	}
      }
#     endif
      if(free(monix)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt evid list");
    }

#   ifdef vxWorks
    {int status;
      status = db_close_events(ca_temp->ca_evuser);
      if(status==ERROR)
        SEVCHK(ECA_INTERNAL, "could not close event facility by id");
    }

    if(ca_temp->ca_event_buf)
      if(free(ca_temp->ca_event_buf))
	abort();
#   endif

    while(chix = (chid) lstGet(&ca_temp->ca_chidlist_conn)){
#     ifdef vxWorks
        if(chix->iocix == LOCAL_IIU)
          if(free(chix->paddr)<0)
	    SEVCHK(ECA_INTERNAL, "Corrupt paddr in chid list");
#     endif
      if(free(chix)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt connected chid list");
    }

    while(chix = (chid) lstGet(&ca_temp->ca_chidlist_pend)){
      if(free(chix)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt pending chid list");
    }

    while(chix = (chid) lstGet(&ca_temp->ca_chidlist_noreply)){
      if(free(chix)<0)
	SEVCHK(ECA_INTERNAL, "Corrupt unanswered chid list");
    }

/**************
    UNLOCK;
***************/

    if(free(ca_temp)<0)
      SEVCHK(ECA_INTERNAL, "Unable to free task static variables");


/*
	Only remove task variable if user is call this 
	from ca_task_exit with the task id = 0

	This is because if I delete the task variable from
	a vxWorks exit handler it botches vxWorks task exit
*/
#   ifdef vxWorks                                                 
    if(tid == VXTHISTASKID)
    {int status;
      status = taskVarDelete(tid, &ca_static);                          
      if(status == ERROR)  
        SEVCHK(ECA_INTERNAL, "Unable to remove ca_static from task var list");

    }
#   endif

    ca_static = (struct ca_static *) NULL;

  }
}


ca_array_build
#ifdef VAXC
(
char			*name_str,
chtype			get_type,
unsigned int		get_count,
chid			*chixptr,
void			*pvalue
)
#else
(name_str,get_type,get_count,chixptr,pvalue)
char 			*name_str;
chtype			get_type;
unsigned int		get_count;
chid			*chixptr; 
void			*pvalue;
#endif
{
  long			status;
  chid			chix;
  int			strcnt;   
  unsigned short	castix;
  struct in_addr	broadcast_addr();

  /*
  make sure that chix is NULL on fail
  */
  *chixptr = NULL;

  INITCHK;

  if(INVALID_DB_REQ(get_type) && get_type != TYPENOTCONN)
    return ECA_BADTYPE;

  /*
	Cant check his count on build since we dont know the 
	native count yet.
  */

  /* Put some reasonable limit on user supplied string size */
  strcnt = strlen(name_str) + 1;
  if(strcnt > MAX_STRING_SIZE || strcnt==1 )
    return ECA_STRTOBIG;


  if(strcnt==1 )
    return ECA_BADSTR;


  /* Make sure string is aligned to short word boundaries for MC68000 */
  strcnt = BI_ROUND(strcnt)<<1;

  /* allocate CHIU (channel in use) block */
  /* also allocate enough for a copy of this msg */
  *chixptr = chix = (chid) malloc(sizeof(*chix) + strcnt);
  if(!chix)
    return ECA_ALLOCMEM;


# ifdef vxWorks    /* only for IO machines */
  {
    struct db_addr	tmp_paddr;

    /* Find out quickly if channel is on this node  */
    status = db_name_to_addr(name_str, &tmp_paddr);
    if(status==OK){
      chix->paddr = (void *) malloc(sizeof(struct db_addr));
      if(!chix->paddr)
	abort();

      *(struct db_addr *)chix->paddr = tmp_paddr;
      chix->type =  ( (struct db_addr *)chix->paddr )->field_type;
      chix->count = ( (struct db_addr *)chix->paddr )->no_elements;
      chix->iocix = LOCAL_IIU;
      strncpy(chix+1, name_str, strcnt);

      /* check for just a search */
      if(get_count && get_type != TYPENOTCONN){ 
        status = db_get_field(&tmp_paddr, get_type, pvalue, get_count, FALSE);
        if(status!=OK){
	  free(chix->paddr);
	  *chixptr = (chid) free(chix);
	  return ECA_GETFAIL;
	}
      }
      LOCK;
      lstAdd(&chidlist_conn, chix);
      UNLOCK;

      return ECA_NORMAL;
    }
  }
# endif                                                         

  LOCK;

  status = alloc_ioc	(
			broadcast_addr(),
			IPPROTO_UDP,
			&chix->iocix
			);
  if(~status & CA_M_SUCCESS){
    *chixptr = (chid) free(chix);
    goto exit;
  }

  chix->type = TYPENOTCONN;	/* invalid initial type 	*/
  chix->paddr = (void *)NULL;	/* invalid initial paddr	*/

  /* save stuff for build retry if required */
  chix->build_type = get_type;
  chix->build_count = get_count;
  chix->build_value = (void *) pvalue;
  chix->name_length = strcnt;

  /* Save this channels name for retry if required */
  strncpy(chix+1, name_str, strcnt);

  lstAdd(&chidlist_pend, chix);
  status = build_msg(chix, FALSE);

exit:

  UNLOCK;

  return status;

}



/*

NOTE:	*** lock must be on while in this routine ***

*/
build_msg(chix,retry)
chid			chix;
int			retry;
{
  register int size; 
  register int cmd;

  if(chix->build_count && chix->build_type != TYPENOTCONN){ 
    size = chix->name_length + sizeof(struct extmsg);
    cmd = IOC_BUILD;
  }
  else{
    size = chix->name_length;
    cmd = IOC_SEARCH;
  }


  SNDBEG_VARIABLE_NOLOCK(size)

    mptr->m_cmmd 	= htons(cmd);
    mptr->m_postsize 	= htons(size);
    mptr->m_available 	= (long) chix;
    mptr->m_type	= 0;
    mptr->m_count	= 0; 
    mptr->m_paddr	= 0;

    if(cmd == IOC_BUILD){
      /*	msg header only on db read req	*/
      mptr++;
      mptr->m_postsize	= 0;
      mptr->m_cmmd 	= htons(IOC_READ);
      mptr->m_type	= htons(chix->build_type);
      mptr->m_count	= htons(chix->build_count); 
      mptr->m_available = (int) chix->build_value;
      mptr->m_paddr	= chix->paddr;

      if(!retry)
        SETPENDRECV;
    }

    /* channel name string */
    /* forces a NULL at the end because strcnt is allways >= strlen + 1 */ 
    mptr++;
    strncpy(mptr, chix+1, chix->name_length);

    if(!retry)
      SETPENDRECV;

  SNDEND_NOLOCK;

  return ECA_NORMAL;

}



ca_array_get
#ifdef VAXC
(
chtype				type,
unsigned int			count,
chid				chix,
register void			*pvalue
)
#else
(type,count,chix,pvalue)
chtype				type;
unsigned int			count;
chid				chix;
register void			*pvalue;
#endif
{
  CHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  if(count > chix->count)
    return ECA_BADCOUNT;

# ifdef vxWorks
  {
    int status;

    if(chix->iocix == LOCAL_IIU){
      status = db_get_field(                  
                                chix->paddr,    
                                type,     
                                pvalue,  
                                count,
				FALSE);   
      if(status==OK)                           
	return ECA_NORMAL;
      else
        return ECA_GETFAIL;
    }
  }
# endif


  SNDBEG(extmsg)

    /*	msg header only on db read req					*/
    mptr->m_postsize	= 0;
    mptr->m_cmmd 	= htons(IOC_READ);
    mptr->m_type	= htons(type);
    mptr->m_available 	= (long) pvalue;
    mptr->m_count 	= htons(count);
    mptr->m_paddr	= chix->paddr;
  SNDEND

  SETPENDRECV;
  return ECA_NORMAL;
}


ca_array_put
#ifdef VAXC
(
chtype				type,
unsigned int			count,
chid				chix,
void				*pvalue
)
#else
(type,count,chix,pvalue)
register chtype			type;
unsigned int			count;
chid				chix;
register void 			*pvalue;
#endif
{
  register int  		postcnt;
  register int			tmp;
  register void			*pdest;
  register int 			i;
  register int			size;

  CHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  if(count > chix->count)
    return ECA_BADCOUNT;

# ifdef vxWorks
  {
    int status;

    if(chix->iocix == LOCAL_IIU){
      status = db_put_field(    chix->paddr,  
                                type,   
                                pvalue,
                                count);            
      if(status==OK)
	return ECA_NORMAL;
      else
        return ECA_PUTFAIL;
    }
  }
# endif

  size = dbr_value_size[type];
  postcnt = dbr_size[type] + size*(count-1);
  SNDBEG_VARIABLE(postcnt)
    pdest = (void *)(mptr+1);

    for(i=0; i< count; i++){

      switch(type){
      case	DBR_INT:
      case	DBR_ENUM:
        *(short *)pdest = htons(*(short *)pvalue);
        break;

      case	DBR_FLOAT:
        /* most compilers pass all floats as doubles */
        htonf(pvalue, pdest);
        break;

      case	DBR_STRING:

	/*
	If arrays of strings are passed I assume each is MAX_STRING_SIZE
	or less and copy count * MAX_STRING_SIZE bytes into the outgoing
	IOC message.
	*/
	if(count>1){
          memcpy(pdest,pvalue,postcnt);
	  goto array_loop_exit;
  	}

        /* Put some reasonable limit on user supplied string size */
	{ unsigned int strsize;
          strsize = strlen(pvalue) + 1;
          /* Make sure string is aligned to short word boundaries for MC68000 */
          /* forces a NULL at the end because strcnt is allways >= strlen + 1 */ 
          strsize = BI_ROUND(strsize+1)<<1;
          if(strsize>MAX_STRING_SIZE)
            return ECA_STRTOBIG;
          memcpy(pdest,pvalue,strsize);
	}
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
#       ifdef VAX
          (*cvrt[type])(pvalue,pdest,TRUE,count);
#       else
          if(pdest != pvalue)
            memcpy(pdest,pvalue,postcnt);
#       endif

	goto array_loop_exit;
      default:
        return ECA_BADTYPE;
      }
      (char *) pdest	+= size;
      (char *) pvalue 	+= size;
    }

array_loop_exit:

    mptr->m_cmmd 	= htons(IOC_WRITE);
    mptr->m_postsize	= htons(postcnt);
    mptr->m_type	= htons(type);
    mptr->m_count 	= htons(count);
    mptr->m_paddr	= chix->paddr;
    mptr->m_available	= (long) chix;
  SNDEND

  return ECA_NORMAL;
}


/*
	Specify a event subroutine to be run once when pending IO completes

	1) event sub runs right away if no IO pending
	2) event sub runs when pending IO completes otherwise

*/
ca_add_io_event
#ifdef VAXC
(
void 		(*ast)(),
void 		*astarg
)
#else
(ast,astarg)
void				(*ast)();
void				*astarg;
#endif
{
  register struct pending_io_event	*pioe;

  if(pndrecvcnt<1)
    (*ast)(astarg);
  else{
    LOCK;
    pioe = (struct pending_io_event *) malloc(sizeof(*pioe));
    if(!pioe)
      return ECA_ALLOCMEM;
    pioe->io_done_arg = astarg;
    pioe->io_done_sub = ast;
    lstAdd(&ioeventlist,pioe);
    UNLOCK;
  }

  return ECA_NORMAL;
}




ca_add_array_event
#ifdef VAXC
(
chtype 		type,
unsigned int	count,
chid 		chix,
void 		(*ast)(),
void 		*astarg,
ca_real		p_delta,
ca_real		n_delta,
ca_real		timeout,
evid 		*monixptr
)
#else
(type,count,chix,ast,astarg,p_delta,n_delta,timeout,monixptr)
chtype				type;
unsigned int			count;
chid				chix;
void				(*ast)();
void				*astarg;
ca_real				p_delta;
ca_real				n_delta;
ca_real				timeout;
evid				*monixptr;
#endif
{
  register evid			monix;

  CHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  if(count > chix->count)
    return ECA_BADCOUNT;

  if(count == 0)
    count = chix->count;


  /*	AST descriptor	*/
# ifdef vxWorks
    monix = (evid) malloc(sizeof(*monix)+db_sizeof_event_block());
# else
    monix = (evid) malloc(sizeof(*monix));
# endif
  if(!monix)
    return ECA_ALLOCMEM;

  /* they dont have to supply one if they dont want to */
  if(monixptr)
    *monixptr = monix;

  monix->chan		= chix;
  monix->usr_func	= ast;
  monix->usr_arg	= astarg;
  monix->type		= type;
  monix->count		= count;

  LOCK;
    /* Place in the channel list */
    lstAdd(&eventlist, monix);
  UNLOCK;

# ifdef vxWorks
  {
    struct monops 	mon;
    register int	status;
    void		ca_event_handler();

    if(chix->iocix == LOCAL_IIU){
      status = db_add_event(	evuser,
				chix->paddr,
				ca_event_handler,
				monix,
				DBE_VALUE | DBE_ALARM,
				monix+1);
      if(status == ERROR)
	return ECA_DBLCLFAIL;

      /* force event to be called at least once */
      db_post_single_event(monix+1);
      return ECA_NORMAL;

    } 
  }
# endif

  SNDBEG(monops)

    /*	msg header	*/
    mptr->m_header.m_cmmd 	= htons(IOC_EVENT_ADD);
    mptr->m_header.m_available 	= (long) monix;
    mptr->m_header.m_postsize	= htons(sizeof(struct mon_info));
    mptr->m_header.m_type	= htons(type);
    mptr->m_header.m_count	= htons(count);
    mptr->m_header.m_paddr	= chix->paddr;

    /*	msg body	*/
    htonf(&p_delta, &mptr->m_info.m_hval);
    htonf(&n_delta, &mptr->m_info.m_lval);
    htonf(&timeout, &mptr->m_info.m_toval);

  SNDEND
  return ECA_NORMAL;
}


#ifdef vxWorks
static void
ca_event_handler(monix, paddr, hold)
register evid 		monix;
register struct db_addr	*paddr;
int			hold;
{
  register int 		status;
  register int		count = monix->count;
  register chtype	type = monix->type;
  union db_access_val	valbuf;
  void			*pval;
  register unsigned	size;

  if(type == paddr->field_type){
    pval = paddr->pfield;
    status = OK;
  }
  else{
    if(count == 1)
      size = dbr_size[type];
    else
      size = (count-1)*dbr_value_size[type]+dbr_size[type];

    if( size <= sizeof(valbuf) )
      pval = (void *) &valbuf;
    else if( size <= event_buf_size )
      pval = event_buf;
    else{
      if(free(event_buf))
	abort();
      pval = event_buf = (void *) malloc(size);
      if(!pval)
        abort();
      event_buf_size = size;
    }
    status = db_get_field(	paddr,
				type,
				pval,
				count,
				TRUE);
  }

  if(status == ERROR)
    logMsg("CA post event get error\n");
  else
    (*monix->usr_func)(		monix->usr_arg, 
				monix->chan, 
				type, 
				count,
				pval);

  return;
}
#endif



#ifdef ZEBRA
ca_clear
#ifdef VAXC
(register chid	chid)
#else
(chid)
register chid	chid;
#endif
{
 
  CHIXCHK(chid);


/*
  cancel outstanding events
*/

/* 
  free resources on the IOC
*/

/* 
  free resources here when IOC confirms
*/


# ifdef vxWorks
    if(chix->iocix == LOCAL_IIU){
      register int status;
      status = db_cancel_event(monix+1);
      if(status == ERROR)
	return ECA_DBLCLFAIL;
      LOCK;
      lstDelete(&eventlist, monix);
      UNLOCK;
      if(free(monix)<0)
	return ECA_INTERNAL;
      return ECA_NORMAL;
    } 
# endif

  SNDBEG(extmsg)

    /*	msg header	*/
    mptr->m_cmmd 	= htons(IOC_EVENT_CANCEL);
    mptr->m_available 	= (long) monix;
    mptr->m_postsize	= 0;
    mptr->m_type	= chix->type;
    mptr->m_count 	= chix->count;
    mptr->m_paddr	= chix->paddr;

    /* 	NOTE: I free the monitor block only after confirmation from IOC */

  SNDEND
  return ECA_NORMAL;
}
#endif


ca_clear_event
#ifdef VAXC
(register evid	monix)
#else
(monix)
register evid			monix;
#endif
{
  register chid			chix = monix->chan;	/* for SNDBEG */ 
 
  CHIXCHK(chix);

# ifdef vxWorks
    if(chix->iocix == LOCAL_IIU){
      register int status;
      status = db_cancel_event(monix+1);
      if(status == ERROR)
	return ECA_DBLCLFAIL;
      LOCK;
      lstDelete(&eventlist, monix);
      UNLOCK;
      if(free(monix)<0)
	return ECA_INTERNAL;
      return ECA_NORMAL;
    } 
# endif

  SNDBEG(extmsg)

    /*	msg header	*/
    mptr->m_cmmd 	= htons(IOC_EVENT_CANCEL);
    mptr->m_available 	= (long) monix;
    mptr->m_postsize	= 0;
    mptr->m_type	= chix->type;
    mptr->m_count 	= chix->count;
    mptr->m_paddr	= chix->paddr;

    /* 	NOTE: I free the monitor block only after confirmation from IOC */

  SNDEND
  return ECA_NORMAL;
}




/*
NOTE:
	This call not implemented with select on VMS or vxWorks
	due to its poor implementation in those environments. 

        Wallangong's SELECT() does not
	return early if a timeout is specified and IO occurs before the end
	of the timeout. Above call works because of the short timeout interval.

	Another wallongong bug will cause problems if the timeout in 
	secs is specified large. I have fixed this by keeping track of 
	large timeouts myself.

	JH
*/

/*	NOTE: DELAYVAL must be less than 1.0		*/
#define DELAYVAL	0.150		/* 150 mS	*/
#define CASTTMO		0.150		/* 150 mS	*/
#define LKUPTMO		0.015		/* 5 mS 	*/

#ifdef	VMS
#define SYSFREQ		10000000	/* 10 MHz	*/
#else
# ifdef vxWorks
#  define SYSFREQ	sysClkRateGet()	/* usually 60 Hz	*/
#  define time(A) 	(tickGet()/sysfreq)
# else
#  define SYSFREQ	1000000		/* 1 MHz		*/
# endif
#endif

/*
NOTE:
1) 	Timeout equal to zero blocks indefinately until
	all oustanding IO is complete.
2)	Timeout other than zero will return with status = ECA_TIMEOUT
	when that time period expires and IO is still outstanding.
*/
ca_pend_io
#ifdef VAXC
(ca_real timeout)
#else
(timeout)
ca_real			timeout;
#endif
{
  void			send_msg();
  time_t 		beg_time;
  time_t		clock();
  int			chidcnt;
  int			chidstart;

# ifdef vxWorks
  static int		sysfreq;

  if(!sysfreq)
    sysfreq = SYSFREQ;
# endif

  INITCHK;

  if(post_msg_active)
    return ECA_EVDISALLOW;


  /*	Flush the send buffers	*/
  LOCK;
  send_msg();
  UNLOCK;

  if(pndrecvcnt<1)
    return ECA_NORMAL;

  if(timeout<0.0)
    return ECA_TIMEOUT;

  /* The number of polls to wait on a chid */
  chidstart		= (CASTTMO+pndrecvcnt*LKUPTMO)/DELAYVAL + 1;
  chidcnt 		= chidstart;

  beg_time = time(NULL);

  while(TRUE){
#   ifdef UNIX
    {
      void		recv_msg_select();
      struct timeval	itimeout;

      itimeout.tv_usec 	= SYSFREQ * DELAYVAL;	
      itimeout.tv_sec  	= 0;
      LOCK;
      recv_msg_select(&itimeout);
      UNLOCK;
    }
#   endif
#   ifdef vxWorks
    {
      int dummy;
      unsigned int clockticks = DELAYVAL * sysfreq;
      vrtxPend(&io_done_flag,clockticks,&dummy);
    }
#   endif
#   ifdef VMS
    {
      int 		status; 
      unsigned int 	systim[2]={-SYSFREQ*DELAYVAL,~0};

      status = sys$setimr(io_done_flag, systim, NULL, MYTIMERID, NULL);
      if(status != SS$_NORMAL)
        lib$signal(status);
      status = sys$waitfr(io_done_flag);
      if(status != SS$_NORMAL)
        lib$signal(status);
    }    
#   endif
    if(pndrecvcnt<1)
      return ECA_NORMAL;

    chidcnt--;      
    if(chidcnt<1){
      if(chidlist_pend.count)
        chid_retry(TRUE);
      chidstart += chidstart;
      chidcnt = chidstart;
    }  
    if(timeout != 0.0)
      if(timeout<time(NULL)-beg_time){
	int i;
        if(chidlist_pend.count)
          chid_retry(FALSE);
	/* move unanswered chids to a waiting list */
	lstConcat(&chidlist_noreply, &chidlist_pend);
	/* set pending IO count back to zero and notify  */
	/* send a sync to each IOC and back */
	/* dont count reads until we recv the sync */
	LOCK;
	for(i=BROADCAST_IIU+1;i<nxtiiu;i++){	
	  struct ioc_in_use	*piiu = &iiu[i];
	  piiu->cur_read_seq++;
  	  SND_BEG_NOLOCK(extmsg, piiu)
    	    *mptr 		= nullmsg;
    	    mptr->m_cmmd 	= htons(IOC_READ_SYNC);
  	  SNDEND_NOLOCK
	}
	pndrecvcnt = 0;
	UNLOCK;

        return ECA_TIMEOUT;
      }
  }    

}


/*
	NOTE:
	1)	Timeout of zero causes user process to pend indefinately for 
		events.
	2)	Timeout other than zero specifies a time to pend for events
		even if no IO is outstanding.
*/
ca_pend_event
#ifdef VAXC
(ca_real timeout)
#else
(timeout)
ca_real			timeout;
#endif
{
  int			status;
  void			send_msg();
# ifdef vxWorks
  static int		sysfreq;

  if(!sysfreq)
    sysfreq = SYSFREQ;
# endif

  INITCHK;

  if(post_msg_active)
    return ECA_EVDISALLOW;

  if(timeout<0.0)
    return ECA_TIMEOUT;

  /*	Flush the send buffers			*/
  LOCK;
  send_msg();
  UNLOCK;

# ifdef vxWorks
  {
    unsigned int clockticks = timeout * sysfreq;

    if(timeout == 0.0){
      SEMAPHORE sem;
      semInit(&sem);
      while(TRUE)
	semTake(&sem);
    }

    taskDelay(clockticks);

  }
# endif


# ifdef VMS
  {
    float  mytimeout = timeout;	/* conservative */

    if(timeout == 0.0)
      while(TRUE)
	sys$hiber();

    status = lib$wait(&mytimeout);
    if(status != SS$_NORMAL)
      lib$signal(status);
  }    
# endif


# ifdef UNIX
  /*
  NOTE: locking here as a comment only in case someone does this on a multi
	threaded UNIX.
  */
  {
    struct timeval	itimeout;
    unsigned		beg_time;
    unsigned		elapsed;
    void		recv_msg_select();

    if(timeout == 0.0)
      while(TRUE){
        LOCK;
        recv_msg_select(NULL);
        UNLOCK;
      }

    beg_time = time(NULL);
    elapsed = 0;
    while(TRUE){
      float delay = timeout-elapsed;
      itimeout.tv_usec 	= (delay- ((int)delay))*SYSFREQ;	
      itimeout.tv_sec  	= delay;
      LOCK;
      recv_msg_select(&itimeout);
      UNLOCK;
      elapsed = time(NULL)-beg_time;
      if(elapsed>timeout)
        break;
    }
  }
# endif

  return ECA_TIMEOUT;
}




ca_flush_io
#ifdef VAXC
(void)
#else
()
#endif
{
  void	send_msg();

  INITCHK;

  /*	Flush the send buffers			*/
  LOCK;
  send_msg();
  UNLOCK;

  return ECA_NORMAL;
}


		ca_signal(ca_status,message)
int		ca_status;
char		*message;
{
  static  char  *severity[] = 
		{
		"Warning",
		"Success",
		"Error",
		"Info",
		"Fatal",
		"Fatal",
		"Fatal",
		"Fatal"
		};

  if( CA_EXTRACT_MSG_NO(ca_status) >= NELEMENTS(ca_message_text) ){
    message = "corrupt status";
    ca_status = ECA_INTERNAL;
  }

  printf(
"CA.Diagnostic.....................................................\n");

  printf(
"    Message: [%s]\n", ca_message_text[CA_EXTRACT_MSG_NO(ca_status)]);

  if(message)
    printf(
"    Severity: [%s] Context: [%s]\n", 
	severity[CA_EXTRACT_SEVERITY(ca_status)],
	message);
  else
    printf(
"    Severity: [%s]\n", severity[CA_EXTRACT_SEVERITY(ca_status)]);


  if( !(ca_status & CA_M_SUCCESS) ){
#   ifdef VMS
      lib$signal(0);
#   endif
#   ifdef vxWorks
      ti(taskIdCurrent);
      tt(taskIdCurrent);
#   endif
    abort();
  }

  printf(
"..................................................................\n");


}




ca_busy_message(piiu)
register struct ioc_in_use	*piiu; 
{
  void			send_msg();

  SND_BEG(extmsg, piiu)
    *mptr 		= nullmsg;
    mptr->m_cmmd 	= htons(IOC_EVENTS_OFF);
  SNDEND_NOLOCK
  send_msg();
  UNLOCK;

}

ca_ready_message(piiu)
register struct ioc_in_use	*piiu; 
{
  void			send_msg();

  SND_BEG(extmsg, piiu)
    *mptr 		= nullmsg;
    mptr->m_cmmd 	= htons(IOC_EVENTS_ON);
  SNDEND_NOLOCK
  send_msg();
  UNLOCK;

}
