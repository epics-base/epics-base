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
/*	3/90		Jeff Hill	Changed error message strings	*/
/*					to allocate in this module only	*/
/*					& further improved vx task exit	*/
/*	021291 joh	fixed potential problem with strncpy and 	*/
/*			the vxWorks task name.				*/
/*	022791 mda	integration of ANL/LANL db code			*/
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
/*	1)	Check timeouts on ca_pend for values to large		*/
/*	2)	Allow them to recieve channel A when channel B changes	*/
/*									*/
/************************************************************************/
/*_end									*/

/* the following support riu.h */
#ifdef VMS
#include		stsdef.h
#include		ssdef.h
#include		psldef.h
#include		prcdef.h
#include 		descrip.h
#else
#endif

#include		<vxWorks.h>

#ifdef vxWorks
#include		<taskLib.h>
#include		<task_params.h>
#endif

/*
 * allocate error message string array 
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

/*
 * allocate db_access message strings here
 */
#define DB_TEXT_GLBLSOURCE
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

#define SNDBEG_NOLOCK(STRUCTURE) \
SND_BEG_NOLOCK(STRUCTURE, &iiu[chix->iocix])

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
/*
	valid type check filters out disconnected channel
*/

#define CHIXCHK(CHIX) \
{ \
  register unsigned short iocix = (CHIX)->iocix; \
  if(	iocix == BROADCAST_IIU || \
	(iocix >= nxtiiu && iocix != LOCAL_IIU) || \
	INVALID_DB_REQ((CHIX)->type)  ) \
    return ECA_BADCHID; \
}

/* type to detect them using deallocated channel in use block */
#define TYPENOTINUSE (-2)

/* allow them to add monitors to a disconnected channel */
#define LOOSECHIXCHK(CHIX) \
{ \
  register unsigned short iocix = CHIX->iocix; \
  if( 	(iocix >= nxtiiu && iocix != LOCAL_IIU) || \
	(CHIX)->type==TYPENOTINUSE ) \
    return ECA_BADCHID; \
}

#define INITCHK  if(!ca_static) ca_task_initialize();


static struct extmsg	nullmsg;


void 	ca_default_exception_handler();
void	*db_init_events();
void	ca_default_exception_handler();
void	ca_repeater_task();
int	ca_import();

#ifdef vxWorks
static check_for_fp();
static ca_add_task_variable();
static void ca_task_exit_tid();
static void issue_get_callback();
static void ca_event_handler();
#endif


/*
 *
 *	CA_TASK_INITIALIZE
 *
 *
 */
ca_task_initialize
#ifdef VAXC
(void)
#else
()
#endif
{
  int 		status;
  char		name[15];

  if(!ca_static){                                                 

    /*
	Spawn the repeater task as needed
    */
    if(!repeater_installed()){
#   ifdef UNIX
	/*
	 * This method of spawn places a path name in the code so it has been
	 * avoided for now
	 * 
	 * use of fork makes ca and repeaters image larger than required 
	 * 
	 * system("/path/repeater &");
	 */
	status = fork();
	if(status<0)
		SEVCHK(ECA_INTERNAL,"couldnt spawn the repeater task");
	if(!status)
		ca_repeater_task();
#   endif
#   ifdef vxWorks
	status = taskSpawn(
			CA_REPEATER_NAME,
			CA_REPEATER_PRI,
			CA_REPEATER_OPT,
			CA_REPEATER_STACK,
			ca_repeater_task
			);
      	if(status<0)
		SEVCHK(ECA_INTERNAL,"couldnt spawn the repeater task");
#   endif
#   ifdef VMS
      {
      	static 		$DESCRIPTOR(image, "LAACS_CA_REPEATER");
      	static 		$DESCRIPTOR(name, "CA repeater");
      	static 		$DESCRIPTOR(in,	"CONSOLE");
      	static 		$DESCRIPTOR(out, "CONSOLE");
      	static 		$DESCRIPTOR(err, "CONSOLE");
	int		privs = ~0;
	unsigned long	pid;

      	status = sys$creprc(
			&pid,
			&image,
			&in,
			&out,
			&err,
			&privs,
			NULL,
			&name,
			4,	/*base priority */
			NULL,
			NULL,
			PRC$M_DETACH);
      	if(status != SS$_NORMAL)
        	lib$signal(status);
printf("spawning repeater %x\n", pid);
      }
#   endif
    }

#   ifdef vxWorks
      check_for_fp();
      if(ca_add_task_variable() == ERROR)
        abort();
#   endif

    ca_static = (struct ca_static *) calloc(1, sizeof(*ca_static));  
    if(!ca_static)
      abort();
              
    ca_static->ca_exception_func = ca_default_exception_handler;
    ca_static->ca_exception_arg = NULL;

#   ifdef VMS
      status = lib$get_ef(&io_done_flag);
      if(status != SS$_NORMAL)
	lib$signal(status);
#   endif

#   ifdef vxWorks
	{
  		char		*taskName();
      		ca_static->ca_tid = taskIdSelf();

      		FASTLOCKINIT(&client_lock);

      		evuser = (void *) db_init_events();
      		if(!evuser)
        		abort();

		strcpy(name, "EV ");
		strncat(
			name, 
			taskName(VXTHISTASKID),
			sizeof(name)-strlen(name)-1);
      		status = db_start_events(
				evuser, 
				name, 
				ca_import, 
				taskIdSelf());
      		if(status != OK)
       			abort();
	}
#   endif                                                         

  }                                                               

  return ECA_NORMAL;
}


