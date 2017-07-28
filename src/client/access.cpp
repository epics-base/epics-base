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
 *  Author: Jeffrey O. Hill
 *
 */

#include <new>
#include <float.h>

#include "dbDefs.h"
#include "epicsExit.h"

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"


/*
 * allocate error message string array
 * here so I can use sizeof
 */
#define CA_ERROR_GLBLSOURCE

/*
 * allocate header version strings here
 */
#define CAC_VERSION_GLOBAL

#define epicsExportSharedSymbols
#include "iocinf.h"
#include "oldAccess.h"
#include "cac.h"

epicsThreadPrivateId caClientContextId;

const char * ca_message_text []
=
{
"Normal successful completion",
"Maximum simultaneous IOC connections exceeded",
"Unknown internet host",
"Unknown internet service",
"Unable to allocate a new socket",

"Unable to connect to internet host or service",
"Unable to allocate additional dynamic memory",
"Unknown IO channel",
"Record field specified inappropriate for channel specified",
"The requested data transfer is greater than available memory or EPICS_CA_MAX_ARRAY_BYTES",

"User specified timeout on IO operation expired",
"Sorry, that feature is planned but not supported at this time",
"The supplied string is unusually large",
"The request was ignored because the specified channel is disconnected",
"The data type specifed is invalid",

"Remote Channel not found",
"Unable to locate all user specified channels",
"Channel Access Internal Failure",
"The requested local DB operation failed",
"Channel read request failed",

"Channel write request failed",
"Channel subscription request failed",
"Invalid element count requested",
"Invalid string",
"Virtual circuit disconnect",

"Identical process variable names on multiple servers",
"Request inappropriate within subscription (monitor) update callback",
"Database value get for that channel failed during channel search",
"Unable to initialize without the vxWorks VX_FP_TASK task option set",
"Event queue overflow has prevented first pass event after event add",

"Bad event subscription (monitor) identifier",
"Remote channel has new network address",
"New or resumed network connection",
"Specified task isnt a member of a CA context",
"Attempt to use defunct CA feature failed",

"The supplied string is empty",
"Unable to spawn the CA repeater thread- auto reconnect will fail",
"No channel id match for search reply- search reply ignored",
"Reseting dead connection- will try to reconnect",
"Server (IOC) has fallen behind or is not responding- still waiting",

"No internet interface with broadcast available",
"Invalid event selection mask",
"IO operations have completed",
"IO operations are in progress",
"Invalid synchronous group identifier",

"Put callback timed out",
"Read access denied",
"Write access denied",
"Requested feature is no longer supported",
"Empty PV search address list",

"No reasonable data conversion between client and server types",
"Invalid channel identifier",
"Invalid function pointer",
"Thread is already attached to a client context",
"Not supported by attached service",

"User destroyed channel",
"Invalid channel priority",
"Preemptive callback not enabled - additional threads may not join context",
"Client's protocol revision does not support transfers exceeding 16k bytes",
"Virtual circuit connection sequence aborted",

"Virtual circuit unresponsive"
};

static epicsThreadOnceId caClientContextIdOnce = EPICS_THREAD_ONCE_INIT;

extern "C" void ca_client_exit_handler (void *)
{
    if ( caClientContextId ) {
        epicsThreadPrivateDelete ( caClientContextId );
        caClientContextId = 0;
    }
}

// runs once only for each process
extern "C" void ca_init_client_context ( void * )
{
    caClientContextId = epicsThreadPrivateCreate ();
    if ( caClientContextId ) {
        epicsAtExit ( ca_client_exit_handler,0 );
    }
}

/*
 * fetchClientContext ();
 */
