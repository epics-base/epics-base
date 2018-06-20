/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* 
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author Jeffrey O. Hill
 *  johill@lanl.gov
 *  505 665 1831
 *
 */

#ifndef INCLcadefh
#define INCLcadefh

/*
 * done in two ifdef steps so that we will remain compatible with
 * traditional C
 */
#ifndef CA_DONT_INCLUDE_STDARGH
#   include <stdarg.h>
#endif

#ifdef epicsExportSharedSymbols
#   define INCLcadefh_accessh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "epicsThread.h"

#ifdef INCLcadefh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif


#include "caerr.h"
#include "db_access.h"
#include "caeventmask.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oldChannelNotify *chid;
typedef chid                    chanId; /* for when the structures field name is "chid" */
typedef long                    chtype;
typedef struct oldSubscription  *evid;
typedef double                  ca_real;

/* arguments passed to user connection handlers */
struct  connection_handler_args {
    chanId  chid;  /* channel id */
    long    op;    /* one of CA_OP_CONN_UP or CA_OP_CONN_DOWN */
};

typedef void caCh (struct connection_handler_args args);

typedef struct ca_access_rights {
    unsigned    read_access:1;
    unsigned    write_access:1;
} caar;

/* arguments passed to user access rights handlers */
struct  access_rights_handler_args {
    chanId  chid;   /* channel id */
    caar    ar;     /* new access rights state */
};

typedef void caArh (struct access_rights_handler_args args);

/*  The conversion routine to call for each type    */
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
typedef struct event_handler_args {
    void            *usr;   /* user argument supplied with request */
    chanId          chid;   /* channel id */
    long            type;   /* the type of the item returned */ 
    long            count;  /* the element count of the item returned */
    const void      *dbr;   /* a pointer to the item returned */
    int             status; /* ECA_XXX status of the requested op from the server */
} evargs;
typedef void caEventCallBackFunc (struct event_handler_args);

epicsShareFunc void epicsShareAPI ca_test_event
(
    struct event_handler_args
);

/* arguments passed to user exception handlers */
struct exception_handler_args {
    void            *usr;   /* user argument supplied when installed */
    chanId          chid;   /* channel id (may be nill) */
    long            type;   /* type requested */
    long            count;  /* count requested */
    void            *addr;  /* user's address to write results of CA_OP_GET */
    long            stat;   /* channel access ECA_XXXX status code */
    long            op;     /* CA_OP_GET, CA_OP_PUT, ..., CA_OP_OTHER */
    const char      *ctx;   /* a character string containing context info */
    const char      *pFile; /* source file name (may be NULL) */
    unsigned        lineNo; /* source file line number (may be zero) */
};

typedef unsigned CA_SYNC_GID;

/*
 *  External OP codes for CA operations
 */
#define CA_OP_GET             0
#define CA_OP_PUT             1
#define CA_OP_CREATE_CHANNEL  2
#define CA_OP_ADD_EVENT       3
#define CA_OP_CLEAR_EVENT     4
#define CA_OP_OTHER           5

/* 
 * used with connection_handler_args 
 */
#define CA_OP_CONN_UP       6
#define CA_OP_CONN_DOWN     7

/* depricated */
#define CA_OP_SEARCH        2

/*
 * provides efficient test and display of channel access errors
 */
#define     SEVCHK(CA_ERROR_CODE, MESSAGE_STRING) \
{ \
    int ca_unique_status_name  = (CA_ERROR_CODE); \
    if(!(ca_unique_status_name & CA_M_SUCCESS)) \
        ca_signal_with_file_and_lineno( \
            ca_unique_status_name, \
            (MESSAGE_STRING), \
            __FILE__, \
            __LINE__); \
}


#define TYPENOTCONN (-1) /* the channel's native type when disconnected   */
epicsShareFunc short epicsShareAPI ca_field_type (chid chan);
epicsShareFunc unsigned long epicsShareAPI ca_element_count (chid chan);
epicsShareFunc const char * epicsShareAPI ca_name (chid chan);
epicsShareFunc void epicsShareAPI ca_set_puser (chid chan, void *puser);
epicsShareFunc void * epicsShareAPI ca_puser (chid chan);
epicsShareFunc unsigned epicsShareAPI ca_read_access (chid chan);
epicsShareFunc unsigned epicsShareAPI ca_write_access (chid chan);

/*
 *  cs_ - `channel state'   
 *  
 *  cs_never_conn       valid chid, IOC not found
 *  cs_prev_conn        valid chid, IOC was found, but unavailable
 *  cs_conn             valid chid, IOC was found, still available
 *  cs_closed           channel deleted by user
 */
