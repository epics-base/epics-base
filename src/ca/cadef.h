/************************************************************************/
/*									*/
/*	        	      L O S  A L A M O S			*/
/*		        Los Alamos National Laboratory			*/
/*		         Los Alamos, New Mexico 87545			*/
/*									*/
/*	Copyright, 1986, The Regents of the University of California.	*/
/*	Author Jeffrey O. Hill						*/
/*	hill@atdiv.lanl.gov						*/
/*	505 665 1831							*/
/*									*/
/*									*/
/*	History								*/
/*	-------								*/
/*									*/
/*	Date	Person	Comments					*/
/*	----	------	--------					*/
/*	08xx87	joh	Init Release					*/
/*	031290	joh	Added db_access auto include 			*/
/*	031991	joh	fixed SEVCHK dbl function call when status	*/
/*			returned indicates unsuccessful completion	*/
/*	060591	joh	delinting					*/
/*	091691	joh	exported channel state				*/
/* 	060392	joh	added ca host name MACRO			*/
/* 	072792	joh	added ca_test_io() decl				*/
/*	072792	joh	changed compile time flag from VAXC to STDC	*/
/*			so the function prototypes can be used by	*/
/*			other compilers					*/
/*	072892	joh	added function prototype for host name function */
/*	101692	joh	use unique name for var in SEVCHK to avoid	*/
/*			clashing with application			*/
/*	120492	joh	turn off VAXC ca_real reduction from double to	*/
/*			float. This was originally provided to ease	*/
/*			integration of the VAX FORTRAN based pt_shell  	*/
/*			code which will in all likelyhood no longer	*/
/*			be used.  					*/
/*	120992	joh	converted to dll list routines			*/
/*	061193	joh	added missing prototype for ca_clear_channel()	*/
/*			and others					*/
/*	080593	rcz	converted to ell list routines			*/
/*	090893	joh	added client id	to channel in use block		*/
/*      010694  joh     added put callback rtn and synch group rtns     */
/*      090194  joh     support C++					*/
/*      101194  joh     merged NT changes				*/
/*									*/
/*_begin								*/
/************************************************************************/
/*									*/
/*	Title:		GTA high level channel access routines C	*/ 
/*			function prototypes				*/
/*	File:		cadef.h						*/
/*	Environment: 	Architecture independent			*/
/*			( Current ports )				*/
/*			VAXC, VMS V4.6					*/
/*			SUNC, BSD UNIX V4.3 				*/
/*			SUNC, vxWorks 					*/
/*									*/
/*									*/
/*	Purpose								*/
/*	-------								*/
/*									*/
/*	GTACS universal remote access library function prototypes.	*/
/*	( C default argument passing mechanisms )			*/
/*									*/
/*	Special comments						*/
/*	------- --------						*/
/*	"ca_" is the standard prefix for the "channel access" library  	*/
/*	function names.							*/
/*									*/
/*									*/
/************************************************************************/
/*_end									*/

#ifndef INCLcadefh
#define INCLcadefh

