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

/*
 * done in two ifdef steps so that we will remain compatible with
 * traditional C
 */
#ifdef __cplusplus
extern "C" {
#define CAC_ANSI_FUNC_PROTO
#endif
#ifdef __STDC__ 
#define CAC_ANSI_FUNC_PROTO
#endif

#include "shareLib.h"


#ifndef HDRVERSIONID
#	define HDRVERSIONID(NAME,VERS)
#endif /*HDRVERSIONID*/

HDRVERSIONID(cadefh, "@(#) $Id$")

/* 
 * auto include of all stuff that cadef.h uses 
 */

/*
 * use two ifdef's for trad C compatibility
 */
#ifndef CA_DONT_INCLUDE_STDARGH
#ifdef CAC_ANSI_FUNC_PROTO 
#include <stdarg.h>
#endif
#endif

#ifndef INCLdb_accessh
#include "db_access.h"
#endif /* INCLdb_accessh */

#ifndef INCLcaerrh
#include "caerr.h"
#endif /* INCLcaerrh */

#ifndef INCLcaeventmaskh
#include "caeventmask.h"
#endif /* INCLcaeventmaskh */

#ifndef INCLellLibh
#include "ellLib.h"
#endif /* INCLellLibh */

/*
 *
 *	NOTE: the channel in use fields type, count, name, and
 *	host name are available to applications. However it is 
 *	recommended that the following MACROS be used to access them.
 *
 */
#define ca_field_type(CHID)     	(ca_get_field_type(CHID))
#define ca_element_count(CHID)   	(ca_get_element_count(CHID))
#define ca_name(CHID)           	((READONLY char *)((CHID)+1))
/*
 * the odd cast here removes const (and allows past practice
 * of using ca_puser(CHID) as an lvalue.
 */
#define ca_puser(CHID)           	(*(void **)&((CHID)->puser))
/*
 * here is the preferred way to load the puser ptr associated with 
 * channel (the cast removes const)
 */
#define ca_set_puser(CHID,PUSER) 	((CHID)->puser=(READONLY void *)(PUSER))
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

#ifdef CAC_ANSI_FUNC_PROTO
typedef void caCh(struct connection_handler_args args);
#else /*CAC_ANSI_FUNC_PROTO*/
typedef void caCh();
#endif /*CAC_ANSI_FUNC_PROTO*/

/* Format for the arguments to user access rights handlers 		*/
struct	access_rights_handler_args{
	struct channel_in_use 	*chid;	/* Channel id	 		*/
	caar			ar;	/* New access rights state	*/
};

#ifdef CAC_ANSI_FUNC_PROTO
typedef void caArh(struct access_rights_handler_args args);
#else /*CAC_ANSI_FUNC_PROTO*/
typedef void caArh();
#endif /*CAC_ANSI_FUNC_PROTO*/

/*
 * The fields in this structure may change or even vanish in the future.
 * Please use the macros above to access the information in this structure.
 */
struct channel_in_use{
	ELLNODE			node;		/* list ptrs			*/
	short			privType;		/* database field type 		*/
#define TYPENOTCONN	(-1)			/* the type when disconnected	*/
	unsigned short	privCount;		/* array element count 		*/
	union{
	  unsigned 		sid;		/* server id			*/
	  struct dbAddr		*paddr;		/* database address		*/
	}			id;
	READONLY void		*puser;		/* user available area		*/
	short			state;		/* connected/ disconnected etc	*/
	unsigned short		retrySeqNo;	/* search retry seq number	*/
	caar			ar;		/* access rights		*/


	caCh			*pConnFunc;
	caArh			*pAccessRightsFunc;
	ELLLIST			eventq;
	unsigned 		cid;	/* client id */
	unsigned		retry;	/* search retry number */
	void			*piiu;	/* private ioc in use block */
#ifdef vxWorks
	void			*ppn;  /* ptr to optional put notify blk */
#endif /* vxWorks */
	unsigned		claimPending:1; /* claim msg was sent if T */
	/*
	 * channel name stored directly after this structure in a 
	 * null terminated string.
	 */
};

typedef	struct channel_in_use		*chid;
typedef long				chtype;
typedef	struct pending_event		*evid;
typedef double				ca_real;

/*	The conversion routine to call for each type	*/
#define VALID_TYPE(TYPE)  (((unsigned short)TYPE)<=LAST_BUFFER_TYPE)


/* 
 * Arguments passed to event handlers and get/put call back handlers.	
 *
 * The status field below is the CA ECA_XXX status of the requested
 * operation which is saved from when the operation was attempted in the
 * server and copied back to the clients call back routine.
 * If the status is not ECA_NORMAL then the dbr pointer will be NULL
 * and the requested operation can not be assumed to be successful.
 */
typedef struct	event_handler_args{
	void		*usr;	/* User argument supplied when event added 	*/
	struct channel_in_use
			*chid;	/* Channel id					*/
	long		type;	/* the type of the value returned		*/ 
	long		count;	/* the element count of the item returned	*/
	READONLY void	*dbr;	/* Pointer to the value returned		*/
	int		status;	/* ECA_XXX Status of the requested op from server	*/
}evargs;

struct pending_event{
  	ELLNODE		node;	/* list ptrs */
#ifdef CAC_ANSI_FUNC_PROTO
  	void		(*usr_func)(struct  event_handler_args args);
#else /*CAC_ANSI_FUNC_PROTO*/
  	void		(*usr_func)();
#endif /*CAC_ANSI_FUNC_PROTO*/
  	READONLY void	*usr_arg;
  	chid		chan;
  	chtype		type;	/* requested type for local CA	*/
  	unsigned long	count;	/* requested count for local CA */
  	/* 
	 * the following provide for reissuing a 
	 * disconnected monitor 
	 */
  	ca_real		p_delta;
  	ca_real		n_delta;
  	ca_real		timeout;
	unsigned	id;
  	unsigned short	mask;
};

epicsShareFunc void epicsShareAPI ca_test_event
	(
#ifdef CAC_ANSI_FUNC_PROTO 
	struct event_handler_args
#endif /*CAC_ANSI_FUNC_PROTO*/
	);

/* Format for the arguments to user exception handlers 			*/
struct	exception_handler_args{
	void		*usr;	/* User argument supplied when event added 	*/
	struct channel_in_use
			*chid;	/* Channel id (may be NULL)			 		*/
	long 		type;	/* Requested type for the operation		*/
	long 		count;	/* Requested count for the operation		*/
	void 		*addr;	/* User's address to write results of CA_OP_GET	*/
	long		stat;	/* Channel access std status code 		*/
	long		op;	/* External codes for channel access operations	*/
	READONLY char	*ctx;	/* A character string containing context info	*/
	READONLY char	*pFile; /* source file name (may be NULL) */
	unsigned	lineNo; /* source file line number */
};

typedef unsigned CA_SYNC_GID;

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

/*
 * provides efficient test and display of channel access errors
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

#ifdef CAC_ANSI_FUNC_PROTO

epicsShareFunc short epicsShareAPI ca_get_field_type (chid chan);
epicsShareFunc unsigned short epicsShareAPI ca_get_element_count (chid chan);

/************************************************************************/
/*	Perform Library Initialization					*/
/*									*/
/*	Must be called once before calling any of the other routines	*/
/************************************************************************/
epicsShareFunc int epicsShareAPI ca_task_initialize (void);


/************************************************************************/
/*	Remove CA facility from your task				*/
/*									*/
/*	Normally called automatically at task exit			*/
/************************************************************************/
epicsShareFunc int epicsShareAPI ca_task_exit (void);

/************************************************************************
 * anachronistic entry points                                           *
 * **** Fetching a value while searching no longer supported****        *
 ************************************************************************/
#define ca_build_channel(NAME,XXXXX,CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, 1, CHIDPTR, YYYYY, 0, 0)

#define ca_array_build(NAME,XXXXX, ZZZZZZ, CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, ZZZZZZ, CHIDPTR, YYYYY, 0, 0)


/************************************************************************/
/*	Return a channel identification for the supplied channel name	*/
/*	(and attempt to create a virtual circuit)			*/
/************************************************************************/

/*
 * ca_search()
 *
 * a preferred search request API 
 *
 * pChanName 	R 	channel name string
 * pChanID 	RW 	channel id written here
 */
#define ca_search(pChanName, pChanID)\
ca_search_and_connect(pChanName, pChanID, 0, 0)


/*
 * ca_search_and_connect()
 *
 * a preferred search request API 
 *
 * pChanName 	R 	channel name string
 * pChanID 	RW 	channel id written here
 * pFunc 	R 	address of connection call-back function
 * pArg		R 	placed in the channel's puser field
 *			(fetched later by ca_puser(CHID))
 *			(passed as void * arg to (*pFunc)() above)
 */
epicsShareFunc int epicsShareAPI ca_search_and_connect
(
	 READONLY char *pChanName, 
	 chid *pChanID,	
	 void (*pFunc)(struct connection_handler_args), 
	 READONLY void *pArg
);
	
/*
 * anachronistic entry point
 * **** Fetching a value while searching no longer supported **** 
 *
 * pChanName 	R 	channel name string
 * pChanID 	RW 	channel id written here
 * pFunc 	R 	address of connection call-back function
 * pArg		R 	placed in the channel's puser field
 *			(fetched later by ca_puser(CHID))
 *			(passed as void * arg to (*pFunc)() above)
 */
epicsShareFunc int epicsShareAPI ca_build_and_connect
(
	 READONLY char *pChanName, 
	 chtype, /* pass TYPENOTCONN */
	 unsigned long, /* pass 0 */
	 chid *pChanID, 
	 void *, /* pass NULL */
	 void (*pFunc)(struct connection_handler_args),
	 void *pArg
);

/*
 * ca_change_connection_event()
 *
 * chan 	R	channel identifier	
 * pfunc	R	address of connection call-back function
 */
epicsShareFunc int epicsShareAPI ca_change_connection_event
(
	 chid	chan,
	 void	(*pfunc)(struct connection_handler_args)
);

/*
 * ca_replace_access_rights_event ()
 *
 * chan 	R	channel identifier	
 * pfunc	R	address of access rights call-back function
 */
epicsShareFunc int epicsShareAPI ca_replace_access_rights_event(
	 chid	chan,
	 void 	(*pfunc)(struct access_rights_handler_args)
);

/*
 * ca_add_exception_event ()
 *
 * replace the default exception handler
 *
 * pfunc	R	address of exception call-back function
 * pArg		R	copy of this pointer passed to exception 
 *			call-back function
 */
epicsShareFunc int epicsShareAPI ca_add_exception_event
(
	 void           (*pfunc) (struct exception_handler_args),
	 READONLY void	*pArg
);

/*
 * ca_clear_channel()
 * - deallocate resources reserved for a channel
 *
 * chanId	R	channel ID
 */
epicsShareFunc int epicsShareAPI ca_clear_channel
(
	 chid	chanId
);

/************************************************************************/
/*	Write a value to a channel					*/
/************************************************************************/
/*
 * ca_bput()
 *
 * WARNING: this copies the new value from a string (dbr_string_t) 
 *		(and not as an integer)
 *
 * chan 	R	channel identifier	
 * pValue       R       new channel value string copied from this location
 */
#define	ca_bput(chan, pValue) \
ca_array_put(DBR_STRING, 1u, chan, (READONLY dbr_string_t *) (pValue))

/*
 * ca_rput()
 *
 * WARNING: this copies the new value from a dbr_float_t 
 *
 * chan 	R	channel identifier	
 * pValue       R       new channel value copied from this location
 */
#define	ca_rput(chan,pValue) \
ca_array_put(DBR_FLOAT, 1u, chan, (READONLY dbr_float_t *) pValue)

/*
 * ca_put()
 *
 * type		R	data type from db_access.h
 * chan 	R	channel identifier	
 * pValue       R       new channel value copied from this location
 */
#define ca_put(type, chan, pValue) ca_array_put(type, 1u, chan, pValue)

/*
 * ca_array_put()
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pValue       R       new channel value copied from this location
 */
epicsShareFunc int epicsShareAPI ca_array_put
(
	 chtype type,	
	 unsigned long count,	
	 chid chanId,
	 READONLY void *pValue
);

/*
 * ca_array_put_callback()
 *
 * This routine functions identically to the original ca put request 
 * with the addition of a callback to the user supplied function 
 * after recod processing completes in the IOC. The arguments
 * to the user supplied callback function are declared in
 * the structure event_handler_args and include the pointer
 * sized user argument supplied when ca_array_put_callback() is called.
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pValue       R       new channel value copied from this location
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 */
epicsShareFunc int epicsShareAPI ca_array_put_callback
(
	 chtype type,	
	 unsigned long count,	
	 chid chanId,
	 READONLY void *pValue,
	 void (*pFunc)(struct event_handler_args),
	 READONLY void *pArg
);

#define ca_put_callback(type, chan, pValue, pFunc, pArg) \
	ca_array_put_callback(type, 1u, chan, pValue, pFunc, pArg)

/************************************************************************/
/*	Read a value from a channel					*/
/************************************************************************/

/*
 * ca_bget()
 *
 * WARNING: this copies the new value into a string (dbr_string_t) 
 *		(and not into an integer)
 *
 * chan 	R	channel identifier	
 * pValue	W	channel value copied to this location
 */
#define	ca_bget(chan, pValue) \
ca_array_get(DBR_STRING, 1u, chan, (dbr_string_t *)(pValue))

/*
 * ca_rget()
 *
 * WARNING: this copies the new value into a 32 bit float (dbr_float_t) 
 *
 * chan 	R	channel identifier	
 * pValue	W	channel value copied to this location
 */
#define	ca_rget(chan, pValue) \
ca_array_get(DBR_FLOAT, 1u, chan, (dbr_float_t *)(pValue))

/*
 * ca_rget()
 *
 * type		R	data type from db_access.h
 * chan 	R	channel identifier	
 * pValue	W	channel value copied to this location
 */
#define ca_get(type, chan, pValue) ca_array_get(type, 1u, chan, pValue)

/*
 * ca_array_get()
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pValue	W	channel value copied to this location
 */
epicsShareFunc int epicsShareAPI ca_array_get
(
	 chtype type,	
	 unsigned long count,	
	 chid chanId,
	 void *pValue
);

/************************************************************************/
/*	Read a value from a channel and run a callback when the value	*/
/*	returns								*/
/*									*/
/*									*/
/************************************************************************/
/*
 * ca_bget_callback()
 *
 * WARNING: this returns the new value as a string (dbr_string_t) 
 *		(and not as an integer)
 *
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 */
#define	ca_bget_callback(chan, pFunc, pArg)\
ca_array_get_callback(DBR_STRING, 1u, chan, pFunc, pArg)

/*
 * ca_rget_callback()
 *
 * WARNING: this returns the new value as a float (dbr_float_t) 
 *
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 */
#define	ca_rget_callback(chan, pFunc, pArg)\
ca_array_get_callback(DBR_FLOAT, 1u, chan, pFunc, pArg)

/*
 * ca_get_callback()
 *
 * type		R	data type from db_access.h
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 */
#define	ca_get_callback(type, chan, pFunc, pArg)\
ca_array_get_callback(type, 1u, chan, pFunc, pArg)

/*
 * ca_array_get_callback()
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 */
epicsShareFunc int epicsShareAPI ca_array_get_callback
(
	 chtype type,	
	 unsigned long count,	
	 chid chanId,
	 void (*pFunc)(struct event_handler_args),
	 READONLY void *pArg
);

/************************************************************************/
/*	Specify a function to be executed whenever significant changes	*/
/*	occur to a channel.						*/
/*	NOTES:								*/
/*	1)	Evid may be omited by passing a NULL pointer		*/
/*									*/
/*	2)	An array count of zero specifies the native db count	*/ 
/*									*/
/************************************************************************/

/*
 * ca_add_event ()
 *
 * assumes "delta" info comes from the database or defaults
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 * pEventID	W	event id written at specified address
 */
#define ca_add_event(type,chan,pFunc,pArg,pEventID)\
ca_add_array_event(type,1u,chan,pFunc,pArg,0.0,0.0,0.0,pEventID)


/*	Sets both P_DELTA and M_DELTA below to argument DELTA		*/
#define ca_add_delta_event(TYPE,CHID,ENTRY,ARG,DELTA,EVID)\
	ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,DELTA,DELTA,0.0,EVID)

