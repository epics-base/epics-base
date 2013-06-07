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
 * CA regression test
 * Authors:
 * Jeff Hill
 * Murali Shankar - initial versions of verifyMultithreadSubscr
 * Michael Abbott - initial versions of multiSubscrDestroyNoLateCallbackTest
 * 
 */

/*
 * ANSI
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <string.h>

/*
 * EPICS
 */
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#include "epicsAssert.h"
#include "epicsMutex.h"
#include "epicsEvent.h"
#include "epicsTime.h"
#include "dbDefs.h"
#include "envDefs.h"
#include "caDiagnostics.h"
#include "cadef.h"
#include "fdmgr.h"
#include "epicsExit.h"

typedef struct appChan {
    char name[64];
    chid channel;
    evid subscription;
    unsigned char connected;
    unsigned char accessRightsHandlerInstalled;
    unsigned subscriptionUpdateCount;
    unsigned accessUpdateCount;
    unsigned connectionUpdateCount;
    unsigned getCallbackCount;
} appChan;

unsigned subscriptionUpdateCount;
unsigned accessUpdateCount;
unsigned connectionUpdateCount;
unsigned getCallbackCount;

static epicsTimeStamp showProgressBeginTime;

static const double timeoutToPendIO = 1e20;

#define verify(exp) ((exp) ? (void)0 : \
    epicsAssert(__FILE__, __LINE__, #exp, epicsAssertAuthor))

void showProgressBegin ( const char *pTestName, unsigned interestLevel )
{

    if ( interestLevel > 0 ) {
        if ( interestLevel > 1 ) {
            printf ( "%s ", pTestName );
            epicsTimeGetCurrent ( & showProgressBeginTime );
        }
        printf ( "{" );
    }
    fflush ( stdout );
}

void showProgressEnd ( unsigned interestLevel )
{
    if ( interestLevel > 0 ) {
        printf ( "}" );
        if ( interestLevel > 1 ) {
            epicsTimeStamp showProgressEndTime;
            double delay;
            epicsTimeGetCurrent ( & showProgressEndTime );
            delay = epicsTimeDiffInSeconds ( &showProgressEndTime, &showProgressBeginTime );
            printf ( " %f sec\n", delay );
        }
        else {
            fflush ( stdout );
        }
    }
}

void showProgress ( unsigned interestLevel )
{
    if ( interestLevel > 0 ) {
        printf ( "." );
        fflush ( stdout );
    }
}

void nUpdatesTester ( struct event_handler_args args )
{
    if ( args.status == ECA_NORMAL ) {
        unsigned *pCtr = (unsigned *) args.usr;
        ( *pCtr ) ++;
    }
    else {
        printf ( "subscription update failed for \"%s\" because \"%s\"", 
            ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

void monitorSubscriptionFirstUpdateTest ( const char *pName, chid chan, unsigned interestLevel )
{
    int status;
    struct dbr_ctrl_double currentVal;
    double delta;
    unsigned eventCount = 0u;
    unsigned waitCount = 0u;
    evid id;
    chid chan2;

    showProgressBegin ( "monitorSubscriptionFirstUpdateTest", interestLevel );

    /*
     * verify that the first event arrives (with evid)
     * and channel connected
     */
    status = ca_add_event ( DBR_FLOAT, 
                chan, nUpdatesTester, &eventCount, &id );
    SEVCHK ( status, 0 );
    ca_flush_io ();
    epicsThreadSleep ( 0.1 );
    ca_poll (); /* emulate typical GUI */
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "e" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    verify ( eventCount > 0 );

    /* clear any knowledge of old gets */
    ca_pend_io ( 1e-5 );

    /* verify that a ca_put() produces an update, but */
    /* this may fail if there is an unusual deadband */
    status = ca_get ( DBR_CTRL_DOUBLE, chan, &currentVal );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, NULL );
    eventCount = 0u;
    waitCount = 0u;
    delta = ( currentVal.upper_ctrl_limit - currentVal.lower_ctrl_limit ) / 4.0;
    if ( delta <= 0.0 ) {
        delta = 100.0;
    }
    if ( currentVal.value + delta < currentVal.upper_ctrl_limit ) {
        currentVal.value += delta;
    }
    else {
        currentVal.value -= delta;
    }
    status = ca_put ( DBR_DOUBLE, chan, &currentVal.value );
    SEVCHK ( status, NULL );
    ca_flush_io ();
    epicsThreadSleep ( 0.1 );
    ca_poll (); /* emulate typical GUI */
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "p" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    verify ( eventCount > 0 );

    status = ca_clear_event ( id );
    SEVCHK (status, 0);

    /*
     * verify that the first event arrives (w/o evid)
     * and when channel initially disconnected
     */
    eventCount = 0u;
    waitCount = 0u;
    status = ca_search ( pName, &chan2 );
    SEVCHK ( status, 0 );
    status = ca_add_event ( DBR_FLOAT, chan2, 
                nUpdatesTester, &eventCount, 0 );
    SEVCHK ( status, 0 );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK (status, 0);
    epicsThreadSleep ( 0.1 );
    ca_poll ();
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "w" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    verify ( eventCount > 0 );

    /* verify that a ca_put() produces an update, but */
    /* this may fail if there is an unusual deadband */
    status = ca_get ( DBR_CTRL_DOUBLE, chan2, &currentVal );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, NULL );
    eventCount = 0u;
    waitCount = 0u;
    delta = ( currentVal.upper_ctrl_limit - currentVal.lower_ctrl_limit ) / 4.0;
    if ( delta <= 0.0 ) {
        delta = 100.0;
    }
    if ( currentVal.value + delta < currentVal.upper_ctrl_limit ) {
        currentVal.value += delta;
    }
    else {
        currentVal.value -= delta;
    }
    status = ca_put ( DBR_DOUBLE, chan2, &currentVal.value );
    SEVCHK ( status, NULL );
    ca_flush_io ();
    epicsThreadSleep ( 0.1 );
    ca_poll ();
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "t" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    verify ( eventCount > 0 );

    /* clean up */
    status = ca_clear_channel ( chan2 );
    SEVCHK ( status, 0 );

    showProgressEnd ( interestLevel );
}

void ioTesterGet ( struct event_handler_args args )
{
    if ( args.status == ECA_NORMAL ) {
        unsigned *pCtr = (unsigned *) args.usr;
        ( *pCtr ) ++;
    }
    else {
        printf("get call back failed for \"%s\" because \"%s\"", 
            ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

void ioTesterEvent ( struct event_handler_args args )
{
    if ( args.status == ECA_NORMAL ) {
        int status;
        status = ca_get_callback ( DBR_STS_STRING, args.chid, ioTesterGet, args.usr );
        SEVCHK ( status, 0 );
    }
    else {
        printf ( "subscription update failed for \"%s\" because \"%s\"", 
            ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

void verifyMonitorSubscriptionFlushIO ( chid chan, unsigned interestLevel  )
{
    int status;
    unsigned eventCount = 0u;
    unsigned waitCount = 0u;
    evid id;

    showProgressBegin ( "verifyMonitorSubscriptionFlushIO", interestLevel );

    /*
     * verify that the first event arrives
     */
    status = ca_add_event ( DBR_FLOAT, 
                chan, nUpdatesTester, &eventCount, &id );
    SEVCHK (status, 0);
    ca_flush_io ();
    epicsThreadSleep ( 0.1 );
    ca_poll ();
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "-" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    verify ( eventCount > 0 );
    status = ca_clear_event ( id );
    SEVCHK (status, 0);

    showProgressEnd ( interestLevel );
}

void accessRightsStateChange ( struct access_rights_handler_args args )
{
    appChan *pChan = (appChan *) ca_puser ( args.chid );

    verify ( pChan->channel == args.chid );
    verify ( args.ar.read_access == ca_read_access ( args.chid ) );
    verify ( args.ar.write_access == ca_write_access ( args.chid ) );
    accessUpdateCount++;
    pChan->accessUpdateCount++;
}

void getCallbackStateChange ( struct event_handler_args args )
{
    appChan *pChan = (appChan *) args.usr;

    verify ( pChan->channel == args.chid );
    verify ( pChan->connected );
    if ( args.status != ECA_NORMAL ) {
        printf ( "getCallbackStateChange abnormal status was \"%s\"\n", 
            ca_message ( args.status ) ); 
        verify ( args.status == ECA_NORMAL );
    }

    getCallbackCount++;
    pChan->getCallbackCount++;
}

void connectionStateChange ( struct connection_handler_args args )
{
    int status;

    appChan *pChan = (appChan *) ca_puser ( args.chid );

    verify ( pChan->channel == args.chid );

    if ( args.op == CA_OP_CONN_UP ) {
        if ( pChan->accessRightsHandlerInstalled ) {
            verify ( pChan->accessUpdateCount > 0u );
        }
        verify ( ! pChan->connected );
        pChan->connected = 1;
        status = ca_get_callback ( DBR_STS_STRING, args.chid, getCallbackStateChange, pChan );
        SEVCHK (status, 0);
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        verify ( pChan->connected );
        pChan->connected = 0u;
        verify ( ! ca_read_access ( args.chid ) );
        verify ( ! ca_write_access ( args.chid ) );
    }
    else {
        verify ( 0 );
    }
    pChan->connectionUpdateCount++;
    connectionUpdateCount++;
}

void subscriptionStateChange ( struct event_handler_args args )
{
    struct dbr_sts_string * pdbrgs = ( struct dbr_sts_string * ) args.dbr;
    appChan *pChan = (appChan *) args.usr;

    verify ( args.status == ECA_NORMAL );
    verify ( pChan->channel == args.chid );
    verify ( pChan->connected );
    verify ( args.type == DBR_STS_STRING );
    verify ( strlen ( pdbrgs->value ) <= MAX_STRING_SIZE );
    pChan->subscriptionUpdateCount++;
    subscriptionUpdateCount++;
}

void noopSubscriptionStateChange ( struct event_handler_args args )
{
    if ( args.status != ECA_NORMAL ) {
        printf ( "noopSubscriptionStateChange: subscription update failed for \"%s\" because \"%s\"", 
            ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

/*
 * verifyConnectionHandlerConnect ()
 *
 * 1) verify that connection handler runs during connect
 *
 * 2) verify that access rights handler runs during connect
 *
 * 3) verify that get call back runs from connection handler
 *    (and that they are not required to flush in the connection handler)
 *
 * 4) verify that first event callback arrives after connect
 *
 * 5) verify subscription can be cleared before channel is cleared
 *
 * 6) verify subscription can be cleared by clearing the channel
 *
 * 7) verify that a nill access rights handler can be installed
 */
void verifyConnectionHandlerConnect ( appChan *pChans, unsigned chanCount, 
                        unsigned repetitionCount, unsigned interestLevel )
{
    int status;
    unsigned i, j;

    showProgressBegin ( "verifyConnectionHandlerConnect", interestLevel );

    for ( i = 0; i < repetitionCount; i++ ) {
        subscriptionUpdateCount = 0u;
        accessUpdateCount = 0u;
        connectionUpdateCount = 0u;
        getCallbackCount = 0u;

        for ( j = 0u; j < chanCount; j++ ) {

            pChans[j].subscriptionUpdateCount = 0u;
            pChans[j].accessUpdateCount = 0u;
            pChans[j].accessRightsHandlerInstalled = 0;
            pChans[j].connectionUpdateCount = 0u;
            pChans[j].getCallbackCount = 0u;
            pChans[j].connected = 0u;

            status = ca_search_and_connect ( pChans[j].name,
                &pChans[j].channel, connectionStateChange, &pChans[j] );
            SEVCHK ( status, NULL );

            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
            SEVCHK ( status, NULL );
            pChans[j].accessRightsHandlerInstalled = 1;

            status = ca_add_event ( DBR_STS_STRING, pChans[j].channel,
                    subscriptionStateChange, &pChans[j], &pChans[j].subscription );
            SEVCHK ( status, NULL );

            verify ( ca_test_io () == ECA_IODONE );
        }

        ca_flush_io ();

        showProgress ( interestLevel );

        while ( connectionUpdateCount < chanCount || 
            getCallbackCount < chanCount ) {
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }

        for ( j = 0u; j < chanCount; j++ ) {
            verify ( pChans[j].getCallbackCount == 1u);
            verify ( pChans[j].connectionUpdateCount > 0 );
            if ( pChans[j].connectionUpdateCount > 1u ) {
                printf ("Unusual connection activity count = %u on channel %s?\n",
                    pChans[j].connectionUpdateCount, pChans[j].name );
            }
            verify ( pChans[j].accessUpdateCount > 0 );
            if ( pChans[j].accessUpdateCount > 1u ) {
                printf ("Unusual access rights activity count = %u on channel %s?\n",
                    pChans[j].connectionUpdateCount, pChans[j].name );
            }
        }

        ca_self_test ();

        showProgress ( interestLevel );

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_clear_event ( pChans[j].subscription );
            SEVCHK ( status, NULL );
        }

        ca_self_test ();

        showProgress ( interestLevel );
        
        for ( j = 0u; j < chanCount; j++ ) {
            status = ca_replace_access_rights_event (
                    pChans[j].channel, 0 );
            SEVCHK ( status, NULL );
        }

        for ( j = 0u; j < chanCount; j++ ) {
            status = ca_clear_channel ( pChans[j].channel );
            SEVCHK ( status, NULL );
        }

        ca_self_test ();

        showProgress ( interestLevel );

    }
    showProgressEnd ( interestLevel );
}

/*
 * verifyBlockingConnect ()
 *
 * 1) verify that we dont print a disconnect message when 
 * we delete the last channel
 *
 * 2) verify that we delete the connection to the IOC
 * when the last channel is deleted.
 *
 * 3) verify channel connection state variables
 *
 * 4) verify ca_test_io () and ca_pend_io () work with 
 * channels w/o connection handlers
 *
 * 5) verify that the pending IO count is properly
 * maintained when we are add/removing a connection
 * handler
 *
 * 6) verify that the pending IO count goes to zero
 * if the channel is deleted before it connects.
 */
void verifyBlockingConnect ( appChan *pChans, unsigned chanCount, 
                            unsigned repetitionCount, unsigned interestLevel )
{
    int status;
    unsigned i, j;
    unsigned connections;
    unsigned backgroundConnCount = ca_get_ioc_connection_count ();

    showProgressBegin ( "verifyBlockingConnect", interestLevel );

    i = 0;
    while ( backgroundConnCount > 1u ) {
        backgroundConnCount = ca_get_ioc_connection_count ();
        verify ( i++ < 10 );
        printf ( "Z" );
        fflush ( stdout );
        epicsThreadSleep ( 1.0 );
    }

    for ( i = 0; i < repetitionCount; i++ ) {

        for ( j = 0u; j < chanCount; j++ ) {
            pChans[j].subscriptionUpdateCount = 0u;
            pChans[j].accessUpdateCount = 0u;
            pChans[j].accessRightsHandlerInstalled = 0;
            pChans[j].connectionUpdateCount = 0u;
            pChans[j].getCallbackCount = 0u;
            pChans[j].connected = 0u;

            status = ca_search_and_connect ( pChans[j].name, &pChans[j].channel, NULL, &pChans[j] );
            SEVCHK ( status, NULL );

            if ( ca_state ( pChans[j].channel ) == cs_conn ) {
                verify ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
            }
            else {
                verify ( INVALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
                verify ( ca_test_io () == ECA_IOINPROGRESS );
            }
            
            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
            SEVCHK ( status, NULL );
            pChans[j].accessRightsHandlerInstalled = 1;
        }

        showProgress ( interestLevel );

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_change_connection_event ( pChans[j].channel, connectionStateChange );
            SEVCHK ( status, NULL );
        }

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_change_connection_event ( pChans[j].channel, NULL );
            SEVCHK ( status, NULL );
        }

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_change_connection_event ( pChans[j].channel, connectionStateChange );
            SEVCHK ( status, NULL );
        }

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_change_connection_event ( pChans[j].channel, NULL );
            SEVCHK ( status, NULL );
        }

        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, NULL );

        ca_self_test ();

        showProgress ( interestLevel );

        verify ( ca_test_io () == ECA_IODONE );

        connections = ca_get_ioc_connection_count ();
        verify ( connections == backgroundConnCount );

        for ( j = 0u; j < chanCount; j++ ) {
            verify ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
            verify ( ca_state ( pChans[j].channel ) == cs_conn );
            SEVCHK ( ca_clear_channel ( pChans[j].channel ), NULL );
        }

        ca_self_test ();

        ca_flush_io ();

        showProgress ( interestLevel );

        /*
         * verify that connections to IOC's that are 
         * not in use are dropped
         */
        if ( ca_get_ioc_connection_count () != backgroundConnCount ) {
            epicsThreadSleep ( 0.1 );
            ca_poll ();
            j=0;
            while ( ca_get_ioc_connection_count () != backgroundConnCount ) {
                epicsThreadSleep ( 0.1 );
                ca_poll (); /* emulate typical GUI */
                verify ( ++j < 100 );
            }
        }
        showProgress ( interestLevel );
    }

    for ( j = 0u; j < chanCount; j++ ) {
        status = ca_search ( pChans[j].name, &pChans[j].channel );
        SEVCHK ( status, NULL );
    }

    for ( j = 0u; j < chanCount; j++ ) {
        status = ca_clear_channel ( pChans[j].channel );
        SEVCHK ( status, NULL );
    }

    verify ( ca_test_io () == ECA_IODONE );

    /*
     * verify ca_pend_io() does not see old search requests
     * (that did not specify a connection handler)
     */
    status = ca_search_and_connect ( pChans[0].name, &pChans[0].channel, NULL, NULL);
    SEVCHK ( status, NULL );

    if ( ca_state ( pChans[0].channel ) == cs_never_conn ) {
        /* force an early timeout */
        status = ca_pend_io ( 1e-16 );
        if ( status == ECA_TIMEOUT ) {
            /*
             * we end up here if the channel isnt on the same host
             */
            epicsThreadSleep ( 0.1 );
            ca_poll ();
            if ( ca_state( pChans[0].channel ) != cs_conn ) {
                while ( ca_state ( pChans[0].channel ) != cs_conn ) {
                    epicsThreadSleep ( 0.1 );
                    ca_poll (); /* emulate typical GUI */
                }
            }

            status = ca_search_and_connect ( pChans[1].name, &pChans[1].channel, NULL, NULL  );
            SEVCHK ( status, NULL );
            status = ca_pend_io ( 1e-16 );
            if ( status != ECA_TIMEOUT ) {
                verify ( ca_state ( pChans[1].channel ) == cs_conn );
            }
            status = ca_clear_channel ( pChans[1].channel );
            SEVCHK ( status, NULL );
        }
        else {
            verify ( ca_state( pChans[0].channel ) == cs_conn );
        }
    }
    status = ca_clear_channel( pChans[0].channel );
    SEVCHK ( status, NULL );

    ca_self_test ();

    showProgressEnd ( interestLevel );
}

/*
 * 1) verify that use of NULL evid does not cause problems
 * 2) verify clear before connect
 */
void verifyClear ( appChan *pChans, unsigned interestLevel )
{
    int status;

    showProgressBegin ( "verifyClear", interestLevel );

    /*
     * verify channel clear before connect
     */
    status = ca_search ( pChans[0].name, &pChans[0].channel );
    SEVCHK ( status, NULL );

    status = ca_clear_channel ( pChans[0].channel );
    SEVCHK ( status, NULL );

    /*
     * verify subscription clear before connect
     * and verify that NULL evid does not cause failure 
     */
    status = ca_search ( pChans[0].name, &pChans[0].channel );
    SEVCHK ( status, NULL );

    SEVCHK ( status, NULL );
    status = ca_add_event ( DBR_GR_DOUBLE, 
            pChans[0].channel, noopSubscriptionStateChange, NULL, NULL );
    SEVCHK ( status, NULL );

    status = ca_clear_channel ( pChans[0].channel );
    SEVCHK ( status, NULL );
    showProgressEnd ( interestLevel );
}

/*
 * grEnumTest
 */
void grEnumTest ( chid chan, unsigned interestLevel )
{
    struct dbr_gr_enum ge;
    unsigned count;
    int status;
    unsigned i;

    showProgressBegin ( "grEnumTest", interestLevel );

    ge.no_str = -1;

    status = ca_get (DBR_GR_ENUM, chan, &ge);
    SEVCHK (status, "DBR_GR_ENUM ca_get()");

    status = ca_pend_io (timeoutToPendIO);
    verify (status == ECA_NORMAL);

    verify ( ge.no_str >= 0 && ge.no_str < NELEMENTS(ge.strs) );
    if ( ge.no_str > 0 ) {
        printf ("Enum state str = {");
        count = (unsigned) ge.no_str;
        for (i=0; i<count; i++) {
            printf ("\"%s\" ", ge.strs[i]);
        }
        printf ("}");
    }
    showProgressEnd ( interestLevel );
}

/*
 * ctrlDoubleTest
 */
void ctrlDoubleTest ( chid chan, unsigned interestLevel )
{
    struct dbr_ctrl_double *pCtrlDbl;
    dbr_double_t *pDbl;
    unsigned nElem = ca_element_count(chan);
    double slice = 3.14159 / nElem;
    size_t size;
    int status;
    unsigned i;


    if (!ca_write_access(chan)) {
        printf ("skipped ctrl dbl test - no write access\n");
        return;
    }

    if (dbr_value_class[ca_field_type(chan)]!=dbr_class_float) {
        printf ("skipped ctrl dbl test - not an analog type\n");
        return;
    }

    showProgressBegin ( "ctrlDoubleTest", interestLevel );

    size = sizeof (*pDbl)*ca_element_count(chan);
    pDbl = malloc (size);
    verify (pDbl!=NULL);

    /*
     * initialize the array
     */
    for (i=0; i<nElem; i++) {
        pDbl[i] = sin (i*slice);
    }

    /*
     * write the array to the PV
     */
    status = ca_array_put (DBR_DOUBLE,
                    ca_element_count(chan),
                    chan, pDbl);
    SEVCHK (status, "ctrlDoubleTest, ca_array_put");

    size = dbr_size_n(DBR_CTRL_DOUBLE, ca_element_count(chan));
    pCtrlDbl = (struct dbr_ctrl_double *) malloc (size); 
    verify (pCtrlDbl!=NULL);

    /*
     * read the array from the PV
     */
    status = ca_array_get (DBR_CTRL_DOUBLE,
                    ca_element_count(chan),
                    chan, pCtrlDbl);
    SEVCHK (status, "ctrlDoubleTest, ca_array_get");
    status = ca_pend_io ( timeoutToPendIO );
    verify (status==ECA_NORMAL);

    /*
     * verify the result
     */
    for (i=0; i<nElem; i++) {
        double diff = pDbl[i] - sin (i*slice);
        verify (fabs(diff) < DBL_EPSILON*4);
    }

    free (pCtrlDbl);
    free (pDbl);
    showProgressEnd ( interestLevel );
}

/*
 * ca_pend_io() must block
 */
void verifyBlockInPendIO ( chid chan, unsigned interestLevel  )
{
    int status;

    if ( ca_read_access (chan) ) {
        dbr_long_t req;
        dbr_long_t resp;

        showProgressBegin ( "verifyBlockInPendIO", interestLevel );
        req = 0;
        resp = -100;
        SEVCHK ( ca_put (DBR_LONG, chan, &req), NULL );
        SEVCHK ( ca_get (DBR_LONG, chan, &resp), NULL );
        status = ca_pend_io (1.0e-12);
        if ( status == ECA_NORMAL ) {
            if ( resp != req ) {
                printf (
    "short tmo get block test failed - val written %d\n", req );
                printf (
    "short tmo tmo get block test failed - val read %d\n", resp );
                verify ( 0 );
            }
        }
        else if ( resp != -100 ) {
            printf ( "CA didnt block for get to return?\n" );
        }
            
        req = 1;
        resp = -100;
        SEVCHK ( ca_put (DBR_LONG, chan, &req), NULL );
        SEVCHK ( ca_get (DBR_LONG, chan, &resp), NULL );
        SEVCHK ( ca_pend_io (timeoutToPendIO) , NULL );
        if ( resp != req ) {
            printf (
    "long tmo get block test failed - val written %d\n", req);
            printf (
    "long tmo get block test failed - val read %d\n", resp);
            verify (0);
        }
        showProgressEnd ( interestLevel );
    }
    else {
        printf ("skipped pend IO block test - no read access\n");
    }
}

/*
 * floatTest ()
 */
void floatTest ( chid chan, dbr_float_t beginValue, dbr_float_t increment,
            dbr_float_t epsilon, unsigned iterations )
{
    unsigned i;
    dbr_float_t fval;
    dbr_float_t fretval;
    int status;

    fval = beginValue;
    for ( i=0; i < iterations; i++ ) {
        fretval = FLT_MAX;
        status = ca_put ( DBR_FLOAT, chan, &fval );
        SEVCHK ( status, NULL );
        status = ca_get ( DBR_FLOAT, chan, &fretval );
        SEVCHK ( status, NULL );
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK (status, NULL);
        if ( fabs ( fval - fretval ) > epsilon ) {
            printf ( "float test failed val written %f\n", fval );
            printf ( "float test failed val read %f\n", fretval );
            verify (0);
        }
        fval += increment;
    }
}

/*
 * doubleTest ()
 */
void doubleTest ( chid chan, dbr_double_t beginValue, 
    dbr_double_t  increment, dbr_double_t epsilon,
    unsigned iterations)
{
    unsigned i;
    dbr_double_t fval;
    dbr_double_t fretval;
    int status;

    fval = beginValue;
    for ( i = 0; i < iterations; i++ ) {
        fretval = DBL_MAX;
        status = ca_put ( DBR_DOUBLE, chan, &fval );
        SEVCHK ( status, NULL );
        status = ca_get ( DBR_DOUBLE, chan, &fretval );
        SEVCHK ( status, NULL );
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, NULL );
        if ( fabs ( fval - fretval ) > epsilon ) {
            printf ( "double test failed val written %f\n", fval );
            printf ( "double test failed val read %f\n", fretval );
            verify ( 0 );
        }
        fval += increment;
    }
}

/*
 * Verify that we can write and then read back
 * the same analog value
 */
void verifyAnalogIO ( chid chan, int dataType, double min, double max, 
               int minExp, int maxExp, double epsilon, unsigned interestLevel )
{
    int i;
    double incr;
    double epsil;
    double base;
    unsigned iter;


    if ( ! ca_write_access ( chan ) ) {
        printf ("skipped analog test - no write access\n");
        return;
    }

    if ( dbr_value_class[ca_field_type ( chan )] != dbr_class_float ) {
        printf ("skipped analog test - not an analog type\n");
        return;
    }

    showProgressBegin ( "verifyAnalogIO", interestLevel );

    epsil = epsilon * 4.0;
    base = min;
    for ( i = minExp; i <= maxExp; i += maxExp / 10 ) {
        incr = ldexp ( 0.5, i );
        if ( fabs (incr) > max /10.0 ) {
            iter = ( unsigned ) ( max / fabs (incr) );
        }
        else {
            iter = 10u;
        }
        if ( dataType == DBR_FLOAT ) {
            floatTest ( chan, (dbr_float_t) base, (dbr_float_t) incr, 
                (dbr_float_t) epsil, iter );
        }
        else if (dataType == DBR_DOUBLE ) {
            doubleTest ( chan, (dbr_double_t) base, (dbr_double_t) incr, 
                (dbr_double_t) epsil, iter );
        }
        else {
            verify ( 0 );
        }
    }
    base = max;
    for ( i = minExp; i <= maxExp; i += maxExp / 10 ) {
        incr = - ldexp ( 0.5, i );
        if ( fabs (incr) > max / 10.0 ) {
            iter = (unsigned) ( max / fabs (incr) );
        }
        else {
            iter = 10u;
        }
        if ( dataType == DBR_FLOAT ) {
            floatTest ( chan, (dbr_float_t) base, (dbr_float_t) incr, 
                (dbr_float_t) epsil, iter );
        }
        else if (dataType == DBR_DOUBLE ) {
            doubleTest ( chan, (dbr_double_t) base, (dbr_double_t) incr, 
                (dbr_double_t) epsil, iter );
         }
        else {
            verify ( 0 );
        }
    }
    base = - max;
    for ( i = minExp; i <= maxExp; i += maxExp / 10 ) {
        incr = ldexp ( 0.5, i );
        if ( fabs (incr) > max / 10.0 ) {
            iter = (unsigned) ( max / fabs ( incr ) );
        }
        else {
            iter = 10l;
        }
        if ( dataType == DBR_FLOAT ) {
            floatTest ( chan, (dbr_float_t) base, (dbr_float_t) incr, 
                (dbr_float_t) epsil, iter );
        }
        else if (dataType == DBR_DOUBLE ) {
            doubleTest ( chan, (dbr_double_t) base, (dbr_double_t) incr, 
                (dbr_double_t) epsil, iter );
         }
        else {
            verify ( 0 );
        }
    }
    showProgressEnd ( interestLevel );
}

/*
 * Verify that we can write and then read back
 * the same DBR_LONG value
 */
void verifyLongIO ( chid chan, unsigned interestLevel  )
{
    int status;

    dbr_long_t iter, rdbk, incr;
    struct dbr_ctrl_long cl;

    if ( ca_write_access ( chan ) ) {
        return;
    }

    status = ca_get ( DBR_CTRL_LONG, chan, &cl );
    SEVCHK ( status, "control long fetch failed\n" );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "control long pend failed\n" );

    incr = ( cl.upper_ctrl_limit - cl.lower_ctrl_limit );
    if ( incr >= 1 ) {
        showProgressBegin ( "verifyLongIO", interestLevel );
        incr /= 1000;
        if ( incr == 0 ) {
            incr = 1;
        }
        for ( iter = cl.lower_ctrl_limit; 
            iter <= cl.upper_ctrl_limit; iter+=incr ) {

            ca_put ( DBR_LONG, chan, &iter );
            ca_get ( DBR_LONG, chan, &rdbk );
            status = ca_pend_io ( timeoutToPendIO );
            SEVCHK ( status, "get pend failed\n" );
            verify ( iter == rdbk );
        }
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "strange limits configured for channel \"%s\"\n", ca_name (chan) );
    }
}

/*
 * Verify that we can write and then read back
 * the same DBR_SHORT value
 */
void verifyShortIO ( chid chan, unsigned interestLevel  )
{
    int status;

    dbr_short_t iter, rdbk, incr;
    struct dbr_ctrl_short cl;

    if ( ca_write_access ( chan ) ) {
        return;
    }

    status = ca_get ( DBR_CTRL_SHORT, chan, &cl );
    SEVCHK ( status, "control short fetch failed\n" );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "control short pend failed\n" );

    incr = (dbr_short_t) ( cl.upper_ctrl_limit - cl.lower_ctrl_limit );
    if ( incr >= 1 ) {
        showProgressBegin ( "verifyShortIO", interestLevel );

        incr /= 1000;
        if ( incr == 0 ) {
            incr = 1;
        }
        for ( iter = (dbr_short_t) cl.lower_ctrl_limit; 
            iter <= (dbr_short_t) cl.upper_ctrl_limit; 
            iter = (dbr_short_t) (iter + incr) ) {

            ca_put ( DBR_SHORT, chan, &iter );
            ca_get ( DBR_SHORT, chan, &rdbk );
            status = ca_pend_io ( timeoutToPendIO );
            SEVCHK ( status, "get pend failed\n" );
            verify ( iter == rdbk );
        }
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "Strange limits configured for channel \"%s\"\n", ca_name (chan) );
    }
}

void verifyHighThroughputRead ( chid chan, unsigned interestLevel )
{
    int status;
    unsigned i;

    /*
     * verify we dont jam up on many uninterrupted
     * solicitations
     */
    if ( ca_read_access (chan) ) {
        dbr_float_t temp;
        showProgressBegin ( "verifyHighThroughputRead", interestLevel );
        for ( i=0; i<10000; i++ ) {
            status = ca_get ( DBR_FLOAT, chan, &temp );
            SEVCHK ( status ,NULL );
        }
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, NULL );
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "Skipped highthroughput read test - no read access\n" );
    }
}