#ifdef __cplusplus
extern "C" {
#define CAC_FUNC_PROTO
#endif

#include <shareLib.h>

#ifdef __STDC__ 
#define CAC_FUNC_PROTO
#endif

#ifndef HDRVERSIONID
#	define HDRVERSIONID(NAME,VERS)
#endif /*HDRVERSIONID*/

HDRVERSIONID(cadefh, "@(#) $Id$")

/* auto include of all stuff that cadef.h uses */
#if defined(CAC_FUNC_PROTO) && !defined(CA_DONT_INCLUDE_STDARGH)
#include <stdarg.h>
#endif

#ifndef INCLdb_accessh
#include <db_access.h>
#endif /* INCLdb_accessh */

#ifndef INCLcaerrh
#include <caerr.h>
#endif /* INCLcaerrh */

#ifndef INCLcaeventmaskh
#include <caeventmask.h>
#endif /* INCLcaeventmaskh */

#ifndef INCLellLibh
#include <ellLib.h>
#endif /* INCLellLibh */

/*
 *
 *	NOTE: the channel in use fields type, count, name, and
 *	host name are available to applications. However it is 
 *	recommended that the following MACROS be used to access them.
 *
 */
#define ca_field_type(CHID)     	((CHID)->type)
#define ca_element_count(CHID)   	((CHID)->count)
#define ca_name(CHID)           	((char *)((CHID)+1))
#define ca_puser(CHID)           	((CHID)->puser)
#define ca_host_name(CHID)           	ca_host_name_function(CHID)
#define ca_read_access(CHID)		((CHID)->ar.read_access)
#define ca_write_access(CHID)		((CHID)->ar.write_access)

/*
 *	cs_ - `channel state'	
 *	
 *	cs_never_conn		valid chid, IOC not found
 * 	cs_prev_conn		valid chid, IOC was found, but unavailable
 *	cs_conn			valid chid, IOC was found, still available
 * 	cs_closed		invalid chid
 */
enum channel_state{cs_never_conn, cs_prev_conn, cs_conn, cs_closed};
#define ca_state(CHID)           	((CHID)->state)

typedef struct ca_access_rights{
	unsigned	read_access:1;
	unsigned	write_access:1;
}caar;


/* Format for the arguments to user connection handlers 		*/
struct channel_in_use;
struct	connection_handler_args{
	struct channel_in_use	*chid;	/* Channel id	 		*/
	long			op;	/* External codes for CA op	*/
};

#ifdef CAC_FUNC_PROTO
typedef void caCh(struct connection_handler_args args);
#else /*CAC_FUNC_PROTO*/
typedef void caCh();
#endif /*CAC_FUNC_PROTO*/

/* Format for the arguments to user access rights handlers 		*/
struct	access_rights_handler_args{
	struct channel_in_use 	*chid;	/* Channel id	 		*/
	caar			ar;	/* New access rights state	*/
};

#ifdef CAC_FUNC_PROTO
typedef void caArh(struct access_rights_handler_args args);
#else /*CAC_FUNC_PROTO*/
typedef void caArh();
#endif /*CAC_FUNC_PROTO*/

struct channel_in_use{
	ELLNODE			node;	/* list ptrs			*/
	short			type;	/* database field type 		*/
#define TYPENOTCONN	(-1)		/* the type when disconnected	*/
	unsigned short		count;	/* array element count 		*/
	union{
	  unsigned 		sid;	/* server id			*/
	  struct db_addr	*paddr;	/* database address		*/
	}			id;
	void			*puser;	/* user available area		*/
	enum channel_state	state;	/* connected/ disconnected etc	*/
	caar			ar;	/* access rights		*/

	/*
	 *	The following fields may change or even vanish in the future
	 */
	caCh			*pConnFunc;
	caArh			*pAccessRightsFunc;
	ELLLIST			eventq;
	unsigned 		cid;	/* client id */
	unsigned		retry;	/* search retry number */
	void			*piiu;	/* private ioc in use block */
#ifdef vxWorks
	void			*ppn;  /* ptr to optional put notify blk */
#endif /* vxWorks */
	/*
	 * channel name stored directly after this structure in a 
	 * null terminated string.
	 */
};

typedef	struct channel_in_use	*chid;
typedef long			chtype;
typedef	struct pending_event	*evid;
typedef double			ca_real;

/*	The conversion routine to call for each type	*/
#define VALID_TYPE(TYPE)  (((unsigned short)TYPE)<=LAST_BUFFER_TYPE)


/* argument passed to event handlers and callback handlers		*/
struct	event_handler_args{
	void			*usr;	/* User argument supplied when event added 	*/
	struct channel_in_use 	*chid;	/* Channel id					*/
	long			type;	/* the type of the value returned		*/ 
	long			count;	/* the element count of the item returned	*/
	void 			*dbr;	/* Pointer to the value returned		*/
	int			status;	/* CA Status of the op from server - CA V4.1	*/
};

struct pending_event{
  	ELLNODE			node;	/* list ptrs */
#ifdef CAC_FUNC_PROTO
  	void			(*usr_func)(struct  event_handler_args args);
#else /*CAC_FUNC_PROTO*/
  	void			(*usr_func)();
#endif /*CAC_FUNC_PROTO*/
  	void			*usr_arg;
  	struct channel_in_use 	*chan;
  	chtype			type;	/* requested type for local CA	*/
  	unsigned long		count;	/* requested count for local CA */
  	/* 
	 * the following provide for reissuing a 
	 * disconnected monitor 
	 */
  	ca_real			p_delta;
  	ca_real			n_delta;
  	ca_real			timeout;
	unsigned		id;
  	unsigned short		mask;
};

void epicsShareAPI ca_test_event
	(
#ifdef CAC_FUNC_PROTO 
	struct event_handler_args
#endif /*CAC_FUNC_PROTO*/
	);

/* Format for the arguments to user exception handlers 			*/
struct	exception_handler_args{
	void			*usr;	/* User argument supplied when event added 	*/
	struct channel_in_use	*chid;	/* Channel id			 		*/
	long 			type;	/* Requested type for the operation		*/
	long 			count;	/* Requested count for the operation		*/
	void 			*addr;	/* User's address to write results of CA_OP_GET	*/
	long			stat;	/* Channel access std status code 		*/
	long			op;	/* External codes for channel access operations	*/
	char			*ctx;	/* A character string containing context info	*/
	char			*pFile; /* source file name (may be NULL) */
	unsigned		lineNo; /* source file line number */
};




/*
 *
 *	External OP codes for CA operations
 *
 */
#define CA_OP_GET 		0
#define CA_OP_PUT 		1
#define CA_OP_SEARCH 		2
#define CA_OP_ADD_EVENT 	3
#define CA_OP_CLEAR_EVENT 	4
#define CA_OP_OTHER 		5
#define CA_OP_CONN_UP 		6
#define CA_OP_CONN_DOWN 	7

/************************************************************************/
/*	Perform Library Initialization					*/
/*									*/
/*	Must be called once before calling any of the other routines	*/
/************************************************************************/
int epicsShareAPI ca_task_initialize
	(
#ifdef CAC_FUNC_PROTO	
	void
#endif /*CAC_FUNC_PROTO*/
	);

/************************************************************************/
/*	Remove CA facility from your task				*/
/*									*/
/*	Normally called automatically at task exit			*/
/************************************************************************/
int epicsShareAPI ca_task_exit
	(
#ifdef CAC_FUNC_PROTO	
	void
#endif /*CAC_FUNC_PROTO*/
	);


/************************************************************************/
/*	Return a channel identification for the supplied channel name	*/
/*	(and attempt to create a virtual circuit)			*/
/************************************************************************/

/*
 * preferred search mechanism
 */
#define ca_search(NAME,CHIDPTR)\
ca_search_and_connect(NAME, CHIDPTR, 0, 0)
/*	ca_search
	(
		Name	IO	Value					
		----	--	-----					
char *,	 	NAME	R	NULL term ASCII channel name string	
chid *	 	CHIDPTR	RW	channel index written here		
	);
*/

/************************************************************************
 * anachronistic entry points						*
 * **** Fetching a value while searching nolonger supported****		*
 ************************************************************************/
#define ca_build_channel(NAME,XXXXX,CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, 1, CHIDPTR, YYYYY, 0, 0)

#define ca_array_build(NAME,XXXXX, ZZZZZZ, CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, ZZZZZZ, CHIDPTR, YYYYY, 0, 0)


/*
 * preferred search mechanism
 */
int epicsShareAPI ca_search_and_connect
	(
#ifdef CAC_FUNC_PROTO
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
char *,/* 	NAME	R	NULL term ASCII channel name string	*/
chid *,/* 	CHIDPTR	RW	channel index written here		*/
void (*)(struct connection_handler_args),
	/*	PFUNC	R	the address of user's connection handler*/
void * 	/*	PUSER	R	placed in the channel's puser field 	*/
#endif /*CAC_FUNC_PROTO*/
);

/************************************************************************
 * anachronistic entry point						*
 * **** Fetching a value while searching nolonger supported****		*
 ************************************************************************/
int epicsShareAPI ca_build_and_connect
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
char *,	/* 	NAME	R	NULL term ASCII channel name string	*/
chtype,	/*	GET_TYPE R 	external channel type to get		*/
unsigned/*			(no get if invalid type)		*/	
long,	/*	GET_COUNT R	array count to get			*/
chid *,	/* 	CHIDPTR	RW	channel index written here		*/
void *,	/*	PVALUE	W	A pointer to a user supplied buffer	*/	
void (*)(struct connection_handler_args),
	/*	PFUNC	R	the address of user's connection handler*/
void *	/*	PUSER	R	placed in the channel's puser field 	*/	
#endif /*CAC_FUNC_PROTO*/
	);