/*
 *
 *	CHECK_FOR_FP()
 *
 *
 */
#ifdef vxWorks
static check_for_fp()
{
    {
      int options;

      if(taskOptionsGet(taskIdSelf(),&options)==ERROR)
	abort();
      if(!(options & VX_FP_TASK)){
	ca_signal(ECA_NEEDSFP, NULL);

      	if(taskOptionsSet(taskIdSelf(),VX_FP_TASK,VX_FP_TASK)==ERROR)
	  abort();
      }
    }
}
#endif


/*
 *
 *	CA_IMPORT()
 *
 *
 */
#ifdef vxWorks
ca_import(moms_tid)
int	moms_tid;
{
  int                   status; 
  struct ca_static	*pcas;                             
                                                                    
  pcas = (struct ca_static*) taskVarGet(moms_tid, &ca_static);
  if(pcas == (struct ca_static*) ERROR)                         
    return ECA_NOCACTX;   

  status = taskVarAdd(VXTHISTASKID, &ca_static);                    
  if(status == ERROR)                                               
    abort();                                                        
                                                                    
  ca_static = pcas;

  check_for_fp();

  return ECA_NORMAL;                                                  
}
#endif





/*
 *
 *	CA_ADD_TASK_VARIABLE()
 *
 */
#ifdef vxWorks
static ca_add_task_variable()
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
 *	CA_TASK_EXIT()
 *
 * 	call this routine if you wish to free resources prior to task
 * 	exit- ca_task_exit() is also executed routinely at task exit.
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
  ca_task_exit_tid(NULL);
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

/*
 *
 *	CA_TASK_EXIT_TID
 *	attempts to release all resources alloc to a channel access client
 *
 *	NOTE: on vxWorks if a CA task is deleted or crashes while a 
 *	lock is set then a deadlock will occur when this routine is called.
 *
 */
static void
ca_task_exit_tid(tid)
int				tid;
{
  int 				i;
  chid				chix;
  struct ca_static		*ca_temp;
  evid				monix;
  int 				status;

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

    LOCK;
    /*
	Fist I must stop any source of further activity
	on vxWorks
    */

    /*
    	The main task is prevented from further CA activity
   	 by the LOCK placed above
    */

    /*
	stop socket recv tasks
    */
#   ifdef vxWorks
    for(i=0; i< ca_temp->ca_nxtiiu; i++){
	/* 	
		dont do a task delete if the 
		exit handler is running for this task
		- it botches vxWorks -
	*/
	if(tid != ca_temp->ca_iiu[i].recv_tid)
      		taskDelete(ca_temp->ca_iiu[i].recv_tid);
    }
#   endif

    /*
	Cancel all local events
    */
#   ifdef vxWorks
    chix = (chid) &ca_temp->ca_local_chidlist;
    while(chix = (chid) chix->node.next)
      while(monix = (evid) lstGet(&chix->eventq)){
        status = db_cancel_event(monix+1);
        if(status==ERROR)
          abort();
        if(free(monix)<0)
	  ca_signal(ECA_INTERNAL, "Corrupt conn evid list");
      }
#   endif

    /*
	All local events must be canceled prior to closing the 
	local event facility
    */
#   ifdef vxWorks
    {
      status = db_close_events(ca_temp->ca_evuser);
      if(status==ERROR)
        ca_signal(ECA_INTERNAL, "could not close event facility by id");
    }

    if(ca_temp->ca_event_buf)
      if(free(ca_temp->ca_event_buf))
	abort();
#   endif

    /*
	after activity eliminated:
    */
    /*
	close all sockets before clearing chid blocks
	and remote event blocks
    */
    for(i=0; i< ca_temp->ca_nxtiiu; i++){
      if(ca_temp->ca_iiu[i].conn_up)
        if(socket_close(ca_temp->ca_iiu[i].sock_chan)<0)
	  ca_signal(ECA_INTERNAL, "Corrupt iiu list- at close");
      if(free(ca_temp->ca_iiu[i].send)<0)
	ca_signal(ECA_INTERNAL, "Corrupt iiu list- send free");
      if(free(ca_temp->ca_iiu[i].recv)<0)
	ca_signal(ECA_INTERNAL, "Corrupt iiu list- recv free");
    }

    /* 
	remove remote chid blocks and event blocks
    */
    for(i=0; i< ca_temp->ca_nxtiiu; i++){
      while(chix = (chid) lstGet(&ca_temp->ca_iiu[i].chidlist)){
        while(monix = (evid) lstGet(&chix->eventq)){
          if(free(monix)<0)
	    ca_signal(ECA_INTERNAL, "Corrupt conn evid list");
        }
        if(free(chix)<0)
	  ca_signal(ECA_INTERNAL, "Corrupt connected chid list");
      }
    }

    /* 
	remove local chid blocks, paddr blocks, waiting ev blocks
    */
#   ifdef vxWorks
      while(chix = (chid) lstGet(&ca_temp->ca_local_chidlist))
        if(free(chix)<0)
	  ca_signal(ECA_INTERNAL, "Corrupt connected chid list");
      lstFree(&ca_temp->ca_dbfree_ev_list);
#   endif

    /* remove remote waiting ev blocks */
    lstFree(&ca_temp->ca_free_event_list);
    /* remove any pending read blocks */
    lstFree(&ca_temp->ca_pend_read_list);

    UNLOCK;

    if(free(ca_temp)<0)
      ca_signal(ECA_INTERNAL, "Unable to free task static variables");


/*
	Only remove task variable if user is calling this 
	from ca_task_exit with the task id = 0

	This is because if I delete the task variable from
	a vxWorks exit handler it botches vxWorks task exit
*/
#   ifdef vxWorks                                                 
    if(tid == taskIdSelf())
    {int status;
      status = taskVarDelete(tid, &ca_static);                          
      if(status == ERROR)  
        ca_signal(ECA_INTERNAL, "Unable to remove ca_static from task var list");
    }
#   endif

    ca_static = (struct ca_static *) NULL;

  }
}