int fetchClientContext ( ca_client_context **ppcac )
{
    epicsThreadOnce ( &caClientContextIdOnce, ca_init_client_context, 0 );
    if ( caClientContextId == 0 ) {
        return ECA_ALLOCMEM;
    }

    int status;
    *ppcac = ( ca_client_context * ) epicsThreadPrivateGet ( caClientContextId );
    if ( *ppcac ) {
        status = ECA_NORMAL;
    }
    else {
        status = ca_task_initialize ();
        if ( status == ECA_NORMAL ) {
            *ppcac = (ca_client_context *) epicsThreadPrivateGet ( caClientContextId );
            if ( ! *ppcac ) {
                status = ECA_INTERNAL;
            }
        }
    }

    return status;
}

/*
 *  ca_task_initialize ()
 */
// extern "C"
int epicsShareAPI ca_task_initialize ( void )
{
    return ca_context_create ( ca_disable_preemptive_callback );
}

// extern "C"
int epicsShareAPI ca_context_create (
            ca_preemptive_callback_select premptiveCallbackSelect )
{
    ca_client_context *pcac;

    try {
        epicsThreadOnce ( & caClientContextIdOnce, ca_init_client_context, 0);
        if ( caClientContextId == 0 ) {
            return ECA_ALLOCMEM;
        }

        pcac = ( ca_client_context * ) epicsThreadPrivateGet ( caClientContextId );
	    if ( pcac ) {
            if ( premptiveCallbackSelect == ca_enable_preemptive_callback &&
                ! pcac->preemptiveCallbakIsEnabled() ) {
                return ECA_NOTTHREADED;
            }
		    return ECA_NORMAL;
	    }

        pcac = new ca_client_context (
            premptiveCallbackSelect == ca_enable_preemptive_callback );
	    if ( ! pcac ) {
		    return ECA_ALLOCMEM;
	    }

        epicsThreadPrivateSet ( caClientContextId, (void *) pcac );
    }
    catch ( ... ) {
        return ECA_ALLOCMEM;
    }
    return ECA_NORMAL;
}

//
// ca_modify_host_name ()
//
// defunct
//
// extern "C"
int epicsShareAPI ca_modify_host_name ( const char * )
{
    return ECA_NORMAL;
}

//
// ca_modify_user_name()
//
// defunct
//
// extern "C"
int epicsShareAPI ca_modify_user_name ( const char * )
{
    return ECA_NORMAL;
}

//
// ca_context_destroy ()
//
// extern "C"
void epicsShareAPI ca_context_destroy ()
{
    ca_client_context   *pcac;

    if ( caClientContextId != NULL ) {
        pcac = (ca_client_context *) epicsThreadPrivateGet ( caClientContextId );
        if ( pcac ) {
            delete pcac;
            epicsThreadPrivateSet ( caClientContextId, 0 );
        }
    }
}

/*
 *  ca_task_exit()
 *
 *  releases all resources alloc to a channel access client
 */
// extern "C"
int epicsShareAPI ca_task_exit ()
{
    ca_context_destroy ();
    return ECA_NORMAL;
}

/*
 *
 *      CA_BUILD_AND_CONNECT
 *
 *      backwards compatible entry point to ca_search_and_connect()
 */
// extern "C"
int epicsShareAPI ca_build_and_connect ( const char *name_str, chtype get_type,
            arrayElementCount get_count, chid * chan, void *pvalue,
            caCh *conn_func, void *puser )
{
    if ( get_type != TYPENOTCONN && pvalue != 0 && get_count != 0 ) {
        return ECA_ANACHRONISM;
    }

    return ca_search_and_connect ( name_str, chan, conn_func, puser );
}

/*
 *  ca_search_and_connect()
 */
// extern "C"
int epicsShareAPI ca_search_and_connect (
    const char * name_str, chid * chanptr,
    caCh * conn_func, void * puser )
{
    return ca_create_channel ( name_str, conn_func,
        puser, CA_PRIORITY_DEFAULT, chanptr );
}