int epicsShareAPI ca_change_connection_event
(
#ifdef CAC_FUNC_PROTO
chid	chix,
void	(*pfunc)(struct connection_handler_args)
#endif /*CAC_FUNC_PROTO*/
);

int epicsShareAPI ca_replace_access_rights_event(
#ifdef CAC_FUNC_PROTO
chid	chix,
void 	(*pfunc)(struct access_rights_handler_args)
#endif /*CAC_FUNC_PROTO*/
);

/*
 *
 * replace the default exception handler
 *
 */
int epicsShareAPI ca_add_exception_event
(
#ifdef CAC_FUNC_PROTO
void            (*pfunc)(struct exception_handler_args),
void            *arg
#endif /*CAC_FUNC_PROTO*/
);

/************************************************************************/
/*	deallocate resources reserved for a channel			*/	
/************************************************************************/
int epicsShareAPI ca_clear_channel
	(
#ifdef CAC_FUNC_PROTO 
	/*      Name    IO      Value					*/
	/*      ----    --      -----					*/
chid	/*	CHAN,	R	channel identifier			*/
#endif /*CAC_FUNC_PROTO*/
	);

/************************************************************************/
/*	Write a value to a channel					*/
/************************************************************************/
#define	ca_bput(CHID,PVALUE)  	ca_array_put(DBR_STRING, 1, CHID, (PVALUE))
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 			
char *	 	PVALUE	R	pointer to a binary value string
*/

