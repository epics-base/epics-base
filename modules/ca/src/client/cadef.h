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

/** \file cadef.h
 *
 * \brief Channel Access library.
 */

#ifndef INC_cadef_H
#define INC_cadef_H

/*
 * done in two ifdef steps so that we will remain compatible with
 * traditional C
 */
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

/** \brief Channel identifier. */
typedef struct oldChannelNotify *chid;
/** \brief Channel identifier. */
typedef chid                    chanId; /* for when the structures field name is "chid" */
typedef long                    chtype;
typedef struct oldSubscription  *evid;
typedef double                  ca_real;

/** \brief Arguments passed to user connection handlers. */
struct  connection_handler_args {
    chanId  chid;  /**< channel id */
    long    op;    /**< one of ::CA_OP_CONN_UP or ::CA_OP_CONN_DOWN */
};

typedef void caCh (struct connection_handler_args args);

/** \brief Access rights.
 */
typedef struct ca_access_rights {
    unsigned    read_access:1; /**< Read access. */
    unsigned    write_access:1; /**< Write access. */
} caar;

/** \brief Arguments passed to user access rights handlers
 */
struct  access_rights_handler_args {
    chanId  chid;   /**< Channel id. */
    caar    ar;     /**< New access rights state. */
};

typedef void caArh (struct access_rights_handler_args args);

/*  The conversion routine to call for each type    */
#define VALID_TYPE(TYPE)  (((unsigned short)TYPE)<=LAST_BUFFER_TYPE)

/** \brief Arguments passed to event handlers and get/put call back handlers.
 *
 * The status field below is the CA ECA_XXX status of the requested
 * operation which is saved from when the operation was attempted in the
 * server and copied back to the clients call back routine.
 * If the status is not ECA_NORMAL then the dbr pointer will be NULL
 * and the requested operation can not be assumed to be successful.
 */
typedef struct event_handler_args {
    void            *usr;   /**< User argument supplied with request. */
    chanId          chid;   /**< Channel id. */
    long            type;   /**< The type of the item returned. */
    long            count;  /**< The element count of the item returned. */
    const void      *dbr;   /**< A pointer to the item returned. */
    int             status; /**< `ECA_XXXX` status of the requested op from the server. */
} evargs;
typedef void caEventCallBackFunc (struct event_handler_args);

/** \brief A built-in subscription update callback handler
 * for debugging purposes that prints diagnostics to standard out.
 *
 * # Examples
 *
 * ```c
 * void ca_test_event ();
 * status = ca_create_subscription ( type, chid, ca_test_event, NULL, NULL );
 * SEVCHK ( status, .... );
 * ```
 *
 * \sa ca_create_subscription()
 */
LIBCA_API void epicsStdCall ca_test_event
(
    struct event_handler_args
);

/** \brief Arguments passed to user exception handlers.
 */
struct exception_handler_args {
    void            *usr;   /**< User argument supplied when installed */
    chanId          chid;   /**< Channel id (may be null) */
    long            type;   /**< Type requested */
    long            count;  /**< Count requested */
    void            *addr;  /**< User's address to write results of ::CA_OP_GET */
    long            stat;   /**< Channel access ECA_XXXX status code */
    long            op;     /**< ::CA_OP_GET, ::CA_OP_PUT, ..., ::CA_OP_OTHER */
    const char      *ctx;   /**< A character string containing context info */
    const char      *pFile; /**< Source file name (may be NULL) */
    unsigned        lineNo; /**< Source file line number (may be zero) */
};

typedef unsigned CA_SYNC_GID;

/*
 *  External OP codes for CA operations
 */

/**
 * \name Channel Access operations
 * @{
 */

/** \brief GET Channel Access operation. */
#define CA_OP_GET             0
/** \brief PUT Channel Access operation. */
#define CA_OP_PUT             1
/** \brief CREATE_CHANNEL Channel Access operation. */
#define CA_OP_CREATE_CHANNEL  2
/** \brief ADD_EVENT Channel Access operation. */
#define CA_OP_ADD_EVENT       3
/** \brief CLEAR_EVENT Channel Access operation. */
#define CA_OP_CLEAR_EVENT     4
/** \brief Other Channel Access operation. */
#define CA_OP_OTHER           5

/** \brief Connection up Channel Access operation.
 *
 * Used with connection_handler_args
 */
#define CA_OP_CONN_UP       6

/** \brief Connection down Channel Access operation.
 *
 * Used with connection_handler_args
 */
#define CA_OP_CONN_DOWN     7

/** \brief SEARCH Channel Access operation.
 *
 * \deprecated
 */
#define CA_OP_SEARCH        2

/** @} */

/** \brief Provides efficient test and display of channel access errors.
 *
 * SEVCHK() is a macro envelope around ca_signal() which only calls
 * ca_signal() if the supplied error code indicates an unsuccessful
 * operation. SEVCHK() is the recommended error handler for simple applications
 * which do not wish to write code testing the status returned from each
 * channel access call.
 *
 * # Examples
 *
 * ```c
 * status = ca_context_create (...);
 * SEVCHK ( status, "Unable to create a CA client context" );
 * ```
 *
 * \sa ca_signal().
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

/** \brief The channel's native type when disconnected. */
#define TYPENOTCONN (-1)

/** \brief Return the native type in the server of the process variable.
 *
 * \param[in] chan Channel identifier.
 * \returns The data type code will be a member of the set of `DBF_XXXX` in `db_access.h`.
 * The constant TYPENOTCONN is returned if the channel is disconnected.
 */
LIBCA_API short epicsStdCall ca_field_type (chid chan);

/** \brief Return the maximum array element count in the server for the specified IO channel.
 *
 * \param[in] chan Channel identifier.
 * \returns The maximum array element count in the server.
 * An element count of zero is returned if the channel is disconnected.
 */
LIBCA_API unsigned long epicsStdCall ca_element_count (chid chan);

/** \brief Return the name provided when the supplied channel id was created.
 *
 * \param[in] chan Channel identifier.
 * \returns The channel name.
 * The string returned is valid as long as the channel specified exists.
 */
LIBCA_API const char * epicsStdCall ca_name (chid chan);

/** \brief Set a user private void pointer variable retained
 * with each channel for use at the users discretion.
 *
 * \param[in] chan Channel identifier.
 * \param[in] puser user private void pointer
 */
LIBCA_API void epicsStdCall ca_set_puser (chid chan, void *puser);

/** \brief Return a user private void pointer variable
 * retained with each channel for use at the users discretion.
 *
 * \param[in] chan Channel identifier.
 * \returns User private pointer
 */
LIBCA_API void * epicsStdCall ca_puser (chid chan);

/** \brief Returns whether the client currently has read access
 * to the specified channel.
 *
 * \param[in] chan Channel identifier.
 * \returns Boolean true if the client currently has read access to the specified channel
 * and boolean false otherwise.
 */
LIBCA_API unsigned epicsStdCall ca_read_access (chid chan);

/** \brief Returns whether the client currently has write access
 * to the specified channel.
 *
 * \param[in] chan Channel identifier.
 * \returns Boolean true if the client currently has write access to the specified channel
 * and boolean false otherwise
 */
LIBCA_API unsigned epicsStdCall ca_write_access (chid chan);

/** \brief Channel state
 */
