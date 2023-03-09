/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
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
/**
 * \file cadef.h
 * \brief CA functions and interfaces definition header file.
 * \author Jeffrey O. Hill
 * \note done in two ifdef steps so that we will remain compatible with traditional C
 * 
 * It includes many other header files (operating system specific and otherwise), and therefore
 * the application must also specify "<EPICS base>/include/os/<arch>" in its header file search path.
 * 
 */
#ifndef INC_cadef_H
#define INC_cadef_H

#ifndef CA_DONT_INCLUDE_STDARGH
#   include <stdarg.h>
#endif

#include "epicsThread.h"

#include "libCaAPI.h"
#include "caerr.h"
#include "db_access.h"
#include "caeventmask.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct oldChannelNotify *chid;
typedef chid                    chanId; /**< \brief for when the structures field name is "chid" */
typedef long                    chtype;
typedef struct oldSubscription  *evid;
typedef double                  ca_real;

/** \brief arguments passed to user connection handlers */
struct  connection_handler_args {
    chanId  chid;  /**< \brief channel id */
    long    op;    /**< \brief one of CA_OP_CONN_UP or CA_OP_CONN_DOWN */
};

typedef void caCh (struct connection_handler_args args);

typedef struct ca_access_rights {
    unsigned    read_access:1;
    unsigned    write_access:1;
} caar;

/** \brief arguments passed to user access rights handlers */
struct  access_rights_handler_args {
    chanId  chid;   /**< \brief channel id */
    caar    ar;     /**< \brief new access rights state */
};

typedef void caArh (struct access_rights_handler_args args);

/** \brief The conversion routine to call for each type */
#define VALID_TYPE(TYPE)  (((unsigned short)TYPE)<=LAST_BUFFER_TYPE)

/**
 * \brief Arguments passed to event handlers and get/put call back handlers.
 *
 * The status field below is the CA ECA_XXX status of the requested
 * operation which is saved from when the operation was attempted in the
 * server and copied back to the clients call back routine.
 * If the status is not ECA_NORMAL then the dbr pointer will be NULL
 * and the requested operation can not be assumed to be successful.
 */
typedef struct event_handler_args {
    void            *usr;   /**< \brief user argument supplied with request */
    chanId          chid;   /**< \brief channel id */
    long            type;   /**< \brief the type of the item returned */
    long            count;  /**< \brief the element count of the item returned */
    const void      *dbr;   /**< \brief a pointer to the item returned */
    int             status; /**< \brief ECA_XXX status of the requested op from the server */
} evargs;
typedef void caEventCallBackFunc (struct event_handler_args);

/** \brief A built-in subscription update callback handler for debugging purposes that prints diagnostics to standard out. */
LIBCA_API void epicsStdCall ca_test_event
(
    struct event_handler_args
);

/** \brief arguments passed to user exception handlers */
struct exception_handler_args {
    void            *usr;   /**< \brief user argument supplied when installed */
    chanId          chid;   /**< \brief channel id (may be nill) */
    long            type;   /**< \brief type requested */
    long            count;  /**< \brief count requested */
    void            *addr;  /**< \brief user's address to write results of CA_OP_GET */
    long            stat;   /**< \brief channel access ECA_XXXX status code */
    long            op;     /**< \brief CA_OP_GET, CA_OP_PUT, ..., CA_OP_OTHER */
    const char      *ctx;   /**< \brief a character string containing context info */
    const char      *pFile; /**< \brief source file name (may be NULL) */
    unsigned        lineNo; /**< \brief source file line number (may be zero) */
};

/** \brief Identifier for a CA Synchronous Group */
typedef unsigned CA_SYNC_GID;

/** \brief External OP code for CA operation "GET" */
#define CA_OP_GET             0
/** \brief External OP code for CA operation "PUT" */
#define CA_OP_PUT             1
/** \brief External OP code for CA operation "CREATE CHANNEL" */
#define CA_OP_CREATE_CHANNEL  2
/** \brief External OP code for CA operation "ADD EVENT" */
#define CA_OP_ADD_EVENT       3
/** \brief External OP code for CA operation "CLEAR EVENT" */
#define CA_OP_CLEAR_EVENT     4
/** \brief External OP code for CA operation "OTHER" */
#define CA_OP_OTHER           5
/** \brief xternal OP code for CA operation used with connection_handler_args "CONN UP" */
#define CA_OP_CONN_UP       6
/** \brief xternal OP code for CA operation used with connection_handler_args "CONN DOWN" */
#define CA_OP_CONN_DOWN     7
/** \brief deprecated */
#define CA_OP_SEARCH        2