void verifyHighThroughputWrite ( chid chan, unsigned interestLevel  ) 
{
    int status;
    unsigned i;

    if (ca_write_access ( chan ) ) {
        showProgressBegin ( "verifyHighThroughputWrite", interestLevel );
        for ( i=0; i<10000; i++ ) {
            dbr_double_t fval = 3.3;
            status = ca_put ( DBR_DOUBLE, chan, &fval );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_pend_io (timeoutToPendIO), NULL );
        showProgressEnd ( interestLevel );
    }
    else{
        printf("Skipped multiple put test - no write access\n");
    }
}

/*
 * verify we dont jam up on many uninterrupted
 * get callback requests
 */
void verifyHighThroughputReadCallback ( chid chan, unsigned interestLevel ) 
{
    unsigned i;
    int status;

    if ( ca_read_access ( chan ) ) { 
        unsigned count = 0u;
        showProgressBegin ( "verifyHighThroughputReadCallback", interestLevel );
        for ( i=0; i<10000; i++ ) {
            status = ca_array_get_callback (
                    DBR_FLOAT, 1, chan, nUpdatesTester, &count );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_flush_io (), NULL );
        while ( count < 10000u ) {
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "Skipped multiple get cb test - no read access\n" );
    }
}

/*
 * verify we dont jam up on many uninterrupted
 * put callback request
 */