// extern "C"
int epicsShareAPI ca_create_channel (
     const char * name_str, caCh * conn_func, void * puser,
     capri priority, chid * chanptr )
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( & pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    {
        CAFDHANDLER * pFunc = 0;
        void * pArg = 0;
        {
            epicsGuard < epicsMutex >
                guard ( pcac->mutex );
            if ( pcac->fdRegFuncNeedsToBeCalled ) {
                pFunc = pcac->fdRegFunc;
                pArg = pcac->fdRegArg;
                pcac->fdRegFuncNeedsToBeCalled = false;
            }
        }
        if ( pFunc ) {
            ( *pFunc ) ( pArg, pcac->sock, true );
        }
    }

    try {
        epicsGuard < epicsMutex > guard ( pcac->mutex );
        oldChannelNotify * pChanNotify =
            new ( pcac->oldChannelNotifyFreeList )
                oldChannelNotify ( guard, *pcac, name_str,
                    conn_func, puser, priority );
        // make sure that their chan pointer is set prior to
        // calling connection call backs
        *chanptr = pChanNotify;
        pChanNotify->initiateConnect ( guard );
        // no need to worry about a connect preempting here because
        // the connect sequence will not start untill initiateConnect()
        // is called
    }
    catch ( cacChannel::badString & ) {
        return ECA_BADSTR;
    }
    catch ( std::bad_alloc & ) {
        return ECA_ALLOCMEM;
    }
    catch ( cacChannel::badPriority & ) {
        return ECA_BADPRIORITY;
    }
    catch ( cacChannel::unsupportedByService & ) {
        return ECA_UNAVAILINSERV;
    }
    catch ( std :: exception & except ) {
        pcac->printFormated (
            "ca_create_channel: "
            "unexpected exception was \"%s\"",
            except.what () );
        return ECA_INTERNAL;
    }
    catch ( ... ) {
        return ECA_INTERNAL;
    }

    return ECA_NORMAL;
}

/*
 *  ca_clear_channel ()
 *
 * a known defect here is that there will be a
 * crash if they destroy the channel after destroying
 * its context
 */
// extern "C"
int epicsShareAPI ca_clear_channel ( chid pChan )
{
    ca_client_context & cac = pChan->getClientCtx ();
    {
        epicsGuard < epicsMutex > guard ( cac.mutex );
        try {
            pChan->eliminateExcessiveSendBacklog ( guard );
        }
        catch ( cacChannel::notConnected & ) {
            // intentionally ignored
        }
    }
    if ( cac.pCallbackGuard.get() &&
            cac.createdByThread == epicsThreadGetIdSelf () ) {
        epicsGuard < epicsMutex > guard ( cac.mutex );
        pChan->destructor ( *cac.pCallbackGuard.get(), guard );
        cac.oldChannelNotifyFreeList.release ( pChan );
    }
    else {
        //
        // we will definately stall out here if all of the
        // following are true
        //
        // o user creates non-preemtive mode client library context
        // o user doesnt periodically call a ca function
        // o user calls this function from an auxiillary thread
        //
        CallbackGuard cbGuard ( cac.cbMutex );
        epicsGuard < epicsMutex > guard ( cac.mutex );
        pChan->destructor ( *cac.pCallbackGuard.get(), guard );
        cac.oldChannelNotifyFreeList.release ( pChan );
    }
    return ECA_NORMAL;
}

/*
 *  Specify an event subroutine to be run for asynch exceptions
 */
// extern "C"
int epicsShareAPI ca_add_exception_event ( caExceptionHandler *pfunc, void *arg )
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->changeExceptionEvent ( pfunc, arg );

    return ECA_NORMAL;
}

/*
 *  ca_add_masked_array_event
 */
int epicsShareAPI ca_add_masked_array_event (
        chtype type, arrayElementCount count, chid pChan,
        caEventCallBackFunc *pCallBack, void *pCallBackArg,
        ca_real, ca_real, ca_real,
        evid *monixptr, long mask )
{
    return ca_create_subscription ( type, count, pChan, mask,
        pCallBack, pCallBackArg, monixptr );
}