#define	ca_rput(CHID,PVALUE)\
ca_array_put(DBR_FLOAT, 1, CHID, (PVALUE))
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 			
dbr_float_t *	PVALUE	R	pointer to a real value
*/

#define ca_put(CHTYPE, CHID, PVALUE) ca_array_put(CHTYPE, 1, CHID, PVALUE)
/*
		Name	IO	Value					
		----	--	-----					
chtype,		TYPE	R	channel type				
chid,	 	CHID	R	channel index	 			
void *		PVALUE	R	pointer to new channel value of type specified	
				i.e. status = ca_put(DBF_INT,chid,&value)	
*/

int epicsShareAPI ca_array_put
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
chtype,	/*	TYPE	R	channel type				*/
unsigned long,	
	/*	COUNT	R	array element count			*/
chid,	/* 	CHID	R	channel index	 			*/
void *	/*	PVALUE	R	pointer to new channel value of type	*/ 
	/*			specified.				*/
#endif /*CAC_FUNC_PROTO*/
	);

/*
 * This routine functions identically to the original ca put request 
 * with the addition of a callback to the user supplied function 
 * after recod processing completes in the IOC. The arguments
 * to the user supplied callback function are declared in
 * the structure event_handler_args and include the pointer
 * sized user argument supplied when ca_array_get_callback() is called.
 */
int epicsShareAPI ca_array_put_callback
        (
#ifdef CAC_FUNC_PROTO
        /*      Name    IO      Value                                   */
        /*      ----    --      -----                                   */
chtype, /*      TYPE    R       channel type                            */
unsigned long,
        /*      COUNT   R       array element count                     */
chid,   /*      CHID    R       channel index                           */
void *, /*      PVALUE  R       pointer to new channel value of type    */
void (*)(struct event_handler_args),
        /*      USRFUNC R       the address of a user supplied function */
void *  /*      USRARG  R       An argument copied to the above function*/
#endif /*CAC_FUNC_PROTO*/
        );



/************************************************************************/
/*	Read a value from a channel					*/
/*									*/
/*									*/
/************************************************************************/

#define	ca_bget(CHID,PVALUE)  	ca_array_get(DBR_STRING, 1, CHID, PVALUE)
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 			
char *	 	PVALUE	W	string value pointer 
*/

#define	ca_rget(CHID,PVALUE)  	ca_array_get(DBR_FLOAT, 1, CHID, PVALUE)
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 			
dbr_float_t *	PVALUE	W	real value pointer
*/