#define ca_add_general_event(TYPE,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)

#define	ca_add_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_masked_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID, DBE_VALUE | DBE_ALARM)

/*
 * ca_add_masked_array_event ()
 *
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pFunc	R	pointer to call-back function
 * pArg		R	copy of this pointer passed to pFunc
 * p_delta	R	not currently used (set to 0.0)	
 * n_delta	R	not currently used (set to 0.0)
 * timeout	R	not currently used (set to 0.0)
 * pEventID	W	event id written at specified address
 * mask		R	event mask - one of {DBE_VALUE, DBE_ALARM, DBE_LOG}
 */
epicsShareFunc int epicsShareAPI ca_add_masked_array_event
(
	 chtype type,	
	 unsigned long count,	
	 chid chanId,
	 void (*pFunc)(struct event_handler_args),
	 READONLY void *pArg,
	 ca_real p_delta,
	 ca_real n_delta,
	 ca_real timeout,
	 evid *pEventID, 
	 long mask
);

/************************************************************************/
/*	Remove a function from a list of those specified to run 	*/
/*	whenever significant changes occur to a channel			*/
/*									*/
/************************************************************************/
/*
 * ca_clear_event()
 *
 * eventID	R	event id
 */
epicsShareFunc int epicsShareAPI ca_clear_event
(
	 evid eventID
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

/*
 * ca_pend()
 *
 * timeOut	R	delay in seconds
 * early	R	T: return early if all get requests (or search
 *			requests with null connection handler pointer 
 *			have completed)
 *			F: wait for the entire delay to expire
 */
epicsShareFunc int epicsShareAPI ca_pend
(
	 ca_real timeOut,
	 int early
);

/*
 * ca_test_io()
 *
 * returns TRUE when get requests (or search requests with null 
 * connection handler pointer) are outstanding
 */
epicsShareFunc int epicsShareAPI ca_test_io (void);

/************************************************************************/
/*	Send out all outstanding messages in the send queue		*/
/************************************************************************/
/*
 * ca_flush_io()
 */
epicsShareFunc int epicsShareAPI ca_flush_io (void);


/*
 *  ca_signal()
 *
 * errorCode	R	status returned from channel access function
 * pCtxStr	R	context string included with error print out
 */
epicsShareFunc void epicsShareAPI ca_signal
(
	 long errorCode, 	
	 READONLY char *pCtxStr 	
);

/*
 * ca_signal_with_file_and_lineno()
 * errorCode	R	status returned from channel access function
 * pCtxStr	R	context string included with error print out
 * pFileStr	R	file name string included with error print out
 * lineNo	R	line number included with error print out
 *
 */
epicsShareFunc void epicsShareAPI ca_signal_with_file_and_lineno
(
	 long errorCode,	
	 READONLY char *pCtxStr,	
	 READONLY char *pFileStr,	
	 int lineNo		
);

/*
 * ca_host_name_function()
 *
 * channel	R	channel identifier
 */
epicsShareFunc READONLY char * epicsShareAPI ca_host_name_function ( chid channel);

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
typedef void CAFDHANDLER(void *parg, int fd, int opened); 

/*
 * ca_add_fd_registration()
 *
 * pHandler	R	pointer to function which is to be called
 *			when an fd is created or deleted
 * pArg		R	argument passed to above function
 */
epicsShareFunc int epicsShareAPI ca_add_fd_registration
(
	 CAFDHANDLER 	*pHandler,
	 READONLY void    	*pArg
);

/*
 * ca_channel_status()
 *
 * tid		R	task id
 */
#ifdef vxWorks
epicsShareFunc int epicsShareAPI ca_channel_status( int tid);
#endif

/*
 * ca_import ()
 *
 * tid		R	task id
 */
#ifdef vxWorks
int ca_import(int tid);
#endif /* vxWorks */

/*
 * ca_import_cancel ()
 *
 * tid		R	task id
 */
#ifdef vxWorks
int ca_import_cancel(int tid);
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

/*
 * ca_sg_create()
 *
 * create a sync group
 *
 * pgid		W	pointer to sync group id that will be written	
 */
epicsShareFunc int epicsShareAPI ca_sg_create( CA_SYNC_GID *  pgid);

/*
 * ca_sg_delete()
 *
 * delete a sync group
 *
 * gid		R	sync group id 
 */
epicsShareFunc int epicsShareAPI ca_sg_delete(READONLY CA_SYNC_GID gid);

/*
 * ca_sg_block()
 *
 * block for IO performed within a sync group to complete 
 *
 * gid		R	sync group id 
 * timeout	R	wait for this duration prior to timing out
 *			and returning ECA_TIMEOUT
 */
epicsShareFunc int epicsShareAPI ca_sg_block(READONLY CA_SYNC_GID gid, ca_real timeout);

/*
 * ca_sg_test()
 *
 * test for sync group IO operations in progress
 *
 * gid		R	sync group id
 * 
 * returns one of ECA_BADSYNCGRP, ECA_IOINPROGRESS, ECA_IODONE
 */
epicsShareFunc int epicsShareAPI ca_sg_test(READONLY CA_SYNC_GID gid);

/*
 * ca_sg_reset
 *
 * gid		R	sync group id
 */
epicsShareFunc int epicsShareAPI ca_sg_reset(READONLY CA_SYNC_GID gid);

/*
 * ca_sg_array_get()
 *
 * initiate a get within a sync group
 * (essentially a ca_array_get() with a sync group specified)
 *
 * gid		R	sync group id
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pValue	W	channel value copied to this location
 */
epicsShareFunc int epicsShareAPI ca_sg_array_get
(
	READONLY CA_SYNC_GID gid,
	chtype type, /*      TYPE    R       channel type                            */
	unsigned long count,
	chid chan,
	void *pValue  
);

#define ca_sg_get(gid, type, chan, pValue) \
ca_sg_array_get(gid, type, 1u, chan, pValue)

/*
 * ca_sg_array_put()
 *
 * initiate a put within a sync group
 * (essentially a ca_array_put() with a sync group specified)
 *
 * gid		R	sync group id
 * type		R	data type from db_access.h
 * count	R	array element count
 * chan 	R	channel identifier	
 * pValue	R	new channel value copied from this location
 */
epicsShareFunc int epicsShareAPI ca_sg_array_put
(
	READONLY CA_SYNC_GID gid,
	chtype type, 
	unsigned long count,
	chid chan,
	READONLY void *pValue  
);

#define ca_sg_put(gid, type, chan, pValue) \
ca_sg_array_put(gid, type, 1u, chan, pValue)

/*
 * ca_sg_stat()
 *
 * print status of a sync group
 *
 * gid		R	sync group id
 */
epicsShareFunc int epicsShareAPI ca_sg_stat(READONLY CA_SYNC_GID gid);

/*
 * ca_modify_user_name()
 *
 * Modify or override the default
 * client user name.
 *
 * pUserName	R	new user name string copied from this location	
 */
epicsShareFunc int epicsShareAPI ca_modify_user_name(READONLY char *pUserName);

/*
 * CA_MODIFY_HOST_NAME()
 *
 * Modify or override the default
 * client host name.
 *
 * pHostName	R	new host name string copied from this location	
 */
epicsShareFunc int epicsShareAPI ca_modify_host_name(READONLY char *pHostName);

/*
 * ca_v42_ok()
 *
 * Put call back is available if the CA server is on version is 4.2 
 *	or higher.
 *
 * chan		R	channel identifier
 * 
 * (returns true or false)
 */
epicsShareFunc int epicsShareAPI ca_v42_ok(chid chan);

/*
 * ca_version()
 *
 * returns the CA version string
 */
epicsShareFunc READONLY char * epicsShareAPI ca_version(void);

/*
 * ca_replace_printf_handler ()
 *
 * for apps that want to change where ca formatted
 * text output goes
 *
 * use two ifdef's for trad C compatibility
 *
 * ca_printf_func	R	pointer to new function called when
 *				CA prints an error message
 */
#ifndef CA_DONT_INCLUDE_STDARGH
epicsShareFunc int epicsShareAPI ca_replace_printf_handler (
	int (*ca_printf_func)(READONLY char *pformat, va_list args)
);
#endif /*CA_DONT_INCLUDE_STDARGH*/

/*
 * ca_get_ioc_connection_count()
 *
 * returns the number of IOC's that CA is connected to
 * (for testing purposes only)
 */
epicsShareFunc unsigned epicsShareAPI ca_get_ioc_connection_count ();

#else /* CAC_ANSI_FUNC_PROTO */
epicsShareFunc short epicsShareAPI ca_get_field_type ();
epicsShareFunc unsigned short epicsShareAPI ca_get_element_count ();
epicsShareFunc int epicsShareAPI ca_task_initialize ();
epicsShareFunc int epicsShareAPI ca_task_exit ();
epicsShareFunc int epicsShareAPI ca_search_and_connect ();
epicsShareFunc int epicsShareAPI ca_build_and_connect ();
epicsShareFunc int epicsShareAPI ca_change_connection_event ();
epicsShareFunc int epicsShareAPI ca_replace_access_rights_event ();
epicsShareFunc int epicsShareAPI ca_add_exception_event ();
epicsShareFunc int epicsShareAPI ca_clear_channel ();
epicsShareFunc int epicsShareAPI ca_array_put ();
epicsShareFunc int epicsShareAPI ca_array_put_callback ();
epicsShareFunc int epicsShareAPI ca_array_get ();
epicsShareFunc int epicsShareAPI ca_array_get_callback ();
epicsShareFunc int epicsShareAPI ca_add_masked_array_event ();
epicsShareFunc int epicsShareAPI ca_clear_event ();
epicsShareFunc int epicsShareAPI ca_pend ();
epicsShareFunc int epicsShareAPI ca_test_io ();
epicsShareFunc int epicsShareAPI ca_flush_io ();
epicsShareFunc void epicsShareAPI ca_signal ();
epicsShareFunc void epicsShareAPI ca_signal_with_file_and_lineno ();
epicsShareFunc char * epicsShareAPI ca_host_name_function ();
typedef void CAFDHANDLER(); 
epicsShareFunc int epicsShareAPI ca_add_fd_registration();
epicsShareFunc int epicsShareAPI ca_replace_printf_handler ();
epicsShareFunc int epicsShareAPI ca_sg_create();
epicsShareFunc int epicsShareAPI ca_sg_delete();
epicsShareFunc int epicsShareAPI ca_sg_block();
epicsShareFunc int epicsShareAPI ca_sg_test();
epicsShareFunc int epicsShareAPI ca_sg_reset();
epicsShareFunc int epicsShareAPI ca_sg_array_get();
epicsShareFunc int epicsShareAPI ca_sg_array_put();
epicsShareFunc int epicsShareAPI ca_sg_stat();
epicsShareFunc int epicsShareAPI ca_modify_user_name();
epicsShareFunc int epicsShareAPI ca_modify_host_name();
epicsShareFunc int epicsShareAPI ca_v42_ok();
epicsShareFunc char * epicsShareAPI ca_version();
#define ca_build_channel(NAME,XXXXX,CHIDPTR,YYYYY)\
	ca_build_and_connect(NAME, XXXXX, 1, CHIDPTR, YYYYY, 0, 0)
#define ca_array_build(NAME,XXXXX, ZZZZZZ, CHIDPTR,YYYYY)\
	ca_build_and_connect(NAME, XXXXX, ZZZZZZ, CHIDPTR, YYYYY, 0, 0)
#define ca_search(pChanName, pChanID)\
	ca_search_and_connect(pChanName, pChanID, 0, 0)
#define ca_bput(chan, pValue) \
	ca_array_put(DBR_STRING, 1, chan, (READONLY dbr_string_t *) (pValue))
#define ca_rput(chan,pValue) \
	ca_array_put(DBR_FLOAT, 1, chan, (READONLY dbr_float_t *) pValue)
#define ca_put(type, chan, pValue) ca_array_put(type, 1, chan, pValue)
#define ca_bget(chan, pValue) \
	ca_array_get(DBR_STRING, 1, chan, (dbr_string_t *)(pValue))
#define ca_rget(chan, pValue) \
	ca_array_get(DBR_FLOAT, 1, chan, (dbr_float_t *)(pValue))
#define ca_get(type, chan, pValue) ca_array_get(type, 1, chan, pValue)
#define ca_bget_callback(chan, pFunc, pArg)\
	ca_array_get_callback(DBR_STRING, 1, chan, pFunc, pArg)
#define ca_rget_callback(chan, pFunc, pArg)\
	ca_array_get_callback(DBR_FLOAT, 1, chan, pFunc, pArg)
#define ca_get_callback(type, chan, pFunc, pArg)\
	ca_array_get_callback(type, 1, chan, pFunc, pArg)
#define ca_put_callback(type, chan, pValue, pFunc, pArg) \
	ca_array_put_callback(type, 1u, chan, pValue, pFunc, pArg)
#define ca_add_event(type,chan,pFunc,pArg,pEventID)\
	ca_add_array_event(type,1,chan,pFunc,pArg,0.0,0.0,0.0,pEventID)
#define ca_add_delta_event(TYPE,CHID,ENTRY,ARG,DELTA,EVID)\
        ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,DELTA,DELTA,0.0,EVID)
#define ca_add_general_event(TYPE,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
	ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)
#define ca_add_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_masked_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID, DBE_VALUE | DBE_ALARM)
#define ca_poll() ca_pend((1e-12), 0/*FALSE*/)
#define ca_pend_event(TIMEOUT) ca_pend((TIMEOUT), 0/*FALSE*/)
#define ca_pend_io(TIMEOUT) ca_pend((TIMEOUT), 1/*TRUE*/)
#define ca_sg_get(gid, type, chan, pValue) \
ca_sg_array_get(gid, type, 1u, chan, pValue)
#define ca_sg_put(gid, type, chan, pValue) \
ca_sg_array_put(gid, type, 1u, chan, pValue)

#ifdef vxWorks
	epicsShareFunc int epicsShareAPI ca_channel_status()
	epicsShareFunc int epicsShareAPI ca_import();
	epicsShareFunc int epicsShareAPI ca_import_cancel();
#endif

#endif /* CAC_ANSI_FUNC_PROTO */

#ifdef __cplusplus
}
#endif

/*
 *	no additions below this endif
 */
#endif /* INCLcadefh */