/*
 *
 *	CA_BUILD_AND_CONNECT
 *
 *
 */
ca_build_and_connect
#ifdef VAXC
(
char			*name_str,
chtype			get_type,
unsigned int		get_count,
chid			*chixptr,
void			*pvalue,
void			(*conn_func)(),
void			*puser
)
#else
(name_str,get_type,get_count,chixptr,pvalue,conn_func,puser)
char 			*name_str;
chtype			get_type;
unsigned int		get_count;
chid			*chixptr; 
void			*pvalue;
void			(*conn_func)();
void			*puser;
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
  if(strcnt > MAX_STRING_SIZE)
    return ECA_STRTOBIG;
  if(strcnt == 1)
    return ECA_EMPTYSTR;


  if(strcnt==1 )
    return ECA_BADSTR;


  /* Make sure string is aligned to short word boundaries for MC68000 */
  strcnt = BI_ROUND(strcnt)<<1;

# ifdef vxWorks    /* only for IO machines */
  {
    struct db_addr			tmp_paddr;
    struct connection_handler_args	args;

    /* Find out quickly if channel is on this node  */
    status = db_name_to_addr(name_str, &tmp_paddr);
    if(status==OK){

      /* allocate CHIU (channel in use) block */
      /* also allocate enough for the channel name & paddr block*/
      *chixptr = chix = (chid) malloc(sizeof(*chix) 
			+ strcnt + sizeof(struct db_addr));
      if(!chix)
        abort();

      chix->paddr =  (void *) (strcnt + (char *)(chix+1));
      *(struct db_addr *)chix->paddr = tmp_paddr;
      chix->puser = puser;
      chix->connection_func = conn_func;
      chix->type =  ( (struct db_addr *)chix->paddr )->field_type;
      chix->count = ( (struct db_addr *)chix->paddr )->no_elements;
      chix->iocix = LOCAL_IIU;
      chix->state = cs_conn;
      lstInit(&chix->eventq);
      strncpy(chix+1, name_str, strcnt);

      /* check for just a search */
      if(get_count && get_type != TYPENOTCONN && pvalue){ 
        status=db_get_field(&tmp_paddr, get_type, pvalue, get_count);
        if(status!=OK){
	  free(chix->paddr);
	  *chixptr = (chid) free(chix);
	  return ECA_GETFAIL;
	}
      }
      LOCK;
      lstAdd(&local_chidlist, chix);
      UNLOCK;

      if(chix->connection_func){
		args.chid = chix;
		args.op = CA_OP_CONN_UP;

        	(*chix->connection_func)(args);
      }

      return ECA_NORMAL;
    }
  }