#define ca_get(CHTYPE, CHID, PVALUE) ca_array_get(CHTYPE, 1, CHID, PVALUE)
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chtype,		TYPE	R	channel type				
chid,	 	CHID	R	channel index	 			
void *		PVALUE	W	ptr to where channel value written	
*/

int epicsShareAPI ca_array_get
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
chtype,	/*	TYPE	R	channel type				*/
unsigned long,	
	/*	COUNT	R	array element count			*/
chid,	/* 	CHID	R	channel index	 			*/
void *	/*	PVALUE	W	ptr to where channel value written	*/
#endif /*CAC_FUNC_PROTO*/
	);
/************************************************************************/
/*	Read a value from a channel and run a callback when the value	*/
/*	returns								*/
/*									*/
/*									*/
/************************************************************************/
#define	ca_bget_callback(CHID,PFUNC,ARG)\
ca_array_get_callback(DBR_STRING, 1, CHID, PFUNC, ARG)
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 
void (*)(),	PFUNC	R	the address of a user supplied function	
void *,		ARG	R	An argument copied to the above function	
*/

#define	ca_rget_callback(CHID,PFUNC,ARG)\
ca_array_get_callback(DBR_FLOAT, 1, CHID, PFUNC, ARG)

/*
C Type		Name	IO	Value					
------		-----	--	-----					
chid	 	CHID	R	channel id	 			
void (*)(),	PFUNC	R	the address of a user supplied function	
void *,		ARG	R	An argument copied to the above function	
*/

#define	ca_get_callback(CHTYPE, CHID,PFUNC,ARG)\
ca_array_get_callback(CHTYPE, 1, CHID, PFUNC, ARG)
/*
C Type		Name	IO	Value					
------		-----	--	-----					
chtype,		TYPE	R	channel type				
chid,	 	CHID	R	channel index	 			
void (*)(),	PFUNC	R	the address of a user supplied function	
void *,		ARG	R	An argument copied to the above function
*/

int epicsShareAPI ca_array_get_callback
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
chtype,	/*	TYPE	R	channel type				*/
unsigned long,	
	/*	COUNT	R	array element count			*/
chid,	/* 	CHID	R	channel index	 			*/
void (*)(struct event_handler_args),	
	/*	USRFUNC	R	the address of a user supplied function	*/
void *	/*	USRARG	R	An argument copied to the above function	*/
#endif /*CAC_FUNC_PROTO*/
	);



/************************************************************************/
/*	Specify a function to be executed whenever significant changes	*/
/*	occur to a channel.						*/
/*	NOTES:								*/
/*	1)	Evid may be omited by passing a NULL pointer		*/
/*									*/
/*	2)	AN array count of zero specifies the native db count	*/ 
/*									*/
/************************************************************************/
typedef struct event_handler_args  evargs;

/*	Assumes DELTA info comes from the database or defaults		*/
#define ca_add_event(TYPE,CHID,ENTRY,ARG,EVID)\
ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,0.0,0.0,0.0,EVID)

/*		Name	IO	Value					
		----	--	-----					
chid,	 	CHID	R	channel index	 			
void
(*)(),		USRFUNC	R	the address of a user supplied function	
void *,		USRARG	R	An argument copied to the above function	
evid *  	EVIDPTR	W	An id to refer to this event by	
*/



/*	Sets both P_DELTA and M_DELTA below to argument DELTA		*/
#define ca_add_delta_event(TYPE,CHID,ENTRY,ARG,DELTA,EVID)\
	ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,DELTA,DELTA,0.0,EVID)
/*		Name	IO	Value					
		----	--	-----					
chid,	 	CHID	R	channel index	 			
void
(*)(),		USRFUNC	R	the address of a user supplied function	
void *,		USRARG	R	An argument copied to the above function	
ca_real,	DELTA	R	Generate events after +-delta excursions	
evid *  	EVIDPTR	W	An id to refer to this event by	
*/


#define ca_add_general_event(TYPE,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)

#define	ca_add_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_masked_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID, DBE_VALUE | DBE_ALARM)



int epicsShareAPI ca_add_masked_array_event
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
chtype,	/*	TYPE	R	requested external channel type		*/
unsigned long,
	/*	COUNT	R	array element count			*/