void verifyHighThroughputWriteCallback ( chid chan, unsigned interestLevel )
{
    unsigned i;
    int status;

    if ( ca_write_access (chan) && ca_v42_ok (chan) ) {
        unsigned count = 0u;
        dbr_double_t dval;
        showProgressBegin ( "verifyHighThroughputWriteCallback", interestLevel );
        for ( i=0; i<10000; i++ ) {
            dval = i + 1;
            status = ca_array_put_callback (
                    DBR_DOUBLE, 1, chan, &dval,
                    nUpdatesTester, &count );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_flush_io (), NULL );
        dval = 0.0;
        status = ca_get ( DBR_DOUBLE, chan, &dval );
        SEVCHK ( status, 
            "verifyHighThroughputWriteCallback, verification get" );
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, 
            "verifyHighThroughputWriteCallback, verification get pend" );
        verify ( dval == i );
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "Skipped multiple put cb test - no write access\n" );
    }
}

void verifyBadString ( chid chan, unsigned interestLevel  )
{
    int status;

    /*
     * verify that we detect that a large string has been written
     */
    if ( ca_write_access (chan) ) {
        dbr_string_t    stimStr;
        dbr_string_t    respStr;
        showProgressBegin ( "verifyBadString", interestLevel );
        memset (stimStr, 'a', sizeof (stimStr) );
        status = ca_array_put ( DBR_STRING, 1u, chan, stimStr );
        verify ( status != ECA_NORMAL );
        sprintf ( stimStr, "%u", 8u );
        status = ca_array_put ( DBR_STRING, 1u, chan, stimStr );
        verify ( status == ECA_NORMAL );
        status = ca_array_get ( DBR_STRING, 1u, chan, respStr );
        verify ( status == ECA_NORMAL );
        status = ca_pend_io ( timeoutToPendIO );
        verify ( status == ECA_NORMAL );
        if ( strcmp ( stimStr, respStr ) ) {
            printf (
    "Test fails if stim \"%s\" isnt roughly equiv to resp \"%s\"\n",
                stimStr, respStr);
        }
        showProgressEnd ( interestLevel );
    }
    else {
        printf ( "Skipped bad string test - no write access\n" );
    }
}

/*
 * multiple_sg_requests()
 */
void multiple_sg_requests ( chid chix, CA_SYNC_GID gid )
{
    int status;
    unsigned i;
    static dbr_float_t fvalput = 3.3F;
    static dbr_float_t fvalget;

    for ( i=0; i < 1000; i++ ) {
        if ( ca_write_access (chix) ){
            status = ca_sg_array_put ( gid, DBR_FLOAT, 1,
                        chix, &fvalput);
            SEVCHK ( status, NULL );
        }

        if ( ca_read_access (chix) ) {
            status = ca_sg_array_get ( gid, DBR_FLOAT, 1,
                    chix, &fvalget);
            SEVCHK ( status, NULL );
        }
    }
}

/*
 * test_sync_groups()
 */
void test_sync_groups ( chid chan, unsigned interestLevel  )
{
    int status;
    CA_SYNC_GID gid1=0;
    CA_SYNC_GID gid2=0;

    if ( ! ca_v42_ok ( chan ) ) {
        printf ( "skipping sync group test - server is on wrong version\n" );
    }

    showProgressBegin ( "test_sync_groups", interestLevel );

    status = ca_sg_create ( &gid1 );
    SEVCHK ( status, NULL );

    multiple_sg_requests ( chan, gid1 );
    status = ca_sg_reset ( gid1 );
    SEVCHK ( status, NULL );

    status = ca_sg_create ( &gid2 );
    SEVCHK ( status, NULL );

    multiple_sg_requests ( chan, gid2 );
    multiple_sg_requests ( chan, gid1 );
    status = ca_sg_test ( gid2 );
    SEVCHK ( status, "SYNC GRP2" );
    status = ca_sg_test ( gid1 );
    SEVCHK ( status, "SYNC GRP1" );
    status = ca_sg_block ( gid1, 500.0 );
    SEVCHK ( status, "SYNC GRP1" );
    status = ca_sg_block ( gid2, 500.0 );
    SEVCHK ( status, "SYNC GRP2" );

    status = ca_sg_delete ( gid2 );
    SEVCHK (status, NULL);
    status = ca_sg_create ( &gid2 );
    SEVCHK (status, NULL);

    multiple_sg_requests ( chan, gid1 );
    multiple_sg_requests ( chan, gid2 );
    status = ca_sg_block ( gid1, 500.0 );
    SEVCHK ( status, "SYNC GRP1" );
    status = ca_sg_block ( gid2, 500.0 );
    SEVCHK ( status, "SYNC GRP2" );
    status = ca_sg_delete  ( gid1 );
    SEVCHK ( status, NULL );
    status = ca_sg_delete ( gid2 );
    SEVCHK ( status, NULL );

    showProgressEnd ( interestLevel );
}

#define multiSubscrDestroyNoLateCallbackEventCount 500

struct MultiSubscrDestroyNoLateCallbackEventData {
    evid m_id;
    size_t m_nCallback;
    int m_callbackIsOk;
    struct MultiSubscrDestroyNoLateCallbackTestData * m_pTestData;
};

struct MultiSubscrDestroyNoLateCallbackTestData {
    const char * m_pChanName;
    chid m_chan;
    epicsMutexId m_mutex;
    epicsEventId m_testDoneEvent;
    unsigned m_interestLevel;
    struct MultiSubscrDestroyNoLateCallbackEventData
        m_eventData [multiSubscrDestroyNoLateCallbackEventCount];
};

static void noLateCallbackDetect ( struct event_handler_args args )
{
    int callbackIsOk;
    struct MultiSubscrDestroyNoLateCallbackEventData * const pEventData = args.usr;
    epicsMutexLockStatus lockStatus = epicsMutexLock ( pEventData->m_pTestData->m_mutex );
    callbackIsOk = pEventData->m_callbackIsOk;
    pEventData->m_nCallback++;
    epicsMutexUnlock ( pEventData->m_pTestData->m_mutex );
    verify ( lockStatus == epicsMutexLockOK );
    verify ( callbackIsOk );
}

static void multiSubscrDestroyNoLateCallbackThread ( void * pParm ) 
{
    struct MultiSubscrDestroyNoLateCallbackTestData * const pTestData = 
              ( struct MultiSubscrDestroyNoLateCallbackTestData * ) pParm;
    unsigned i, j;
    int status;

    status = ca_context_create ( ca_enable_preemptive_callback );
    verify ( status == ECA_NORMAL );

    status = ca_create_channel ( pTestData->m_pChanName, 0, 0,
                           CA_PRIORITY_DEFAULT, &pTestData->m_chan );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "multiSubscrDestroyLateNoCallbackTest: channel connect failed" );
    verify ( status == ECA_NORMAL );

    /*
     * create a set of subscriptions
     */
    for ( i=0; i < 10000; i++ ) {
        unsigned int priorityOfTestThread;
        for ( j=0; j < multiSubscrDestroyNoLateCallbackEventCount; j++ ) {
            epicsMutexLockStatus lockStatus = epicsMutexLock ( pTestData->m_mutex );
            verify ( lockStatus == epicsMutexLockOK );
            pTestData->m_eventData[j].m_nCallback = 0;
            pTestData->m_eventData[j].m_callbackIsOk = TRUE;
            pTestData->m_eventData[j].m_pTestData = pTestData;
            epicsMutexUnlock ( pTestData->m_mutex );
            SEVCHK ( ca_add_event ( DBR_GR_FLOAT, pTestData->m_chan, noLateCallbackDetect,
                        &pTestData->m_eventData[j], &pTestData->m_eventData[j].m_id ) , NULL );
        }
        SEVCHK ( ca_flush_io(), NULL ); 

        /*
         * raise the priority of the current thread hoping to improve our 
         * likelyhood of detecting a bug
         */
        priorityOfTestThread = epicsThreadGetPrioritySelf ();
        epicsThreadSetPriority ( epicsThreadGetIdSelf(), epicsThreadPriorityHigh );


        /*
         * wait for the first subscription update to arrive
         */
        {
            epicsMutexLockStatus lockStatus = epicsMutexLock ( pTestData->m_mutex );
            verify ( lockStatus == epicsMutexLockOK );
            while ( pTestData->m_eventData[0].m_nCallback == 0 ) {
                epicsMutexUnlock ( pTestData->m_mutex );
                epicsThreadSleep ( 50e-6 );
                lockStatus = epicsMutexLock ( pTestData->m_mutex );
                verify ( lockStatus == epicsMutexLockOK );
            }
            epicsMutexUnlock ( pTestData->m_mutex );
        }
        /*
         * try to destroy all of the subscriptions at precisely the same time that
         * their first callbacks are running
         */
        for ( j=0; j < multiSubscrDestroyNoLateCallbackEventCount; j++ ) {
            epicsMutexLockStatus lockStatus;
            SEVCHK ( ca_clear_event ( pTestData->m_eventData[j].m_id ) , NULL );
            lockStatus = epicsMutexLock ( pTestData->m_mutex );
            verify ( lockStatus == epicsMutexLockOK );
            pTestData->m_eventData[j].m_callbackIsOk = FALSE;
            epicsMutexUnlock ( pTestData->m_mutex );
        }
        /*
         * return to the original priority
         */
        epicsThreadSetPriority ( epicsThreadGetIdSelf(), priorityOfTestThread );

        if ( i % 1000 == 0 ) {
            showProgress ( pTestData->m_interestLevel );
        }
    }

    SEVCHK ( ca_clear_channel ( pTestData->m_chan ), NULL );

    ca_context_destroy ();

    epicsEventSignal ( pTestData->m_testDoneEvent );
}