enum channel_state {
    /// valid chid, IOC not found or unavailable
	cs_never_conn,
    /// valid chid, IOC was found, but unavailable (previously connected to server)
	cs_prev_conn,
    /// valid chid, IOC was found, still available
	cs_conn,
    /// channel deleted by user
	cs_closed
};

/** \brief Returns an enumerated type indicating the current state of the specified IO channel.
 *
 * \param[in] chan Channel identifier.
 * \returns the connection state.
 */
LIBCA_API enum channel_state epicsStdCall ca_state (chid chan);

/************************************************************************/
/*  Perform Library Initialization                                      */
/*                                                                      */
/*  Must be called once before calling any of the other routines        */
/************************************************************************/

/** \brief Initialize a Channel Access task.
 *
 * \deprecated Use ca_context_create() instead.
 */
LIBCA_API int epicsStdCall ca_task_initialize (void);

enum ca_preemptive_callback_select
{ ca_disable_preemptive_callback, ca_enable_preemptive_callback };

/** \brief Create a Channel Access context.
 *
 * This function, or ca_attach_context(), should be called once from each
 * thread prior to making any of the other Channel Access calls. If one of the
 * above is not called before making other CA calls then a non-preemptive
 * context is created by default, and future attempts to create a preemptive
 * context for the current threads will fail.
 *
 * If ::ca_disable_preemptive_callback is specified then additional threads are
 * *not* allowed to join the CA context using ca_attach_context() because
 * allowing other threads to join implies that CA callbacks will be called
 * preemptively from more than one thread.
 *
 * \param[in] select
 * \parblock
 * This argument specifies if preemptive invocation of callback
 * functions is allowed. If so your callback functions might be called
 * when the thread that calls this routine is not executing in the CA
 * client library. There are two implications to consider.
 *
 * First, if preemptive callback mode is enabled the developer must
 * provide mutual exclusion protection for his data structures. In this
 * mode it's possible for two threads to touch the application's data
 * structures at once: this might be the initializing thread (the
 * thread that called ca_context_create()) and also a private thread
 * created by the CA client library for the purpose of receiving
 * network messages and calling callbacks. It might be prudent for
 * developers who are unfamiliar with mutual exclusion locking in a
 * multi-threaded environment to specify
 * ::ca_disable_preemptive_callback.
 *
 * Second, if preemptive callback mode is enabled the application is no
 * longer burdened with the necessity of periodically polling the CA
 * client library in order that it might take care of its background
 * activities. If ca_enable_preemptive_callback is specified then CA
 * client background activities, such as connection management, will
 * proceed even if the thread that calls this routine is not executing
 * in the CA client library. Furthermore, in preemptive callback mode
 * callbacks might be called with less latency because the library is
 * not required to wait until the initializing thread (the thread that
 * called ca_context_create()) is executing within the CA client library.
 * \endparblock
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_ALLOCMEM - Failed, unable to allocate space in pool
 * \returns ::ECA_NOTTHREADED - Current thread is already a member of a non-preemptive callback CA context (possibly created implicitly)
 *
 * \sa ca_context_destroy()
 */
LIBCA_API int epicsStdCall 
        ca_context_create (enum ca_preemptive_callback_select select);

/** \brief Detach from any CA context currently attached to the calling thread.
 *
 * This does *not* cleanup or shutdown any currently attached CA context (for
 * that use ca_context_destroy()).
 *
 * \sa ca_current_context(), ca_attach_context(), ca_context_create(), ca_context_destroy()
 */
LIBCA_API void epicsStdCall ca_detach_context ();

/************************************************************************/
/*  Remove CA facility from your task                                   */
/*                                                                      */
/*  Normally called automatically at task exit                          */
/************************************************************************/

/** \brief Exit a Channel Access task.
 *
 * \deprecated Use ca_context_destroy() instead.
 */
LIBCA_API int epicsStdCall ca_task_exit (void);

/** \brief Destroy a Channel Access context.
 *
 * Shut down the calling thread's channel access client context and free
 * any resources allocated. Detach the calling thread from any CA client
 * context.
 *
 * Any user-created threads that have attached themselves to the CA context
 * must stop using it prior to its being destroyed. A program running in an
 * IOC context must delete all of its channels prior to calling
 * ca_context_destroy() to avoid a crash.
 *
 * A CA client application that calls epicsExit() *must* install an EPICS
 * exit handler that calls ca_context_destroy() only *after* first
 * calling ca_context_create(). This will guarantee that the EPICS exit
 * handlers get called in the correct order.
 *
 * On many OS that execute programs in a process based environment the
 * resources used by the client library such as sockets and allocated
 * memory are automatically released by the system when the process exits
 * and ca_context_destroy() hasn't been called, but on light weight
 * systems such as vxWorks or RTEMS no cleanup occurs unless the
 * application calls ca_context_destroy().
 *
 * \note This operation blocks until any user callbacks for any channel
 * created in the current context have run to completion. If callbacks take
 * a lock (mutex) then it is the user's responsibility to ensure that this
 * lock is not held when ca_clear_context() is called, otherwise a
 * deadlock may ensue. (See also ca_clear_channel() and
 * ca_clear_subscription().)
 *
 * \sa ca_context_create()
 */
LIBCA_API void epicsStdCall ca_context_destroy (void);

typedef unsigned capri;
#define CA_PRIORITY_MAX 99
#define CA_PRIORITY_MIN 0
#define CA_PRIORITY_DEFAULT CA_PRIORITY_MIN

#define CA_PRIORITY_DB_LINKS 80
#define CA_PRIORITY_ARCHIVE 20
#define CA_PRIORITY_OPI 0