/** \brief provides efficient test and display of channel access errors */
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

#define TYPENOTCONN (-1) /**< \brief the channel's native type when disconnected */
LIBCA_API short epicsStdCall ca_field_type (chid chan);
LIBCA_API unsigned long epicsStdCall ca_element_count (chid chan);
LIBCA_API const char * epicsStdCall ca_name (chid chan);
LIBCA_API void epicsStdCall ca_set_puser (chid chan, void *puser);
LIBCA_API void * epicsStdCall ca_puser (chid chan);
LIBCA_API unsigned epicsStdCall ca_read_access (chid chan);
LIBCA_API unsigned epicsStdCall ca_write_access (chid chan);

/** \brief cs_ - `channel state'
 *
 *  cs_never_conn       valid chid, IOC not found
 *  cs_prev_conn        valid chid, IOC was found, but unavailable
 *  cs_conn             valid chid, IOC was found, still available
 *  cs_closed           channel deleted by user
 */
enum channel_state {cs_never_conn, cs_prev_conn, cs_conn, cs_closed};
LIBCA_API enum channel_state epicsStdCall ca_state (chid chan);

/** \brief Perform Task Initialization
 * 
 * Must be called once before calling any of the other routines
 */
LIBCA_API int epicsStdCall ca_task_initialize (void);
enum ca_preemptive_callback_select
{ ca_disable_preemptive_callback, ca_enable_preemptive_callback };
/** \brief CA context creation
 * 
 * This function, or ca_attach_context(), should be called once from each thread
 * prior to making any of the other Channel Access calls. If one of the above is not
 * called before making other CA calls then a non-preemptive context is created by default,
 * and future attempts to create a preemptive context for the current threads will fail.
 * 
 * Thie select argument specifies if preemptive invocation of callback functions is allowed.
 * If so your callback functions might be called when the thread that calls this routine is
 * not executing in the CA client library. 
 */
LIBCA_API int epicsStdCall 
        ca_context_create (enum ca_preemptive_callback_select select);
LIBCA_API void epicsStdCall ca_detach_context (); 

/** \brief Call at the end of task.
 * 
 * Remove CA facility from your task called automatically at task exit. */
LIBCA_API int epicsStdCall ca_task_exit (void);
/** \brief Shut down the calling thread's channel access client context and free any resources allocated.
 * 
 * Also detach the calling thread from any CA client context.
 */
LIBCA_API void epicsStdCall ca_context_destroy (void);

typedef unsigned capri;
#define CA_PRIORITY_MAX 99
#define CA_PRIORITY_MIN 0
#define CA_PRIORITY_DEFAULT CA_PRIORITY_MIN

#define CA_PRIORITY_DB_LINKS 80
#define CA_PRIORITY_ARCHIVE 20
#define CA_PRIORITY_OPI 0

/** \brief creates a CA channel.
 * 
 * The CA client library will attempt to establish and maintain a virtual circuit between
 * the caller's application and a named process variable in a CA server. Each call to ca
 * create_channel() allocates resources in the CA client library and potentially also a CA server.
 * The function ca_clear_channel() is used to release these resources. If successful, the routine writes
 * a channel identifier into the user's variable of type "chid". This identifier can be used
 * with any channel access call that operates on a channel.
 */
LIBCA_API int epicsStdCall ca_create_channel
(
     const char     *pChanName,          /**< R - channel name string */
     caCh           *pConnStateCallback, /**< R - address of connection state change callback function */
     void           *pUserPrivate,       /**< R - placed in the channel's user private field, can be fetched later by ca_puser(CHID), passed as void * arg to *pConnectCallback above */
     capri          priority,            /**< R - priority level in the server 0 - 100 */
     chid           *pChanID             /**< RW - channel id written here */
);

/** \brief The change connection event */
LIBCA_API int epicsStdCall ca_change_connection_event
(
     chid       chan,  /**< R - channel identifier */
     caCh *     pfunc  /**< R - address of connection call-back function */
);