/*
 * verify that, in a preemtive callback mode client, a subscription callback never 
 * comes after the subscription is destroyed
 */
static void multiSubscrDestroyNoLateCallbackTest ( const char *pName, unsigned interestLevel )
{
    struct MultiSubscrDestroyNoLateCallbackTestData * pTestData;

    showProgressBegin ( "multiSubscrDestroyNoLateCallbackTest", interestLevel );

    pTestData = calloc ( 1u, sizeof ( struct MultiSubscrDestroyNoLateCallbackTestData ) );
    verify ( pTestData );
    pTestData->m_mutex = epicsMutexMustCreate (); 
    pTestData->m_testDoneEvent = epicsEventMustCreate ( epicsEventEmpty );
    pTestData->m_pChanName = pName;
    pTestData->m_interestLevel = interestLevel;
    epicsThreadMustCreate (
                      "multiSubscrDestroyNoLateCallbackTest",
                      epicsThreadPriorityLow, 
                      epicsThreadGetStackSize ( epicsThreadStackMedium ),
                      multiSubscrDestroyNoLateCallbackThread,
                      pTestData );

    /*
     * wait for test to complete
     */
    epicsEventMustWait ( pTestData->m_testDoneEvent );

    /*
     * cleanup
     */
    epicsMutexDestroy ( pTestData->m_mutex );
    epicsEventDestroy ( pTestData->m_testDoneEvent );
    free ( pTestData );

    showProgressEnd ( interestLevel );
}

/*
 * multiSubscriptionDeleteTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 * 3) attempt to delete monitors while many monitors 
 *      are running
 */
void multiSubscriptionDeleteTest ( chid chan, unsigned interestLevel  )
{
    unsigned count = 0u;
    evid mid[1000];
    dbr_float_t temp, getResp;
    unsigned i;
    
    showProgressBegin ( "multiSubscriptionDeleteTest", interestLevel );

    for ( i=0; i < NELEMENTS (mid); i++ ) {
        SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, noopSubscriptionStateChange,
            &count, &mid[i]) , NULL );
    }

    /*
     * force all of the monitors subscription requests to
     * complete
     *
     * NOTE: this hopefully demonstrates that when the
     * server is very busy with monitors the client 
     * is still able to punch through with a request.
     */
    SEVCHK ( ca_get ( DBR_FLOAT,chan,&getResp ), NULL );
    SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );

    showProgress ( interestLevel );

    /*
     * attempt to generate heavy event traffic before initiating
     * the monitor delete
     */  
    if ( ca_write_access (chan) ) {
        for ( i=0; i < NELEMENTS (mid); i++ ) {
            temp = (float) i;
            SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
        }
    }

    showProgress ( interestLevel );

    /*
     * without pausing begin deleting the event subscriptions 
     * while the queue is full
     *
     * continue attempting to generate heavy event traffic 
     * while deleting subscriptions with the hope that we will 
     * deleting an event at the instant that its callback is 
     * occurring
     */
    for ( i=0; i < NELEMENTS (mid); i++ ) {
        if ( ca_write_access (chan) ) {
            temp = (float) i;
            SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
        }
        SEVCHK ( ca_clear_event ( mid[i]), NULL );
    }

    showProgress ( interestLevel );

    /*
     * force all of the clear event requests to complete
     */
    SEVCHK ( ca_get (DBR_FLOAT,chan,&temp), NULL );
    SEVCHK ( ca_pend_io (timeoutToPendIO), NULL );

    showProgressEnd ( interestLevel );
} 


/*
 * singleSubscriptionDeleteTest
 *
 * verify that we dont fail when we repeatedly create 
 * and delete only one subscription with a high level of 
 * traffic on it
 */
void singleSubscriptionDeleteTest ( chid chan, unsigned interestLevel  )
{
    unsigned count = 0u;
    evid sid;
    dbr_float_t temp, getResp;
    unsigned i;
    
    showProgressBegin ( "singleSubscriptionDeleteTest", interestLevel );

    for ( i=0; i < 1000; i++ ){
        count = 0u;
        SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, noopSubscriptionStateChange,
            &count, &sid) , NULL );

        if ( i % 100 == 0 ) {
            showProgress ( interestLevel );
        }

        /*
         * force the subscription request to complete
         */
        SEVCHK ( ca_get ( DBR_FLOAT, chan, &getResp ), NULL );
        SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );

        /*
         * attempt to generate heavy event traffic before initiating
         * the monitor delete
         *
         * try to interrupt the recv thread while it is processing 
         * incoming subscription updates
         */  
        if ( ca_write_access (chan) ) {
            unsigned j = 0;
            while ( j < i ) {
                temp = (float) j++;
                SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), 
                    "singleSubscriptionDeleteTest - one of multiple puts" );
            }
            ca_flush_io ();
        }

        SEVCHK ( ca_clear_event ( sid ), NULL );
    }

    showProgressEnd ( interestLevel );
} 

/*
 * channelClearWithEventTrafficTest
 *
 * verify that we can delete a channel that has subcriptions
 * attached with heavy update traffic
 */
void channelClearWithEventTrafficTest ( const char *pName, unsigned interestLevel )
{
    unsigned count = 0u;
    evid sid;
    dbr_float_t temp, getResp;
    unsigned i;
    
    showProgressBegin ( "channelClearWithEventTrafficTest", interestLevel );

    for ( i=0; i < 1000; i++ ) {
        chid chan;

        int status = ca_create_channel ( pName, 0, 0, 
            CA_PRIORITY_DEFAULT, &chan );
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "channelClearWithEventTrafficTest: channel connect failed" );
        verify ( status == ECA_NORMAL );

        count = 0u;
        SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, noopSubscriptionStateChange,
            &count, &sid ) , NULL );

        /*
         * force the subscription request to complete
         */
        SEVCHK ( ca_get ( DBR_FLOAT, chan, &getResp ), NULL );
        SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );

        /*
         * attempt to generate heavy event traffic before initiating
         * the channel delete
         *
         * try to interrupt the recv thread while it is processing 
         * incoming subscription updates
         */  
        if ( ca_write_access (chan) ) {
            unsigned j = 0;
            while ( j < i ) {
                temp = (float) j++;
                SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
            }
            ca_flush_io ();
            epicsThreadSleep ( 0.001 );
        }

        SEVCHK ( ca_clear_channel ( chan ), NULL );
    }

    showProgressEnd ( interestLevel );
} 



evid globalEventID;

void selfDeleteEvent ( struct event_handler_args args )
{
    int status;
    status = ca_clear_event ( globalEventID );
    verify ( status == ECA_NORMAL ); 
}

void eventClearTest ( chid chan )
{
    int status;
    evid monix1, monix2, monix3;

    status = ca_add_event ( DBR_FLOAT, chan, noopSubscriptionStateChange, 
                NULL, &monix1 );
    SEVCHK ( status, NULL );

    status = ca_clear_event ( monix1 );
    SEVCHK ( status, NULL );

    status = ca_add_event ( DBR_FLOAT, chan, noopSubscriptionStateChange, 
                NULL, &monix1 );
    SEVCHK ( status, NULL );

    status = ca_add_event ( DBR_FLOAT, chan, noopSubscriptionStateChange, 
                NULL, &monix2);
    SEVCHK (status, NULL);

    status = ca_clear_event ( monix2 );
    SEVCHK ( status, NULL);

    status = ca_add_event ( DBR_FLOAT, chan, noopSubscriptionStateChange, 
                NULL, &monix2);
    SEVCHK ( status, NULL );

    status = ca_add_event ( DBR_FLOAT, chan, noopSubscriptionStateChange, 
                NULL, &monix3);
    SEVCHK ( status, NULL );

    status = ca_clear_event ( monix2 );
    SEVCHK ( status, NULL);

    status = ca_clear_event ( monix1 );
    SEVCHK ( status, NULL);

    status = ca_clear_event ( monix3 );
    SEVCHK ( status, NULL);

    status = ca_add_event ( DBR_FLOAT, chan, selfDeleteEvent, 
                0, &globalEventID );
    SEVCHK ( status, NULL );
}

unsigned acctstExceptionCount = 0u;
void acctstExceptionNotify ( struct exception_handler_args args )
{
    acctstExceptionCount++;
}

static unsigned arrayEventExceptionNotifyComplete = 0;
void arrayEventExceptionNotify ( struct event_handler_args args )
{
    if ( args.status == ECA_NORMAL ) {
        printf (
            "arrayEventExceptionNotify: expected "
            "exception but didnt receive one against \"%s\" \n", 
            ca_name ( args.chid ) );
    }
    else {
        arrayEventExceptionNotifyComplete = 1;
    }
}