chid,	/* 	CHID	R	channel index	 			*/
void (*)(struct event_handler_args),	
	/*	USRFUNC	R	the address of a user supplied function	*/
void *,	/*	USRARG	R	An argument copied to the above function*/
ca_real,/*	P_DELTA	R	Generate events after +delta excursions	*/
ca_real,/*	N_DELTA	R	Generate events after -delta excursions	*/
ca_real,/*	TIMEOUT	R	Generate events after timeout sec	*/
evid *, /*	EVIDPTR	W	An id to refer to this event by		*/
long	/*	MASK	R	event trigger type			*/
#endif /*CAC_FUNC_PROTO*/
	);



/************************************************************************/
/*	Remove a function from a list of those specified to run 	*/
/*	whenever significant changes occur to a channel			*/
/*									*/
/************************************************************************/
int epicsShareAPI ca_clear_event
	(
#ifdef CAC_FUNC_PROTO 
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
evid	/* 	EVID	R	Event id returned by add event		*/
#endif /*CAC_FUNC_PROTO*/
	);


/************************************************************************/
/*									*/
/*	 Requested data is not necessarily stable prior to		*/
/*	 return from called subroutine.  Call ca_pend_io()		*/
/*	 to guarantee that requested data is stable.  Call the routine	*/
/*	 ca_flush_io() to force all outstanding subroutine calls to be	*/
/*	 sent out over the network.  Significant increases in 		*/
/*	 performance have been measured when batching several remote 	*/
/*	 subroutine calls together into one message.  Additional	*/
/*	 improvements can be obtained by performing local processing	*/
/*	 in parallel with outstanding remote processing.		*/
/*	 								*/
/*	 FLOW OF TYPICAL APPLICATION					*/
/*									*/
/*	 search()		! Obtain Channel ids			*/
/*	 .			! "					*/
/*									*/
/*	 get()			! several requests for remote info	*/
/*	 get()			! "					*/
/*	 add_event()		! "					*/	
/*	 get()			! "					*/
/*	 .								*/
/*	 .								*/
/*	 .								*/
/*	 flush_io()		! send get requests			*/
/*				! optional parallel processing		*/
/*	 .			! "					*/
/*	 .			! "					*/
/*	 pend_io()		! wait for replies from get requests	*/
/*	 .			! access to requested data 		*/
/*	 .			! "					*/
/*	 pend_event()		! wait for requested events  		*/
/*									*/
/************************************************************************/

/************************************************************************/
/*	This routine pends waiting for channel events and calls the 	*/
/*	functions specified with add_event when events occur. If the 	*/
/*	timeout is specified as 0 an infinite timeout is assumed.	*/
/*	if the argument early is specified TRUE then CA_NORMAL is 	*/
/*	returned when outstanding queries complete. Otherwise if the 	*/
/*	argument early is FALSE the routine does not return until the 	*/
/*	entire delay specified by the timeout argument has expired.	*/
/*	ca_flush_io() is called by this routine. If the argument	*/
/*	early is TRUE then ca_pend() will return immediately without	*/
/* 	processing outstanding CA labor if no queries are outstanding	*/
/************************************************************************/
#define ca_poll() ca_pend((1e-12), 0/*FALSE*/)
#define ca_pend_event(TIMEOUT) ca_pend((TIMEOUT), 0/*FALSE*/)
#define ca_pend_io(TIMEOUT) ca_pend((TIMEOUT), 1/*TRUE*/)

int epicsShareAPI ca_pend
	(
#ifdef CAC_FUNC_PROTO
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
ca_real,/*	TIMEOUT	R	timeout in seconds			*/
int	/*	EARLY	R	return early if IO completes		*/
#endif /*CAC_FUNC_PROTO*/
	);

/*
 * returns TRUE when queries are outstanding
 */
int epicsShareAPI ca_test_io
	(
#ifdef CAC_FUNC_PROTO
        /*      Name    IO      Value                                   */
        /*      ----    --      -----                                   */
void
#endif /*CAC_FUNC_PROTO*/
	);

/************************************************************************/
/*	Send out all outstanding messages in the send queue		*/
/************************************************************************/
int epicsShareAPI ca_flush_io
	(
#ifdef CAC_FUNC_PROTO
	void
#endif /*CAC_FUNC_PROTO*/
	);