/** \brief Install or replace the access rights state change callback handler for the specified channel.
 * 
 * The callback handler is called in the following situations.
 * - whenever CA connects the channel immediately before the channel's connection handler is called
 * - whenever CA disconnects the channel immediately after the channel's disconnect callback is called
 * - once immediately after installation if the channel is connected.
 * - whenever the access rights state of a connected channel changes
 * When a channel is created no access rights handler is installed.
 */
LIBCA_API int epicsStdCall ca_replace_access_rights_event (
     chid   chan,   /**< R - channel identifier */
     caArh  *pfunc  /**< R - address of access rights call-back function */
);

/** \brief Replace the currently installed CA context global exception handler callback.
 *
 * When an error occurs in the server asynchronous to the clients thread then information about this
 * type of error is passed from the server to the client in an exception message. When the client
 * receives this exception message an exception handler callback is called.The default exception
 * handler prints a diagnostic message on the client's standard out and terminates execution if
 * the error condition is severe.
 * \note Certain fields in "struct exception_handler_args" are not applicable in the context of some error messages.
 */
typedef void caExceptionHandler (struct exception_handler_args);
LIBCA_API int epicsStdCall ca_add_exception_event
(
     caExceptionHandler *pfunc, /**< R - address of exception call-back function */
     void               *pArg   /**< R - copy of this pointer passed to exception call-back function */
);

/** \brief Shutdown and reclaim resources associated with a channel created by ca_create_channel().
 * 
 * All remote operation requests such as the above are accumulated (buffered) and not forwarded to
 * the IOC until one of ca_flush_io(), ca_pend_io(), ca_pend_event(), or ca_sg_block() are called.
 * This allows several requests to be efficiently sent over the network in one message.
 */
LIBCA_API int epicsStdCall ca_clear_channel
(
     chid   chanId   /**< R - channel ID */
);

/************************************************************************/
/*  Write a value to a channel                  */
/************************************************************************/
/**
 * \brief ca_bput()
 *
 * \warning this copies the new value from a string (dbr_string_t, not as an integer)
 *
 * \param chan         R - channel identifier
 * \param pValue       R - new channel value string copied from this location
 */
#define ca_bput(chan, pValue) \
ca_array_put(DBR_STRING, 1u, chan, (const dbr_string_t *) (pValue))

/**
 * \brief ca_rput()
 *
 * \warning this copies the new value from a dbr_float_t
 *
 * \param chan         R - channel identifier
 * \param pValue       R - new channel value copied from this location
 */
#define ca_rput(chan,pValue) \
ca_array_put(DBR_FLOAT, 1u, chan, (const dbr_float_t *) pValue)

/**
 * \brief ca_put()
 *
 * \param type         R - data type from db_access.h
 * \param chan         R - channel identifier
 * \param pValue       R - new channel value copied from this location
 */
#define ca_put(type, chan, pValue) ca_array_put (type, 1u, chan, pValue)

/**
 * \brief ca_array_put()
 *
 * \param type         R - data type from db_access.h
 * \param count        R - array element count
 * \param chan         R - channel identifier
 * \param pValue       R - new channel value copied from this location
 */
LIBCA_API int epicsStdCall ca_array_put
(
     chtype         type,
     unsigned long  count,
     chid           chanId,
     const void *   pValue
);

/**
 * \brief ca_array_put_callback()
 *
 * This routine functions identically to the original ca put request
 * with the addition of a callback to the user supplied function
 * after recod processing completes in the IOC. The arguments
 * to the user supplied callback function are declared in
 * the structure event_handler_args and include the pointer
 * sized user argument supplied when ca_array_put_callback() is called.
 */
LIBCA_API int epicsStdCall ca_array_put_callback
(
     chtype                 type,    /**< \brief R - data type from db_access.h */
     unsigned long          count,   /**< \brief R - array element count */
     chid                   chanId,  /**< \brief R - channel identifier */
     const void *           pValue,  /**< \brief R - new channel value copied from this location */
     caEventCallBackFunc *  pFunc,   /**< \brief R - pointer to call-back function */
     void *                 pArg     /**< \brief R - copy of this pointer passed to pFunc */
);

#define ca_put_callback(type, chan, pValue, pFunc, pArg) \
    ca_array_put_callback(type, 1u, chan, pValue, pFunc, pArg)