void exceptionTest ( chid chan, unsigned interestLevel )
{
    int status;

    showProgressBegin ( "exceptionTest", interestLevel );

    /*
     * force a get exception to occur
     */
    {
        dbr_put_ackt_t *pRS;

        acctstExceptionCount = 0u;
        status = ca_add_exception_event ( acctstExceptionNotify, 0 );
        SEVCHK ( status, "exception notify install failed" );

        pRS = malloc ( ca_element_count (chan) * sizeof (*pRS) );
        verify ( pRS );
        status = ca_array_get ( DBR_PUT_ACKT, 
            ca_element_count (chan), chan, pRS ); 
        SEVCHK  ( status, "array read request failed" );
        ca_pend_io ( 1e-5 );
        epicsThreadSleep ( 0.1 );
        ca_poll ();
        while ( acctstExceptionCount < 1u ) {
            printf ( "G" );
            fflush ( stdout );
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        status = ca_add_exception_event ( 0, 0 );
        SEVCHK ( status, "exception notify install failed" );
        free ( pRS );
    }

    /*
     * force a get call back exception to occur
     */
    {
        arrayEventExceptionNotifyComplete = 0u;
        status = ca_array_get_callback ( DBR_PUT_ACKT, 
            ca_element_count (chan), chan, arrayEventExceptionNotify, 0 ); 
        if ( status != ECA_NORMAL ) {
            verify ( status == ECA_BADTYPE || status == ECA_GETFAIL );
            arrayEventExceptionNotifyComplete = 1;
        }
        ca_flush_io ();
        epicsThreadSleep ( 0.1 );
        ca_poll ();
        while ( ! arrayEventExceptionNotifyComplete ) {
            printf ( "GCB" );
            fflush ( stdout );
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
    }

    /*
     * force a subscription exception to occur
     */
    {
        evid id;

        arrayEventExceptionNotifyComplete = 0u;
        status = ca_add_array_event ( DBR_PUT_ACKT, ca_element_count ( chan ), 
                        chan, arrayEventExceptionNotify, 0, 0.0, 0.0, 0.0, &id ); 
        if ( status != ECA_NORMAL ) {
            verify ( status == ECA_BADTYPE || status == ECA_GETFAIL );
            arrayEventExceptionNotifyComplete = 1;
        }
        ca_flush_io ();
        epicsThreadSleep ( 0.1 );
        ca_poll ();
        while ( ! arrayEventExceptionNotifyComplete ) {
            printf ( "S" );
            fflush ( stdout );
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        status = ca_clear_event ( id );
        SEVCHK ( status, "subscription clear failed" );
    }

    /*
     * force a put exception to occur
     */
    {
        dbr_string_t * pWS;
        unsigned i;

        acctstExceptionCount = 0u;
        status = ca_add_exception_event ( acctstExceptionNotify, 0 );
        SEVCHK ( status, "exception notify install failed" );

        pWS = malloc ( ca_element_count (chan) * MAX_STRING_SIZE );
        verify ( pWS );
        for ( i = 0; i < ca_element_count (chan); i++ ) {
            strcpy ( pWS[i], "@#$%" );
        }
        status = ca_array_put ( DBR_STRING, 
            ca_element_count (chan), chan, pWS ); 
        if ( status != ECA_NORMAL ) {
            verify ( status == ECA_BADTYPE || status == ECA_PUTFAIL );
            acctstExceptionCount++; /* local PV case */
        }
        ca_flush_io ();
        epicsThreadSleep ( 0.1 );
        ca_poll ();
        while ( acctstExceptionCount < 1u ) {
            printf ( "P" );
            fflush ( stdout );
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        status = ca_add_exception_event ( 0, 0 );
        SEVCHK ( status, "exception notify install failed" );
        free ( pWS );
    }

    /*
     * force a put callback exception to occur
     */
    {
        dbr_string_t *pWS;
        unsigned i;

        pWS = malloc ( ca_element_count (chan) * MAX_STRING_SIZE );
        verify ( pWS );
        for ( i = 0; i < ca_element_count (chan); i++ ) {
            strcpy ( pWS[i], "@#$%" );
        }
        arrayEventExceptionNotifyComplete = 0u;
        status = ca_array_put_callback ( DBR_STRING, 
            ca_element_count (chan), chan, pWS,
            arrayEventExceptionNotify, 0); 
        if ( status != ECA_NORMAL ) {
            arrayEventExceptionNotifyComplete = 1;
        }
        ca_flush_io ();
        epicsThreadSleep ( 0.1 );
        ca_poll ();
        while ( ! arrayEventExceptionNotifyComplete ) {
            printf ( "PCB" );
            fflush ( stdout );
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        free ( pWS );
    }

    showProgressEnd ( interestLevel );
}

/*
 * array test
 *
 * verify that we can at least write and read back the same array
 * if multiple elements are present
 */
static unsigned arrayReadNotifyComplete = 0;
static unsigned arrayWriteNotifyComplete = 0;
void arrayReadNotify ( struct event_handler_args args )
{
    dbr_double_t *pWF = ( dbr_double_t * ) ( args.usr );
    dbr_double_t *pRF = ( dbr_double_t * ) ( args.dbr );
    int i;
    for ( i = 0; i < args.count; i++ ) {
        verify ( pWF[i] == pRF[i] );
    }
    arrayReadNotifyComplete = 1;
}
void arrayWriteNotify ( struct event_handler_args args )
{
    if ( args.status == ECA_NORMAL ) {
        arrayWriteNotifyComplete = 1;
    }
    else {
       printf ( "arrayWriteNotify: update failed for \"%s\" because \"%s\"", 
            ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

void arrayTest ( chid chan, unsigned maxArrayBytes, unsigned interestLevel )
{
    dbr_double_t *pRF, *pWF;
    unsigned i;
    int status;
    evid id;

    if ( ! ca_write_access ( chan ) ) {
        printf ( "skipping array test - no write access\n" );
        return;
    }

    showProgressBegin ( "arrayTest", interestLevel );

    pRF = (dbr_double_t *) calloc ( ca_element_count (chan), sizeof (*pRF) );
    verify ( pRF != NULL );

    pWF = (dbr_double_t *) calloc ( ca_element_count (chan), sizeof (*pWF) );
    verify ( pWF != NULL );

    /*
     * write some random numbers into the array
     */
    for ( i = 0; i < ca_element_count (chan); i++ ) {
        pWF[i] =  rand ();
        pRF[i] = - pWF[i];
    }
    status = ca_array_put ( DBR_DOUBLE, ca_element_count ( chan ), 
                    chan, pWF ); 
    SEVCHK ( status, "array write request failed" );

    /*
     * read back the array
     */
    status = ca_array_get ( DBR_DOUBLE,  ca_element_count (chan), 
                    chan, pRF ); 
    SEVCHK ( status, "array read request failed" );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "array read failed" );

    /*
     * verify read response matches values written
     */
    for ( i = 0; i < ca_element_count ( chan ); i++ ) {
        if ( pWF[i] != pRF[i] ) {
            printf ( "i=%u, pWF[i]=%f, pRF[i]=%f\n",
                i, pWF[i], pRF[i]);
        }
        verify ( pWF[i] == pRF[i] );
    }

    /*
     * read back the array as strings
     */
    {
        char *pRS;
        unsigned size = ca_element_count (chan) * MAX_STRING_SIZE;

        if ( size <= maxArrayBytes ) {

            pRS = malloc ( ca_element_count (chan) * MAX_STRING_SIZE );
            verify ( pRS );
            status = ca_array_get ( DBR_STRING, 
                ca_element_count (chan), chan, pRS ); 
            SEVCHK  ( status, "array read request failed" );
            status = ca_pend_io ( timeoutToPendIO );
            SEVCHK ( status, "array read failed" );
            free ( pRS );
        }
        else {
            printf ( "skipping the fetch array in string data type test - does not fit\n" );
        }
    }

    /*
     * write some random numbers into the array
     */
    for ( i = 0; i < ca_element_count (chan); i++ ) {
        pWF[i] =  rand ();
        pRF[i] = - pWF[i];
    }
    status = ca_array_put_callback ( DBR_DOUBLE, ca_element_count (chan), 
                    chan, pWF, arrayWriteNotify, 0 ); 
    SEVCHK ( status, "array write notify request failed" );
    status = ca_array_get_callback ( DBR_DOUBLE, ca_element_count (chan), 
                    chan, arrayReadNotify, pWF ); 
    SEVCHK  ( status, "array read notify request failed" );
    ca_flush_io ();
    while ( ! arrayWriteNotifyComplete || ! arrayReadNotifyComplete ) {
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }

    /*
     * write some random numbers into the array
     */
    for ( i = 0; i < ca_element_count (chan); i++ ) {
        pWF[i] =  rand ();
        pRF[i] = - pWF[i];
    }
    arrayReadNotifyComplete = 0;
    status = ca_array_put ( DBR_DOUBLE, ca_element_count ( chan ), 
                    chan, pWF ); 
    SEVCHK ( status, "array write notify request failed" );
    status = ca_add_array_event ( DBR_DOUBLE, ca_element_count ( chan ), 
                    chan, arrayReadNotify, pWF, 0.0, 0.0, 0.0, &id ); 
    SEVCHK ( status, "array subscription request failed" );
    ca_flush_io ();
    while ( ! arrayReadNotifyComplete ) {
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    status = ca_clear_event ( id );
    SEVCHK ( status, "clear event request failed" );

    /*
     * a get request should fail or fill with zeros
     * when the array size is too large
     */
    {
        acctstExceptionCount = 0u;
        status = ca_add_exception_event ( acctstExceptionNotify, 0 );
        SEVCHK ( status, "exception notify install failed" );
        status = ca_array_get ( DBR_DOUBLE, 
            ca_element_count (chan)+1, chan, pRF ); 
        if ( status == ECA_NORMAL ) {
            ca_poll ();
            while ( acctstExceptionCount < 1u ) {
                printf ( "N" );
                fflush ( stdout );
                epicsThreadSleep ( 0.1 );
                ca_poll (); /* emulate typical GUI */
            }
        }
        else {
            verify ( status == ECA_BADCOUNT );
        }
        status = ca_add_exception_event ( 0, 0 );
        SEVCHK ( status, "exception notify install failed" );
    }

    free ( pRF );
    free ( pWF );

    showProgressEnd ( interestLevel );
}

/* 
 * verify that unequal send/recv buffer sizes work
 * (a bug related to this test was detected in early R3.14)
 *
 * this test must be run when no other channels are connected
 */
void unequalServerBufferSizeTest ( const char * pName, unsigned interestLevel )
{
    dbr_double_t *pRF, *pWF;
    unsigned connections;
    chid newChan;
    int status;

    showProgressBegin ( "unequalServerBufferSizeTest", interestLevel );

    /* this test must be run when no channels are connected */
    connections = ca_get_ioc_connection_count ();
    verify ( connections == 0u );

    status = ca_create_channel ( pName, 0, 0, 0, & newChan );
    verify ( status == ECA_NORMAL );
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );

    showProgress ( interestLevel );

    if ( ! ca_write_access ( newChan ) ) {
        printf ( "skipping unequal buffer size test - no write access\n" );
        status = ca_clear_channel ( newChan );
        verify ( status == ECA_NORMAL );
        return;
    }

    pRF = (dbr_double_t *) calloc ( ca_element_count (newChan), sizeof (*pRF) );
    verify ( pRF != NULL );

    pWF = (dbr_double_t *) calloc ( ca_element_count (newChan), sizeof (*pWF) );
    verify ( pWF != NULL );

    status = ca_array_get ( DBR_DOUBLE, ca_element_count ( newChan ), 
                newChan, pRF ); 
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );
    status = ca_clear_channel ( newChan );
    verify ( status == ECA_NORMAL );

    showProgress ( interestLevel );

    status = ca_create_channel ( pName, 0, 0, 0, &newChan );
    verify ( status == ECA_NORMAL );
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );

    showProgress ( interestLevel );

    status = ca_array_put ( DBR_DOUBLE, ca_element_count ( newChan ), 
                newChan, pWF ); 
    verify ( status == ECA_NORMAL );
    status = ca_array_get ( DBR_DOUBLE, 1, 
                newChan, pRF ); 
    verify ( status == ECA_NORMAL );
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );
    status = ca_clear_channel ( newChan );
    verify ( status == ECA_NORMAL );

    free ( pRF );
    free ( pWF );

    showProgressEnd ( interestLevel );
}

/*
 * pend_event_delay_test()
 */
void pend_event_delay_test ( dbr_double_t request )
{
    int     status;
    epicsTimeStamp    end_time;
    epicsTimeStamp    start_time;
    dbr_double_t    delay;
    dbr_double_t    accuracy;

    epicsTimeGetCurrent ( &start_time );
    status = ca_pend_event ( request );
    if (status != ECA_TIMEOUT) {
        SEVCHK(status, NULL);
    }
    epicsTimeGetCurrent(&end_time);
    delay = epicsTimeDiffInSeconds ( &end_time, &start_time );
    accuracy = 100.0*(delay-request)/request;
    printf ( "CA pend event delay = %f sec results in error = %f %%\n",
        request, accuracy );
    verify ( fabs(accuracy) < 10.0 );
}

void caTaskExitTest ( unsigned interestLevel )
{
    int status;

    showProgressBegin ( "caTaskExitTest", interestLevel );

    status = ca_task_exit ();
    SEVCHK ( status, NULL );

    showProgressEnd ( interestLevel );
}

void verifyDataTypeMacros (void)
{
    int type;

    type = dbf_type_to_DBR ( DBF_SHORT );
    verify ( type == DBR_SHORT );
    type = dbf_type_to_DBR_STS ( DBF_SHORT );
    verify ( type == DBR_STS_SHORT );
    type = dbf_type_to_DBR_GR ( DBF_SHORT );
    verify ( type == DBR_GR_SHORT );
    type = dbf_type_to_DBR_CTRL ( DBF_SHORT );
    verify ( type == DBR_CTRL_SHORT );
    type = dbf_type_to_DBR_TIME ( DBF_SHORT );
    verify ( type == DBR_TIME_SHORT );
    verify ( strcmp ( dbr_type_to_text( DBR_SHORT ), "DBR_SHORT" )  == 0 );
    verify ( strcmp ( dbf_type_to_text( DBF_SHORT ), "DBF_SHORT" )  == 0 );
    verify ( dbr_type_is_SHORT ( DBR_SHORT ) );
    verify ( dbr_type_is_valid ( DBR_SHORT ) );
    verify ( dbf_type_is_valid ( DBF_SHORT ) );
    {
        int dataType = -1;
        dbf_text_to_type ( "DBF_SHORT", dataType ); 
        verify ( dataType == DBF_SHORT );
        dbr_text_to_type ( "DBR_CLASS_NAME", dataType ); 
        verify ( dataType == DBR_CLASS_NAME );
    }
}

typedef struct {
    evid            id;
    dbr_float_t     lastValue;
    unsigned        count;
} eventTest;

/*
 * updateTestEvent ()
 */
void updateTestEvent ( struct event_handler_args args )
{
    eventTest *pET = (eventTest *) args.usr;
    struct dbr_gr_float *pGF = (struct dbr_gr_float *) args.dbr;
    pET->lastValue = pGF->value; 
    pET->count++;
}

dbr_float_t monitorUpdateTestPattern ( unsigned iter )
{
    return ( (float) iter ) * 10.12345f + 10.7f;
}

void callbackClearsChannel ( struct event_handler_args args )
{
    int status;
    status = ca_clear_channel ( args.chid );
    SEVCHK ( status, "clearChannelInXxxxCallbackTest clear channel" );
}

void clearChannelInGetCallbackTest ( const char *pName, unsigned level )
{
    unsigned i;
    chid chan;
    int status;
    
    showProgressBegin ( "clearChannelInGetCallbackTest", level );

    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }

    status = ca_create_channel ( pName, 0, 0, 0, & chan );
    SEVCHK ( status, "clearChannelInGetCallbackTest create channel" );

    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "clearChannelInGetCallbackTest connect channel" );
    
    status = ca_get_callback ( DBR_DOUBLE, chan, callbackClearsChannel, 0 );
    SEVCHK ( status, "clearChannelInGetCallbackTest get callback" );
    
    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }
    
    showProgressEnd ( level );
}

void clearChannelInPutCallbackTest ( const char *pName, unsigned level )
{
    unsigned i;
    const dbr_double_t value = 1.1;
    chid chan;
    int status;
    
    showProgressBegin ( "clearChannelInPutCallbackTest", level );

    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }

    status = ca_create_channel ( pName, 0, 0, 0, & chan );
    SEVCHK ( status, "clearChannelInPutCallbackTest create channel" );

    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "clearChannelInPutCallbackTest connect channel" );

    status = ca_put_callback ( DBR_DOUBLE, chan, & value, 
                        callbackClearsChannel, 0 );
    SEVCHK ( status, "clearChannelInPutCallbackTest get callback" );
    
    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }
    
    showProgressEnd ( level );
}

void clearChannelInSubscrCallbackTest ( const char *pName, unsigned level )
{
    unsigned i;
    chid chan;
    int status;
    
    showProgressBegin ( "clearChannelInSubscrCallbackTest", level );

    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }

    status = ca_create_channel ( pName, 0, 0, 0, & chan );
    SEVCHK ( status, "clearChannelInSubscrCallbackTest create channel" );

    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, "clearChannelInSubscrCallbackTest connect channel" );
                        
    status = ca_create_subscription ( DBR_DOUBLE, 1, chan, 
        DBE_VALUE, callbackClearsChannel, 0, 0 );
    SEVCHK ( status, "clearChannelInSubscrCallbackTest subscribe" );

    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }
    
    showProgressEnd ( level );
}