/** \brief Create a Channel Access channel.
 *
 * This function creates a CA channel. The CA client library will attempt
 * to establish and maintain a virtual circuit between the caller's
 * application and a named process variable in a CA server. Each call to
 * ca_create_channel() allocates resources in the CA client library and
 * potentially also a CA server. The function ca_clear_channel() is used
 * to release these resources. If successful, the routine writes a channel
 * identifier into the user's variable of type "chid". This identifier
 * can be used with any channel access call that operates on a channel.
 *
 * The circuit may be initially connected or disconnected depending on the
 * state of the network and the location of the channel. A channel will
 * only enter a connected state after the server's address is determined,
 * and only if channel access successfully establishes a virtual circuit
 * through the network to the server. Channel access routines that send a
 * request to a server will return ::ECA_DISCONNCHID if the channel is
 * currently disconnected.
 *
 * There are two ways to obtain asynchronous notification when a channel
 * enters a connected state.
 *
 * -   The first and simplest method requires that you call ca_pend_io(),
 *     and wait for successful completion, prior to using a channel that
 *     was created specifying a null connection callback function pointer.
 * -   The second method requires that you register a connection handler by
 *     supplying a valid connection callback function pointer. This
 *     connection handler is called whenever the connection state of the
 *     channel changes. If you have installed a connection handler then
 *     ca_pend_io() will *not* block waiting for the channel to enter a
 *     connected state.
 *
 * The function ca_state(CHID) can be used to test the connection state of
 * a channel. Valid connections may be isolated from invalid ones with this
 * function if ca_pend_io() times out.
 *
 * Due to the inherently transient nature of network connections the order
 * of connection callbacks relative to the order that ca_create_channel()
 * calls are made by the application can't be guaranteed, and application
 * programs may need to be prepared for a connected channel to enter a
 * disconnected state at any time.
 *
 * # Example
 *
 * See `caExample.c` in the example application created by `makeBaseApp.pl`.
 *
 * \param[in] pChanName A nil terminated process variable name string.
 * \parblock
 *  EPICS process control * function block database variable names are of the
 *  form `<record name>.<field name>`. If the field name and the period
 *  separator are omitted then the `VAL` field is implicit. For example
 *  `RFHV01` and `RFHV01.VAL` reference the same EPICS process control function
 *  block database variable.
 * \endparblock
 *
 * \param[in] pConnStateCallback address of connection state change callback function
 * \parblock
 * Optional pointer to the user's callback function to be run when the
 * connection state changes. Casual users of channel access may decide
 * to set this field to null or 0 if they do not need to have a
 * callback function run in response to each connection state change
 * event.
 *
 * A connection_handler_args structure is passed *by value* to the user's
 * connection callback function. The `op` field will be set by the CA
 * client library to ::CA_OP_CONN_UP when the channel connects, and to
 * ::CA_OP_CONN_DOWN when the channel disconnects. See ca_puser()
 * if the `pUserPrivate` argument is required in your callback handler.
 * \endparblock
 *
 * \param[in] pUserPrivate The value of this void pointer argument is retained in storage
 * associated with the specified channel.
 * Casual users of channel access may wish to set this field to null or 0.
 * Can be fetched later by ca_puser(CHID).
 * passed as `void *` arg to `*pConnStateCallback` above
 *
 * \param[in] priority The priority level for dispatch within the server or
 * network with 0 specifying the lowest dispatch priority and 99 the highest.
 * This parameter currently does not impact dispatch priorities within the
 * client, but this might change in the future. The abstract priority range
 * specified is mapped into an operating system specific range of priorities
 * within the server. This parameter is ignored if the server is running on a
 * network or operating system that does not have native support for
 * prioritized delivery or execution respectively. Specifying many different
 * priorities within the same program can increase resource consumption in the
 * client and the server because an independent virtual circuit, and associated
 * data structures, is created for each priority that is used on a particular
 * server.
 *
 * \param[inout] pChanID The user supplied channel identifier storage is overwritten
 * with a channel identifier if this routine is successful.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_STRTOBIG - Unusually large string
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 */
LIBCA_API int epicsStdCall ca_create_channel
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
LIBCA_API int epicsStdCall ca_change_connection_event
(
     chid       chan,
     caCh *     pfunc
);

/** \brief Install or replace the access rights state change callback handler
 * for the specified channel.
 *
 * The callback handler is called in the following situations.
 *
 * -   whenever CA connects the channel immediately before the channel's
 *     connection handler is called
 * -   whenever CA disconnects the channel immediately after the channel's
 *     disconnect callback is called
 * -   once immediately after installation if the channel is connected.
 * -   whenever the access rights state of a connected channel changes
 *
 * When a channel is created no access rights handler is installed.
 *
 * \param[in] chan Channel identifier.
 * \param[in] pfunc Pointer to a user supplied callback function.
 * A null pointer uninstalls the current handler.
 * An access_rights_handler_args structure is passed *by value* to the supplied callback handler.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 */
LIBCA_API int epicsStdCall ca_replace_access_rights_event (
     chid   chan,
     caArh  *pfunc
);

typedef void caExceptionHandler (struct exception_handler_args);

/** \brief Replace the currently installed CA context global exception handler callback.
 *
 * When an error occurs in the server asynchronous to the clients thread then
 * information about this type of error is passed from the server to the client
 * in an exception message. When the client receives this exception message an
 * exception handler callback is called. The default exception handler prints a
 * diagnostic message on the client's standard out and terminates execution if
 * the error condition is severe.
 *
 * Note that certain fields in struct exception_handler_args are not
 * applicable in the context of some error messages. For instance, a failed get
 * will supply the address in the client task where the returned value was
 * requested to be written. For other failed operations the value of the addr
 * field should not be used.
 *
 * # Example
 *
 * ```c
 * void ca_exception_handler (
 *     struct exception_handler_args args)
 * {
 *     char buf[512];
 *     char *pName;
 *
 *     if ( args.chid ) {
 *         pName = ca_name ( args.chid );
 *     }
 *     else {
 *         pName = "?";
 *     }
 *     sprintf ( buf,
 *             "%s - with request chan=%s op=%d data type=%s count=%d",
 *             args.ctx, pName, args.op, dbr_type_to_text ( args.type ), args.count );
 *     ca_signal ( args.stat, buf );
 * }
 * ca_add_exception_event ( ca_exception_handler , 0 );
 * ```
 *
 * \param[in] pfunc Pointer to a user callback function to be executed when exceptions occur.
 * Passing a null value causes the default exception handler to be reinstalled.
 * An exception_handler_args structure is passed by value to the user's callback function.
 * Currently, the `op` field can be one of ::CA_OP_GET, ::CA_OP_PUT,
 * ::CA_OP_CREATE_CHANNEL, ::CA_OP_ADD_EVENT, ::CA_OP_CLEAR_EVENT,
 * or ::CA_OP_OTHER.
 *
 * \param[in] pArg Pointer sized variable retained and passed back to user function above.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 */
LIBCA_API int epicsStdCall ca_add_exception_event
(
     caExceptionHandler *pfunc,
     void               *pArg
);

/** \brief Shutdown and reclaim resources associated with a channel created by ca_create_channel().
 *
 * All remote operation requests such as the above are accumulated
 * (buffered) and not forwarded to the IOC until one of ca_flush_io(),
 * ca_pend_io(), ca_pend_event(), or ca_sg_block() are called. This
 * allows several requests to be efficiently sent over the network in one
 * message.
 *
 * Clearing a channel does not cause its disconnect handler to be called,
 * but clearing a channel does shutdown and reclaim any channel state
 * change event subscriptions (monitors) registered with the channel.
 *
 * Note: This operation blocks until any user callbacks for this channel
 * have run to completion. If callbacks take a lock (mutex) then it is the
 * user's responsibility to ensure that this lock is not held when
 * ca_clear_channel() is called, otherwise a deadlock may ensue.
 *
 * \sa ca_clear_subscription()
 *
 * \param[in] chanId Identifies the channel to delete.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 */
LIBCA_API int epicsStdCall ca_clear_channel
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