/************************************************************************/
/*  Read a value from a channel                                         */
/************************************************************************/

/**
 * \brief ca_bget()
 *
 * \warning this copies the new value into a string (dbr_string_t)
 *      (and not into an integer)
 *
 * \param chan     R - channel identifier
 * \param pValue   W - channel value copied to this location
 */
#define ca_bget(chan, pValue) \
ca_array_get(DBR_STRING, 1u, chan, (dbr_string_t *)(pValue))

/**
 * \brief ca_rget()
 *
 * \warning this copies the new value into a 32 bit float (dbr_float_t)
 *
 * \param chan     R - channel identifier
 * \param pValue   W - channel value copied to this location
 */
#define ca_rget(chan, pValue) \
ca_array_get(DBR_FLOAT, 1u, chan, (dbr_float_t *)(pValue))

/**
 * \brief ca_rget()
 *
 * \param type     R - data type from db_access.h
 * \param chan     R - channel identifier
 * \param pValue   W - channel value copied to this location
 */
#define ca_get(type, chan, pValue) ca_array_get(type, 1u, chan, pValue)

/**
 * \brief ca_array_get()
 *
 * \param type     R - data type from db_access.h
 * \param count    R - array element count
 * \param chan     R - channel identifier
 * \param pValue   W - channel value copied to this location
 */
LIBCA_API int epicsStdCall ca_array_get
(
     chtype         type,
     unsigned long  count,
     chid           chanId,
     void *         pValue
);

/************************************************************************/
/*  Read a value from a channel and run a callback when the value       */
/*  returns                                                             */
/************************************************************************/
/**
 * \brief ca_bget_callback()
 *
 * \warning this returns the new value as a string (dbr_string_t, not as an integer)
 *
 * \param chan     R - channel identifier
 * \param pFunc    R - pointer to call-back function
 * \param pArg     R - copy of this pointer passed to pFunc
 */
#define ca_bget_callback(chan, pFunc, pArg)\
ca_array_get_callback (DBR_STRING, 1u, chan, pFunc, pArg)

/**
 * \brief ca_rget_callback()
 *
 * \warning this returns the new value as a float (dbr_float_t)
 *
 * \param chan     R - channel identifier
 * \param pFunc    R - pointer to call-back function
 * \param pArg     R - copy of this pointer passed to pFunc
 */
#define ca_rget_callback(chan, pFunc, pArg)\
ca_array_get_callback (DBR_FLOAT, 1u, chan, pFunc, pArg)

/**
 * \brief ca_get_callback()
 *
 * \param type     R - data type from db_access.h
 * \param chan     R - channel identifier
 * \param pFunc    R - pointer to call-back function
 * \param pArg     R - copy of this pointer passed to pFunc
 */
#define ca_get_callback(type, chan, pFunc, pArg)\
ca_array_get_callback (type, 1u, chan, pFunc, pArg)

/**
 * \brief ca_array_get_callback()
 *
 * \param type     R - data type from db_access.h
 * \param count    R - array element count
 * \param chan     R - channel identifier
 * \param pFunc    R - pointer to call-back function
 * \param pArg     R - copy of this pointer passed to pFunc
 */
LIBCA_API int epicsStdCall ca_array_get_callback
(
     chtype                 type,
     unsigned long          count,
     chid                   chanId,
     caEventCallBackFunc *  pFunc,
     void *                 pArg
);

/**
 * \brief Specify a function to be executed whenever significant changes occur to a channel.
 *
 * Register a state change subscription and specify a callback function to be invoked
 * whenever the process variable undergoes significant state changes. A significant
 * change can be a change in the process variable's value, alarm status, or alarm severity.
 * In the process control function block database the deadband field determines
 * the magnitude of a significant change for the process variable's value. Each call
 * to this function consumes resources in the client library and potentially a CA server
 * until one of ca_clear_channel() or ca_clear_subscription() is called.
 * \note 1)  Evid may be omitted by passing a NULL pointer
 * \note 2)  An array count of zero specifies the native db count
 */