void monitorAddConnectionCallback ( struct connection_handler_args args ) 
{
    if ( args.op == CA_OP_CONN_UP ) {
        unsigned * pEventCount = ( unsigned * ) ca_puser ( args.chid );
        int status;
        verify ( *pEventCount == 0u );
        (*pEventCount)++;
        status = ca_create_subscription ( DBR_DOUBLE, 1, 
            args.chid, DBE_VALUE, nUpdatesTester, ca_puser ( args.chid ), 0 );
        SEVCHK ( status, "monitorAddConnectionCallback create subscription" );
    }
    else {
        fprintf ( stderr, "disconnect during monitorAddConnectionCallbackTest?\n" );
    }
}

/*
 * monitorAddConnectionCallbackTest
 * 1) subscription add from within connection callback needs to work
 * 2) check for problems where subscription is installed twice if
 * its installed within the connection callback handler
 */
void monitorAddConnectionCallbackTest ( const char *pName, unsigned interestLevel )
{
    unsigned i;
    chid chan;
    int status;
    unsigned eventCount = 0u;
    unsigned getCallbackCount = 0u;
    
    showProgressBegin ( "monitorAddConnectionCallbackTest", interestLevel );

    for ( i = 0; ca_get_ioc_connection_count () > 0 ; i++ ) {
        ca_pend_event ( 0.1 );
        verify ( i < 100 );
    }

    status = ca_create_channel ( pName, 
        monitorAddConnectionCallback, &eventCount, 0, & chan );
    SEVCHK ( status, "monitorAddConnectionCallbackTest create channel" );

    while ( eventCount < 2 ) {
        ca_pend_event ( 0.1 );
    }
    verify ( eventCount >= 2u );
    
    status = ca_get_callback ( DBR_DOUBLE, chan, nUpdatesTester, &getCallbackCount );
    SEVCHK ( status, "monitorAddConnectionCallback get callback" );
    while ( getCallbackCount == 0 ) {
        ca_pend_event ( 0.1 );
    }
    verify ( eventCount >= 2u );
    verify ( getCallbackCount == 1u );

    status = ca_clear_channel ( chan );
    SEVCHK ( status, "monitorAddConnectionCallbackTest clear channel" );
    
    status = ca_flush_io ();
    SEVCHK ( status, "monitorAddConnectionCallbackTest flush" );

    showProgressEnd ( interestLevel );
}


/*
 * monitorUpdateTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 */
void monitorUpdateTest ( chid chan, unsigned interestLevel )
{
    eventTest       test[100];
    dbr_float_t     temp, getResp;
    unsigned        i, j;
    unsigned        flowCtrlCount = 0u;
    unsigned        prevPassCount;
    unsigned        tries;

    if ( ! ca_write_access ( chan ) ) {
        printf ("skipped monitorUpdateTest test - no write access\n");
        return;
    }

    if ( dbr_value_class[ca_field_type ( chan )] != dbr_class_float ) {
        printf ("skipped monitorUpdateTest test - not an analog type\n");
        return;
    }
    
    showProgressBegin ( "monitorUpdateTest", interestLevel );

    /*
     * set channel to known value
     */
    temp = 1;
    SEVCHK ( ca_put ( DBR_FLOAT, chan, &temp ), NULL );

    for ( i = 0; i < NELEMENTS(test); i++ ) {
        test[i].count = 0;
        test[i].lastValue = -1.0f;
        SEVCHK(ca_add_event(DBR_GR_FLOAT, chan, updateTestEvent,
            &test[i], &test[i].id),NULL);
    }

    /*
     * force all of the monitors subscription requests to
     * complete
     *
     * NOTE: this hopefully demonstrates that when the
     * server is very busy with monitors the client 
     * is still able to punch through with a request.
     */
    SEVCHK ( ca_get ( DBR_FLOAT, chan, &getResp) ,NULL );
    SEVCHK ( ca_pend_io ( timeoutToPendIO ) ,NULL );
            
    showProgress ( interestLevel );

    /*
     * pass the test only if we get the first monitor update
     */
    tries = 0;
    while ( 1 ) {
        unsigned nFailed = 0u;
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
        for ( i = 0; i < NELEMENTS ( test ); i++ ) {
            if ( test[i].count > 0 ) {
                if ( test[i].lastValue != temp ) {
                    nFailed++;
                }
            }
            else {
                nFailed++;
            }
        }
        if ( nFailed == 0u ) {
            break;
        }
        printf ( "-" );
        fflush ( stdout );
        verify ( tries++ < 50 );
    }

    showProgress ( interestLevel );

    /*
     * attempt to uncover problems where the last event isnt sent
     * and hopefully get into a flow control situation
     */   
    prevPassCount = 0u;
    for ( i = 0; i < 10; i++ ) {
        for ( j = 0; j < NELEMENTS(test); j++ ) {
            SEVCHK ( ca_clear_event ( test[j].id ), NULL );
            test[j].count = 0;
            test[j].lastValue = -1.0f;
            SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, updateTestEvent,
                &test[j], &test[j].id ) , NULL );
        } 

        for ( j = 0; j <= i; j++ ) {
            temp = monitorUpdateTestPattern ( j );
            SEVCHK ( ca_put ( DBR_FLOAT, chan, &temp ), NULL );
        }

        /*
         * wait for the above to complete
         */
        getResp = -1;
        SEVCHK ( ca_get ( DBR_FLOAT, chan, &getResp ), NULL );
        SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );

        if ( getResp != temp ) {
            printf ( "getResp=%f, temp=%f\n", getResp, temp );
            verify ( getResp == temp );
        }

        /*
         * wait for all of the monitors to have correct values
         */
        tries = 0;
        while (1) {
            unsigned passCount = 0;
            unsigned tmpFlowCtrlCount = 0u;
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
            for ( j = 0; j < NELEMENTS ( test ); j++ ) {
                /*
                 * we shouldnt see old monitors because 
                 * we resubscribed
                 */
                verify ( test[j].count <= i + 2 );
                if ( test[j].lastValue == temp ) {
                    if ( test[j].count < i + 1 ) {
                        tmpFlowCtrlCount++;
                    }
                    passCount++;
                }
            }
            if ( passCount == NELEMENTS ( test ) ) {
                flowCtrlCount += tmpFlowCtrlCount;
                break;
            }
            if ( passCount == prevPassCount ) {
                verify ( tries++ < 500 );
                if ( tries % 50 == 0 ) {
                    for ( j = 0; j <= i; j++ ) {
                        dbr_float_t pat = monitorUpdateTestPattern ( j );
                        if ( pat == test[0].lastValue ) {
                            break;
                        }
                    }
                    if ( j <= i) {
                        printf ( "-(%d)", i-j );
                    }
                    else {
                        printf ( "." );
                    }
                    fflush ( stdout );
                }
            }
            prevPassCount = passCount;
        }
    }

    showProgress ( interestLevel );

    /*
     * delete the event subscriptions 
     */
    for ( i = 0; i < NELEMENTS ( test ); i++ ) {
        SEVCHK ( ca_clear_event ( test[i].id ), NULL );
    }
        
    /*
     * force all of the clear event requests to
     * complete
     */
    SEVCHK ( ca_get ( DBR_FLOAT, chan, &temp ), NULL );
    SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );

    /* printf ( "flow control bypassed %u events\n", flowCtrlCount ); */

    showProgressEnd ( interestLevel );
} 

void verifyReasonableBeaconPeriod ( chid chan, unsigned interestLevel )
{
    if ( ca_get_ioc_connection_count () > 0 ) {
        double beaconPeriod;
        double watchDogDelay;
        unsigned i;

        showProgressBegin ( "verifyReasonableBeaconPeriod", interestLevel );


        printf ( "Beacon anomalies detected since program start %u\n",
            ca_beacon_anomaly_count () );

        beaconPeriod = ca_beacon_period ( chan );
        printf ( "Estimated beacon period for channel %s = %g sec.\n", 
            ca_name ( chan ), beaconPeriod );

        watchDogDelay = ca_receive_watchdog_delay ( chan );
        verify ( watchDogDelay >= 0.0 );

        printf ( "busy: receive watchdog for \"%s\" expires in %g sec.\n",
            ca_name ( chan ), watchDogDelay );

        /* 
         * let one default connection timeout go by w/o receive activity 
         * so we can see if beacons reset the watchdog
         */
        for ( i = 0u; i < 15u; i++ ) {
            ca_pend_event ( 2.0 );
            showProgress ( interestLevel );
        }
        if ( interestLevel > 0 ) {
            printf ( "\n" );
        }

        watchDogDelay = ca_receive_watchdog_delay ( chan );
        verify ( watchDogDelay >= 0.0 );

        printf ( "inactive: receive watchdog for \"%s\" expires in %g sec.\n",
            ca_name ( chan ), watchDogDelay );

        showProgressEnd ( interestLevel );
    }
}

void verifyOldPend ( unsigned interestLevel )
{
    int status;

    showProgressBegin ( "verifyOldPend", interestLevel );

    /*
     * verify that the old ca_pend() is in the symbol table
     */
    status = ca_pend ( 100000.0, 1 );
    verify ( status == ECA_NORMAL );
    status = ca_pend ( 1e-12, 0 );
    verify ( status == ECA_TIMEOUT );

    showProgressEnd ( interestLevel );
}

void verifyTimeStamps ( chid chan, unsigned interestLevel )
{
    struct dbr_time_double first;
    struct dbr_time_double last;
    epicsTimeStamp localTime;
    char buf[128];
    size_t length;
    double diff;
    int status;

    showProgressBegin ( "verifyTimeStamps", interestLevel );

    status = epicsTimeGetCurrent ( & localTime );
    verify ( status >= 0 );

    status = ca_get ( DBR_TIME_DOUBLE, chan, & first );
    SEVCHK ( status, "fetch of dbr time double failed\n" );
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );

    status = ca_get ( DBR_TIME_DOUBLE, chan, & last );
    SEVCHK ( status, "fetch of dbr time double failed\n" );
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );

    length = epicsTimeToStrftime ( buf, sizeof ( buf ), 
        "%a %b %d %Y %H:%M:%S.%f", & first.stamp );
    verify ( length );
    printf ("Processing time of channel \"%s\" was \"%s\"\n", 
        ca_name ( chan ), buf );

    diff = epicsTimeDiffInSeconds ( & last.stamp, & first.stamp );
    printf ("Time difference between two successive reads was %g sec\n", 
        diff );

    diff = epicsTimeDiffInSeconds ( & first.stamp, & localTime );
    printf ("Time difference between client and server %g sec\n", 
        diff );

    showProgressEnd ( interestLevel );
}

/*
 * attempts to verify from the client side that
 * channel priorities work correctly
 */
void verifyChannelPriorities ( const char *pName, unsigned interestLevel )
{
    static const unsigned nPrio = 30;
    unsigned i;

    showProgressBegin ( "verifyChannelPriorities", interestLevel );

    for ( i = 0u; i < nPrio; i++ ) {
        int status;
        double value;
        chid chan0, chan1;
        unsigned priority0, priority1;
        
        priority0 = ( i * ( CA_PRIORITY_MAX - CA_PRIORITY_MIN ) ) / nPrio;
        priority0 += CA_PRIORITY_MIN;
        if ( priority0 > CA_PRIORITY_MAX ) {
            priority0 = CA_PRIORITY_MAX;
        }

        priority1 = ( (i+1) * ( CA_PRIORITY_MAX - CA_PRIORITY_MIN ) ) / nPrio;
        priority1 += CA_PRIORITY_MIN;
        if ( priority1 > CA_PRIORITY_MAX ) {
            priority1 = CA_PRIORITY_MAX;
        }

        status = ca_create_channel ( pName, 0, 0, 
            priority0, &chan0 );
        SEVCHK ( status, "prioritized channel create failed" );

        status = ca_create_channel ( pName, 0, 0, 
            priority1, &chan1 );
        SEVCHK ( status, "prioritized channel create failed" );

        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "prioritized channel connect failed" );
        verify ( status == ECA_NORMAL );

        value = i;
        status = ca_put ( DBR_DOUBLE, chan0, &value );
        SEVCHK ( status, "prioritized channel put failed" );
        status = ca_put ( DBR_DOUBLE, chan1, &value );
        SEVCHK ( status, "prioritized channel put failed" );

        status = ca_flush_io ();
        SEVCHK ( status, "prioritized channel flush failed" );

        status = ca_get ( DBR_DOUBLE, chan0, &value );
        SEVCHK ( status, "prioritized channel get failed" );
        status = ca_get ( DBR_DOUBLE, chan1, &value );
        SEVCHK ( status, "prioritized channel get failed" );

        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "prioritized channel pend io failed" );

        status = ca_clear_channel ( chan0 );
        SEVCHK ( status, "prioritized channel clear failed" );
        status = ca_clear_channel ( chan1 );
        SEVCHK ( status, "prioritized channel clear failed" );
    }

    showProgressEnd ( interestLevel );
}

void verifyTearDownWhenChannelConnected ( const char * pName, 
        enum ca_preemptive_callback_select select, 
        unsigned interestLevel )
{
    static const unsigned chanCount = 20;
    static const unsigned loopCount = 100;

    chid * const pChans = (chid * const) calloc ( chanCount, sizeof ( *pChans ) );
    double * const pValues = (double * const) calloc ( chanCount, sizeof ( *pValues ) );
    unsigned i, j;
    
    verify ( pChans && pValues );
    
    showProgressBegin ( "verifyTearDownWhenChannelConnected", interestLevel );

    for ( i = 0u; i < loopCount; i++ ) {
        int status;
        ca_context_create ( select );
        
        for ( j = 0; j < chanCount; j++ ) {
            status = ca_create_channel ( pName, 0, 0, 0, & pChans[j] );
            SEVCHK ( status, "immediate tear down channel create failed" );
        }
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "immediate tear down channel connect failed" );
        verify ( status == ECA_NORMAL );

        for ( j = 0; j < chanCount; j++ ) {
            status = ca_get ( DBR_DOUBLE, pChans[j], &pValues[j] );
            SEVCHK ( status, "immediate tear down channel get failed" );
        }
        
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "immediate tear down get pend io failed" );

        ca_context_destroy ();    
    }

    ca_context_create ( select );

    free ( pChans );
    free ( pValues );

    showProgressEnd ( interestLevel );
}