enum channel_state {cs_never_conn, cs_prev_conn, cs_conn, cs_closed};
epicsShareFunc enum channel_state epicsShareAPI ca_state (chid chan);

/************************************************************************/
/*  Perform Library Initialization                                      */
/*                                                                      */
/*  Must be called once before calling any of the other routines        */
/************************************************************************/
epicsShareFunc int epicsShareAPI ca_task_initialize (void);
enum ca_preemptive_callback_select 
{ ca_disable_preemptive_callback, ca_enable_preemptive_callback };
epicsShareFunc int epicsShareAPI 
        ca_context_create (enum ca_preemptive_callback_select select);
epicsShareFunc void epicsShareAPI ca_detach_context (); 

/************************************************************************/
/*  Remove CA facility from your task                                   */
/*                                                                      */
/*  Normally called automatically at task exit                          */
/************************************************************************/
epicsShareFunc int epicsShareAPI ca_task_exit (void);
epicsShareFunc void epicsShareAPI ca_context_destroy (void);

typedef unsigned capri; 
#define CA_PRIORITY_MAX 99
#define CA_PRIORITY_MIN 0
#define CA_PRIORITY_DEFAULT CA_PRIORITY_MIN

#define CA_PRIORITY_DB_LINKS 80
#define CA_PRIORITY_ARCHIVE 20
#define CA_PRIORITY_OPI 0

/*
 * ca_create_channel ()
 *
 * pChanName            R   channel name string
 * pConnStateCallback   R   address of connection state change 
 *                          callback function
 * pUserPrivate         R   placed in the channel's user private field
 *                          o can be fetched later by ca_puser(CHID)
 *                          o passed as void * arg to *pConnectCallback above
 * priority             R   priority level in the server 0 - 100
 * pChanID              RW  channel id written here
 */
epicsShareFunc int epicsShareAPI ca_create_channel
(
     const char     *pChanName, 
     caCh           *pConnStateCallback, 
     void           *pUserPrivate,
     capri          priority,
     chid           *pChanID
);

/*
 * ca_change_connection_event()
 *
 * chan     R   channel identifier  
 * pfunc    R   address of connection call-back function
 */
epicsShareFunc int epicsShareAPI ca_change_connection_event
(
     chid       chan,
     caCh *     pfunc
);

/*
 * ca_replace_access_rights_event ()
 *
 * chan     R   channel identifier  
 * pfunc    R   address of access rights call-back function
 */
epicsShareFunc int epicsShareAPI ca_replace_access_rights_event (
     chid   chan,
     caArh  *pfunc
);

/*
 * ca_add_exception_event ()
 *
 * replace the default exception handler
 *
 * pfunc    R   address of exception call-back function
 * pArg     R   copy of this pointer passed to exception 
 *          call-back function
 */
typedef void caExceptionHandler (struct exception_handler_args);
epicsShareFunc int epicsShareAPI ca_add_exception_event
(
     caExceptionHandler *pfunc,
     void               *pArg
);

/*
 * ca_clear_channel()
 * - deallocate resources reserved for a channel
 *
 * chanId   R   channel ID
 */
epicsShareFunc int epicsShareAPI ca_clear_channel
(
     chid   chanId
);

/************************************************************************/
/*  Write a value to a channel                  */
/************************************************************************/
/*
 * ca_bput()
 *
 * WARNING: this copies the new value from a string (dbr_string_t) 
 *      (and not as an integer)
 *
 * chan         R       channel identifier  
 * pValue       R       new channel value string copied from this location
 */
#define ca_bput(chan, pValue) \
ca_array_put(DBR_STRING, 1u, chan, (const dbr_string_t *) (pValue))

/*
 * ca_rput()
 *
 * WARNING: this copies the new value from a dbr_float_t 
 *
 * chan         R       channel identifier  
 * pValue       R       new channel value copied from this location
 */
#define ca_rput(chan,pValue) \
ca_array_put(DBR_FLOAT, 1u, chan, (const dbr_float_t *) pValue)

/*
 * ca_put()
 *
 * type         R   data type from db_access.h
 * chan         R   channel identifier  
 * pValue       R   new channel value copied from this location
 */
#define ca_put(type, chan, pValue) ca_array_put (type, 1u, chan, pValue)

/*
 * ca_array_put()
 *
 * type         R   data type from db_access.h
 * count        R   array element count
 * chan         R   channel identifier  
 * pValue       R       new channel value copied from this location
 */