/*
 *  ca_clear_event ()
 */
int epicsShareAPI ca_clear_event ( evid pMon )
{
    return ca_clear_subscription ( pMon );
}

// extern "C"
chid epicsShareAPI ca_evid_to_chid ( evid pMon )
{
    return & pMon->channel ();
}

// extern "C"
int epicsShareAPI ca_pend ( ca_real timeout, int early )
{
    if ( early ) {
        return ca_pend_io ( timeout );
    }
    else {
        return ca_pend_event ( timeout );
    }
}

/*
 * ca_pend_event ()
 */
// extern "C"
int epicsShareAPI ca_pend_event ( ca_real timeout )
{
    ca_client_context *pcac;
    int status = fetchClientContext ( &pcac );
    if ( status != ECA_NORMAL ) {
        return status;
    }

    try {
        // preserve past odd ball behavior of waiting forever when
        // the delay is zero
    	if ( timeout == 0.0 ) {
            while ( true ) {
                pcac->pendEvent ( 60.0 );
            }
        }
        return pcac->pendEvent ( timeout );
    }
    catch ( ... ) {
        return ECA_INTERNAL;
    }
}

/*
 * ca_pend_io ()
 */
// extern "C"
int epicsShareAPI ca_pend_io ( ca_real timeout )
{
    ca_client_context *pcac;
    int status = fetchClientContext ( &pcac );
    if ( status != ECA_NORMAL ) {
        return status;
    }

    try {
        // preserve past odd ball behavior of waiting forever when
        // the delay is zero
        if ( timeout == 0.0 ) {
            return pcac->pendIO ( DBL_MAX );
        }

        return pcac->pendIO ( timeout );
    }
    catch ( ... ) {
        return ECA_INTERNAL;
    }
}

/*
 *  ca_flush_io ()
 */
int epicsShareAPI ca_flush_io ()
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    epicsGuard < epicsMutex > guard ( pcac->mutex );
    pcac->flush ( guard );

    return ECA_NORMAL;
}

/*
 *  CA_TEST_IO ()
 */
int epicsShareAPI ca_test_io ()
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( pcac->ioComplete () ) {
        return ECA_IODONE;
    }
    else{
        return ECA_IOINPROGRESS;
    }
}

/*
 *  CA_SIGNAL()
 */
// extern "C"
void epicsShareAPI ca_signal ( long ca_status, const char *message )
{
    ca_signal_with_file_and_lineno ( ca_status, message, NULL, 0 );
}

/*
 * ca_message (long ca_status)
 *
 * - if it is an unknown error code then it possible
 * that the error string generated below
 * will be overwritten before (or while) the caller
 * of this routine is calling this routine
 * (if they call this routine again).
 */
// extern "C"
const char * epicsShareAPI ca_message ( long ca_status )
{
    unsigned msgNo = CA_EXTRACT_MSG_NO ( ca_status );

    if ( msgNo < NELEMENTS (ca_message_text) ) {
        return ca_message_text[msgNo];
    }
    else {
        return "new CA message number known only by server - see caerr.h";
    }
}

/*
 * ca_signal_with_file_and_lineno()
 */
// extern "C"
void epicsShareAPI ca_signal_with_file_and_lineno ( long ca_status,
            const char *message, const char *pfilenm, int lineno )
{
    ca_signal_formated ( ca_status, pfilenm, lineno, message );
}

/*
 * ca_signal_formated()
 */
// extern "C"
void epicsShareAPI ca_signal_formated ( long ca_status, const char *pfilenm,
                                       int lineno, const char *pFormat, ... )
{
    ca_client_context *pcac;

    if ( caClientContextId ) {
        pcac = ( ca_client_context * ) epicsThreadPrivateGet ( caClientContextId );
    }
    else {
        pcac = 0;
    }

    va_list theArgs;
    va_start ( theArgs, pFormat );
    if ( pcac ) {
        pcac->vSignal ( ca_status, pfilenm, lineno, pFormat, theArgs );
    }
    else {
        fprintf ( stderr, "CA exception in thread w/o CA ctx: status=%s file=%s line=%d: \n",
            ca_message ( ca_status ), pfilenm, lineno );
        if ( pFormat ) {
            vfprintf ( stderr, pFormat, theArgs );
        }
    }
    va_end ( theArgs );
}