LIBCA_API int epicsStdCall ca_create_subscription
(
     chtype                 type,     /**< \brief R - ata type from db_access.h */
     unsigned long          count,    /**< \brief R - array element count */
     chid                   chanId,   /**< \brief R - channel identifier */
     long                   mask,     /**< \brief R - event mask - one of (DBE_VALUE, DBE_ALARM, DBE_LOG) */
     caEventCallBackFunc *  pFunc,    /**< \brief R - pointer to call-back function */
     void *                 pArg,     /**< \brief R - copy of this pointer passed to pFunc */
     evid *                 pEventID  /**< \brief W - event id written at specified address */
);

/**
 * \brief Cancel a subscription.
 * 
 * All cancel-subscription requests such as the above are accumulated (buffered) and not forwarded
 * to the server until one of ca_flush_io(), ca_pend_io(), ca_pend_event(), or ca_sg_block()
 * are called. This allows several requests to be efficiently sent together in one message.
 * \note This operation blocks until any user callbacks for this channel have run to completion.
 */
LIBCA_API int epicsStdCall ca_clear_subscription
(
     evid eventID    /**< \brief R - event id */
);

LIBCA_API chid epicsStdCall ca_evid_to_chid ( evid id );


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
/*   .              ! "                                                 */
/*   .              ! "                                                 */
/*   pend_io        ! wait for channels to connect                      */
/*                                                                      */
/*   get()          ! several requests for remote info                  */
/*   get()          ! "                                                 */
/*   add_event()    ! "                                                 */
/*   get()          ! "                                                 */
/*   .                                                                  */
/*   .                                                                  */
/*   .                                                                  */
/*   flush_io()     ! send get requests                                 */
/*                  ! optional parallel processing                      */
/*   .              ! "                                                 */
/*   .              ! "                                                 */
/*   pend_io()      ! wait for replies from get requests                */
/*   .              ! access to requested data                          */
/*   .              ! "                                                 */
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

/**
 * \brief When invoked, the send buffer is flushed and CA background activity is processed for TIMEOUT seconds.
 *
 * timeout  R   wait for this delay in seconds
 */
LIBCA_API int epicsStdCall ca_pend_event (ca_real timeout);
#define ca_poll() ca_pend_event(1e-12)

/**
 * \brief Flushes the send buffer and then blocks
 * 
 * Blocks until outstanding ca_get() requests complete, and until channels created
 * specifying null connection handler function pointers connect for the first time.
 * Wait for timeout but return early if all get requests (or search requests with null
 * connection handler pointer have completed).
 *
 * \param timeout  R - wait for this delay in seconds
 */
LIBCA_API int epicsStdCall ca_pend_io (ca_real timeout);

/** \brief calls ca_pend_io() if early is true otherwise ca_pend_event() is called */
LIBCA_API int epicsStdCall ca_pend (ca_real timeout, int early);

/**
 * \brief Tests if all ca_get() requests are complete and channels created specifying a null connection callback function pointer are connected.
 *
 * It will report the status of outstanding ca_get() requests issued, and channels
 * created specifying a null connection callback function pointer, after the last
 * call to ca_pend_io() or CA context initialization whichever is later.
 * 
 * returns TRUE when get requests (or search requests with null
 * connection handler pointer) are outstanding
 */
LIBCA_API int epicsStdCall ca_test_io (void);

/************************************************************************/
/*  Send out all outstanding messages in the send queue                 */
/************************************************************************/
/**
 * \brief Flush outstanding IO requests to the server
 * 
 * This routine might be useful to users who need to flush requests prior to
 * performing client side labor in parallel with labor performed in the server.
 * Outstanding requests are also sent whenever the buffer which holds them becomes full.
 */
LIBCA_API int epicsStdCall ca_flush_io (void);

/**
 * \brief Provide the error message character string associated with the supplied channel access error code and the supplied error context to diagnostics.
 * 
 * If the error code indicates an unsuccessful operation a stack dump is printed, if this capability
 * is available on the local operating system, and execution is terminated.
 * SEVCHK is a macro envelope around ca_signal() which only calls ca_signal() if the supplied error
 * code indicates an unsuccessful operation. SEVCHK is the recommended error handler for simple applications
 */
LIBCA_API void epicsStdCall ca_signal
(
     long errorCode,      /**< \brief R - status returned from channel access function */
     const char *pCtxStr  /**< \brief R - context string included with error print out */
);

/**
 * \brief ca_signal_with_file_and_lineno()
 */