# endif            
                                             
  /* allocate CHIU (channel in use) block */
  /* also allocate enough for the channel name */
  *chixptr = chix = (chid) malloc(sizeof(*chix) + strcnt);
  if(!chix)
    return ECA_ALLOCMEM;

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

  chix->puser = puser;
  chix->connection_func = conn_func;
  chix->type = TYPENOTCONN;	/* invalid initial type 	*/
  chix->count = 0;		/* invalid initial count	*/
  chix->paddr = (void *)NULL;	/* invalid initial paddr	*/

  /* save stuff for build retry if required */
  chix->build_type = get_type;
  chix->build_count = get_count;
  chix->build_value = (void *) pvalue;
  chix->name_length = strcnt;
  chix->state = cs_never_conn;
  lstInit(&chix->eventq);

  /* Save this channels name for retry if required */
  strncpy(chix+1, name_str, strcnt);

  lstAdd(&iiu[BROADCAST_IIU].chidlist, chix);
  /* set the conn tries back to zero so this channel can be found */
  iiu[BROADCAST_IIU].nconn_tries = 0;

  status = build_msg(chix, FALSE, DONTREPLY);
  if(!chix->connection_func){
    SETPENDRECV;
    if(VALID_BUILD(chix)) 
      SETPENDRECV;
  }

exit:

  UNLOCK;

  return status;

}



/*
 *	BUILD_MSG()
 *
 * 	NOTE:	*** lock must be applied while in this routine ***
 * 
 */
build_msg(chix,reply_type)
chid			chix;
int			reply_type;
{
  register int size; 
  register int cmd;

  if(VALID_BUILD(chix)){ 
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
    mptr->m_available 	= (int) chix;
    mptr->m_type	= reply_type;
    mptr->m_count	= 0; 
    mptr->m_pciu	= (void *) chix;

    if(cmd == IOC_BUILD){
      /*	msg header only on db read req	*/
      mptr++;
      mptr->m_postsize	= 0;
      mptr->m_cmmd 	= htons(IOC_READ_BUILD);
      mptr->m_type	= htons(chix->build_type);
      mptr->m_count	= htons(chix->build_count); 
      mptr->m_available = (int) chix->build_value;
      mptr->m_pciu	= 0;


    }

    /* channel name string */
    /* forces a NULL at the end because strcnt is allways >= strlen + 1 */ 
    mptr++;
    strncpy(mptr, chix+1, chix->name_length);

  SNDEND_NOLOCK;

  return ECA_NORMAL;

}



/*
 *
 *	CA_ARRAY_GET()
 *
 *
 */
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
                                count );
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
    mptr->m_pciu	= chix->paddr;
  SNDEND

  SETPENDRECV;
  return ECA_NORMAL;
}


/*
 *
 *	CA_ARRAY_GET_CALLBACK()
 *
 *
 *
 */
ca_array_get_callback
#ifdef VAXC
(
chtype				type,
unsigned int			count,
chid				chix,
void				(*pfunc)(),
void				*arg
)
#else
(type,count,chix,pfunc,arg)
chtype		type;
unsigned int	count;
chid		chix;
void		(*pfunc)();
void		*arg;
#endif
{
  int 		status;
  evid 		monix;
  void		ca_event_handler();
  void		issue_get_callback();

  CHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  if(count > chix->count)
    return ECA_BADCOUNT;

# ifdef vxWorks
  if(chix->iocix == LOCAL_IIU){
    struct pending_event	ev;

    ev.usr_func = pfunc;
    ev.usr_arg = arg;
    ev.chan = chix;
    ev.type = type;
    ev.count = count;
    ca_event_handler(&ev, chix->paddr, NULL);
    return ECA_NORMAL;
  }
# endif

  LOCK;
  if(!(monix = (evid) lstGet(&free_event_list)))
    monix = (evid) malloc(sizeof *monix);

  if(monix){

    monix->chan		= chix;
    monix->usr_func	= pfunc;
    monix->usr_arg	= arg;
    monix->type		= type;
    monix->count	= count;

    lstAdd(&pend_read_list, monix);

    issue_get_callback(monix);

    status = ECA_NORMAL;
  }else
    status = ECA_ALLOCMEM;

  UNLOCK;
  return status;
}


/*
 *
 *	ISSUE_GET_CALLBACK()
 *	(lock must be on)
 */
static void issue_get_callback(monix)
register evid		monix;
{
    register chid chix = monix->chan;

    SNDBEG_NOLOCK(extmsg)

    /*	msg header only on db read notify req	*/
    mptr->m_cmmd 	= htons(IOC_READ_NOTIFY);
    mptr->m_type	= htons(monix->type);
    mptr->m_available 	= (long) monix;
    mptr->m_count 	= htons(monix->count);
    mptr->m_pciu	= chix->paddr;
    mptr->m_postsize	= 0;

    SNDEND_NOLOCK;
}