/*
 *  CA_ADD_FD_REGISTRATION
 *
 *  call their function with their argument whenever
 *  a new fd is added or removed
 *  (for a manager of the select system call under UNIX)
 *
 */
// extern "C"
int epicsShareAPI ca_add_fd_registration ( CAFDHANDLER * func, void * arg )
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->registerForFileDescriptorCallBack ( func, arg );

    return ECA_NORMAL;
}

/*
 * ca_version()
 * function that returns the CA version string
 */
// extern "C"
const char * epicsShareAPI ca_version ()
{
    return CA_VERSION_STRING ( CA_MINOR_PROTOCOL_REVISION );
}

/*
 * ca_replace_printf_handler ()
 */
// extern "C"
int epicsShareAPI ca_replace_printf_handler ( caPrintfFunc *ca_printf_func )
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->replaceErrLogHandler ( ca_printf_func );

    return ECA_NORMAL;
}

/*
 * ca_get_ioc_connection_count()
 *
 * returns the number of IOC's that CA is connected to
 * (for testing purposes only)
 */
// extern "C"
unsigned epicsShareAPI ca_get_ioc_connection_count ()
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( & pcac );
    if ( caStatus != ECA_NORMAL ) {
        return 0u;
    }

    return pcac->circuitCount ();
}

unsigned epicsShareAPI ca_beacon_anomaly_count ()
{
    ca_client_context * pcac;
    int caStatus = fetchClientContext ( & pcac );
    if ( caStatus != ECA_NORMAL ) {
        return 0u;
    }

    return pcac->beaconAnomaliesSinceProgramStart ();
}

// extern "C"
int epicsShareAPI ca_channel_status ( epicsThreadId /* tid */ )
{
    ::printf ("The R3.14 EPICS OS abstraction API does not allow peeking at thread private storage of another thread.\n");
    ::printf ("Please call \"ca_client_status ( unsigned level )\" from the subsystem specific diagnostic code.\n");
	return ECA_ANACHRONISM;
}

// extern "C"
int epicsShareAPI ca_client_status ( unsigned level )
{
    ca_client_context *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->show ( level );
    return ECA_NORMAL;
}

int epicsShareAPI ca_context_status ( ca_client_context * pcac, unsigned level )
{
    pcac->show ( level );
    return ECA_NORMAL;
}

/*
 * ca_current_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
// extern "C"
struct ca_client_context * epicsShareAPI ca_current_context ()
{
    struct ca_client_context *pCtx;
    if ( caClientContextId ) {
        pCtx = ( struct ca_client_context * )
            epicsThreadPrivateGet ( caClientContextId );
    }
    else {
        pCtx = 0;
    }
    return pCtx;
}

/*
 * ca_attach_context ()
 *
 * used when an auxillary thread needs to join a CA client context started
 * by another thread
 */
// extern "C"
int epicsShareAPI ca_attach_context ( struct ca_client_context * pCtx )
{
    ca_client_context *pcac = (ca_client_context *) epicsThreadPrivateGet ( caClientContextId );
    if ( pcac && pCtx != 0 ) {
        return ECA_ISATTACHED;
    }
    if ( ! pCtx->preemptiveCallbakIsEnabled() ) {
        return ECA_NOTTHREADED;
    }
    epicsThreadPrivateSet ( caClientContextId, pCtx );
    return ECA_NORMAL;
}

void epicsShareAPI ca_detach_context ()
{
    if ( caClientContextId ) {
        epicsThreadPrivateSet ( caClientContextId, 0 );
    }
}