void epicsShareAPI ca_signal
	(
#ifdef CAC_FUNC_PROTO
	/*	Name	IO	Value						*/
	/*	----	--	-----						*/
long,	/*	CODE	R	status returned from channel access function	*/
char *	/*	MSG	R	null term string printed on error		*/
#endif /*CAC_FUNC_PROTO*/
	);

void epicsShareAPI ca_signal_with_file_and_lineno
	(
#ifdef CAC_FUNC_PROTO
        /*      Name    IO      Value                                           */
        /*      ----    --      -----                                           */
long,	/*	CODE	R	status returned from channel access function    */
char *,	/*	MSG	R       null term string printed on error               */
char *,	/*	FILE	R	pointer to null terminated file name string	*/
int	/*	LINENO	R	line number					*/
#endif /*CAC_FUNC_PROTO*/
	);


/*
 *	Provided for efficient test and display of channel access errors
 */
#define		SEVCHK(CA_ERROR_CODE, MESSAGE_STRING) \
{ \
	int ca_unique_status_name  = (CA_ERROR_CODE); \
	if(!(ca_unique_status_name & CA_M_SUCCESS)) \
		ca_signal_with_file_and_lineno( \
			ca_unique_status_name, \
			(MESSAGE_STRING), \
			__FILE__, \
			__LINE__); \
}

char * epicsShareAPI ca_host_name_function
	(
#ifdef CAC_FUNC_PROTO
	/*	Name	IO	Value					*/
	/*	----	--	-----					*/
chid	/*	CHID	R	Channel ID				*/
#endif /*CAC_FUNC_PROTO*/
	);

/*
 *      CA_ADD_FD_REGISTRATION
 *
 *      call their function with their argument whenever 
 *	a new fd is added or removed
 *      (for use with a manager of the select system call under UNIX)
 *
 *	if (opened) then fd was created
 *	if (!opened) then fd was deleted
 *
 */
#ifdef CAC_FUNC_PROTO
typedef void CAFDHANDLER(void *parg, int fd, int opened); 
#else /*CAC_FUNC_PROTO*/
typedef void CAFDHANDLER(); 
#endif /*CAC_FUNC_PROTO*/

int epicsShareAPI ca_add_fd_registration(
#ifdef CAC_FUNC_PROTO
CAFDHANDLER 	*pHandler,
void    	*pArg
#endif /*CAC_FUNC_PROTO*/
);

#ifdef vxWorks
int epicsShareAPI ca_channel_status(
#ifdef CAC_FUNC_PROTO
int tid
#endif /*CAC_FUNC_PROTO*/
);
#endif

#ifdef vxWorks
#ifdef CAC_FUNC_PROTO
int ca_import(int tid);
#else /*CAC_FUNC_PROTO*/
int ca_import();
#endif /*CAC_FUNC_PROTO*/
#endif /* vxWorks */

#ifdef vxWorks
#ifdef CAC_FUNC_PROTO
int ca_import_cancel(int tid);
#else /*CAC_FUNC_PROTO*/
int ca_import_cancel();
#endif /*CAC_FUNC_PROTO*/
#endif /* vxWorks */

/*
 * CA synch groups
 *
 * This facility will allow the programmer to create
 * any number of synchronization groups. The programmer might then
 * interleave IO requests within any of the groups. Once The
 * IO operations are initiated then the programmer is free to
 * block for IO completion within any one of the groups as needed.
 */
typedef unsigned WRITEABLE_CA_SYNC_GID;
#ifdef CAC_FUNC_PROTO
typedef const unsigned CA_SYNC_GID;
#else
typedef unsigned CA_SYNC_GID;
#endif

/*
 * create a sync group
 */
int epicsShareAPI ca_sg_create(
#ifdef CAC_FUNC_PROTO
CA_SYNC_GID *pgid
#endif /*CAC_FUNC_PROTO*/
);

/*
 * delete a sync group
 */
int epicsShareAPI ca_sg_delete(
#ifdef CAC_FUNC_PROTO
CA_SYNC_GID gid
#endif /*CAC_FUNC_PROTO*/
);

/*
 * block for IO performed within a sync group to complete 
 */