void verifyImmediateTearDown ( const char * pName, 
        enum ca_preemptive_callback_select select, 
        unsigned interestLevel )
{
    int i;

    showProgressBegin ( "verifyImmediateTearDown", interestLevel );

    for ( i = 0u; i < 100; i++ ) {
        chid chan;
        int status;
        dbr_long_t value = i % 8;
        ca_context_create ( select );
        ca_task_initialize ();
        status = ca_create_channel ( pName, 0, 0, 0, & chan );
        SEVCHK ( status, "immediate tear down channel create failed" );
        status = ca_pend_io ( timeoutToPendIO );
        SEVCHK ( status, "immediate tear down channel connect failed" );
        verify ( status == ECA_NORMAL );
        /* 
         * verify that puts pending when we call ca_task_exit() 
         * get flushed out
         */
        if ( i > 0 ) {
            dbr_long_t currentValue = value;
            status = ca_get ( DBR_LONG, chan, & currentValue );
            SEVCHK ( status, "immediate tear down channel get failed" );
            status = ca_pend_io ( timeoutToPendIO );
            SEVCHK ( status, "immediate tear down channel get failed" );
            if ( currentValue != ( (i - 1) % 8 ) ) {
                printf ( "currentValue = %i, i = %i\n", currentValue, i );
                verify ( currentValue == ( (i - 1) % 8 ) );
            }
        }
        status = ca_put ( DBR_LONG, chan, & value );
        SEVCHK ( status, "immediate tear down channel put failed" );
        status = ca_clear_channel ( chan );
        SEVCHK ( status, "immediate tear down channel clear failed" );
        ca_context_destroy ();
        /* epicsThreadSleep ( 1e-15 ); */
        if ( i % 10 == 0 ) {
            showProgress ( interestLevel );
        }
    }

    ca_context_create ( select );

    showProgressEnd ( interestLevel );
}

/*
 * keeping these tests together detects a bug
 */
void eventClearAndMultipleMonitorTest ( chid chan, unsigned interestLevel )
{
    eventClearTest ( chan );
    monitorUpdateTest ( chan, interestLevel );
}

void fdcb ( void * parg )
{
    ca_poll ();
}

void fdRegCB ( void * parg, int fd, int opened )
{
    int status;

    fdctx * mgrCtx = ( fdctx * ) parg;
    if ( opened ) {
        status = fdmgr_add_callback ( 
            mgrCtx, fd, fdi_read, fdcb, 0 );
        verify ( status >= 0 );
    }
    else {
        status = fdmgr_clear_callback ( 
            mgrCtx, fd, fdi_read );
        verify ( status >= 0 );
    }
}

typedef struct {
    char m_chanName[100u];
    struct ca_client_context * m_pCtx;
    chid m_chan;
    epicsMutexId m_mutex;
    epicsEventId m_testCompleteEvent;
    epicsEventId m_threadExitEvent;
    size_t m_nUpdatesReceived;
    size_t m_nUpdatesRequired;
    int m_testInitiated;
    int m_testComplete;
    unsigned m_interestLevel;
} MultiThreadSubscrTest;

static void testMultithreadSubscrSubscrCallback 
                        ( struct event_handler_args eha )
{
    const epicsEventId firstUpdateEvent = ( epicsEventId ) eha.usr;
    epicsEventSignal ( firstUpdateEvent );
}

static void testMultithreadSubscrCreateSubscr ( void * pParm ) 
{
    static unsigned nElem = 0;
    int testComplete = FALSE;
    evid id;
    epicsEventId firstUpdateEvent;
    epicsEventWaitStatus eventWaitStatus;   
    MultiThreadSubscrTest * const pMultiThreadSubscrTest = 
                                ( MultiThreadSubscrTest * ) pParm;

    /* this is required for the ca_flush below to work correctly */
    int status = ca_attach_context ( pMultiThreadSubscrTest->m_pCtx );
    verify ( status == ECA_NORMAL );
    firstUpdateEvent = epicsEventMustCreate ( epicsEventEmpty );
    verify ( firstUpdateEvent );
    status = ca_create_subscription ( 
                            DBR_TIME_LONG, 
                            nElem, 
                            pMultiThreadSubscrTest->m_chan, 
                            DBE_VALUE, 
                            testMultithreadSubscrSubscrCallback, 
                            firstUpdateEvent, 
                            & id );
    verify ( status == ECA_NORMAL );
    status = ca_flush_io ();
    verify ( status == ECA_NORMAL );
    /* wait for first update */
    eventWaitStatus = epicsEventWaitWithTimeout ( 
                            firstUpdateEvent, 60.0 * 10 );
    verify ( eventWaitStatus ==  epicsEventWaitOK );
    epicsEventDestroy ( firstUpdateEvent );
    status = ca_clear_subscription ( id );
    verify ( status == ECA_NORMAL );
    epicsMutexMustLock ( pMultiThreadSubscrTest->m_mutex );
    pMultiThreadSubscrTest->m_nUpdatesReceived++;
    testComplete = ( pMultiThreadSubscrTest->m_nUpdatesReceived == 
                    pMultiThreadSubscrTest->m_nUpdatesRequired );
    pMultiThreadSubscrTest->m_testComplete = testComplete;
    epicsMutexUnlock ( pMultiThreadSubscrTest->m_mutex );
    if ( testComplete ) {
        epicsEventSignal ( pMultiThreadSubscrTest->m_testCompleteEvent );
    }
}

void testMultithreadSubscrConnHandler ( struct connection_handler_args args )
{
    MultiThreadSubscrTest * const pMultiThreadSubscrTest = 
                        ( MultiThreadSubscrTest * ) ca_puser ( args.chid );
    epicsMutexMustLock ( pMultiThreadSubscrTest->m_mutex );
    if ( !pMultiThreadSubscrTest->m_testInitiated && 
            args.op == CA_OP_CONN_UP ) {
        int i;
        pMultiThreadSubscrTest->m_testInitiated = TRUE;
        for ( i = 0; i < pMultiThreadSubscrTest->m_nUpdatesRequired; i++ ) {
            char threadname[64];
            epicsThreadId threadId;
            sprintf(threadname, "testSubscr%06u", i);
            threadId = epicsThreadCreate ( threadname, 
                        epicsThreadPriorityMedium, 
                        epicsThreadGetStackSize(epicsThreadStackSmall), 
                        testMultithreadSubscrCreateSubscr, 
                        pMultiThreadSubscrTest );
            verify ( threadId );
        }
    }
    epicsMutexUnlock ( pMultiThreadSubscrTest->m_mutex );
}

void testMultithreadSubscr ( void * pParm )
{
    MultiThreadSubscrTest * const pMultiThreadSubscrTest = 
                                ( MultiThreadSubscrTest * ) pParm;
    int status;
    unsigned i;

    status = ca_context_create ( ca_enable_preemptive_callback );
    verify ( status == ECA_NORMAL );
    pMultiThreadSubscrTest->m_pCtx = ca_current_context ();
    verify ( pMultiThreadSubscrTest->m_pCtx );
    status = ca_create_channel (
                pMultiThreadSubscrTest->m_chanName,
                testMultithreadSubscrConnHandler,
                pMultiThreadSubscrTest,
                CA_PRIORITY_MIN, 
                & pMultiThreadSubscrTest->m_chan );
    verify ( status == ECA_NORMAL );

    showProgressBegin ( "verifyMultithreadSubscr", 
            pMultiThreadSubscrTest->m_interestLevel );
    i = 0;
    while ( TRUE ) {
        int success = FALSE;
        epicsEventWaitStatus eventWaitStatus;
        epicsMutexMustLock ( pMultiThreadSubscrTest->m_mutex );
        success = pMultiThreadSubscrTest->m_testComplete;
        epicsMutexUnlock ( pMultiThreadSubscrTest->m_mutex );
        if ( success ) {
            break;
        }
        eventWaitStatus = epicsEventWaitWithTimeout ( 
                            pMultiThreadSubscrTest->m_testCompleteEvent, 0.1 );
        verify ( eventWaitStatus ==  epicsEventWaitOK || 
                eventWaitStatus ==  epicsEventWaitTimeout );
        if ( i++ % 100 == 0u ) 
            showProgress ( pMultiThreadSubscrTest->m_interestLevel );
        verify ( i < 1000 );
    }
    showProgressEnd ( pMultiThreadSubscrTest->m_interestLevel );

    status = ca_clear_channel ( pMultiThreadSubscrTest->m_chan );
    verify ( status == ECA_NORMAL );
    ca_context_destroy ();
    epicsEventSignal ( pMultiThreadSubscrTest->m_threadExitEvent );
}

/*
 * test installation of subscriptions similar to usage paterns
 * employed by modern versions of the sequencer
 */
void verifyMultithreadSubscr ( const char * pName, unsigned interestLevel )
{
    static unsigned nSubscr = 3000;
    epicsThreadId threadId;
    MultiThreadSubscrTest * const pMultiThreadSubscrTest = 
                                (MultiThreadSubscrTest*) calloc ( 1, 
                                    sizeof ( MultiThreadSubscrTest ) );
    verify ( pMultiThreadSubscrTest);
    pMultiThreadSubscrTest->m_mutex = epicsMutexMustCreate ();
    verify ( pMultiThreadSubscrTest->m_mutex );
    pMultiThreadSubscrTest->m_testCompleteEvent = 
                epicsEventMustCreate ( epicsEventEmpty );
    verify ( pMultiThreadSubscrTest->m_testCompleteEvent );
    pMultiThreadSubscrTest->m_threadExitEvent = 
                epicsEventMustCreate ( epicsEventEmpty );
    verify ( pMultiThreadSubscrTest->m_threadExitEvent );
    strncpy ( pMultiThreadSubscrTest->m_chanName, pName, 
            sizeof ( pMultiThreadSubscrTest->m_chanName ) );
    pMultiThreadSubscrTest->m_chanName
        [ sizeof ( pMultiThreadSubscrTest->m_chanName ) - 1u ] = '\0';
    pMultiThreadSubscrTest->m_nUpdatesRequired = nSubscr;
    pMultiThreadSubscrTest->m_interestLevel = interestLevel;
    threadId = epicsThreadCreate ( 
                "testMultithreadSubscr", 
                epicsThreadPriorityMedium, 
                epicsThreadGetStackSize(epicsThreadStackSmall), 
                testMultithreadSubscr, pMultiThreadSubscrTest );
    verify ( threadId );
    {
        epicsEventWaitStatus eventWaitStatus;
        eventWaitStatus = epicsEventWaitWithTimeout ( 
                pMultiThreadSubscrTest->m_threadExitEvent, 1000.0 );
        verify  ( eventWaitStatus == epicsEventWaitOK );
    }
    epicsEventDestroy ( pMultiThreadSubscrTest->m_testCompleteEvent );
    epicsEventDestroy ( pMultiThreadSubscrTest->m_threadExitEvent );
    epicsMutexDestroy ( pMultiThreadSubscrTest->m_mutex );
    free ( pMultiThreadSubscrTest );
}

void fdManagerVerify ( const char * pName, unsigned interestLevel )
{
    int status;
    fdctx * mgrCtx;
    struct timeval tmo;
    chid newChan;
    evid subscription;
    unsigned eventCount = 0u;
    epicsTimeStamp begin, end;
    
    mgrCtx = fdmgr_init ();
    verify ( mgrCtx );

    showProgressBegin ( "fdManagerVerify", interestLevel );

    status = ca_add_fd_registration ( fdRegCB, mgrCtx );
    verify ( status == ECA_NORMAL );

    status = ca_create_channel ( pName, 0, 0, 0, & newChan );
    verify ( status == ECA_NORMAL );

    while ( ca_state ( newChan ) != cs_conn ) {
        tmo.tv_sec = 6000;
        tmo.tv_usec = 0;
        status = fdmgr_pend_event ( mgrCtx, & tmo );
        verify ( status >= 0 );
    }

    showProgress ( interestLevel );

    status = ca_add_event ( DBR_FLOAT, newChan, 
        nUpdatesTester, & eventCount,  & subscription );
    verify ( status == ECA_NORMAL );

    status = ca_flush_io ();
    verify ( status == ECA_NORMAL );

    while ( eventCount < 1 ) {
        tmo.tv_sec = 6000;
        tmo.tv_usec = 0;
        status = fdmgr_pend_event ( mgrCtx, & tmo );
        verify ( status >= 0 );
    }

    showProgress ( interestLevel );

    status = ca_clear_event ( subscription );
    verify ( status == ECA_NORMAL );

    status = ca_flush_io ();
    verify ( status == ECA_NORMAL );

    /* look for infinite loop in fd manager schedualing */
    epicsTimeGetCurrent ( & begin );
    eventCount = 0u;
    while ( 1 ) {
        double delay;
        tmo.tv_sec = 1;
        tmo.tv_usec = 0;
        status = fdmgr_pend_event ( mgrCtx, & tmo );
        verify ( status >= 0 );
        epicsTimeGetCurrent ( & end );
        delay = epicsTimeDiffInSeconds ( & end, & begin );
        if ( delay >= 1.0 ) {
            break;
        }
        verify ( eventCount++ < 100 );
    }

    showProgress ( interestLevel );

    status = ca_clear_channel ( newChan );
    verify ( status == ECA_NORMAL );

    status = ca_add_fd_registration ( 0, 0 );
    verify ( status == ECA_NORMAL );

    status = fdmgr_delete ( mgrCtx );
    verify ( status >= 0 );

    showProgressEnd ( interestLevel );
}