int epicsShareAPI ca_preemtive_callback_is_enabled ()
{
    ca_client_context *pcac = (ca_client_context *) epicsThreadPrivateGet ( caClientContextId );
    if ( ! pcac ) {
        return 0;
    }
    return pcac->preemptiveCallbakIsEnabled ();
}


// extern "C"
void epicsShareAPI ca_self_test ()
{
    ca_client_context *pcac = (ca_client_context *) epicsThreadPrivateGet ( caClientContextId );
    if ( ! pcac ) {
        return;
    }
    pcac->selfTest ();
}

// extern "C"
epicsShareDef const int epicsTypeToDBR_XXXX [lastEpicsType+1] = {
    DBR_SHORT, /* forces conversion fronm uint8 to int16 */
    DBR_CHAR,
    DBR_SHORT,
    DBR_LONG, /* forces conversion fronm uint16 to int32 */
    DBR_ENUM,
    DBR_LONG,
    DBR_LONG, /* very large unsigned number will not map */
    DBR_FLOAT,
    DBR_DOUBLE,
    DBR_STRING,
    DBR_STRING
};

// extern "C"
epicsShareDef const epicsType DBR_XXXXToEpicsType [LAST_BUFFER_TYPE+1] = {
	epicsOldStringT,
	epicsInt16T,
	epicsFloat32T,
	epicsEnum16T,
	epicsUInt8T,
	epicsInt32T,
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,
	epicsFloat32T,
	epicsEnum16T,
	epicsUInt8T,
	epicsInt32T,
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,
	epicsFloat32T,
	epicsEnum16T,
	epicsUInt8T,
	epicsInt32T,
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,
	epicsFloat32T,
	epicsEnum16T,
	epicsUInt8T,
	epicsInt32T,
	epicsFloat64T,

	epicsOldStringT,
	epicsInt16T,
	epicsFloat32T,
	epicsEnum16T,
	epicsUInt8T,
	epicsInt32T,
	epicsFloat64T,

	epicsUInt16T,
	epicsUInt16T,
	epicsOldStringT,
	epicsOldStringT
};

// extern "C"
epicsShareDef const unsigned short dbr_size[LAST_BUFFER_TYPE+1] = {
	sizeof(dbr_string_t),		/* string max size		*/
	sizeof(dbr_short_t),		/* short			*/
	sizeof(dbr_float_t),		/* IEEE Float			*/
	sizeof(dbr_enum_t),		/* item number			*/
	sizeof(dbr_char_t),		/* character			*/

	sizeof(dbr_long_t),		/* long				*/
	sizeof(dbr_double_t),		/* double			*/
	sizeof(struct dbr_sts_string),	/* string field	with status	*/
	sizeof(struct dbr_sts_short),	/* short field with status	*/
	sizeof(struct dbr_sts_float),	/* float field with status	*/

	sizeof(struct dbr_sts_enum),	/* item number with status	*/
	sizeof(struct dbr_sts_char),	/* char field with status	*/
	sizeof(struct dbr_sts_long),	/* long field with status	*/
	sizeof(struct dbr_sts_double),	/* double field with time	*/
	sizeof(struct dbr_time_string),	/* string field	with time	*/

	sizeof(struct dbr_time_short),	/* short field with time	*/
	sizeof(struct dbr_time_float),	/* float field with time	*/
	sizeof(struct dbr_time_enum),	/* item number with time	*/
	sizeof(struct dbr_time_char),	/* char field with time		*/
	sizeof(struct dbr_time_long),	/* long field with time		*/

	sizeof(struct dbr_time_double),	/* double field with time	*/
	sizeof(struct dbr_sts_string),	/* graphic string info		*/
	sizeof(struct dbr_gr_short),	/* graphic short info		*/
	sizeof(struct dbr_gr_float),	/* graphic float info		*/
	sizeof(struct dbr_gr_enum),	/* graphic item info		*/

	sizeof(struct dbr_gr_char),	/* graphic char info		*/
	sizeof(struct dbr_gr_long),	/* graphic long info		*/
	sizeof(struct dbr_gr_double),	/* graphic double info		*/
	sizeof(struct dbr_sts_string),	/* control string info		*/
	sizeof(struct dbr_ctrl_short),	/* control short info		*/

	sizeof(struct dbr_ctrl_float),	/* control float info		*/
	sizeof(struct dbr_ctrl_enum),	/* control item info		*/
	sizeof(struct dbr_ctrl_char),	/* control char info		*/
	sizeof(struct dbr_ctrl_long),	/* control long info		*/
	sizeof(struct dbr_ctrl_double),	/* control double info		*/

	sizeof(dbr_put_ackt_t),		/* put ackt			*/
	sizeof(dbr_put_acks_t),		/* put acks			*/
	sizeof(struct dbr_stsack_string),/* string field with status/ack*/
	sizeof(dbr_string_t),		/* string max size		*/
};