LIBCA_API void epicsStdCall ca_signal_with_file_and_lineno
(
     long errorCode,        /**< \brief R - status returned from channel access function */
     const char *pCtxStr,   /**< \brief R - context string included with error print out */
     const char *pFileStr,  /**< \brief R - file name string included with error print out */
     int lineNo             /**< \brief R - line number included with error print out */
);

/**
 * \brief ca_signal_formated()
 */
LIBCA_API void epicsStdCall ca_signal_formated
(
     long ca_status,       /**< \brief R - status returned from channel access function */
     const char *pfilenm,  /**< \brief R - file name string included with error print out */
     int lineno,           /**< \brief R - line number included with error print out */
     const char *pFormat,  /**< \brief R - printf dtyle format string (and optional arguments */
...);

/**
 * \brief Returns host name.
 *
 * channel  R   channel identifier
 *
 * \warning this function is _not_ thread safe !!!!
 */
LIBCA_API const char * epicsStdCall ca_host_name (chid channel);
/**
 * \brief Returns host name.
 * 
 * Thread safe version of ca_host_name()
 */
LIBCA_API unsigned epicsStdCall ca_get_host_name ( chid pChan, 
    char *pBuf, unsigned bufLength );

/**
 * \brief CA_ADD_FD_REGISTRATION
 *
 *  call their function with their argument whenever
 *  a new fd is added or removed
 *  (for use with a manager of the select system call under UNIX)
 *
 *  if (opened) then fd was created
 *  if (!opened) then fd was deleted
 */
typedef void CAFDHANDLER (void *parg, int fd, int opened);

/**
 * \brief Adds file description manager.
 * 
 * For use with the services provided by a file descriptor manager (IO multiplexor) such as "fdmgr.c".
 * A file descriptor manager is often needed when two file descriptor IO intensive libraries such as
 * the EPICS channel access client library and the X window system client library must coexist in the
 * same UNIX process. This function allows an application code to be notified whenever the CA client
 * library places a new file descriptor into service and whenever the CA client library removes a
 * file descriptor from service. Specifying USERFUNC=NULL disables file descriptor registration (this
 * is the default).
 */