epicsShareFunc int epicsShareAPI ca_array_put
(
     chtype         type,   
     unsigned long  count,   
     chid           chanId,
     const void *   pValue
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
 * type         R   data type from db_access.h
 * count        R   array element count
 * chan         R   channel identifier  
 * pValue       R       new channel value copied from this location
 * pFunc        R   pointer to call-back function
 * pArg         R   copy of this pointer passed to pFunc
 */
epicsShareFunc int epicsShareAPI ca_array_put_callback
(
     chtype                 type,   
     unsigned long          count,   
     chid                   chanId,
     const void *           pValue,
     caEventCallBackFunc *  pFunc,
     void *                 pArg
);

#define ca_put_callback(type, chan, pValue, pFunc, pArg) \
    ca_array_put_callback(type, 1u, chan, pValue, pFunc, pArg)

/************************************************************************/
/*  Read a value from a channel                                         */
/************************************************************************/

/*
 * ca_bget()
 *
 * WARNING: this copies the new value into a string (dbr_string_t) 
 *      (and not into an integer)
 *
 * chan     R   channel identifier  
 * pValue   W   channel value copied to this location
 */
#define ca_bget(chan, pValue) \
ca_array_get(DBR_STRING, 1u, chan, (dbr_string_t *)(pValue))

/*
 * ca_rget()
 *
 * WARNING: this copies the new value into a 32 bit float (dbr_float_t) 
 *
 * chan     R   channel identifier  
 * pValue   W   channel value copied to this location
 */
#define ca_rget(chan, pValue) \
ca_array_get(DBR_FLOAT, 1u, chan, (dbr_float_t *)(pValue))

/*
 * ca_rget()
 *
 * type     R   data type from db_access.h
 * chan     R   channel identifier  
 * pValue   W   channel value copied to this location
 */
#define ca_get(type, chan, pValue) ca_array_get(type, 1u, chan, pValue)

/*
 * ca_array_get()
 *
 * type     R   data type from db_access.h
 * count    R   array element count
 * chan     R   channel identifier  
 * pValue   W   channel value copied to this location
 */
epicsShareFunc int epicsShareAPI ca_array_get
(
     chtype         type,   
     unsigned long  count,   
     chid           chanId,
     void *         pValue
);

/************************************************************************/
/*  Read a value from a channel and run a callback when the value       */
/*  returns                                                             */
/*                                                                      */
/*                                                                      */
/************************************************************************/
/*
 * ca_bget_callback()
 *
 * WARNING: this returns the new value as a string (dbr_string_t) 
 *      (and not as an integer)
 *
 * chan     R   channel identifier  
 * pFunc    R   pointer to call-back function
 * pArg     R   copy of this pointer passed to pFunc
 */
#define ca_bget_callback(chan, pFunc, pArg)\
ca_array_get_callback (DBR_STRING, 1u, chan, pFunc, pArg)

/*
 * ca_rget_callback()
 *
 * WARNING: this returns the new value as a float (dbr_float_t) 
 *
 * chan     R   channel identifier  
 * pFunc    R   pointer to call-back function
 * pArg     R   copy of this pointer passed to pFunc
 */
#define ca_rget_callback(chan, pFunc, pArg)\
ca_array_get_callback (DBR_FLOAT, 1u, chan, pFunc, pArg)

/*
 * ca_get_callback()
 *
 * type     R   data type from db_access.h
 * chan     R   channel identifier  
 * pFunc    R   pointer to call-back function
 * pArg     R   copy of this pointer passed to pFunc
 */
#define ca_get_callback(type, chan, pFunc, pArg)\
ca_array_get_callback (type, 1u, chan, pFunc, pArg)

/*
 * ca_array_get_callback()
 *
 * type     R   data type from db_access.h
 * count    R   array element count
 * chan     R   channel identifier  
 * pFunc    R   pointer to call-back function
 * pArg     R   copy of this pointer passed to pFunc
 */
epicsShareFunc int epicsShareAPI ca_array_get_callback
(
     chtype                 type,   
     unsigned long          count,   
     chid                   chanId,
     caEventCallBackFunc *  pFunc,
     void *                 pArg
);

/************************************************************************/
/*  Specify a function to be executed whenever significant changes      */
/*  occur to a channel.                                                 */
/*  NOTES:                                                              */
/*  1)  Evid may be omited by passing a NULL pointer                    */
/*                                                                      */
/*  2)  An array count of zero specifies the native db count            */ 
/*                                                                      */
/************************************************************************/

/*
 * ca_create_subscription ()
 *
 * type     R   data type from db_access.h
 * count    R   array element count
 * chan     R   channel identifier  
 * mask     R   event mask - one of {DBE_VALUE, DBE_ALARM, DBE_LOG}
 * pFunc    R   pointer to call-back function
 * pArg     R   copy of this pointer passed to pFunc
 * pEventID W   event id written at specified address
 */
epicsShareFunc int epicsShareAPI ca_create_subscription
(
     chtype                 type,   
     unsigned long          count,   
     chid                   chanId,
     long                   mask,
     caEventCallBackFunc *  pFunc,
     void *                 pArg,
     evid *                 pEventID
);

/************************************************************************/
/*  Remove a function from a list of those specified to run             */
/*  whenever significant changes occur to a channel                     */
/*                                                                      */
/************************************************************************/
/*
 * ca_clear_subscription()
 *
 * eventID  R   event id
 */
epicsShareFunc int epicsShareAPI ca_clear_subscription
(
     evid eventID
);

epicsShareFunc chid epicsShareAPI ca_evid_to_chid ( evid id );


/************************************************************************/
/*                                                                      */
/*   Requested data is not necessarily stable prior to                  */
/*   return from called subroutine.  Call ca_pend_io()                  */
/*   to guarantee that requested data is stable.  Call the routine      */
/*   ca_flush_io() to force all outstanding requests to be              */
/*   sent out over the network.  Significant increases in               */
/*   performance have been measured when batching several remote        */
/*   requests together into one message.  Additional                    */
/*   improvements can be obtained by performing local processing        */
/*   in parallel with outstanding remote processing.                    */
/*                                                                      */
/*   FLOW OF TYPICAL APPLICATION                                        */
/*                                                                      */
/*   search()       ! Obtain Channel ids                                */
/*   .              ! "				                                    */
/*   .              ! "                                                 */
/*   pend_io        ! wait for channels to connect                      */
/*                                                                      */
/*   get()          ! several requests for remote info                  */
/*   get()          ! "					                                */
/*   add_event()    ! "					                                */	
/*   get()          ! "					                                */
/*   .                                                                  */
/*   .                                                                  */
/*   .                                                                  */
/*   flush_io()     ! send get requests                                 */
/*                  ! optional parallel processing                      */
/*   .              ! "					                                */
/*   .              ! "					                                */
/*   pend_io()      ! wait for replies from get requests                */
/*   .              ! access to requested data                          */
/*   .              ! "					                                */
/*   pend_event()   ! wait for requested events                         */
/*                                                                      */
/************************************************************************/

/************************************************************************/
/*  These routines wait for channel subscription events and call the    */
/*  functions specified with add_event when events occur. If the        */
/*  timeout is specified as 0 an infinite timeout is assumed.           */
/*  ca_flush_io() is called by this routine. If ca_pend_io ()           */
/*  is called when no IO is outstanding then it will return immediately */ 
/*  without processing.                                                 */
/************************************************************************/

/*
 * ca_pend_event()
 *
 * timeOut  R   wait for this delay in seconds
 */
epicsShareFunc int epicsShareAPI ca_pend_event (ca_real timeOut);
#define ca_poll() ca_pend_event(1e-12)

/*
 * ca_pend_io()
 *
 * timeOut  R   wait for this delay in seconds but return early 
 *              if all get requests (or search requests with null 
 *              connection handler pointer have completed)
 */
epicsShareFunc int epicsShareAPI ca_pend_io (ca_real timeOut);

/* calls ca_pend_io() if early is true otherwise ca_pend_event() is called */
epicsShareFunc int epicsShareAPI ca_pend (ca_real timeout, int early);

/*
 * ca_test_io()
 *
 * returns TRUE when get requests (or search requests with null 
 * connection handler pointer) are outstanding
 */
epicsShareFunc int epicsShareAPI ca_test_io (void);

/************************************************************************/
/*  Send out all outstanding messages in the send queue                 */
/************************************************************************/
/*
 * ca_flush_io()
 */
epicsShareFunc int epicsShareAPI ca_flush_io (void);


/*
 *  ca_signal()
 *
 * errorCode    R   status returned from channel access function
 * pCtxStr  R   context string included with error print out
 */
epicsShareFunc void epicsShareAPI ca_signal
(
     long errorCode,    
     const char *pCtxStr     
);

/*
 * ca_signal_with_file_and_lineno()
 * errorCode    R   status returned from channel access function
 * pCtxStr  R   context string included with error print out
 * pFileStr R   file name string included with error print out
 * lineNo   R   line number included with error print out
 *
 */
epicsShareFunc void epicsShareAPI ca_signal_with_file_and_lineno
(
     long errorCode,    
     const char *pCtxStr,    
     const char *pFileStr,   
     int lineNo     
);

/*
 * ca_signal_formated()
 * errorCode    R   status returned from channel access function
 * pFileStr     R   file name string included with error print out
 * lineNo       R   line number included with error print out
 * pFormat      R   printf dtyle format string (and optional arguments)
 *
 */
epicsShareFunc void epicsShareAPI ca_signal_formated (long ca_status, const char *pfilenm, 
                                       int lineno, const char *pFormat, ...);

/*
 * ca_host_name_function()
 *
 * channel  R   channel identifier
 *
 * !!!! this function is _not_ thread safe !!!!
 */
epicsShareFunc const char * epicsShareAPI ca_host_name (chid channel);
/* thread safe version */
epicsShareFunc unsigned epicsShareAPI ca_get_host_name ( chid pChan, 
    char *pBuf, unsigned bufLength );

/*
 *  CA_ADD_FD_REGISTRATION
 *
 *  call their function with their argument whenever 
 *  a new fd is added or removed
 *  (for use with a manager of the select system call under UNIX)
 *
 *  if (opened) then fd was created
 *  if (!opened) then fd was deleted
 *
 */
typedef void CAFDHANDLER (void *parg, int fd, int opened); 

/*
 * ca_add_fd_registration()
 *
 * pHandler R   pointer to function which is to be called
 *                  when an fd is created or deleted
 * pArg     R   argument passed to above function
 */
epicsShareFunc int epicsShareAPI ca_add_fd_registration
(
     CAFDHANDLER    *pHandler,
     void           *pArg
);


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
 * pgid     W   pointer to sync group id that will be written   
 */
epicsShareFunc int epicsShareAPI ca_sg_create (CA_SYNC_GID *  pgid);

/*
 * ca_sg_delete()
 *
 * delete a sync group
 *
 * gid      R   sync group id 
 */
epicsShareFunc int epicsShareAPI ca_sg_delete (const CA_SYNC_GID gid);

/*
 * ca_sg_block()
 *
 * block for IO performed within a sync group to complete 
 *
 * gid      R   sync group id 
 * timeout  R   wait for this duration prior to timing out
 *          and returning ECA_TIMEOUT
 */
epicsShareFunc int epicsShareAPI ca_sg_block (const CA_SYNC_GID gid, ca_real timeout);

/*
 * ca_sg_test()
 *
 * test for sync group IO operations in progress
 *
 * gid      R   sync group id
 * 
 * returns one of ECA_BADSYNCGRP, ECA_IOINPROGRESS, ECA_IODONE
 */
epicsShareFunc int epicsShareAPI ca_sg_test (const CA_SYNC_GID gid);

/*
 * ca_sg_reset
 *
 * gid      R   sync group id
 */
epicsShareFunc int epicsShareAPI ca_sg_reset(const CA_SYNC_GID gid);

/*
 * ca_sg_array_get()
 *
 * initiate a get within a sync group
 * (essentially a ca_array_get() with a sync group specified)
 *
 * gid      R   sync group id
 * type     R   data type from db_access.h
 * count    R   array element count
 * chan     R   channel identifier  
 * pValue   W   channel value copied to this location
 */
epicsShareFunc int epicsShareAPI ca_sg_array_get
(
    const CA_SYNC_GID gid,
    chtype type, 
    unsigned long count,
    chid chan,
    void *pValue  
);

#define ca_sg_get(gid, type, chan, pValue) \
ca_sg_array_get (gid, type, 1u, chan, pValue)

/*
 * ca_sg_array_put()
 *
 * initiate a put within a sync group
 * (essentially a ca_array_put() with a sync group specified)
 *
 * gid      R   sync group id
 * type     R   data type from db_access.h
 * count    R   array element count
 * chan     R   channel identifier  
 * pValue   R   new channel value copied from this location
 */
epicsShareFunc int epicsShareAPI ca_sg_array_put
(
    const CA_SYNC_GID gid,
    chtype type, 
    unsigned long count,
    chid chan,
    const void *pValue  
);

#define ca_sg_put(gid, type, chan, pValue) \
ca_sg_array_put (gid, type, 1u, chan, pValue)

/*
 * ca_sg_stat()
 *
 * print status of a sync group
 *
 * gid      R   sync group id
 */
epicsShareFunc int epicsShareAPI ca_sg_stat (CA_SYNC_GID gid);

epicsShareFunc void epicsShareAPI ca_dump_dbr (chtype type, unsigned count, const void * pbuffer);


/*
 * ca_v42_ok()
 *
 * Put call back is available if the CA server is on version is 4.2 
 *  or higher.
 *
 * chan     R   channel identifier
 * 
 * (returns true or false)
 */
epicsShareFunc int epicsShareAPI ca_v42_ok (chid chan);

/*
 * ca_version()
 *
 * returns the CA version string
 */
epicsShareFunc const char * epicsShareAPI ca_version (void);

/*
 * ca_replace_printf_handler ()
 *
 * for apps that want to change where ca formatted
 * text output goes
 *
 * use two ifdef's for trad C compatibility
 *
 * ca_printf_func   R   pointer to new function called when
 *                          CA prints an error message
 */
#ifndef CA_DONT_INCLUDE_STDARGH
typedef int caPrintfFunc (const char *pformat, va_list args);
epicsShareFunc int epicsShareAPI ca_replace_printf_handler (
    caPrintfFunc    *ca_printf_func
);
#endif /*CA_DONT_INCLUDE_STDARGH*/

/*
 * (for testing purposes only)
 */
epicsShareFunc unsigned epicsShareAPI ca_get_ioc_connection_count (void);
epicsShareFunc int epicsShareAPI ca_preemtive_callback_is_enabled (void);
epicsShareFunc void epicsShareAPI ca_self_test (void);
epicsShareFunc unsigned epicsShareAPI ca_beacon_anomaly_count (void);
epicsShareFunc unsigned epicsShareAPI ca_search_attempts (chid chan);
epicsShareFunc double epicsShareAPI ca_beacon_period (chid chan);
epicsShareFunc double epicsShareAPI ca_receive_watchdog_delay (chid chan);

/*
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
epicsShareFunc struct ca_client_context * epicsShareAPI ca_current_context ();
epicsShareFunc int epicsShareAPI ca_attach_context ( struct ca_client_context * context );


epicsShareFunc int epicsShareAPI ca_client_status ( unsigned level );
epicsShareFunc int epicsShareAPI ca_context_status ( struct ca_client_context *, unsigned level );

/*
 * deprecated
 */
#define ca_build_channel(NAME,XXXXX,CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, 1, CHIDPTR, YYYYY, 0, 0)
#define ca_array_build(NAME,XXXXX, ZZZZZZ, CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, ZZZZZZ, CHIDPTR, YYYYY, 0, 0)
epicsShareFunc int epicsShareAPI ca_build_and_connect
    ( const char *pChanName, chtype, unsigned long, 
    chid * pChanID, void *, caCh * pFunc, void * pArg );
#define ca_search(pChanName, pChanID)\
ca_search_and_connect (pChanName, pChanID, 0, 0)
epicsShareFunc int epicsShareAPI ca_search_and_connect
    ( const char * pChanName, chid * pChanID, 
    caCh *pFunc, void * pArg );
epicsShareFunc int epicsShareAPI ca_channel_status (epicsThreadId tid);
epicsShareFunc int epicsShareAPI ca_clear_event ( evid eventID );
#define ca_add_event(type,chan,pFunc,pArg,pEventID)\
ca_add_array_event(type,1u,chan,pFunc,pArg,0.0,0.0,0.0,pEventID)
#define ca_add_delta_event(TYPE,CHID,ENTRY,ARG,DELTA,EVID)\
    ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,DELTA,DELTA,0.0,EVID)
#define ca_add_general_event(TYPE,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)
#define ca_add_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_masked_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID, DBE_VALUE | DBE_ALARM)
epicsShareFunc int epicsShareAPI ca_add_masked_array_event
    ( chtype type, unsigned long count, chid chanId, caEventCallBackFunc * pFunc,
        void * pArg, ca_real p_delta, ca_real n_delta, ca_real timeout,
        evid * pEventID, long mask );

/*
 * defunct
 */
epicsShareFunc int epicsShareAPI ca_modify_user_name ( const char *pUserName );
epicsShareFunc int epicsShareAPI ca_modify_host_name ( const char *pHostName );

#ifdef __cplusplus
}
#endif

/*
 * no additions below this endif
 */
#endif /* ifndef INCLcadefh */