/*
 *	CA_ARRAY_PUT()
 *
 *
 */
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
      case	DBR_DOUBLE:
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
      case DBR_STS_DOUBLE:
      case DBR_STS_ENUM:
      case DBR_GR_INT:
      case DBR_GR_FLOAT:
      case DBR_GR_DOUBLE:
      case DBR_CTRL_INT:
      case DBR_CTRL_FLOAT:
      case DBR_CTRL_DOUBLE:
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
    mptr->m_pciu	= chix->paddr;
    mptr->m_available	= (long) chix;
  SNDEND

  return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for connection events
 *
 */
ca_change_connection_event
#ifdef VAXC
(
chid		chix,
void 		(*pfunc)()
)
#else
(chix, pfunc)
chid		chix;
void		(*pfunc)();
#endif
{

  	INITCHK;
  	LOOSECHIXCHK(chix);

	if(chix->connection_func == pfunc)
  		return ECA_NORMAL;

  	LOCK;
  	if(chix->type == TYPENOTCONN){
		if(!chix->connection_func){
    			CLRPENDRECV;
    			if(VALID_BUILD(chix)) 
    				CLRPENDRECV;
		}
		if(!pfunc){
    			SETPENDRECV;
    			if(VALID_BUILD(chix)) 
    				SETPENDRECV;
		}
	}
  	chix->connection_func = pfunc;
  	UNLOCK;

  	return ECA_NORMAL;
}


/*
 *	Specify an event subroutine to be run for asynch exceptions
 *
 */
ca_add_exception_event
#ifdef VAXC
(
void 		(*pfunc)(),
void 		*arg
)
#else
(pfunc,arg)
void				(*pfunc)();
void				*arg;
#endif
{

  INITCHK;
  LOCK;
  if(pfunc){
    ca_static->ca_exception_func = pfunc;
    ca_static->ca_exception_arg = arg;
  }
  else{
    ca_static->ca_exception_func = ca_default_exception_handler;
    ca_static->ca_exception_arg = NULL;
  }
  UNLOCK;

  return ECA_NORMAL;
}


/*
 * Specify an event subroutine to be run once pending IO completes
 * 
 * 1) event sub runs right away if no IO pending 
 * 2) otherwise event sub runs when pending IO completes
 * 
 * Undocumented entry for the VAX OPI which may vanish in the future.
 *
 */
ca_add_io_event
#ifdef VAXC
(
void 		(*ast)(),
void 		*astarg
)
#else
(ast,astarg)
void		(*ast)();
void		*astarg;
#endif
{
  register struct pending_io_event	*pioe;

  INITCHK;

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




/*
 *	CA_ADD_MASKED_ARRAY_EVENT
 *
 *
 */
ca_add_masked_array_event
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
evid 		*monixptr,
long		mask
)
#else
(type,count,chix,ast,astarg,p_delta,n_delta,timeout,monixptr,mask)
chtype				type;
unsigned int			count;
chid				chix;
void				(*ast)();
void				*astarg;
ca_real				p_delta;
ca_real				n_delta;
ca_real				timeout;
evid				*monixptr;
unsigned			mask;
#endif
{
  register evid			monix;
  void				ca_request_event();
  int				evsize;
  register int			status;

  LOOSECHIXCHK(chix);

  if(INVALID_DB_REQ(type))
    return ECA_BADTYPE;

  /*
   *
   *
   */
  if(count > chix->count && chix->type != TYPENOTCONN)
    return ECA_BADCOUNT;

  LOCK;

  /*	event descriptor	*/
# ifdef vxWorks
  {
    static int			dbevsize;

    if(chix->iocix == LOCAL_IIU){

      if(!dbevsize)
        dbevsize = db_sizeof_event_block();


      if(!(monix = (evid)lstGet(&dbfree_ev_list)))
        monix = (evid)malloc(sizeof(*monix)+dbevsize);
    }
    else
      if(!(monix = (evid)lstGet(&free_event_list)))
        monix = (evid)malloc(sizeof *monix);
  }
# else
  if(!(monix = (evid)lstGet(&free_event_list)))
    monix = (evid) malloc(sizeof *monix);
# endif

  if(!monix){
    status = ECA_ALLOCMEM;
    goto unlock_rtn;
  }

  /* they dont have to supply one if they dont want to */
  if(monixptr)
    *monixptr = monix;

  monix->chan		= chix;
  monix->usr_func	= ast;
  monix->usr_arg	= astarg;
  monix->type		= type;
  monix->count		= count;
  monix->p_delta	= p_delta;
  monix->n_delta	= n_delta;
  monix->timeout	= timeout;
  monix->mask		= mask;

# ifdef vxWorks
  {
    struct monops 	mon;
    void		ca_event_handler();

    if(chix->iocix == LOCAL_IIU){
      status = db_add_event(	evuser,
				chix->paddr,
				ca_event_handler,
				monix,
				mask,
				monix+1);
      if(status == ERROR){
        status = ECA_DBLCLFAIL;
        goto unlock_rtn;
      }

      /* 
	Place in the channel list 
	- do it after db_add_event so there
	is no chance that it will be deleted 
	at exit before it is completely created
      */
      lstAdd(&chix->eventq, monix);

      /* 
	force event to be called at least once
      	return warning msg if they have made the queue to full 
	to force the first (untriggered) event.
      */
      if(db_post_single_event(monix+1)==ERROR){
        status = ECA_OVEVFAIL;
        goto unlock_rtn;
      }

      status = ECA_NORMAL;
      goto unlock_rtn;

    } 
  }
# endif

  /* It can be added to the list any place if it is remote */
  /* Place in the channel list */
  lstAdd(&chix->eventq, monix);

  /* 
	dont send the message if the conn is down 
	(it will be sent once connected)
  */
  if(chix->iocix!=BROADCAST_IIU)
    if(iiu[chix->iocix].conn_up)
      ca_request_event(monix);

  status = ECA_NORMAL;

unlock_rtn:

  UNLOCK;

  return status;
}