LIBCA_API int epicsStdCall ca_add_fd_registration
(
     CAFDHANDLER    *pHandler,  /**< \brief R - ointer to function which is to be called when an fd is created or deleted */
     void           *pArg       /**< \brief R - argument passed to above function */
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

/**
 * \brief Create a synchronous group and return an identifier for it.
 *
 * A synchronous group can be used to guarantee that a set of channel access requests have completed.
 * Once a synchronous group has been created then channel access get and put requests may be issued within
 * it using ca_sg_array_get() and ca_sg_array_put() respectively. The routines ca_sg_block() and ca_sg_test()
 * can be used to block for and test for completion respectively. The routine ca_sg_reset() is used to
 * discard knowledge of old requests which have timed out and in all likelihood will never be satisfied.
 * Any number of asynchronous groups can have application requested operations outstanding within them at any given time.
 * 
 * \param pgid     W - pointer to sync group id that will be written
 */
LIBCA_API int epicsStdCall ca_sg_create (CA_SYNC_GID *  pgid);

/**
 * \brief Deletes a synchronous group.
 *
 * \param gid      R - sync group id
 */
LIBCA_API int epicsStdCall ca_sg_delete (const CA_SYNC_GID gid);

/**
 * \brief Block for IO performed within a sync group to complete
 * 
 * Flushes the send buffer and then waits until outstanding requests complete or the specified time out expires.
 * At this time outstanding requests include calls to ca_sg_array_get() and calls to ca_sg_array_put().
 * If ECA_TIMEOUT is returned then failure must be assumed for all outstanding queries. Operations
 * can be reissued followed by another ca_sg_block(). This routine will only block on outstanding
 * queries issued after the last call to ca_sg_block(), ca_sg_reset(), or ca_sg_create() whichever
 * occurs later in time. If no queries are outstanding then ca_sg_block() will return immediately
 * without processing any pending channel access activities.
 *
 * \param gid      R - sync group id
 * \param timeout  R - wait for this duration prior to timing out and returning ECA_TIMEOUT
 */
LIBCA_API int epicsStdCall ca_sg_block (const CA_SYNC_GID gid, ca_real timeout);

/**
 * \brief Test to see if all requests made within a synchronous group have completed.
 *
 * \param gid      R - sync group id
 *
 * \note returns one of ECA_BADSYNCGRP, ECA_IOINPROGRESS, ECA_IODONE
 */
LIBCA_API int epicsStdCall ca_sg_test (const CA_SYNC_GID gid);

/**
 * \brief Resets the number of outstanding requests within the specified synchronous group
 * 
 * Reset the number of outstanding requests within the specified synchronous groupto zero so that
 * ca_sg_test() will return ECA_IODONE and ca_sg_block() will not block unless additional
 * subsequent requests are made.
 *
 * \param gid      R - sync group id
 */
LIBCA_API int epicsStdCall ca_sg_reset(const CA_SYNC_GID gid);

/**
 * \brief initiate a get within a sync group
 * 
 * Read a value from a channel and increment the outstanding request count of a synchronous group.
 * The ca_sg_get() and ca_sg_array_get() functionality is implemented using ca_array_get_callback().
 * The values written into your program's variables by ca_sg_get() or ca_sg_array_get() should not be
 * referenced by your program until ECA_NORMAL has been received from ca_sg_block(), or
 * until ca_sg_test() returns ECA_IODONE.
 * All remote operation requests such as the above are accumulated (buffered) and not
 * forwarded to the server until one of ca_flush_io(), ca_pend_io(), ca_pend_event(),
 * or ca_sg_block() are called. This allows several requests to be efficiently sent in one message.
 * 
 * \note (essentially a ca_array_get() with a sync group specified)
 *
 * \param gid      R - sync group id
 * \param type     R - data type from db_access.h
 * \param count    R - array element count
 * \param chan     R - channel identifier
 * \param pValue   W - channel value copied to this location
 */
LIBCA_API int epicsStdCall ca_sg_array_get
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
LIBCA_API int epicsStdCall ca_sg_array_put
(
    const CA_SYNC_GID gid,
    chtype type,
    unsigned long count,
    chid chan,
    const void *pValue
);

#define ca_sg_put(gid, type, chan, pValue) \
ca_sg_array_put (gid, type, 1u, chan, pValue)

/**
 * \brief print status of a sync group
 *
 * Write a value, or array of values, to a channel and increment the outstanding request
 * count of a synchronous group. The ca_sg_put() and ca_sg_array_put() functionality is
 * implemented using ca_array_put_callback().
 * All remote operation requests such as the above are accumulated (buffered) and not
 * forwarded to the server until one of ca_flush_io(), ca_pend_io(), ca_pend_event(),
 * or ca_sg_block() are called. This allows several requests to be efficiently sent
 * in one message.
 * If a connection is lost and then resumed outstanding puts are not reissued.
 * 
 * \param gid      R - sync group id
 */
LIBCA_API int epicsStdCall ca_sg_stat (CA_SYNC_GID gid);

LIBCA_API void epicsStdCall ca_dump_dbr (chtype type, unsigned count, const void * pbuffer);


/**
 * \brief Server is on version is 4.2 or higher
 * 
 * Put call back is available if the CA server is on version is 4.2 or higher.
 *
 * \param chan     R - channel identifier
 *
 * \note (returns true or false)
 */
LIBCA_API int epicsStdCall ca_v42_ok (chid chan);

/**
 * \brief returns the CA version string
 */
LIBCA_API const char * epicsStdCall ca_version (void);

/**
 * \brief Replace the default handler for formatted diagnostic message output.
 * 
 * The default handler uses fprintf to send messages to 'stderr'.
 * For apps that want to change where ca formatted text output goes
 *
 * \note use two ifdef's for trad C compatibility
 *
 * \param ca_printf_func   R - pointer to new function called when CA prints an error message
 */
#ifndef CA_DONT_INCLUDE_STDARGH
typedef int caPrintfFunc (const char *pformat, va_list args);
LIBCA_API int epicsStdCall ca_replace_printf_handler (
    caPrintfFunc    *ca_printf_func
);
#endif /*CA_DONT_INCLUDE_STDARGH*/

/** \brief (for testing purposes only) */
LIBCA_API unsigned epicsStdCall ca_get_ioc_connection_count (void);
/** \brief (for testing purposes only) */
LIBCA_API int epicsStdCall ca_preemtive_callback_is_enabled (void);
/** \brief (for testing purposes only) */
LIBCA_API void epicsStdCall ca_self_test (void);
/** \brief (for testing purposes only) */
LIBCA_API unsigned epicsStdCall ca_beacon_anomaly_count (void);
/** \brief (for testing purposes only) */
LIBCA_API unsigned epicsStdCall ca_search_attempts (chid chan);
/** \brief (for testing purposes only) */
LIBCA_API double epicsStdCall ca_beacon_period (chid chan);
/** \brief (for testing purposes only) */
LIBCA_API double epicsStdCall ca_receive_watchdog_delay (chid chan);

/**
 * \brief Returns a pointer to the current thread's CA context.
 * 
 * If none then null is returned.
 * Used when an auxiliary thread needs to join a CA client context started by another thread.
 */
LIBCA_API struct ca_client_context * epicsStdCall ca_current_context ();
/**
 * \brief The calling thread becomes a member of the specified CA context.
 * 
 * If ca_disable_preemptive_callback is specified when ca_context_create() is called 
 * (or if ca_task_initialize() is called) then additional threads are not allowed to
 * join the CA context because allowing other threads to join implies that CA callbacks
 * will be called preemptively from more than one thread.
 */
LIBCA_API int epicsStdCall ca_attach_context ( struct ca_client_context * context );
/**
 * \brief Prints information about the client context
 * 
 * Including, at higher interest levels, status for each channel. Lacking a CA context pointer,
 * ca_client_status() prints information about the calling threads CA context.
 */
LIBCA_API int epicsStdCall ca_client_status ( unsigned level );
/**
 * \brief Prints information about the client context
 * 
 * Including, at higher interest levels, status for each channel. Lacking a CA context pointer,
 * ca_client_status() prints information about the calling threads CA context.
 */
LIBCA_API int epicsStdCall ca_context_status ( struct ca_client_context *, unsigned level );

/*
 * deprecated
 */
#define ca_build_channel(NAME,XXXXX,CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, 1, CHIDPTR, YYYYY, 0, 0)
#define ca_array_build(NAME,XXXXX, ZZZZZZ, CHIDPTR,YYYYY)\
ca_build_and_connect(NAME, XXXXX, ZZZZZZ, CHIDPTR, YYYYY, 0, 0)
LIBCA_API int epicsStdCall ca_build_and_connect
    ( const char *pChanName, chtype, unsigned long,
    chid * pChanID, void *, caCh * pFunc, void * pArg );
#define ca_search(pChanName, pChanID)\
ca_search_and_connect (pChanName, pChanID, 0, 0)
LIBCA_API int epicsStdCall ca_search_and_connect
    ( const char * pChanName, chid * pChanID,
    caCh *pFunc, void * pArg );
LIBCA_API int epicsStdCall ca_channel_status (epicsThreadId tid);
LIBCA_API int epicsStdCall ca_clear_event ( evid eventID );
#define ca_add_event(type,chan,pFunc,pArg,pEventID)\
ca_add_array_event(type,1u,chan,pFunc,pArg,0.0,0.0,0.0,pEventID)
#define ca_add_delta_event(TYPE,CHID,ENTRY,ARG,DELTA,EVID)\
    ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,DELTA,DELTA,0.0,EVID)
#define ca_add_general_event(TYPE,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_array_event(TYPE,1,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)
#define ca_add_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID)\
ca_add_masked_array_event(TYPE,COUNT,CHID,ENTRY,ARG,P_DELTA,N_DELTA,TO,EVID, DBE_VALUE | DBE_ALARM)
LIBCA_API int epicsStdCall ca_add_masked_array_event
    ( chtype type, unsigned long count, chid chanId, caEventCallBackFunc * pFunc,
        void * pArg, ca_real p_delta, ca_real n_delta, ca_real timeout,
        evid * pEventID, long mask );

/*
 * defunct
 */
LIBCA_API int epicsStdCall ca_modify_user_name ( const char *pUserName );
LIBCA_API int epicsStdCall ca_modify_host_name ( const char *pHostName );

#ifdef __cplusplus
}
#endif

/*
 * no additions below this endif
 */
#endif /* ifndef INC_cadef_H */