void verifyConnectWithDisconnectedChannels ( 
    const char *pName, unsigned interestLevel )
{
    int status;
    chid bogusChan[300];
    chid validChan;
    unsigned i;

    showProgressBegin ( "verifyConnectWithDisconnectedChannels", interestLevel );

    for ( i= 0u; i < NELEMENTS ( bogusChan ); i++ ) {
        char buf[256];
        sprintf ( buf, "aChannelThatShouldNeverNeverNeverExist%u", i );
        status = ca_create_channel ( buf, 0, 0, 0, & bogusChan[i] );
        verify ( status == ECA_NORMAL );
    }

    status = ca_pend_io ( 0.001 );
    verify ( status == ECA_TIMEOUT );

    /* wait a long time for the search interval to increase */
    for ( i= 0u; i < 10; i++ ) {
        epicsThreadSleep ( 1.0 );
        showProgress ( interestLevel );
    }

    status = ca_create_channel ( pName, 0, 0, 0, & validChan );
    verify ( status == ECA_NORMAL );

    /* 
     * we should be able to connect to a valid 
     * channel within a reasonable delay even
     * though there is one permanently 
     * diasconnected channel 
     */
    status = ca_pend_io ( timeoutToPendIO );
    verify ( status == ECA_NORMAL );

    status = ca_clear_channel ( validChan );
    verify ( status == ECA_NORMAL );

    for ( i= 0u; i < NELEMENTS ( bogusChan ); i++ ) {
        status = ca_clear_channel ( bogusChan[i] );
        verify ( status == ECA_NORMAL );
    }

    showProgressEnd ( interestLevel );
}

void verifyClearChannelOnDisconnectCallback (
    struct connection_handler_args args )
{
    int * pDisconnectFlag = ca_puser ( args.chid );
    if ( args.op == CA_OP_CONN_DOWN ) {
        ca_clear_channel ( args.chid );
        *pDisconnectFlag = 1;
    }
}

void noopExceptionCallback ( struct exception_handler_args args )
{
}

void verifyDisconnect ( 
    const char * pName, unsigned interestLevel )
{
    int disconnectFlag = 0;
    unsigned count = 0;
    chid chan0;
    chid chan1;
    int status;

    status = ca_create_channel  ( 
        pName, verifyClearChannelOnDisconnectCallback, 
        & disconnectFlag, 0, & chan0 );
    SEVCHK ( status, NULL );

    fprintf ( stdout, "Waiting for test channel to connect." );
    fflush ( stdout );
    do {
        ca_pend_event ( 0.1 );
        if ( count++%50 == 0 ) {
            fprintf ( stdout, "." );
            fflush ( stdout );
        }
    } while ( ca_state ( chan0 ) != cs_conn );
    fprintf ( stdout, "confirmed.\n" );

    /*
     * if its a local channel and will never disconnect
     * then skip the portions of this test that cant be
     * completed.
     */
    if ( ca_get_ioc_connection_count () == 0 ) {
        status = ca_clear_channel ( chan0 );
        SEVCHK ( status, NULL );
        return;
    }

    status = ca_add_exception_event ( noopExceptionCallback, 0 );
    SEVCHK ( status, NULL );

    fprintf ( stdout, "Please force test channel to disconnect." );
    fflush ( stdout );
    do {
        ca_pend_event ( 0.1 );;
        if ( count++%50 == 0 ) {
            fprintf ( stdout, "." );
            fflush ( stdout );
        }
    } while ( ! disconnectFlag );
    fprintf ( stdout, "confirmed.\n" );
    /* channel cleared by disconnect handler */

    status = ca_create_channel  ( 
        pName, 0, 0, 0, & chan1 );
    SEVCHK ( status, NULL );

    fprintf ( stdout, "Waiting for test channel to connect." );
    fflush ( stdout );
    while ( ca_state ( chan1 ) != cs_conn ) {
        ca_pend_event ( 5.0 );
        fprintf ( stdout, "." );
        fflush ( stdout );
    }
    status = ca_clear_channel ( chan1 );
    SEVCHK ( status, NULL );
    fprintf ( stdout, "confirmed.\n" );

    status = ca_add_exception_event ( 0, 0 );
    SEVCHK ( status, NULL );
}

void verifyName ( 
    const char * pName, unsigned interestLevel )
{
    chid chan;
    int status = ca_create_channel  ( 
                    pName, 0, 0, 0, & chan );
    SEVCHK ( status, NULL );
    if ( strcmp ( pName, ca_name ( chan ) ) != 0 ) {
        printf ( "Canonical name for channel was \"%s\"\n", ca_name ( chan ) );
    }
    status = ca_clear_channel ( chan );
    SEVCHK ( status, NULL );
}

void verifyContextRundownFlush ( const char * pName, unsigned interestLevel )
{
    unsigned i;

    showProgressBegin ( "verifyContextRundownFlush", interestLevel );

    for ( i=0u; i < 1000; i++ ) {
        const dbr_double_t stim = i;
        
        {  
            chid chan;
            int status;
            status = ca_context_create ( ca_disable_preemptive_callback );
            SEVCHK ( status, "context create failed" );
            
            status = ca_create_channel  ( pName, 0, 0, 0, & chan );
            /*
             * currently in-memory channels cant be used with this test
             * !!!! FIX ME, FIX ME, FIX ME, FIX ME !!!!
             */
            if ( status != ECA_UNAVAILINSERV ) {
                SEVCHK ( status, NULL );
                
                status = ca_pend_io( timeoutToPendIO );
                SEVCHK ( status, "channel connect failed" );
                
                status = ca_put ( DBR_DOUBLE, chan, & stim );
                SEVCHK ( status, "channel put failed" );
        
                status = ca_clear_channel ( chan );
                SEVCHK ( status, NULL );
            }
            ca_context_destroy ();
        }
        
        {
            chid chan;
            int status;
            dbr_double_t resp;
            status = ca_context_create ( ca_disable_preemptive_callback );
            SEVCHK ( status, "context create failed" );
           
            status = ca_create_channel  ( pName, 0, 0, 0, & chan );
            SEVCHK ( status, NULL );
            /*
             * currently in-memory channels cant be used with this test
             * !!!! FIX ME, FIX ME, FIX ME, FIX ME !!!!
             */
            if ( status != ECA_UNAVAILINSERV ) {
                status = ca_pend_io( timeoutToPendIO );
                SEVCHK ( status, "channel connect failed" );
        
                status = ca_get ( DBR_DOUBLE, chan, & resp );
                SEVCHK ( status, "channel get failed" );
        
                status = ca_pend_io ( timeoutToPendIO );
                SEVCHK ( status, "get, pend io failed" );
                
                verify ( stim == resp );
        
                status = ca_clear_channel ( chan );
                SEVCHK ( status, NULL );
            }
            ca_context_destroy ();
        }

        if ( i % 100 == 0 ) {
            showProgress ( interestLevel );
        }
    }
    
    showProgressEnd ( interestLevel );
}

void verifyContextRundownChanStillExist ( 
    const char * pName, unsigned interestLevel )
{
    chid chan[10000];
    int status;
    unsigned i;

    showProgressBegin ( "verifyContextRundownChanStillExist", interestLevel );

    status = ca_context_create ( ca_disable_preemptive_callback );
    SEVCHK ( status, "context create failed" );
    
    for ( i = 0; i < NELEMENTS ( chan ); i++ ) {
        status = ca_create_channel  ( pName, 0, 0, 0, & chan[i] );
        /*
         * currently in-memory channels cant be used with this test
         * !!!! FIX ME, FIX ME, FIX ME, FIX ME !!!!
         */
        if ( status == ECA_UNAVAILINSERV ) {
            break;
        }
        SEVCHK ( status, NULL );
    }
    
    status = ca_pend_io( timeoutToPendIO );
    SEVCHK ( status, "channel connect failed" );

    ca_context_destroy ();
    
    showProgressEnd ( interestLevel );
}

int acctst ( const char * pName, unsigned interestLevel, unsigned channelCount, 
			unsigned repetitionCount, enum ca_preemptive_callback_select select )
{
    chid chan;
    int status;
    unsigned i;
    appChan *pChans;
    unsigned connections;
    unsigned maxArrayBytes = 10000000;

    printf ( "CA Client V%s, channel name \"%s\", timeout %g\n", 
        ca_version (), pName, timeoutToPendIO );
    if ( select == ca_enable_preemptive_callback ) {
        printf ( "Preemptive call back is enabled.\n" );
    }

    {
        char tmpString[32];
        sprintf ( tmpString, "%u", maxArrayBytes );
        epicsEnvSet ( "EPICS_CA_MAX_ARRAY_BYTES", tmpString ); 
    }

    /*
     * this test creates, and then destroys, a private CA context
     */
    multiSubscrDestroyNoLateCallbackTest ( pName, interestLevel );

    status = ca_context_create ( select );
    SEVCHK ( status, NULL );

    verifyDisconnect ( pName, interestLevel );
    verifyImmediateTearDown ( pName, select, interestLevel );
    verifyTearDownWhenChannelConnected ( pName, select, interestLevel );

    verifyDataTypeMacros ();

    connections = ca_get_ioc_connection_count ();
    verify ( connections == 0u );
    unequalServerBufferSizeTest ( pName, interestLevel );
    clearChannelInGetCallbackTest ( pName, interestLevel );
    clearChannelInPutCallbackTest ( pName, interestLevel );
    clearChannelInSubscrCallbackTest ( pName, interestLevel );
    monitorAddConnectionCallbackTest ( pName, interestLevel );

    showProgressBegin ( "connecting to test channel", interestLevel );
    status = ca_search ( pName, & chan );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( timeoutToPendIO );
    SEVCHK ( status, NULL );
    showProgressEnd ( interestLevel );

    printf ( "native type was %s, native count was %lu\n",
        dbf_type_to_text ( ca_field_type ( chan ) ), 
        ca_element_count ( chan ) );

    connections = ca_get_ioc_connection_count ();
    verify ( connections == 1u || connections == 0u );
    if ( connections == 0u ) {
        printf ( "testing with a local channel\n" );
    }

    verifyName ( pName, interestLevel );
    verifyConnectWithDisconnectedChannels ( pName, interestLevel );
    grEnumTest ( chan, interestLevel );
    test_sync_groups ( chan, interestLevel );
    verifyChannelPriorities ( pName, interestLevel );
    verifyTimeStamps ( chan, interestLevel );
    verifyOldPend ( interestLevel );
    exceptionTest ( chan, interestLevel );
    arrayTest ( chan, maxArrayBytes, interestLevel ); 
    verifyMonitorSubscriptionFlushIO ( chan, interestLevel );
    monitorSubscriptionFirstUpdateTest ( pName, chan, interestLevel );
    ctrlDoubleTest ( chan, interestLevel );
    verifyBlockInPendIO ( chan, interestLevel );
    verifyAnalogIO ( chan, DBR_FLOAT, FLT_MIN, FLT_MAX, 
               FLT_MIN_EXP, FLT_MAX_EXP, FLT_EPSILON, interestLevel );
    verifyAnalogIO ( chan, DBR_DOUBLE, DBL_MIN, DBL_MAX, 
               DBL_MIN_EXP, DBL_MAX_EXP, DBL_EPSILON, interestLevel );
    verifyLongIO ( chan, interestLevel );
    verifyShortIO ( chan, interestLevel );
    multiSubscriptionDeleteTest ( chan, interestLevel );
    singleSubscriptionDeleteTest ( chan, interestLevel );
    channelClearWithEventTrafficTest ( pName, interestLevel );
    eventClearAndMultipleMonitorTest ( chan, interestLevel );
    verifyHighThroughputRead ( chan, interestLevel );
    verifyHighThroughputWrite ( chan, interestLevel );
    verifyHighThroughputReadCallback ( chan, interestLevel );
    verifyHighThroughputWriteCallback ( chan, interestLevel );
    verifyBadString ( chan, interestLevel );
    verifyMultithreadSubscr ( pName, interestLevel );
    if ( select != ca_enable_preemptive_callback ) {
        fdManagerVerify ( pName, interestLevel ); 
    }

    /*
     * CA pend event delay accuracy test
     * (CA asssumes that search requests can be sent
     * at least every 25 mS on all supported os)
     */
    printf ( "\n" );
    pend_event_delay_test ( 1.0 );
    pend_event_delay_test ( 0.1 );
    pend_event_delay_test ( 0.25 ); 

    /* ca_channel_status ( 0 ); */
    ca_client_status ( 0 );
    /* ca_client_status ( 6u ); info about each channel */

    pChans = calloc ( channelCount, sizeof ( *pChans ) );
    verify ( pChans );

    for ( i = 0; i < channelCount; i++ ) {
        strncpy ( pChans[ i ].name, pName, sizeof ( pChans[ i ].name ) );
        pChans[ i ].name[ sizeof ( pChans[i].name ) - 1 ] = '\0';
    }

    verifyConnectionHandlerConnect ( pChans, channelCount, repetitionCount, interestLevel );
    verifyBlockingConnect ( pChans, channelCount, repetitionCount, interestLevel );
    verifyClear ( pChans, interestLevel );

    verifyReasonableBeaconPeriod ( chan, interestLevel );

    /*
     * Verify that we can do IO with the new types for ALH
     */
#if 0
    if ( ca_read_access (chan) && ca_write_access (chan) ) {
    {
        dbr_put_ackt_t acktIn = 1u;
        dbr_put_acks_t acksIn = 1u;
        struct dbr_stsack_string stsackOut;

        SEVCHK ( ca_put ( DBR_PUT_ACKT, chan, &acktIn ), NULL );
        SEVCHK ( ca_put ( DBR_PUT_ACKS, chan, &acksIn ), NULL );
        SEVCHK ( ca_get ( DBR_STSACK_STRING, chan, &stsackOut ), NULL );
        SEVCHK ( ca_pend_io ( timeoutToPendIO ), NULL );
    }
#endif

    /* test that ca_task_exit () works when there is still one channel remaining */
    /* status = ca_clear_channel ( chan ); */
    /* SEVCHK ( status, NULL ); */

    caTaskExitTest ( interestLevel );
    
    verifyContextRundownFlush ( pName, interestLevel );
    verifyContextRundownChanStillExist ( pName, interestLevel );
    
    free ( pChans );

    printf ( "\nTest Complete\n" );

    epicsExit ( EXIT_SUCCESS );

    return 0;
}