/*
 *	CA_REQUEST_EVENT()
 *
 * 	LOCK must be applied while in this routine
 */
void ca_request_event(monix)
evid monix;
{
  register chid		chix = monix->chan;	/* for SNDBEG */ 

  /*
   * clip to the native count
   */
  if(monix->count > chix->count || monix->count == 0)
	monix->count = chix->count;

  SNDBEG_NOLOCK(monops)

    /*	msg header	*/
    mptr->m_header.m_cmmd 	= htons(IOC_EVENT_ADD);
    mptr->m_header.m_available 	= (long) monix;
    mptr->m_header.m_postsize	= htons(sizeof(struct mon_info));
    mptr->m_header.m_type	= htons(monix->type);
    mptr->m_header.m_count	= htons(monix->count);
    mptr->m_header.m_pciu	= chix->paddr;

    /*	msg body	*/
    htonf(&monix->p_delta, &mptr->m_info.m_hval);
    htonf(&monix->n_delta, &mptr->m_info.m_lval);
    htonf(&monix->timeout, &mptr->m_info.m_toval);
    mptr->m_info.m_mask = htons(monix->mask);

  SNDEND_NOLOCK
}


/*
 *
 *	CA_EVENT_HANDLER()
 *	(only for local (for now vxWorks) clients)
 *
 */
#ifdef vxWorks
static void ca_event_handler(monix, paddr, hold, pfl)
register evid 		monix;
register struct db_addr	*paddr;
int			hold;
caddr_t			pfl;
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
    status = db_event_get_field(paddr,
				type,
				pval,
				count,
				pfl);
  }

  /*
   *   @@@@@@ should tell um with the event handler ?
   *   or run an exception
@@@@@@@@@@@@@@@@
   */
  if(status == ERROR){
    SEVCHK(ECA_GETFAIL,"Event lost due to local get fail\n");
  }
  else
    (*monix->usr_func)(		monix->usr_arg, 
				monix->chan, 
				type, 
				count,
				pval);

  return;
}
#endif



/*
 *
 *	CA_CLEAR_EVENT(MONIX)
 *
 *	Cancel an outstanding event for a channel.
 *
 *	NOTE: returns before operation completes in the server 
 *	(and the client). 
 *	This is a good reason not to allow them to make the monix 
 *	block as part of a larger structure.
 *	Nevertheless the caller is gauranteed that his specified
 *	event is disabled and therefore will not run (from this source)
 *	after leaving this routine.
 *
 */
ca_clear_event
#ifdef VAXC
(register evid	monix)
#else
(monix)
register evid		monix;
#endif
{
  register chid		chix = monix->chan;	/* for SNDBEG */ 
 
  LOOSECHIXCHK(chix);

  /* disable any further events from this event block */
  monix->usr_func = NULL;

# ifdef vxWorks
    if(chix->iocix == LOCAL_IIU){
      register int status;

      /* 
	dont allow two threads to delete the same moniitor at once 
      */
      LOCK;
      status = lstFind(&chix->eventq, monix);
      if(status != ERROR){
        lstDelete(&chix->eventq, monix);
        status = db_cancel_event(monix+1);
      }
      UNLOCK;
      if(status == ERROR)
	return ECA_BADMONID;

      lstAdd(&dbfree_ev_list, monix);

      return ECA_NORMAL;
    } 
# endif

  /* 
	dont send the message if the conn is down 
	(just delete from the queue and return)

	check for conn down while locked to avoid a race
  */
  LOCK;
    if(!iiu[chix->iocix].conn_up || chix->iocix == BROADCAST_IIU)
      lstDelete(&monix->chan->eventq, monix); 
  UNLOCK;

  if(chix->iocix == BROADCAST_IIU)
    return ECA_NORMAL;
  if(!iiu[chix->iocix].conn_up)
    return ECA_NORMAL;

  SNDBEG(extmsg)

    /*	msg header	*/
    mptr->m_cmmd 	= htons(IOC_EVENT_CANCEL);
    mptr->m_available 	= (long) monix;
    mptr->m_postsize	= 0;
    mptr->m_type	= chix->type;
    mptr->m_count 	= chix->count;
    mptr->m_pciu	= chix->paddr;

    /* 	NOTE: I free the monitor block only after confirmation from IOC */

  SNDEND
  return ECA_NORMAL;
}