// extern "C"
epicsShareDef const unsigned short dbr_value_size[LAST_BUFFER_TYPE+1] = {
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/

	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/

	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/

	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/

	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/
	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/

	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_short_t),	/* short			*/

	sizeof(dbr_float_t),	/* IEEE Float			*/
	sizeof(dbr_enum_t),	/* item number			*/
	sizeof(dbr_char_t),	/* character			*/
	sizeof(dbr_long_t),	/* long				*/
	sizeof(dbr_double_t),	/* double			*/

	sizeof(dbr_ushort_t), 	/* put_ackt			*/
	sizeof(dbr_ushort_t), 	/* put_acks			*/
	sizeof(dbr_string_t),	/* string max size		*/
	sizeof(dbr_string_t),	/* string max size		*/
};

//extern "C"
epicsShareDef const enum dbr_value_class dbr_value_class[LAST_BUFFER_TYPE+1] = {
	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/

	dbr_class_string,	/* string max size		*/
	dbr_class_int,		/* short			*/
	dbr_class_float,	/* IEEE Float			*/
	dbr_class_int,		/* item number			*/
	dbr_class_int,		/* character			*/
	dbr_class_int,		/* long				*/
	dbr_class_float,	/* double			*/
	dbr_class_int,
	dbr_class_int,
	dbr_class_string,
	dbr_class_string,	/* string max size		*/
};

// extern "C"
epicsShareDef const unsigned short dbr_value_offset[LAST_BUFFER_TYPE+1] = {
	0,					/* string			*/
	0,					/* short			*/
	0,					/* IEEE Float			*/
	0,					/* item number			*/
	0,					/* character			*/
	0,					/* long				*/
	0,					/* IEEE double			*/
	(unsigned short) offsetof(dbr_sts_string,value[0]),/* string field	with status	*/
	(unsigned short) offsetof(dbr_sts_short,value),	/* short field with status	*/
	(unsigned short) offsetof(dbr_sts_float,value),	/* float field with status	*/
	(unsigned short) offsetof(dbr_sts_enum,value),	/* item number with status	*/
	(unsigned short) offsetof(dbr_sts_char,value),	/* char field with status	*/
	(unsigned short) offsetof(dbr_sts_long,value),	/* long field with status	*/
	(unsigned short) offsetof(dbr_sts_double,value),	/* double field with time	*/
	(unsigned short) offsetof(dbr_time_string,value[0] ),/* string field with time	*/
	(unsigned short) offsetof(dbr_time_short,value),	/* short field with time	*/
	(unsigned short) offsetof(dbr_time_float,value),	/* float field with time	*/
	(unsigned short) offsetof(dbr_time_enum,value),	/* item number with time	*/
	(unsigned short) offsetof(dbr_time_char,value),	/* char field with time		*/
	(unsigned short) offsetof(dbr_time_long,value),	/* long field with time		*/
	(unsigned short) offsetof(dbr_time_double,value),	/* double field with time	*/
	(unsigned short) offsetof(dbr_sts_string,value[0]),/* graphic string info		*/
	(unsigned short) offsetof(dbr_gr_short,value),	/* graphic short info		*/
	(unsigned short) offsetof(dbr_gr_float,value),	/* graphic float info		*/
	(unsigned short) offsetof(dbr_gr_enum,value),	/* graphic item info		*/
	(unsigned short) offsetof(dbr_gr_char,value),	/* graphic char info		*/
	(unsigned short) offsetof(dbr_gr_long,value),	/* graphic long info		*/
	(unsigned short) offsetof(dbr_gr_double,value),	/* graphic double info		*/
	(unsigned short) offsetof(dbr_sts_string,value[0]),/* control string info		*/
	(unsigned short) offsetof(dbr_ctrl_short,value),	/* control short info		*/
	(unsigned short) offsetof(dbr_ctrl_float,value),	/* control float info		*/
	(unsigned short) offsetof(dbr_ctrl_enum,value),	/* control item info		*/
	(unsigned short) offsetof(dbr_ctrl_char,value),	/* control char info		*/
	(unsigned short) offsetof(dbr_ctrl_long,value),	/* control long info		*/
	(unsigned short) offsetof(dbr_ctrl_double,value),	/* control double info		*/
	0,					/* put ackt			*/
	0,					/* put acks			*/
	(unsigned short) offsetof(dbr_stsack_string,value[0]),/* string field	with status	*/
	0,					/* string			*/
};