/** \brief Write a scalar value to a process variable.
 *
 * See ca_array_put().
 *
 * \param[in] type The external type of the supplied value to be written.
 * Conversion will occur if this does not match the native type. Specify one
 * from the set of `DBR_XXXX` in `db_access.h`
 *
 * \param[in] chan Channel identifier.
 *
 * \param[in] pValue Pointer to a value or array of values provided by the application
 * to be written to the channel.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_NOWTACCESS - Write access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
#define ca_put(type, chan, pValue) ca_array_put (type, 1u, chan, pValue)

/** \brief Write a scalar or array value to a process variable.
 *
 * When ca_put() or ca_array_put() are invoked the client will receive
 * no response unless the request can not be fulfilled in the server. If
 * unsuccessful an exception handler is run on the client side.
 *
 * When ca_put_callback() or ca_array_put_callback() are invoked the
 * user supplied asynchronous callback is called only after the initiated
 * write operation, and all actions resulting from the initiating write
 * operation, complete. The arguments to the user supplied callback function
 * are declared in the structure event_handler_args and include the pointer
 * sized user argument supplied when ca_array_put_callback() is called.
 *
 * If unsuccessful the callback function is invoked indicating failure
 * status.
 *
 * If the channel disconnects before a put callback request can be
 * completed, then the client's callback function is called with failure
 * status, but this does not guarantee that the server did not receive and
 * process the request before the disconnect. If a connection is lost and
 * then resumed outstanding ca put requests are not automatically reissued
 * following reconnect.
 *
 * All of these functions return ::ECA_DISCONN if the channel is currently
 * disconnected.
 *
 * All put requests are accumulated (buffered) and not forwarded to the IOC
 * until one of ca_flush_io(), ca_pend_io(), ca_pend_event(), or
 * ca_sg_block() are called. This allows several requests to be
 * efficiently combined into one message.
 *
 * # Description (IOC Database Specific)
 *
 * A CA put request causes the record to process if the record's SCAN
 * field is set to passive, and the field being written has its process
 * passive attribute set to true. If such a record is already processing
 * when a put request is initiated the specified field is written
 * immediately, and the record is scheduled to process again as soon as it
 * finishes processing. Earlier instances of multiple put requests
 * initiated while the record is being processing may be discarded, but the
 * last put request initiated is always written and processed.
 *
 * A CA put *callback* request causes the record to process if the
 * record's SCAN field is set to passive, and the field being written has
 * its process passive attribute set to true. For such a record, the
 * user's put callback function is not called until after the record, and
 * any records that the record links to, finish processing. If such a
 * record is already processing when a put *callback* request is initiated
 * the put *callback* request is postponed until the record, and any
 * records it links to, finish processing.
 *
 * If the record's SCAN field is not set to passive, or the field being
 * written has its process passive attribute set to false then the CA put
 * or CA put *callback* request cause the specified field to be immediately
 * written, but they do not cause the record to be processed.
 *
 * \sa ca_flush_io(), ca_pend_event(), ca_sg_array_put()
 *
 * \param[in] type The external type of the supplied value to be written.
 * Conversion will occur if this does not match the native type. Specify one
 * from the set of `DBR_XXXX` in `db_access.h`
 *
 * \param[in] count Element count to be written to the specified channel.
 * This must match the array pointed to by pValue.
 *
 * \param[in] chanId Channel identifier.
 *
 * \param[in] pValue Pointer to a value or array of values provided by the application
 * to be written to the channel.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_NOWTACCESS - Write access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
LIBCA_API int epicsStdCall ca_array_put
(
     chtype         type,
     unsigned long  count,
     chid           chanId,
     const void *   pValue
);

/** \brief Asynchronously write a scalar or array value to a process variable.
 *
 * See ca_array_put().
 *
 * \param[in] type The external type of the supplied value to be written.
 * Conversion will occur if this does not match the native type. Specify one
 * from the set of `DBR_XXXX` in `db_access.h`
 *
 * \param[in] count Element count to be written to the specified channel.
 * This must match the array pointed to by pValue.
 *
 * \param[in] chanId Channel identifier.
 *
 * \param[in] pValue Pointer to a value or array of values provided by the application
 * to be written to the channel.
 *
 * \param[in] pFunc Pointer to a user supplied callback function to be run
 * when the requested operation completes.
 *
 * \param[in] pArg Pointer sized variable retained
 * and then passed back to user supplied function above.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_NOWTACCESS - Write access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
LIBCA_API int epicsStdCall ca_array_put_callback
(
     chtype                 type,
     unsigned long          count,
     chid                   chanId,
     const void *           pValue,
     caEventCallBackFunc *  pFunc,
     void *                 pArg
);

/** \brief Asynchronously write a scalar value to a process variable.
 *
 * See ca_array_put().
 *
 * \param[in] type The external type of the supplied value to be written.
 * Conversion will occur if this does not match the native type. Specify one
 * from the set of `DBR_XXXX` in `db_access.h`
 *
 * \param[in] chan Channel identifier.
 *
 * \param[in] pValue Pointer to a value or array of values provided by the application
 * to be written to the channel.
 *
 * \param[in] pFunc Pointer to a user supplied callback function to be run
 * when the requested operation completes.
 *
 * \param[in] pArg Pointer sized variable retained
 * and then passed back to user supplied function above.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_NOWTACCESS - Write access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
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

/** \brief Read a scalar value from a process variable.
 *
 * See ca_array_get().
 *
 * \param[in] type The external type of the user variable to return the value into.
 * Conversion will occur if this does not match the native type.
 * Specify one from the set of DBR_XXXX in db_access.h
 *
 * \param[in] chan Channel identifier.
 *
 * \param[out] pValue Pointer to an application supplied buffer
 * where the current value of the channel is to be written.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_GETFAIL - A local database get failed
 * \returns ::ECA_NORDACCESS - Read access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
#define ca_get(type, chan, pValue) ca_array_get(type, 1u, chan, pValue)

/** \brief Read a scalar or array value from a process variable.
 *
 * When ca_get() or ca_array_get() are invoked the returned channel
 * value can't be assumed to be stable in the application supplied buffer
 * until after ECA_NORMAL is returned from ca_pend_io(). If a connection
 * is lost outstanding ca get requests are not automatically reissued
 * following reconnect.
 *
 * When ca_get_callback() or ca_array_get_callback() are invoked a
 * value is read from the channel and then the user's callback is invoked
 * with a pointer to the retrieved value. Note that ca_pend_io() will not
 * block for the delivery of values requested by ca_get_callback(). If
 * the channel disconnects before a ca_get_callback() request can be
 * completed, then the client's callback function is called with failure
 * status.
 *
 * All of these functions return ::ECA_DISCONN if the channel is currently
 * disconnected.
 *
 * All get requests are accumulated (buffered) and not forwarded to the IOC
 * until one of ca_flush_io(), ca_pend_io(), ca_pend_event(), or
 * ca_sg_block() are called. This allows several requests to be
 * efficiently sent over the network in one message.
 *
 * ## Description (IOC Database Specific)
 *
 * A CA get or CA get callback request causes the record's field to be
 * read immediately independent of whether the record is currently being
 * processed or not. There is currently no mechanism in place to cause a
 * record to be processed when a CA get request is initiated.
 *
 * # Example
 *
 * See `caExample.c` in the example application created by `makeBaseApp.pl`.
 *
 * \sa ca_pend_io(), ca_pend_event(), ca_sg_array_get()
 *
 * \param[in] type The external type of the user variable to return the value into.
 * Conversion will occur if this does not match the native type.
 * Specify one from the set of DBR_XXXX in db_access.h
 *
 * \param[in] count Element count to be read from the specified channel.
 * Must match the array pointed to by pValue.
 * For ca_array_get_callback() a count of zero means use the current element count from the server.
 *
 * \param[in] chanId Channel identifier.
 *
 * \param[out] pValue Pointer to an application supplied buffer
 * where the current value of the channel is to be written.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_GETFAIL - A local database get failed
 * \returns ::ECA_NORDACCESS - Read access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
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

/** \brief Asynchronously read a scalar value from a process variable.
 *
 * See ca_array_get().
 *
 * \param[in] type The external type of the user variable to return the value into.
 * Conversion will occur if this does not match the native type.
 * Specify one from the set of DBR_XXXX in db_access.h
 *
 * \param[in] chan Channel identifier.
 *
 * \param[in] pFunc Pointer to a user supplied callback function to be run
 * when the requested operation completes.
 *
 * \param[in] pArg Pointer sized variable retained
 * and then passed back to user supplied callback function above.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_GETFAIL - A local database get failed
 * \returns ::ECA_NORDACCESS - Read access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
#define ca_get_callback(type, chan, pFunc, pArg)\
ca_array_get_callback (type, 1u, chan, pFunc, pArg)

/** \brief Asynchronously read a scalar or array value from a process variable.
 *
 * See ca_array_get().
 *
 * \param[in] type The external type of the user variable to return the value into.
 * Conversion will occur if this does not match the native type.
 * Specify one from the set of DBR_XXXX in db_access.h
 *
 * \param[in] count Element count to be read from the specified channel.
 * Must match the array pointed to by pValue.
 * For ca_array_get_callback() a count of zero means use the current element count from the server.
 *
 * \param[in] chanId Channel identifier.
 *
 * \param[in] pFunc Pointer to a user supplied callback function to be run
 * when the requested operation completes.
 *
 * \param[in] pArg Pointer sized variable retained
 * and then passed back to user supplied callback function above.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_GETFAIL - A local database get failed
 * \returns ::ECA_NORDACCESS - Read access denied
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_DISCONN - Channel is disconnected
 */