/*
 *
 *	CA_CLEAR_CHANNEL(CHID)
 *
 *	clear the resources allocated for a channel by search
 *
 *	NOTE: returns before operation completes in the server 
 *	(and the client). 
 *	This is a good reason not to allow them to make the monix 
 *	block part of a larger structure.
 *	Nevertheless the caller is gauranteed that his specified
 *	event is disabled and therefore will not run 
 *	(from this source) after leaving this routine.
 *
 */
ca_clear_channel
#ifdef VAXC
(register chid	chix)
#else
(chix)
register chid	chix;
#endif
{
  register evid		monix;
  struct ioc_in_use	*piiu = &iiu[chix->iocix];

  LOOSECHIXCHK(chix);

  /* disable their further use of deallocated channel */
  chix->type = TYPENOTINUSE;

  /* the while is only so I can break to the lock exit */
  LOCK;
  while(TRUE){
    /* disable any further events from this channel */
    for(	monix = (evid) chix->eventq.node.next; 
		monix; 
		monix = (evid) monix->node.next)
 		monix->usr_func = NULL;
    /* disable any further get callbacks from this channel */
    for(	monix = (evid) pend_read_list.node.next; 
		monix; 
		monix = (evid) monix->node.next)
		if(monix->chan == chix)
 			monix->usr_func = NULL;
 
#   ifdef vxWorks
    if(chix->iocix == LOCAL_IIU){
      int status;

      /*
      clear out the events for this channel
      */
      while(monix = (evid) lstGet(&chix->eventq)){
        status = db_cancel_event(monix+1);
        if(status==ERROR)
          abort();
        lstAdd(&dbfree_ev_list, monix);
      }

      /*
      clear out this channel
      */
      lstDelete(&local_chidlist, chix);
      if(free(chix)<0)
	abort();

      break; /* to unlock exit */
    }
# endif

    /* 
	dont send the message if the conn is down 
	(just delete from the queue and return)

	check for conn down while locked to avoid a race
    */
    if(!piiu->conn_up || chix->iocix == BROADCAST_IIU){
      lstConcat(&free_event_list, &chix->eventq);
      lstDelete(&piiu->chidlist, chix);
      if(free(chix)<0)
	abort();
      if(chix->iocix != BROADCAST_IIU && !piiu->chidlist.count)
        close_ioc(piiu);
    }

    if(chix->iocix == BROADCAST_IIU)
      break;	/* to unlock exit */
    if(!iiu[chix->iocix].conn_up)
      break;	/* to unlock exit */

    /*
  	clear events and all other resources for this chid on the IOC
    */
    SNDBEG_NOLOCK(extmsg)

      /*	msg header	*/
      mptr->m_cmmd 	= htons(IOC_CLEAR_CHANNEL);
      mptr->m_available = (int) chix;
      mptr->m_type	= 0;
      mptr->m_count 	= 0;
      mptr->m_pciu	= chix->paddr;
      mptr->m_postsize	= 0;

      /* 	
	NOTE: I free the chid and monitor blocks only after 
	confirmation from IOC 
      */

    SNDEND_NOLOCK

    break; /* to unlock exit */
  }
	/*
	 * message about unexecuted code from SUN's cheaper cc is a compiler
	 * bug
	 */
  UNLOCK;

  return ECA_NORMAL;
}


/*
 * NOTE: This call not implemented with select on VMS or vxWorks due to its
 * poor implementation in those environments.
 * 
 * Wallangong's SELECT() does not return early if a timeout is specified and IO
 * occurs before the end of the timeout. Above call works because of the
 * short timeout interval.
 * 
 * Another wallongong bug will cause problems if the timeout in secs is
 * specified large. I have fixed this by keeping track of large timeouts
 * myself.
 * 
 */