int epicsShareAPI ca_sg_block(
#ifdef CAC_FUNC_PROTO
CA_SYNC_GID gid, 
ca_real timeout
#endif /*CAC_FUNC_PROTO*/
);

/*
 * test for sync group IO operations in progress
 */
int epicsShareAPI ca_sg_test(
#ifdef CAC_FUNC_PROTO
CA_SYNC_GID gid
#endif /*CAC_FUNC_PROTO*/
);

/*
 * ca_sg_reset
 */
int epicsShareAPI ca_sg_reset(
#ifdef CAC_FUNC_PROTO
CA_SYNC_GID gid
#endif /*CAC_FUNC_PROTO*/
);

/*
 * initiate a get within a sync group
 * (essentially a ca_array_get() with a sync group specified)
 */
int epicsShareAPI ca_sg_array_get
        (
#ifdef CAC_FUNC_PROTO
        /*      Name    IO      Value                                   */
        /*      ----    --      -----                                   */
CA_SYNC_GID,/*  GID     R       synch group id                          */
chtype, /*      TYPE    R       channel type                            */
unsigned long,
        /*      COUNT   R       array element count                     */
chid,   /*      CHID    R       channel index                           */
void *  /*      PVALUE  W       ptr to where channel value written      */
#endif /*CAC_FUNC_PROTO*/
        );

/*
 * initiate a put within a sync group
 * (essentially a ca_array_put() with a sync group specified)
 */
int epicsShareAPI ca_sg_array_put
        (
#ifdef CAC_FUNC_PROTO
        /*      Name    IO      Value                                   */
        /*      ----    --      -----                                   */
CA_SYNC_GID,/*  GID     R       synch group id                          */
chtype, /*      TYPE    R       channel type                            */
unsigned long,
        /*      COUNT   R       array element count                     */
chid,   /*      CHID    R       channel index                           */
void *  /*      PVALUE  R       pointer to new channel value of type    */
        /*                      specified.                              */
#endif /*CAC_FUNC_PROTO*/
	);

/*
 * print status of a sync group
 */
#ifdef CAC_FUNC_PROTO
int epicsShareAPI ca_sg_stat(CA_SYNC_GID gid);
#else /*CAC_FUNC_PROTO*/
int epicsShareAPI ca_sg_stat();
#endif /*CAC_FUNC_PROTO*/

/*
 * CA_MODIFY_USER_NAME()
 *
 * Modify or override the default
 * client user name.
 */
#ifdef CAC_FUNC_PROTO
int epicsShareAPI ca_modify_user_name(char *pUserName);
#else /*CAC_FUNC_PROTO*/
int epicsShareAPI ca_modify_user_name();
#endif /*CAC_FUNC_PROTO*/

/*
 * CA_MODIFY_HOST_NAME()
 *
 * Modify or override the default
 * client host name.
 */
#ifdef CAC_FUNC_PROTO
int epicsShareAPI ca_modify_host_name(char *pHostName);
#else /*CAC_FUNC_PROTO*/
int epicsShareAPI ca_modify_host_name();
#endif /*CAC_FUNC_PROTO*/

/*
 * Put call back is available if the CA server is on version is 4.2 
 *	or higher.
 *
 * (return true or false)
 */
#ifdef CAC_FUNC_PROTO
int epicsShareAPI ca_v42_ok(chid chan);
#else /*CAC_FUNC_PROTO*/
int epicsShareAPI ca_v42_ok();
#endif /*CAC_FUNC_PROTO*/

/*
 * function that returns the CA version string
 */
READONLY char * epicsShareAPI ca_version();

/*
 * ca_replace_printf_handler ()
 *
 * for apps that want to change where ca formatted
 * text output goes
 */
#if defined(CAC_FUNC_PROTO) && !defined(CA_DONT_INCLUDE_STDARGH)
int epicsShareAPI ca_replace_printf_handler (
int (*ca_printf_func)(const char *pformat, va_list args)
);
#else /*CAC_FUNC_PROTO*/
int epicsShareAPI ca_replace_printf_handler ();
#endif /*CAC_FUNC_PROTO*/

#ifdef __cplusplus
}
#endif

/*
 *	no additions below this endif
 */
#endif /* INCLcadefh */