LIBCA_API int epicsStdCall ca_array_get_callback
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
/*  1)  Evid may be omitted by passing a NULL pointer                    */
/*                                                                      */
/*  2)  An array count of zero specifies the native db count            */
/*                                                                      */
/************************************************************************/

/** \brief Register a state change subscription and specify a callback function
 * to be invoked whenever the process variable undergoes significant state
 * changes.
 *
 * A significant change can be a change in the process variable's
 * value, alarm status, or alarm severity. In the process control function
 * block database the deadband field determines the magnitude of a
 * significant change for the process variable's value. Each call to this
 * function consumes resources in the client library and potentially a CA
 * server until one of ca_clear_channel() or ca_clear_subscription() is
 * called.
 *
 * Subscriptions may be installed or canceled against both connected and
 * disconnected channels. The specified pFunc is called once immediately
 * after the subscription is installed with the process variable's current
 * state if the process variable is connected. Otherwise, the specified
 * pFunc is called immediately after establishing a connection (or
 * reconnection) with the process variable. The specified pFunc is
 * called immediately with the process variable's current state from
 * within ca_create_subscription() if the client and the process variable
 * share the same address space.
 *
 * If a subscription is installed on a channel in a disconnected state then
 * the requested count will be set to the native maximum element count of
 * the channel if the requested count is larger.
 *
 * All subscription requests such as the above are accumulated (buffered)
 * and not forwarded to the IOC until one of ca_flush_io(),
 * ca_pend_io(), ca_pend_event(), or ca_sg_block() are called. This
 * allows several requests to be efficiently sent over the network in one
 * message.
 *
 * If at any time after subscribing, read access to the specified process
 * variable is lost, then the callback will be invoked immediately
 * indicating that read access was lost via the status argument. When read
 * access is restored normal event processing will resume starting always
 * with at least one update indicating the current state of the channel.
 *
 * A better name for this function might have been ca_subscribe().
 *
 * # Example
 *
 * See `caMonitor.c` in the example application created by `makeBaseApp.pl`.
 *
 * \sa ca_pend_event(), ca_flush_io()
 *
 * \param[in] type The type of value presented to the callback function.
 * Conversion will occur if it does not match native type.
 * Specify one from the set of `DBR_XXXX` in `db_access.h`
 *
 * \param[in] count The element count to be read from the specified channel.
 * A count of zero means use the current element count from the server,
 * effectively resulting in a variable size array subscription.
 *
 * \param[in] chanId Channel identifier.
 *
 * \param[in] mask
 * \parblock
 * A mask with bits set for each of the event trigger types requested.
 * The event trigger mask must be a *bitwise or* of one or more of the
 * following constants.
 *
 * -   ::DBE_VALUE - Trigger events when the channel value exceeds the
 *     monitor dead band
 * -   ::DBE_ARCHIVE (or ::DBE_LOG) - Trigger events when the channel value
 *     exceeds the archival dead band
 * -   ::DBE_ALARM - Trigger events when the channel alarm state changes
 * -   ::DBE_PROPERTY - Trigger events when a channel property changes.
 *
 * For functions above that do not include a trigger specification,
 * events will be triggered when there are significant changes in the
 * channel's value or when there are changes in the channel's alarm
 * state. This is the same as `DBE_VALUE | DBE_ALARM`.
 * \endparblock
 *
 * \param[in] pFunc Pointer to a user supplied callback function to be invoked
 * with each subscription update.
 *
 * \param[in] pArg Pointer sized variable retained and passed back
 * to user callback function
 *
 * \param[out] pEventID This is a pointer to user supplied event id which is overwritten
 * if successful.
 * This event id can later be used to clear a specific event.
 * This option may be omitted by passing a null pointer.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_ALLOCMEM - Unable to allocate memory
 * \returns ::ECA_ADDFAIL - A local database event add failed
 */
LIBCA_API int epicsStdCall ca_create_subscription
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

/** \brief Cancel a subscription.
 *
 * All cancel-subscription requests such as the above are accumulated
 * (buffered) and not forwarded to the server until one of ca_flush_io(),
 * ca_pend_io(), ca_pend_event(), or ca_sg_block() are called. This
 * allows several requests to be efficiently sent together in one message.
 *
 * \note This operation blocks until any user callbacks for this channel
 * have run to completion. If callbacks take a lock (mutex) then it is the
 * user's responsibility to ensure that this lock is not held when
 * ca_clear_subscription() is called, otherwise a deadlock may ensue.
 * (See also ca_clear_channel().)
 *
 * \sa ca_create_subscription()
 *
 * \param[in] eventID Event id returned by ca_create_subscription().
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADCHID - Corrupted CHID
 */