// extern "C"
epicsShareDef const char *dbf_text[LAST_TYPE+3] = {
	"TYPENOTCONN",
	"DBF_STRING",
	"DBF_SHORT",
	"DBF_FLOAT",
	"DBF_ENUM",
	"DBF_CHAR",
	"DBF_LONG",
	"DBF_DOUBLE",
	"DBF_NO_ACCESS"
};

// extern "C"
epicsShareDef const char *dbf_text_invalid = "DBF_invalid";

// extern "C"
epicsShareDef const short dbf_text_dim = (sizeof dbf_text)/(sizeof (char *));

// extern "C"
epicsShareDef const char *dbr_text[LAST_BUFFER_TYPE+1] = {
    "DBR_STRING",
    "DBR_SHORT",
    "DBR_FLOAT",
    "DBR_ENUM",
    "DBR_CHAR",
    "DBR_LONG",
    "DBR_DOUBLE",
    "DBR_STS_STRING",
    "DBR_STS_SHORT",
    "DBR_STS_FLOAT",
    "DBR_STS_ENUM",
    "DBR_STS_CHAR",
    "DBR_STS_LONG",
    "DBR_STS_DOUBLE",
    "DBR_TIME_STRING",
    "DBR_TIME_SHORT",
    "DBR_TIME_FLOAT",
    "DBR_TIME_ENUM",
    "DBR_TIME_CHAR",
    "DBR_TIME_LONG",
    "DBR_TIME_DOUBLE",
    "DBR_GR_STRING",
    "DBR_GR_SHORT",
    "DBR_GR_FLOAT",
    "DBR_GR_ENUM",
    "DBR_GR_CHAR",
    "DBR_GR_LONG",
    "DBR_GR_DOUBLE",
    "DBR_CTRL_STRING",
    "DBR_CTRL_SHORT",
    "DBR_CTRL_FLOAT",
    "DBR_CTRL_ENUM",
    "DBR_CTRL_CHAR",
    "DBR_CTRL_LONG",
    "DBR_CTRL_DOUBLE",
    "DBR_PUT_ACKT",
    "DBR_PUT_ACKS",
    "DBR_STSACK_STRING",
    "DBR_CLASS_NAME"
};

// extern "C"
epicsShareDef const char *dbr_text_invalid = "DBR_invalid";

// extern "C"
epicsShareDef const short dbr_text_dim = (sizeof dbr_text) / (sizeof (char *)) + 1;
