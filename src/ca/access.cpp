
/*
 *  $Id$
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

#include "iocinf.h"
#include "oldAccess.h"
#include "autoPtrDestroy.h"

epicsThreadPrivateId caClientContextId;

static epicsThreadOnceId caClientContextIdOnce = EPICS_THREAD_ONCE_INIT;

// extern "C"
void ca_client_exit_handler ()
{
    if ( caClientContextId ) {
        epicsThreadPrivateDelete ( caClientContextId );
        caClientContextId = 0;
    }
}

// runs once only for each process
// extern "C"
void ca_init_client_context ( void * )
{
    caClientContextId = epicsThreadPrivateCreate ();
    if ( caClientContextId ) {
        atexit ( ca_client_exit_handler );
    }
}

/*
 * fetchClientContext ();
 */
int fetchClientContext ( oldCAC **ppcac )
{
    if ( caClientContextId == 0 ) {
        epicsThreadOnce ( &caClientContextIdOnce, ca_init_client_context, 0 );
        if ( caClientContextId == 0 ) {
            return ECA_ALLOCMEM;
        }
    }

    int status;
    *ppcac = ( oldCAC * ) epicsThreadPrivateGet ( caClientContextId );
    if ( *ppcac ) {
        status = ECA_NORMAL;
    }
    else {
        status = ca_task_initialize ();
        if ( status == ECA_NORMAL ) {
            *ppcac = (oldCAC *) epicsThreadPrivateGet ( caClientContextId );
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
    oldCAC *pcac;

    try {
        epicsThreadOnce ( &caClientContextIdOnce, ca_init_client_context, 0);
        if ( caClientContextId == 0 ) {
            return ECA_ALLOCMEM;
        }

        pcac = ( oldCAC * ) epicsThreadPrivateGet ( caClientContextId );
	    if ( pcac ) {
		    return ECA_NORMAL;
	    }

        bool enablePreemptiveCallback = 
            premptiveCallbackSelect == ca_enable_preemptive_callback;
        pcac = new oldCAC ( enablePreemptiveCallback );
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
// ca_register_service ()
//
int epicsShareAPI ca_register_service ( cacService *pService )
{
    oldCAC *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }
    if ( pService ) {
        pcac->registerService ( *pService );
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
    oldCAC   *pcac;

    if ( caClientContextId != NULL ) {
        pcac = (oldCAC *) epicsThreadPrivateGet ( caClientContextId );
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
     const char *name_str, caCh * conn_func, void * puser,
     capri priority, chid * chanptr )
{
    oldCAC *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    try {
        oldChannelNotify * pChanNotify = new oldChannelNotify ( 
            * pcac, name_str, conn_func, puser, priority );
        if ( ! pChanNotify ) {
            return ECA_ALLOCMEM;
        }
        // make sure that their chan pointer is set prior to
        // calling connection call backs
        *chanptr = pChanNotify;
        pChanNotify->initiateConnect ();
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
    catch ( ... ) {
        return ECA_INTERNAL;
    }

    return ECA_NORMAL;
}

/*
 *  ca_clear_channel ()
 */
// extern "C"
int epicsShareAPI ca_clear_channel ( chid pChan )
{
    pChan->destroy ();
    return ECA_NORMAL;
}

/*
 * ca_array_get ()
 */
// extern "C"
int epicsShareAPI ca_array_get ( chtype type, 
            arrayElementCount count, chid pChan, void *pValue )
{
    oldCAC *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    autoPtrDestroy < getCopy > pNotify 
        ( new getCopy ( *pcac, *pChan, tmpType, count, pValue ) );
    if ( ! pNotify.get() ) {
        return ECA_ALLOCMEM;
    }

    try {
        pChan->read ( type, count, *pNotify );
        pNotify.release ();
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        pNotify->cancel ();
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        pNotify->cancel ();
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        pNotify->cancel ();
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        pNotify->cancel ();
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        pNotify->cancel ();
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        pNotify->cancel ();
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        pNotify->cancel ();
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        pNotify->cancel ();
        return ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall & ) {
        pNotify->cancel ();
        return ECA_TOLARGE;
    }
    catch ( ... )
    {
        pNotify->cancel ();
        return ECA_GETFAIL;
    }
}

/*
 * ca_array_get_callback ()
 */
// extern "C"
int epicsShareAPI ca_array_get_callback ( chtype type, 
            arrayElementCount count, chid pChan,
            caEventCallBackFunc *pfunc, void *arg )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    autoPtrDestroy < getCallback > pNotify 
        ( new getCallback ( *pChan, pfunc, arg ) );
    if ( ! pNotify.get() ) {
        return ECA_ALLOCMEM;
    }

    try {
        pChan->read ( tmpType, count, *pNotify );
        pNotify.release ();
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noReadAccess & )
    {
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall ) {
        return ECA_TOLARGE;
    }
    catch ( ... )
    {
        return ECA_GETFAIL;
    }
}

/*
 *  ca_array_put_callback ()
 */
// extern "C"
int epicsShareAPI ca_array_put_callback ( chtype type, arrayElementCount count, 
    chid pChan, const void *pValue, caEventCallBackFunc *pfunc, void *usrarg )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    autoPtrDestroy < putCallback > pNotify
            ( new putCallback ( *pChan, pfunc, usrarg ) );
    if ( ! pNotify.get() ) {
        return ECA_ALLOCMEM;
    }

    try {
        pChan->write ( tmpType, count, pValue, *pNotify );
        pNotify.release ();
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        return ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_PUTFAIL;
    }
}

/*
 *  ca_array_put ()
 */
// extern "C"
int epicsShareAPI ca_array_put ( chtype type, arrayElementCount count, 
                                chid pChan, const void *pValue )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    try {
        pChan->write ( tmpType, count, pValue );
        return ECA_NORMAL;
    }
    catch ( cacChannel::badString & )
    {
        return ECA_BADSTR;
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::noWriteAccess & )
    {
        return ECA_NOWTACCESS;
    }
    catch ( cacChannel::notConnected & )
    {
        return ECA_DISCONN;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( cacChannel::requestTimedOut & )
    {
        return ECA_TIMEOUT;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( ... )
    {
        return ECA_PUTFAIL;
    }
}

/*
 *  Specify an event subroutine to be run for connection events
 */
// extern "C"
int epicsShareAPI ca_change_connection_event ( chid pChan, caCh *pfunc )
{
    return pChan->changeConnCallBack ( pfunc );
    return ECA_INTERNAL;
}

/*
 * ca_replace_access_rights_event
 */
// extern "C"
int epicsShareAPI ca_replace_access_rights_event ( chid pChan, caArh *pfunc )
{
    return pChan->replaceAccessRightsEvent ( pfunc );
}

/*
 *  Specify an event subroutine to be run for asynch exceptions
 */
// extern "C"
int epicsShareAPI ca_add_exception_event ( caExceptionHandler *pfunc, void *arg )
{
    oldCAC *pcac;
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
// extern "C"
int epicsShareAPI ca_add_masked_array_event ( 
        chtype type, arrayElementCount count, chid pChan, 
        caEventCallBackFunc *pCallBack, void *pCallBackArg, 
        ca_real, ca_real, ca_real, 
        evid *monixptr, long mask )
{
    if ( type < 0 ) {
        return ECA_BADTYPE;
    }
    unsigned tmpType = static_cast < unsigned > ( type );

    if ( INVALID_DB_REQ (type) ) {
        return ECA_BADTYPE;
    }

    if ( pCallBack == NULL ) {
        return ECA_BADFUNCPTR;
    }

    static const long maskMask = 0xffff;
    if ( ( mask & maskMask ) == 0) {
        return ECA_BADMASK;
    }

    if ( mask & ~maskMask ) {
        return ECA_BADMASK;
    }

    try {
        autoPtrDestroy < oldSubscription > pSubsr
            ( new oldSubscription  ( *pChan, pCallBack, pCallBackArg ) );
        if ( pSubsr.get () ) {
            evid pTmp = pSubsr.release ();
            if ( monixptr ) {
                *monixptr = pTmp;
            }
            pTmp->begin ( tmpType, count, mask );
            // dont touch pTmp after this because
            // the first callback might have canceled it
            return ECA_NORMAL;
        }
        else {
            return ECA_ALLOCMEM;
        }
    }
    catch ( cacChannel::badType & )
    {
        return ECA_BADTYPE;
    }
    catch ( cacChannel::outOfBounds & )
    {
        return ECA_BADCOUNT;
    }
    catch ( cacChannel::badEventSelection & )
    {
        return ECA_BADMASK;
    }
    catch ( cacChannel::noReadAccess & )
    {
        return ECA_NORDACCESS;
    }
    catch ( cacChannel::unsupportedByService & )
    {
        return ECA_NOTINSERVICE;
    }
    catch ( std::bad_alloc & )
    {
        return ECA_ALLOCMEM;
    }
    catch ( cacChannel::msgBodyCacheTooSmall & ) {
        return ECA_TOLARGE;
    }
    catch ( ... )
    {
        return ECA_INTERNAL;
    }
}

/*
 *  ca_clear_event ()
 */
// extern "C"
int epicsShareAPI ca_clear_event ( evid pMon )
{
    pMon->destroy ();
    return ECA_NORMAL;
}

// extern "C"
chid epicsShareAPI ca_evid_to_chid ( evid pMon )
{
    return & pMon->channel (); 
}

// extern "C"
int epicsShareAPI ca_pend ( ca_real timeout, int early ) // X aCC 361
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
    oldCAC *pcac;
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
    oldCAC *pcac;
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
// extern "C"
int epicsShareAPI ca_flush_io ()
{
    oldCAC *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->flushRequest ();

    return ECA_NORMAL;
}

/*
 *  CA_TEST_IO ()
 */
// extern "C"
int epicsShareAPI ca_test_io () // X aCC 361
{
    oldCAC *pcac;
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
    oldCAC *pcac;

    if ( caClientContextId ) {
        pcac = ( oldCAC * ) epicsThreadPrivateGet ( caClientContextId );
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
        fprintf ( stderr, "file=%s line=%d: CA exception delivered to a thread w/o ca context\n", 
            pfilenm, lineno );
        vfprintf ( stderr, pFormat, theArgs );
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
int epicsShareAPI ca_add_fd_registration (CAFDHANDLER *func, void *arg)
{
    oldCAC *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->registerForFileDescriptorCallBack ( func, arg );

    return ECA_NORMAL;
}

/*
 * ca_get_host_name ()
 */
// extern "C"
void epicsShareAPI ca_get_host_name ( chid pChan, char *pBuf, unsigned bufLength )
{
    pChan->hostName ( pBuf, bufLength );
}

/*
 * ca_host_name ()
 */
// extern "C"
const char * epicsShareAPI ca_host_name ( chid pChan )
{
    return pChan->pHostName ();
}

/*
 * ca_v42_ok(chid chan)
 */
// extern "C"
int epicsShareAPI ca_v42_ok ( chid pChan )
{
    return pChan->ca_v42_ok ();
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
    oldCAC *pcac;
    int caStatus = fetchClientContext (&pcac);
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    pcac->replaceErrLogHandler ( ca_printf_func );

    return ECA_NORMAL;
}

/*
 * ca_field_type()
 */
// extern "C"
short epicsShareAPI ca_field_type ( chid pChan ) 
{
    return pChan->nativeType ();
}

/*
 * ca_element_count ()
 */
// extern "C"
arrayElementCount epicsShareAPI ca_element_count ( chid pChan ) 
{
    return pChan->nativeElementCount ();
}

/*
 * ca_state ()
 */
// extern "C"
enum channel_state epicsShareAPI ca_state ( chid pChan ) // X aCC 361
{
    if ( pChan->connected() ) {
        return cs_conn;
    }
    else if ( pChan->previouslyConnected() ){
        return cs_prev_conn;
    }
    else {
        return cs_never_conn;
    }
}

/*
 * ca_set_puser ()
 */
// extern "C"
void epicsShareAPI ca_set_puser ( chid pChan, void *puser )
{
    pChan->setPrivatePointer ( puser );
}

/*
 * ca_get_puser ()
 */
// extern "C"
void * epicsShareAPI ca_puser ( chid pChan )
{
    return pChan->privatePointer ();
}

/*
 * ca_read_access ()
 */
// extern "C"
unsigned epicsShareAPI ca_read_access ( chid pChan )
{
    return pChan->accessRights().readPermit();
}

/*
 * ca_write_access ()
 */
// extern "C"
unsigned epicsShareAPI ca_write_access ( chid pChan )
{
    return pChan->accessRights().writePermit();
}

/*
 * ca_name ()
 */
// extern "C"
const char * epicsShareAPI ca_name ( chid pChan )
{
    return pChan->pName ();
}

// extern "C"
unsigned epicsShareAPI ca_search_attempts ( chid pChan )
{
    return pChan->searchAttempts ();
}

// extern "C"
double epicsShareAPI ca_beacon_period ( chid pChan )
{
    return pChan->beaconPeriod ();
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
    oldCAC *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

    return pcac->connectionCount ();
}

// extern "C"
int epicsShareAPI ca_channel_status ( epicsThreadId /* tid */ )
{
    ::printf ("new OSI API does not allow peeking at thread private storage of another thread\n");
    ::printf ("please call \"ca_client_status ( unsigned level )\" from the subsystem specific diagnostic code.\n");
	return ECA_ANACHRONISM;
}

// extern "C"
int epicsShareAPI ca_client_status ( unsigned level )
{
    oldCAC *pcac;
    int caStatus = fetchClientContext ( &pcac );
    if ( caStatus != ECA_NORMAL ) {
        return caStatus;
    }

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
struct oldCAC * epicsShareAPI ca_current_context ()
{
    struct oldCAC *pCtx;
    if ( caClientContextId ) {
        pCtx = ( struct oldCAC * ) 
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
int epicsShareAPI ca_attach_context ( struct oldCAC * pCtx )
{
    oldCAC *pcac = (oldCAC *) epicsThreadPrivateGet ( caClientContextId );
    if ( pcac && pCtx != 0 ) {
        return ECA_ISATTACHED;
    }
    if ( ! pCtx->preemptiveCallbakIsEnabled() ) {
        return ECA_NOTTHREADED;
    }
    epicsThreadPrivateSet ( caClientContextId, pCtx );
    return ECA_NORMAL;
}

int epicsShareAPI ca_preemtive_callback_is_enabled ()
{
    oldCAC *pcac = (oldCAC *) epicsThreadPrivateGet ( caClientContextId );
    if ( ! pcac ) {
        return 0;
    }
    return pcac->preemptiveCallbakIsEnabled ();
}


// extern "C"
void epicsShareAPI ca_self_test ()
{
    oldCAC *pcac = (oldCAC *) epicsThreadPrivateGet ( caClientContextId );
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

// extern "C"
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
	offsetof(dbr_sts_string,value[0]),/* string field	with status	*/
	offsetof(dbr_sts_short,value),	/* short field with status	*/
	offsetof(dbr_sts_float,value),	/* float field with status	*/
	offsetof(dbr_sts_enum,value),	/* item number with status	*/
	offsetof(dbr_sts_char,value),	/* char field with status	*/
	offsetof(dbr_sts_long,value),	/* long field with status	*/
	offsetof(dbr_sts_double,value),	/* double field with time	*/
	offsetof(dbr_time_string,value[0]),/* string field with time	*/
	offsetof(dbr_time_short,value),	/* short field with time	*/
	offsetof(dbr_time_float,value),	/* float field with time	*/
	offsetof(dbr_time_enum,value),	/* item number with time	*/
	offsetof(dbr_time_char,value),	/* char field with time		*/
	offsetof(dbr_time_long,value),	/* long field with time		*/
	offsetof(dbr_time_double,value),	/* double field with time	*/
	offsetof(dbr_sts_string,value[0]),/* graphic string info		*/
	offsetof(dbr_gr_short,value),	/* graphic short info		*/
	offsetof(dbr_gr_float,value),	/* graphic float info		*/
	offsetof(dbr_gr_enum,value),	/* graphic item info		*/
	offsetof(dbr_gr_char,value),	/* graphic char info		*/
	offsetof(dbr_gr_long,value),	/* graphic long info		*/
	offsetof(dbr_gr_double,value),	/* graphic double info		*/
	offsetof(dbr_sts_string,value[0]),/* control string info		*/
	offsetof(dbr_ctrl_short,value),	/* control short info		*/
	offsetof(dbr_ctrl_float,value),	/* control float info		*/
	offsetof(dbr_ctrl_enum,value),	/* control item info		*/
	offsetof(dbr_ctrl_char,value),	/* control char info		*/
	offsetof(dbr_ctrl_long,value),	/* control long info		*/
	offsetof(dbr_ctrl_double,value),	/* control double info		*/
	0,					/* put ackt			*/
	0,					/* put acks			*/
	offsetof(dbr_stsack_string,value[0]),/* string field	with status	*/
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