LIBCA_API int epicsStdCall ca_clear_subscription
(
     evid eventID
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

/** \brief Flush the send buffer
 * and process CA background activity for `timeout` seconds.
 *
 * The ca_pend_event() function will *not* return before the specified
 * timeout expires and all unfinished channel access labor has been
 * processed, and unlike ca_pend_io() returning from the
 * function does *not* indicate anything about the status of pending IO
 * requests.
 *
 * Both ca_pend_event() and ca_poll() return ::ECA_TIMEOUT when
 * successful. This behavior probably isn\'t intuitive, but it is preserved
 * to insure backwards compatibility.
 *
 * See also "Thread Safety and Preemptive Callback to User Code"
 * from the Channel Access reference manual.
 *
 * \sa ca_poll()
 *
 * \param[in] timeout The duration to block in this routine in seconds.
 * A timeout of zero seconds blocks forever.
 *
 * \returns ::ECA_TIMEOUT - The operation timed out
 * \returns ::ECA_EVDISALLOW - Function inappropriate for use within a callback handler
 */
LIBCA_API int epicsStdCall ca_pend_event (ca_real timeout);

/** \brief Flush the send buffer
 * and process any outstanding CA background activity.
 *
 * See ca_pend_event().
 *
 * \returns ::ECA_TIMEOUT - The operation timed out
 * \returns ::ECA_EVDISALLOW - Function inappropriate for use within a callback * handler
 */
#define ca_poll() ca_pend_event(1e-12)

/** \brief Flushes the send buffer and then blocks
 * until outstanding ca_get() requests complete,
 * and until channels created specifying null connection handler function pointers
 * connect for the first time.
 *
 * -   If ::ECA_NORMAL is returned then it can be safely assumed that all
 *     outstanding ca_get() requests have completed
 *     successfully and channels created specifying null connection handler
 *     function pointers have connected for the first time.
 * -   If ::ECA_TIMEOUT is returned then it must be assumed for all previous
 *     ca_get() requests and properly qualified first time
 *     channel connects have failed.
 *
 * If ::ECA_TIMEOUT is returned then get requests may be reissued followed by
 * a subsequent call to ca_pend_io(). Specifically, the function will
 * block only for outstanding ca_get() requests issued, and
 * also any channels created specifying a null connection handler function
 * pointer, after the last call to ca_pend_io() or ca client context
 * creation whichever is later. Note that
 * ca_create_channel() requests generally should
 * not be reissued for the same process variable unless
 * ca_clear_channel() is called first.
 *
 * If no ca_get() or connection state change events are
 * outstanding then ca_pend_io() will flush the send buffer and return
 * immediately *without processing any outstanding channel access
 * background activities*.
 *
 * The delay specified to ca_pend_io() should take into account worst
 * case network delays such as Ethernet collision exponential back off
 * until retransmission delays which can be quite long on overloaded
 * networks.
 *
 * Unlike ca_pend_event(), this routine will not
 * process CA's background activities if none of the selected IO requests
 * are pending.
 *
 * \sa ca_get(), ca_create_channel(), ca_test_io()
 *
 * \param[in] timeout Specifies the time out interval.
 * A `timeout` interval of zero specifies forever.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_TIMEOUT - Selected IO requests didn't complete before specified timeout
 * \returns ::ECA_EVDISALLOW - Function inappropriate for use within an event handler
 */
LIBCA_API int epicsStdCall ca_pend_io (ca_real timeout);

/* calls ca_pend_io() if early is true otherwise ca_pend_event() is called */
LIBCA_API int epicsStdCall ca_pend (ca_real timeout, int early);

/** \brief Test to see if all ca_get() requests are complete
 * and channels created specifying a null connection callback function pointer are connected.
 *
 * It will report the status of outstanding ca_get() requests issued, and
 * channels created specifying a null connection callback function pointer,
 * after the last call to ca_pend_io() or CA context initialization whichever
 * is later.
 *
 * \sa ca_pend_io()
 *
 * \returns ::ECA_IODONE - All IO operations completed
 * \returns ::ECA_IOINPROGRESS - IO operations still in progress
 */
LIBCA_API int epicsStdCall ca_test_io (void);

/************************************************************************/
/*  Send out all outstanding messages in the send queue                 */
/************************************************************************/

/** \brief Flush outstanding IO requests to the server.
 *
 * This routine might be useful to users who need to flush requests prior to
 * performing client side labor in parallel with labor performed in the server.
 *
 * Outstanding requests are also sent whenever the buffer which holds them
 * becomes full.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 */
LIBCA_API int epicsStdCall ca_flush_io (void);

/** \brief Provide the error message character string associated with the supplied channel access error code
 * and the supplied error context to diagnostics.
 *
 * If the error code indicates an unsuccessful operation a stack dump is
 * printed, if this capability is available on the local operating system, and
 * execution is terminated.
 *
 * \note SEVCHK() is the recommended error handler for simple applications
 * which do not wish to write code testing the status returned from each
 * channel access call.
 *
 * If the application only wishes to print the message associated with an error
 * code or test the severity of an error there are also functions provided for
 * this purpose.
 *
 * \param[in] errorCode The status  returned from a channel access function.
 * \param[in] pCtxStr A null terminated character string to supply
 * as error context to diagnostics.
 */
LIBCA_API void epicsStdCall ca_signal
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
LIBCA_API void epicsStdCall ca_signal_with_file_and_lineno
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
LIBCA_API void epicsStdCall ca_signal_formated (long ca_status, const char *pfilenm, 
                                       int lineno, const char *pFormat, ...);

/** \brief Return a character string which contains the name of the host
 * to which a channel is currently connected.
 *
 * \warning This function is _not_ thread safe.
 * See ca_get_host_name() for a thread safe version.
 *
 * \param[in] channel Channel identifier.
 * \returns The process variable server's host name.
 * If the channel is disconnected the string `<disconnected>` is returned.
 */
LIBCA_API const char * epicsStdCall ca_host_name (chid channel);

/** \brief Return a character string which contains the name of the host
 * to which a channel is currently connected.
 *
 * \param[in] pChan Channel identifier.
 * \param[out] pBuf Where to write the process variable server's host name.
 * If the channel is disconnected the string `<disconnected>` is returned.
 * \param[out] bufLength The size of the pBuf buffer.
 * \returns The effective size of the written host name.
 */
LIBCA_API unsigned epicsStdCall ca_get_host_name ( chid pChan, 
    char *pBuf, unsigned bufLength );

/** \brief Call their function with their argument whenever
 *  a new fd is added or removed.
 *
 *  For use with a manager of the select system call under UNIX.
 *
 *  if (opened) then fd was created
 *  if (!opened) then fd was deleted
 */
typedef void CAFDHANDLER (void *parg, int fd, int opened);

/** \brief For use with the services provided by a file descriptor manager
 * (IO multiplexor) such as "fdmgr.c".
 *
 * A file descriptor manager is often needed when two file descriptor IO
 * intensive libraries such as the EPICS channel access client library and the
 * X window system client library must coexist in the same UNIX process. This
 * function allows an application code to be notified whenever the CA client
 * library places a new file descriptor into service and whenever the CA client
 * library removes a file descriptor from service. Specifying pHandler=NULL
 * disables file descriptor registration (this is the default).
 *
 * # Example
 *
 * ```c
 * int s;
 * static struct myStruct aStruct;
 *
 * void fdReg ( struct myStruct *pStruct, int fd, int opened )
 * {
 *     if ( opened ) printf ( "fd %d was opened\n", fd );
 *     else printf ( "fd %d was closed\n", fd );
 * }
 * s = ca_add_fd_registration ( fdReg, & aStruct );
 * SEVCHK ( s, NULL );
 * ```
 *
 * # Comments
 *
 * When using this function it is advisable to call it only once prior to
 * calling any other CA function, or once just after creating the CA context
 * (if you create the context explicitly). Use of this interface can improve
 * latency slightly in applications that use non preemptive callback mode at
 * the expense of some additional runtime overhead when compared to the
 * alternative which is just polling ca_pend_event() periodically. It would
 * probably not be appropriate to use this function with preemptive callback
 * mode. Starting with R3.14 this function is implemented in a special backward
 * compatibility mode. if ca_add_fd_registration() is called, a single pseudo
 * UDP fd is created which CA pokes whenever something significant happens. Xt
 * and others can watch this fd so that backwards compatibility is preserved,
 * and so that they will not need to use preemptive callback mode but they will
 * nevertheless get the lowest latency response to the arrival of CA messages.
 *
 * \param[in] pHandler Pointer to a user supplied C function returning null with the above arguments.
 * \param[in] pArg User supplied pointer sized variable passed to the above function.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 */
LIBCA_API int epicsStdCall ca_add_fd_registration
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

/** \brief Create a synchronous group and return an identifier for it.
 *
 * A synchronous group can be used to guarantee that a set of channel access
 * requests have completed. Once a synchronous group has been created then
 * channel access get and put requests may be issued within it using
 * ca_sg_array_get() and ca_sg_array_put() respectively. The routines
 * ca_sg_block() and ca_sg_test() can be used to block for and test for
 * completion respectively. The routine ca_sg_reset() is used to discard
 * knowledge of old requests which have timed out and in all likelihood will
 * never be satisfied.
 *
 * Any number of asynchronous groups can have application requested operations
 * outstanding within them at any given time.
 *
 * # Examples
 *
 * ```c
 * CA_SYNC_GID gid;
 * status = ca_sg_create ( &gid );
 * SEVCHK ( status, Sync group create failed );
 * ```
 *
 * \sa ca_sg_delete(), ca_sg_block(), ca_sg_test(), ca_sg_reset(),
 * ca_sg_array_put(), ca_sg_array_get()
 *
 * \param[out] pgid Pointer to a user supplied CA_SYNC_GID
 * that will be written.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_ALLOCMEM - Failed, unable to allocate memory
 */
LIBCA_API int epicsStdCall ca_sg_create (CA_SYNC_GID *  pgid);

/** \brief Deletes a synchronous group.
 *
 * # Examples
 *
 * ```c
 * CA_SYNC_GID gid;
 * status = ca_sg_delete ( gid );
 * SEVCHK ( status, Sync group delete failed );
 * ```
 *
 * \sa ca_sg_create()
 *
 * \param[in] gid Identifier of the synchronous group to be deleted.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 */
LIBCA_API int epicsStdCall ca_sg_delete (const CA_SYNC_GID gid);

/** \brief Flushes the send buffer
 * and then waits until outstanding requests complete
 * or the specified time out expires.
 *
 * At this time outstanding requests include calls to ca_sg_array_get() and
 * calls to ca_sg_array_put(). If ECA_TIMEOUT is returned then failure must be
 * assumed for all outstanding queries. Operations can be reissued followed by
 * another ca_sg_block(). This routine will only block on outstanding queries
 * issued after the last call to ca_sg_block(), ca_sg_reset(), or
 * ca_sg_create() whichever occurs later in time. If no queries are outstanding
 * then ca_sg_block() will return immediately without processing any pending
 * channel access activities.
 *
 * Values written into your program's variables by a channel access synchronous
 * group request should not be referenced by your program until ECA_NORMAL has
 * been received from ca_sg_block(). This routine will process pending channel
 * access background activity while it is waiting.
 *
 * # Examples
 *
 * ```c
 * CA_SYNC_GID gid;
 * status = ca_sg_block(gid, 0.0);
 * SEVCHK(status, Sync group block failed);
 * ```
 *
 * \sa ca_sg_test(), ca_sg_reset()
 *
 * \param[in] gid Identifier of the synchronous group.
 * \param[in] timeout The duration to block in this routine in seconds.
 * A timeout of zero seconds blocks forever.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_TIMEOUT - The operation timed out
 * \returns ::ECA_EVDISALLOW - Function inappropriate for use within an event handler
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 */
LIBCA_API int epicsStdCall ca_sg_block (const CA_SYNC_GID gid, ca_real timeout);

/** \brief Test to see if all requests made within a synchronous group have completed.
 *
 * # Examples
 *
 * ```c
 * CA_SYNC_GID gid;
 * status = ca_sg_test ( gid );
 * ```
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \returns ::ECA_IODONE - IO operations completed
 * \returns ::ECA_IOINPROGRESS - Some IO operations still in progress
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 */
LIBCA_API int epicsStdCall ca_sg_test (const CA_SYNC_GID gid);

/** \brief Reset the number of outstanding requests within the specified synchronous group to zero.
 *
 * So that ca_sg_test() will return ::ECA_IODONE and ca_sg_block() will not
 * block unless additional subsequent requests are made.
 *
 * # Examples
 *
 * ```c
 * CA_SYNC_GID gid;
 * status = ca_sg_reset(gid);
 * ```
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 */
LIBCA_API int epicsStdCall ca_sg_reset(const CA_SYNC_GID gid);


/** \brief Read an array of values from a channel
 * and increment the outstanding request count of a synchronous group.
 *
 * The ca_sg_get() and ca_sg_array_get() functionality is implemented using
 * ca_array_get_callback().
 *
 * The values written into your program's variables by ca_sg_get() or
 * ca_sg_array_get() should not be referenced by your program until
 * ::ECA_NORMAL has been received from ca_sg_block(), or until ca_sg_test()
 * returns ::ECA_IODONE.
 *
 * All remote operation requests such as the above are accumulated (buffered)
 * and not forwarded to the server until one of ca_flush_io(), ca_pend_io(),
 * ca_pend_event(), or ca_sg_block() are called. This allows several requests
 * to be efficiently sent in one message.
 *
 * If a connection is lost and then resumed outstanding gets are not reissued.
 *
 * \sa ca_sg_get(), ca_pend_io(), ca_flush_io(), ca_get_callback()
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \param[in] type External type of returned value.
 * Conversion will occur if this does not match native type.
 * Specify one from the set of `DBR_XXXX` in `db_access.h`.
 *
 * \param[in] count Element count to be read from the specified channel.
 * It must match the array pointed to by `pValue`.
 *
 * \param[in] chan Channel identifier.
 *
 * \param[out] pValue Pointer to application supplied buffer
 * that is to contain the value or array of values to be returned.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_GETFAIL - A local database get failed
 */
LIBCA_API int epicsStdCall ca_sg_array_get
(
    const CA_SYNC_GID gid,
    chtype type,
    unsigned long count,
    chid chan,
    void *pValue
);

/** \brief Read a value from a channel
 * and increment the outstanding request count of a synchronous group.
 *
 * See ca_sg_array_get()
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \param[in] type External type of returned value.
 * Conversion will occur if this does not match native type.
 * Specify one from the set of `DBR_XXXX` in `db_access.h`.
 *
 * \param[in] chan Channel identifier.
 *
 * \param[out] pValue Pointer to application supplied buffer
 * that is to contain the value or array of values to be returned.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_GETFAIL - A local database get failed
 */
#define ca_sg_get(gid, type, chan, pValue) \
ca_sg_array_get (gid, type, 1u, chan, pValue)

/** \brief Write an array of values to a channel
 * and increment the outstanding request count of a synchronous group.
 *
 * The ca_sg_put() and ca_sg_array_put() functionality is implemented using
 * ca_array_put_callback().
 *
 * All remote operation requests such as the above are accumulated (buffered)
 * and not forwarded to the server until one of ca_flush_io(), ca_pend_io(),
 * ca_pend_event(), or ca_sg_block() are called. This allows several requests
 * to be efficiently sent in one message.
 *
 * If a connection is lost and then resumed outstanding puts are not reissued.
 *
 * \sa ca_sg_get(), ca_flush_io()
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \param[in] type The type of supplied value.
 * Conversion will occur if it does not match the native type.
 * Specify one from the set of `DBR_XXXX` in `db_access.h`.
 *
 * \param[in] count Element count to be written to the specified channel.
 * Must match the array pointed to by `pValue`.
 *
 * \param[in] chan Channel identifier.
 *
 * \param[out] pValue A pointer to an application supplied buffer
 * containing the value or array of values returned.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_PUTFAIL - A local database put failed
 */
LIBCA_API int epicsStdCall ca_sg_array_put
(
    const CA_SYNC_GID gid,
    chtype type,
    unsigned long count,
    chid chan,
    const void *pValue
);

/** \brief Write a value to a channel
 * and increment the outstanding request count of a synchronous group.
 *
 * See ca_sg_array_get().
 *
 * \param[in] gid Identifier of the synchronous group.
 *
 * \param[in] type The type of supplied value.
 * Conversion will occur if it does not match the native type.
 * Specify one from the set of `DBR_XXXX` in `db_access.h`.
 *
 * \param[in] chan Channel identifier.
 *
 * \param[out] pValue A pointer to an application supplied buffer
 * containing the value or array of values returned.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_BADSYNCGRP - Invalid synchronous group
 * \returns ::ECA_BADCHID - Corrupted CHID
 * \returns ::ECA_BADTYPE - Invalid DBR_XXXX type
 * \returns ::ECA_BADCOUNT - Requested count larger than native element count
 * \returns ::ECA_STRTOBIG - Unusually large string supplied
 * \returns ::ECA_PUTFAIL - A local database put failed
 */
#define ca_sg_put(gid, type, chan, pValue) \
ca_sg_array_put (gid, type, 1u, chan, pValue)

/*
 * ca_sg_stat()
 *
 * print status of a sync group
 *
 * gid      R   sync group id
 */
LIBCA_API int epicsStdCall ca_sg_stat (CA_SYNC_GID gid);

/** \brief Dumps the specified dbr data type to standard out.
 *
 * \param[in] type The data type
 * (from the `DBR_XXXX` set described in `db_access.h`).
 * \param[in] count The array element count
 * \param[in] pbuffer A pointer to data of the specified count and number.
 */
LIBCA_API void epicsStdCall ca_dump_dbr (chtype type, unsigned count, const void * pbuffer);

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
LIBCA_API int epicsStdCall ca_v42_ok (chid chan);

/*
 * ca_version()
 *
 * returns the CA version string
 */
LIBCA_API const char * epicsStdCall ca_version (void);

/*
 * ca_replace_printf_handler ()
 *
 * for apps that want to change where ca formatted
 * text output goes
 *
 * use two ifdef's for trad C compatibility
 */
#ifndef CA_DONT_INCLUDE_STDARGH
typedef int caPrintfFunc (const char *pformat, va_list args);

/** \brief Replace the default handler for formatted diagnostic message output.
 *
 * The default handler uses fprintf to send messages to 'stderr'.
 *
 * \param[in] ca_printf_func A pointer to a user supplied callback handler
 * to be invoked when CA prints diagnostic messages.
 * Installing a null pointer will cause the default callback handler to be reinstalled.
 *
 * # Examples
 *
 * ```c
 * int my_printf ( char *pformat, va_list args ) {
 *         int status;
 *         status = vfprintf( stderr, pformat, args);
 *         return status;
 * }
 * status = ca_replace_printf_handler ( my_printf );
 * SEVCHK ( status, "failed to install my printf handler" );
 * ```
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 */
LIBCA_API int epicsStdCall ca_replace_printf_handler (
    caPrintfFunc    *ca_printf_func
);
#endif /*CA_DONT_INCLUDE_STDARGH*/

/*
 * (for testing purposes only)
 */
LIBCA_API unsigned epicsStdCall ca_get_ioc_connection_count (void);
LIBCA_API int epicsStdCall ca_preemtive_callback_is_enabled (void);
LIBCA_API void epicsStdCall ca_self_test (void);
LIBCA_API unsigned epicsStdCall ca_beacon_anomaly_count (void);
LIBCA_API unsigned epicsStdCall ca_search_attempts (chid chan);
LIBCA_API double epicsStdCall ca_beacon_period (chid chan);
LIBCA_API double epicsStdCall ca_receive_watchdog_delay (chid chan);

/** \brief Returns a pointer to the current thread's CA context.
 *
 * If none then null is returned.
 *
 * Used when an auxiliary thread needs to join a CA client context started
 * by another thread
 *
 * \sa ca_attach_context(), ca_detach_context(), ca_context_create(), ca_context_destroy()
 */
LIBCA_API struct ca_client_context * epicsStdCall ca_current_context ();

/** \brief The calling thread becomes a member of the specified CA context.
 *
 * If ca_disable_preemptive_callback is specified when ca_context_create() is
 * called (or if ca_task_initialize() is called) then additional threads are
 * *not* allowed to join the CA context because allowing other threads to join
 * implies that CA callbacks will be called preemptively from more than one
 * thread.
 *
 * \sa ca_current_context(), ca_detach_context(), ca_context_create(), ca_context_destroy()
 *
 * \param[in] context A pointer to the CA context to join with.
 *
 * \returns ::ECA_NORMAL - Normal successful completion
 * \returns ::ECA_NOTTHREADED - Context is not preemptive so cannot be joined
 * \returns ::ECA_ISATTACHED - Thread already attached to a CA context
 */
LIBCA_API int epicsStdCall ca_attach_context ( struct ca_client_context * context );

/** \brief Prints information about the client context.
 *
 * Including, at higher interest levels, status for each channel. Lacking a CA
 * context pointer, ca_client_status() prints information about the calling
 * threads CA context.
 *
 * \param[in] level The interest level.
 * Increasing level produces increasing detail.
 */
LIBCA_API int epicsStdCall ca_client_status ( unsigned level );

/** \brief Prints information about the client context.
 *
 * Including, at higher interest levels, status for each channel. Lacking a CA
 * context pointer, ca_client_status() prints information about the calling
 * threads CA context.
 *
 * \param[in] context A pointer to the CA context to examine.
 * \param[in] level The interest level.
 * Increasing level produces increasing detail.
 */
LIBCA_API int epicsStdCall ca_context_status ( struct ca_client_context *context, unsigned level );

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

/** \brief Search for a channel over Channel Access.
 *
 * \deprecated Use ca_create_channel() instead.
 */
#define ca_search(pChanName, pChanID)\
ca_search_and_connect (pChanName, pChanID, 0, 0)

/** \brief Search and connect to a channel over Channel Access.
 *
 * \deprecated Use ca_create_channel() instead.
 */
LIBCA_API int epicsStdCall ca_search_and_connect
    ( const char * pChanName, chid * pChanID,
    caCh *pFunc, void * pArg );

LIBCA_API int epicsStdCall ca_channel_status (epicsThreadId tid);

/** \brief Cancel a subscription.
 *
 * \deprecated Use ca_clear_subscription() instead.
 */
LIBCA_API int epicsStdCall ca_clear_event ( evid eventID );

/** \brief Register a state change subscription and specify a callback function
 * to be invoked whenever the process variable undergoes significant state
 * changes.
 *
 * \deprecated Use ca_create_subscription() instead.
 */
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