/************************************************************************/
/*	This routine pends waiting for channel events and calls the 	*/
/*	function specified with add_event when one occurs. If the 	*/
/*	timeout is specified as 0 infinite timeout is assumed.		*/
/*	if the argument early is specified TRUE then CA_NORMAL is 	*/
/*	returned early (prior to timeout experation) when outstanding 	*/
/*	IO completes.							*/
/*	ca_flush_io() is called by this routine.			*/
/************************************************************************/
#ifdef VAXC
ca_pend(ca_real timeout, int early)
#else
ca_pend(timeout, early)
ca_real			timeout;
int			early;
#endif
{
  void			send_msg();
  time_t 		beg_time;
  time_t		clock();

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
    if(early)
      return ECA_NORMAL;

  if(timeout<DELAYVAL && timeout != 0.0){
    	struct timeval	notimeout;

	/*
	 * on UNIX call recv_msg() with zero timeout. on vxWorks and VMS
	 * recv_msg() need not be called.
	 * 
	 * send_msg() only calls recv_msg on UNIX if the buffer needs to be
	 * flushed.
	 */
#ifdef UNIX
	/*
	 * locking only a comment on UNIX
	 */
    	notimeout.tv_sec = 0;
   	notimeout.tv_usec = 0;
  	LOCK;
   	recv_msg_select(&notimeout);
  	UNLOCK;
#endif
       	return ECA_TIMEOUT;
  }

  beg_time = time(NULL);

  while(TRUE){


#   ifdef UNIX
    {
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
      if(early)
        return ECA_NORMAL;

    LOCK;
      chid_retry(TRUE);
    UNLOCK;

    if(timeout != 0.0)
      if(timeout < time(NULL)-beg_time){
	int i;
	LOCK;
        chid_retry(FALSE);
	/* set pending IO count back to zero and notify  */
	/* send a sync to each IOC and back */
	/* dont count reads until we recv the sync */
	for(i=BROADCAST_IIU+1; i<nxtiiu; i++){	
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
 *	CA_FLUSH_IO()
 *
 *
 */
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



/*
 *	CA_SIGNAL()
 *
 *
 */
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

  /*
   *
   *
   *	Terminate execution if unsuccessful
   *
   *
   */
  if( !(ca_status & CA_M_SUCCESS) && 
		CA_EXTRACT_SEVERITY(ca_status) != CA_K_WARNING ){
#   ifdef VMS
      lib$signal(0);
#   endif
#   ifdef vxWorks
      ti(VXTHISTASKID);
      tt(VXTHISTASKID);
#   endif
/***
    abort(ca_status);
 ***/
    abort();
  }

  printf(
"..................................................................\n");


}





/*
 *	CA_BUSY_MSG()
 *
 */
ca_busy_message(piiu)
register struct ioc_in_use	*piiu; 
{
  void			send_msg();

  LOCK;
  SND_BEG_NOLOCK(extmsg, piiu)
    *mptr 		= nullmsg;
    mptr->m_cmmd 	= htons(IOC_EVENTS_OFF);
  SNDEND_NOLOCK
  send_msg();
  UNLOCK;

}


/*
 *	CA_READY_MSG()
 *
 */
ca_ready_message(piiu)
register struct ioc_in_use	*piiu; 
{
  void			send_msg();

  LOCK;
  SND_BEG_NOLOCK(extmsg, piiu)
    *mptr 		= nullmsg;
    mptr->m_cmmd 	= htons(IOC_EVENTS_ON);
  SNDEND_NOLOCK
  send_msg();
  UNLOCK;

}


/*
 *	NOOP_MSG
 *	(lock must be on)
 *
 */
void
noop_msg(piiu)
register struct ioc_in_use	*piiu; 
{
  SND_BEG_NOLOCK(extmsg, piiu)
    *mptr 		= nullmsg;
    mptr->m_cmmd 	= htons(IOC_NOOP);
  SNDEND_NOLOCK
}



/*
 *
 *	Default Exception Handler
 *
 *
 */
void
ca_default_exception_handler(args)
struct exception_handler_args args;
{
 
      /* 
	NOTE:
	I should print additional diagnostic info when time permits......
      */

      ca_signal(args.stat, args.ctx);
}



/*
 *
 *	call their function with their argument whenever a new fd is added or removed
 *	(for a manager of the select system call under UNIX)
 *
 */
ca_add_fd_registration(func, arg)
void	(*func)();
void	*arg;
{
	fd_register_func = func;
	fd_register_arg = arg;

  	return ECA_NORMAL;
}



/*
 *
 *	call their function with their argument whenever a new fd is added or removed
 *	(for a manager of the select system call under UNIX)
 *
 */
ca_defunct(func, arg)
void	(*func)();
void	*arg;
{
#ifdef VMS
	SEVCHK(ECA_DEFUNCT, "you must have a VMS share image mismatch\n");
#else
	SEVCHK(ECA_DEFUNCT, NULL);
#endif
}


