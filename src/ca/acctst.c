
/*
 * CA regression test
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
#include "epicsTime.h"
#include "envDefs.h"
#include "caDiagnostics.h"
#include "cadef.h"

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef NELEMENTS
#define NELEMENTS(A) ( sizeof (A) / sizeof (A[0]) )
#endif

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

void showProgressBegin ()
{
    printf ( "{" );
    fflush (stdout );
}

void showProgressEnd ()
{
    printf ( "}" );
    fflush (stdout );
}

void showProgress ()
{
    printf ( "." );
    fflush (stdout );
}

void nUpdatesTester ( struct event_handler_args args )
{
    unsigned *pCtr = (unsigned *) args.usr;
    ( *pCtr ) ++;
    if ( args.status != ECA_NORMAL ) {
	        printf("subscription update failed for \"%s\" because \"%s\"", 
                ca_name ( args.chid ), ca_message ( args.status ) );
    }
}

void monitorSubscriptionFirstUpdateTest ( const char *pName, chid chan )
{
    int status;
    struct dbr_ctrl_double currentVal;
    double delta;
    unsigned eventCount = 0u;
    unsigned waitCount = 0u;
    evid id;
    chid chan2;

    showProgressBegin ();

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
    assert ( eventCount > 0 );

    /* verify that a ca_put() produces an update, but */
    /* this may fail if there is an unusual deadband */
    status = ca_get ( DBR_CTRL_DOUBLE, chan, &currentVal );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( 100.0 );
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
    assert ( eventCount > 0 );

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
    status = ca_pend_io ( 20.0 );
    SEVCHK (status, 0);
    epicsThreadSleep ( 0.1 );
    ca_poll ();
    while ( eventCount < 1 && waitCount++ < 100 ) {
        printf ( "w" );
        fflush ( stdout );
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
    }
    assert ( eventCount > 0 );

    /* verify that a ca_put() produces an update, but */
    /* this may fail if there is an unusual deadband */
    status = ca_get ( DBR_CTRL_DOUBLE, chan2, &currentVal );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( 100.0 );
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
    assert ( eventCount > 0 );

    /* clean up */
    status = ca_clear_channel ( chan2 );
    SEVCHK ( status, 0 );

    showProgressEnd ();
}

void ioTesterGet ( struct event_handler_args args )
{
    unsigned *pCtr = (unsigned *) args.usr;
    if ( args.status != ECA_NORMAL ) {
	        printf("get call back failed for \"%s\" because \"%s\"", 
                ca_name ( args.chid ), ca_message ( args.status ) );
    }
    ( *pCtr ) ++;
}

void ioTesterEvent ( struct event_handler_args args )
{
    int status;
    if ( args.status != ECA_NORMAL ) {
	        printf("subscription update failed for \"%s\" because \"%s\"", 
                ca_name ( args.chid ), ca_message ( args.status ) );
    }
    status = ca_get_callback ( DBR_GR_STRING, args.chid, ioTesterGet, args.usr );
    SEVCHK ( status, 0 );
}

void verifyMonitorSubscriptionFlushIO ( chid chan )
{
    int status;
    unsigned eventCount = 0u;
    unsigned waitCount = 0u;
    evid id;

    showProgressBegin ();

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
    assert ( eventCount > 0 );
    status = ca_clear_event ( id );
    SEVCHK (status, 0);

    showProgressEnd ();
}

void accessRightsStateChange ( struct access_rights_handler_args args )
{
    appChan	*pChan = (appChan *) ca_puser ( args.chid );

    assert ( pChan->channel == args.chid );
    assert ( args.ar.read_access == ca_read_access ( args.chid ) );
    assert ( args.ar.write_access == ca_write_access ( args.chid ) );
    accessUpdateCount++;
    pChan->accessUpdateCount++;
}

void getCallbackStateChange ( struct event_handler_args args )
{
    appChan	*pChan = (appChan *) args.usr;

    assert ( pChan->channel == args.chid );
    assert ( pChan->connected );
    assert ( args.status == ECA_NORMAL );

    getCallbackCount++;
    pChan->getCallbackCount++;
}

void connectionStateChange ( struct connection_handler_args args )
{
    int status;

    appChan	*pChan = (appChan *) ca_puser ( args.chid );

    assert ( pChan->channel == args.chid );

    if ( args.op == CA_OP_CONN_UP ) {
        if ( pChan->accessRightsHandlerInstalled ) {
            assert ( pChan->accessUpdateCount > 0u );
        }
        assert ( ! pChan->connected );
        pChan->connected = 1;
        status = ca_get_callback ( DBR_GR_STRING, args.chid, getCallbackStateChange, pChan );
        SEVCHK (status, 0);
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        assert ( pChan->connected );
        pChan->connected = 0u;
        assert ( ! ca_read_access ( args.chid ) );
        assert ( ! ca_write_access ( args.chid ) );
    }
    else {
        assert ( 0 );
    }
    pChan->connectionUpdateCount++;
    connectionUpdateCount++;
}

void subscriptionStateChange ( struct event_handler_args args )
{
    appChan	*pChan = (appChan *) args.usr;

    assert ( pChan->channel == args.chid );
    assert ( pChan->connected );
    assert ( args.type == DBR_GR_STRING );
    pChan->subscriptionUpdateCount++;
    subscriptionUpdateCount++;

    if ( args.status != ECA_NORMAL ) {
	        printf("subscription update failed for \"%s\" because \"%s\"", 
                ca_name ( args.chid ), ca_message ( args.status ) );
    }

    assert ( strlen ( (char *) args.dbr ) <= MAX_STRING_SIZE );
}

void noopSubscriptionStateChange ( struct event_handler_args args )
{
    if ( args.status != ECA_NORMAL ) {
	        printf("subscription update failed for \"%s\" because \"%s\"", 
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
 */
void verifyConnectionHandlerConnect ( appChan *pChans, unsigned chanCount, unsigned repetitionCount )
{
    int status;
    unsigned i, j;

    showProgressBegin ();


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
                &pChans[j].channel, connectionStateChange, &pChans[j]);
            SEVCHK ( status, NULL );

            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
            SEVCHK ( status, NULL );
            pChans[j].accessRightsHandlerInstalled = 1;

            status = ca_add_event ( DBR_GR_STRING, pChans[j].channel,
                    subscriptionStateChange, &pChans[j], &pChans[j].subscription );
            SEVCHK ( status, NULL );

            assert ( ca_test_io () == ECA_IODONE );
        }

        ca_flush_io ();

        showProgress ();

        while ( connectionUpdateCount < chanCount || 
            getCallbackCount < chanCount ) {
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }

        for ( j = 0u; j < chanCount; j++ ) {
            assert ( pChans[j].getCallbackCount == 1u);
            assert ( pChans[j].connectionUpdateCount > 0 );
            if ( pChans[j].connectionUpdateCount > 1u ) {
                printf ("Unusual connection activity count = %u on channel %s?\n",
                    pChans[j].connectionUpdateCount, pChans[j].name );
            }
            assert ( pChans[j].accessUpdateCount > 0 );
            if ( pChans[j].accessUpdateCount > 1u ) {
                printf ("Unusual access rights activity count = %u on channel %s?\n",
                    pChans[j].connectionUpdateCount, pChans[j].name );
            }
        }

        ca_self_test ();

        showProgress ();

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_clear_event ( pChans[j].subscription );
            SEVCHK ( status, NULL );
        }

        ca_self_test ();

        showProgress ();

        for ( j = 0u; j < chanCount; j++ ) {
            status = ca_clear_channel ( pChans[j].channel );
            SEVCHK ( status, NULL );
        }

        ca_self_test ();

        showProgress ();

    }
    showProgressEnd ();
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
void verifyBlockingConnect ( appChan *pChans, unsigned chanCount, unsigned repetitionCount )
{
    int status;
    unsigned i, j;
    unsigned connections;
    unsigned backgroundConnCount = ca_get_ioc_connection_count ();

    showProgressBegin ();

    assert ( backgroundConnCount == 1u || backgroundConnCount == 0u );

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
                assert ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
            }
            else {
                assert ( INVALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
                assert ( ca_test_io () == ECA_IOINPROGRESS );
            }
            
            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
            SEVCHK ( status, NULL );
            pChans[j].accessRightsHandlerInstalled = 1;
        }

        showProgress ();

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

        status = ca_pend_io ( 1000.0 );
        SEVCHK ( status, NULL );

        ca_self_test ();

        showProgress ();

        assert ( ca_test_io () == ECA_IODONE );

        connections = ca_get_ioc_connection_count ();
        assert ( connections == backgroundConnCount );

        for ( j = 0u; j < chanCount; j++ ) {
            assert ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) );
            assert ( ca_state ( pChans[j].channel ) == cs_conn );
            SEVCHK ( ca_clear_channel ( pChans[j].channel ), NULL );
        }

        ca_self_test ();

        ca_flush_io ();

        showProgress ();

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
                assert ( ++j < 100 );
            }
        }
        showProgress ();
    }

    for ( j = 0u; j < chanCount; j++ ) {
        status = ca_search ( pChans[j].name, &pChans[j].channel );
        SEVCHK ( status, NULL );
    }

    for ( j = 0u; j < chanCount; j++ ) {
        status = ca_clear_channel ( pChans[j].channel );
        SEVCHK ( status, NULL );
    }

    assert ( ca_test_io () == ECA_IODONE );

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
                assert ( ca_state ( pChans[1].channel ) == cs_conn );
            }
            status = ca_clear_channel ( pChans[1].channel );
            SEVCHK ( status, NULL );
        }
        else {
            assert ( ca_state( pChans[0].channel ) == cs_conn );
        }
    }
    status = ca_clear_channel( pChans[0].channel );
    SEVCHK ( status, NULL );

    ca_self_test ();

    showProgressEnd ();
}

/*
 * 1) verify that use of NULL evid does not cause problems
 * 2) verify clear before connect
 */
void verifyClear ( appChan *pChans )
{
    int status;

    showProgressBegin ();

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
    showProgressEnd ();
}

/*
 * performGrEnumTest
 */
void performGrEnumTest ( chid chan )
{
    struct dbr_gr_enum ge;
    unsigned count;
    int status;
    unsigned i;

    showProgressBegin ();

    ge.no_str = -1;

    status = ca_get (DBR_GR_ENUM, chan, &ge);
    SEVCHK (status, "DBR_GR_ENUM ca_get()");

    status = ca_pend_io (2.0);
    assert (status == ECA_NORMAL);

    assert ( ge.no_str >= 0 && ge.no_str < NELEMENTS(ge.strs) );
    if ( ge.no_str > 0 ) {
        printf ("Enum state str = {");
        count = (unsigned) ge.no_str;
        for (i=0; i<count; i++) {
            printf ("\"%s\" ", ge.strs[i]);
        }
        printf ("}\n");
    }
    showProgressEnd ();
}

/*
 * performCtrlDoubleTest
 */
void performCtrlDoubleTest (chid chan)
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

    showProgressBegin ();

    size = sizeof (*pDbl)*ca_element_count(chan);
    pDbl = malloc (size);
    assert (pDbl!=NULL);

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
    SEVCHK (status, "performCtrlDoubleTest, ca_array_put");

    size = dbr_size_n(DBR_CTRL_DOUBLE, ca_element_count(chan));
    pCtrlDbl = (struct dbr_ctrl_double *) malloc (size); 
    assert (pCtrlDbl!=NULL);

    /*
     * read the array from the PV
     */
    status = ca_array_get (DBR_CTRL_DOUBLE,
                    ca_element_count(chan),
                    chan, pCtrlDbl);
    SEVCHK (status, "performCtrlDoubleTest, ca_array_get");
    status = ca_pend_io (30.0);
    assert (status==ECA_NORMAL);

    /*
     * verify the result
     */
    for (i=0; i<nElem; i++) {
        double diff = pDbl[i] - sin (i*slice);
        assert (fabs(diff) < DBL_EPSILON*4);
    }

    free (pCtrlDbl);
    free (pDbl);
    showProgressEnd ();
}

/*
 * ca_pend_io() must block
 */
void verifyBlockInPendIO ( chid chan )
{
    int status;


    if ( ca_read_access (chan) ) {
        dbr_float_t req;
        dbr_float_t resp;

        showProgressBegin ();

        req = 56.57f;
        resp = -99.99f;
        SEVCHK ( ca_put (DBR_FLOAT, chan, &req), NULL );
        SEVCHK ( ca_get (DBR_FLOAT, chan, &resp), NULL );
        status = ca_pend_io (1.0e-12);
        if ( status == ECA_NORMAL ) {
            if ( resp != req ) {
                printf (
    "get block test failed - val written %f\n", req );
                printf (
    "get block test failed - val read %f\n", resp );
                assert ( 0 );
            }
        }
        else if ( resp != -99.99f ) {
            printf ( "CA didnt block for get to return?\n" );
        }
            
        req = 33.44f;
        resp = -99.99f;
        SEVCHK ( ca_put (DBR_FLOAT, chan, &req), NULL );
        SEVCHK ( ca_get (DBR_FLOAT, chan, &resp), NULL );
        SEVCHK ( ca_pend_io (2000.0) , NULL );
        if ( resp != req ) {
            printf (
    "get block test failed - val written %f\n", req);
            printf (
    "get block test failed - val read %f\n", resp);
            assert(0);
        }
        showProgressEnd ();
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
        status = ca_pend_io ( 10.0 );
        SEVCHK (status, NULL);
        if ( fabs ( fval - fretval ) > epsilon ) {
            printf ( "float test failed val written %f\n", fval );
            printf ( "float test failed val read %f\n", fretval );
            assert (0);
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
        status = ca_pend_io ( 100.0 );
        SEVCHK ( status, NULL );
        if ( fabs ( fval - fretval ) > epsilon ) {
            printf ( "double test failed val written %f\n", fval );
            printf ( "double test failed val read %f\n", fretval );
            assert ( 0 );
        }
        fval += increment;
    }
}

/*
 * Verify that we can write and then read back
 * the same analog value
 */
void verifyAnalogIO ( chid chan, int dataType, double min, double max, 
               int minExp, int maxExp, double epsilon )
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

    if ( ca_field_type ( chan ) != DBR_FLOAT && 
        ca_field_type ( chan ) != DBR_DOUBLE ) {
        printf ("skipped analog test - not an analog type\n");
        return;
    }

    showProgressBegin ();

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
            assert ( 0 );
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
            assert ( 0 );
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
            assert ( 0 );
        }
    }
    showProgressEnd ();
}

/*
 * Verify that we can write and then read back
 * the same DBR_LONG value
 */
void verifyLongIO ( chid chan )
{
    int status;

    dbr_long_t iter, rdbk, incr;
    struct dbr_ctrl_long cl;

    if ( ca_write_access ( chan ) ) {
        return;
    }

    status = ca_get ( DBR_CTRL_LONG, chan, &cl );
    SEVCHK ( status, "control long fetch failed\n" );
    status = ca_pend_io ( 10.0 );
    SEVCHK ( status, "control long pend failed\n" );

    incr = ( cl.upper_ctrl_limit - cl.lower_ctrl_limit );
    if ( incr >= 1 ) {
        showProgressBegin ();
        incr /= 1000;
        if ( incr == 0 ) {
            incr = 1;
        }
        for ( iter = cl.lower_ctrl_limit; 
            iter <= cl.upper_ctrl_limit; iter+=incr ) {

            status = ca_put ( DBR_LONG, chan, &iter );
            status = ca_get ( DBR_LONG, chan, &rdbk );
            status = ca_pend_io ( 10.0 );
            SEVCHK ( status, "get pend failed\n" );
            assert ( iter == rdbk );
        }
        showProgressEnd ();
    }
    else {
        printf ( "strange limits configured for channel \"%s\"\n", ca_name (chan) );
    }
}

/*
 * Verify that we can write and then read back
 * the same DBR_SHORT value
 */
void verifyShortIO ( chid chan )
{
    int status;

    dbr_short_t iter, rdbk, incr;
    struct dbr_ctrl_short cl;

    if ( ca_write_access ( chan ) ) {
        return;
    }

    status = ca_get ( DBR_CTRL_SHORT, chan, &cl );
    SEVCHK ( status, "control short fetch failed\n" );
    status = ca_pend_io ( 10.0 );
    SEVCHK ( status, "control short pend failed\n" );

    incr = (dbr_short_t) ( cl.upper_ctrl_limit - cl.lower_ctrl_limit );
    if ( incr >= 1 ) {
        showProgressBegin ();

        incr /= 1000;
        if ( incr == 0 ) {
            incr = 1;
        }
        for ( iter = (dbr_short_t) cl.lower_ctrl_limit; 
            iter <= (dbr_short_t) cl.upper_ctrl_limit; iter += incr ) {

            status = ca_put ( DBR_SHORT, chan, &iter );
            status = ca_get ( DBR_SHORT, chan, &rdbk );
            status = ca_pend_io ( 10.0 );
            SEVCHK ( status, "get pend failed\n" );
            assert ( iter == rdbk );
        }
        showProgressEnd ();
    }
    else {
        printf ( "Strange limits configured for channel \"%s\"\n", ca_name (chan) );
    }
}

void verifyHighThroughputRead ( chid chan )
{
    int status;
    unsigned i;

    /*
     * verify we dont jam up on many uninterrupted
     * solicitations
     */
    if ( ca_read_access (chan) ) {
        dbr_float_t temp;
        showProgressBegin ();
        for ( i=0; i<10000; i++ ) {
            status = ca_get ( DBR_FLOAT, chan, &temp );
            SEVCHK ( status ,NULL );
        }
        status = ca_pend_io (2000.0);
        SEVCHK ( status, NULL );
        showProgressEnd ();
    }
    else {
        printf ( "Skipped highthroughput read test - no read access\n" );
    }
}

void verifyHighThroughputWrite ( chid chan ) 
{
    int status;
    unsigned i;

    if (ca_write_access ( chan ) ) {
        showProgressBegin ();
        for ( i=0; i<10000; i++ ) {
            dbr_double_t fval = 3.3;
            status = ca_put ( DBR_DOUBLE, chan, &fval );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_pend_io (2000.0), NULL );
        showProgressEnd ();
    }
    else{
        printf("Skipped multiple put test - no write access\n");
    }
}

/*
 * verify we dont jam up on many uninterrupted
 * get callback requests
 */
void verifyHighThroughputReadCallback ( chid chan ) 
{
    unsigned i;
    int status;

    if ( ca_read_access ( chan ) ) { 
        unsigned count = 0u;
        showProgressBegin ();
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
        showProgressEnd ();
    }
    else {
        printf ( "Skipped multiple get cb test - no read access\n" );
    }
}

/*
 * verify we dont jam up on many uninterrupted
 * put callback request
 */
void verifyHighThroughputWriteCallback ( chid chan ) 
{
    unsigned i;
    int status;

    if ( ca_write_access (chan) && ca_v42_ok (chan) ) {
        unsigned count = 0u;
        showProgressBegin ();
        for ( i=0; i<10000; i++ ) {
            dbr_float_t fval = 3.3F;
            status = ca_array_put_callback (
                    DBR_FLOAT, 1, chan, &fval,
                    nUpdatesTester, &count );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_flush_io (), NULL );
        while ( count < 10000u ) {
            epicsThreadSleep ( 0.1 );
            ca_poll (); /* emulate typical GUI */
        }
        showProgressEnd ();
    }
    else {
        printf ( "Skipped multiple put cb test - no write access\n" );
    }
}

void verifyBadString ( chid chan )
{
    int status;

    /*
     * verify that we detect that a large string has been written
     */
    if ( ca_write_access (chan) ) {
        dbr_string_t    stimStr;
        dbr_string_t    respStr;
        showProgressBegin ();
        memset (stimStr, 'a', sizeof (stimStr) );
        status = ca_array_put ( DBR_STRING, 1u, chan, stimStr );
        assert ( status != ECA_NORMAL );
        sprintf ( stimStr, "%u", 8u );
        status = ca_array_put ( DBR_STRING, 1u, chan, stimStr );
        assert ( status == ECA_NORMAL );
        status = ca_array_get ( DBR_STRING, 1u, chan, respStr );
        assert ( status == ECA_NORMAL );
        status = ca_pend_io ( 0.0 );
        assert ( status == ECA_NORMAL );
        if ( strcmp ( stimStr, respStr ) ) {
            printf (
    "Test fails if stim \"%s\" isnt roughly equiv to resp \"%s\"\n",
                stimStr, respStr);
        }
        showProgressEnd ();
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
void test_sync_groups ( chid chan )
{
    int status;
    CA_SYNC_GID gid1=0;
    CA_SYNC_GID gid2=0;

    if ( ! ca_v42_ok ( chan ) ) {
        printf ( "skipping sycnc group test - serveris on wron version\n" );
    }

    showProgressBegin ();

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
    status = ca_sg_block ( gid1, 15.0 );
    SEVCHK ( status, "SYNC GRP1" );
    status = ca_sg_block ( gid2, 15.0 );
    SEVCHK ( status, "SYNC GRP2" );
    status = ca_sg_delete  ( gid1 );
    SEVCHK ( status, NULL );
    status = ca_sg_delete ( gid2 );
    SEVCHK ( status, NULL );

    showProgressEnd ();
}

/*
 * performDeleteTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 */
void performDeleteTest ( chid chan )
{
    unsigned count = 0u;
    evid mid[1000];
    dbr_float_t temp, getResp;
    unsigned i;
    
    showProgressBegin ();

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
    SEVCHK ( ca_pend_io ( 1000.0 ), NULL );

    showProgress ();

    /*
     * attempt to generate heavy event traffic before initiating
     * the monitor delete
     */  
    if ( ca_write_access (chan) ) {
        for ( i=0; i < 10; i++ ) {
            temp = (float) i;
            SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
        }
    }

    showProgress ();

    /*
     * without pausing begin deleting the event subscriptions 
     * while the queue is full
     */
    for ( i=0; i < NELEMENTS (mid); i++ ) {
        SEVCHK ( ca_clear_event ( mid[i]), NULL );
    }

    showProgress ();

    /*
     * force all of the clear event requests to
     * complete
     */
    SEVCHK ( ca_get (DBR_FLOAT,chan,&temp), NULL );
    SEVCHK ( ca_pend_io (1000.0), NULL );

    showProgressEnd ();
} 

evid globalEventID;

void selfDeleteEvent ( struct event_handler_args args )
{
    int status;
    status = ca_clear_event ( globalEventID );
    assert ( status == ECA_NORMAL ); 
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
                0, &globalEventID);
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
    if ( args.status != ECA_NORMAL ) {
        arrayEventExceptionNotifyComplete = 1;
    }
}
void exceptionTest ( chid chan )
{
    int status;

    showProgressBegin ();

    /*
     * force a get exception to occur
     */
    {
        dbr_put_ackt_t *pRS;

        acctstExceptionCount = 0u;
        status = ca_add_exception_event ( acctstExceptionNotify, 0 );
        SEVCHK ( status, "exception notify install failed" );

        pRS = malloc ( ca_element_count (chan) * sizeof (*pRS) );
        assert ( pRS );
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
        SEVCHK  ( status, "array read request failed" );
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
        SEVCHK ( status, "array subscription notify install failed" );
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
        dbr_string_t *pWS;
        unsigned i;

        acctstExceptionCount = 0u;
        status = ca_add_exception_event ( acctstExceptionNotify, 0 );
        SEVCHK ( status, "exception notify install failed" );

        pWS = malloc ( ca_element_count (chan) * MAX_STRING_SIZE );
        assert ( pWS );
        for ( i = 0; i < ca_element_count (chan); i++ ) {
            strcpy ( pWS[i], "@#$%" );
        }
        status = ca_array_put ( DBR_STRING, 
            ca_element_count (chan), chan, pWS ); 
        SEVCHK  ( status, "array put request failed" );
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
        assert ( pWS );
        for ( i = 0; i < ca_element_count (chan); i++ ) {
            strcpy ( pWS[i], "@#$%" );
        }
        arrayEventExceptionNotifyComplete = 0u;
        status = ca_array_put_callback ( DBR_STRING, 
            ca_element_count (chan), chan, pWS,
            arrayEventExceptionNotify, 0); 
        SEVCHK  ( status, "array put callback request failed" );
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

    showProgressEnd ();
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
        if ( pWF[i] != pRF[i] ) {
            assert ( 0 );
        }
    }
    arrayReadNotifyComplete = 1;
}
void arrayWriteNotify ( struct event_handler_args args )
{
    arrayWriteNotifyComplete = 1;
}

void arrayTest ( chid chan )
{
    dbr_double_t *pRF, *pWF;
    unsigned i;
    int status;
    evid id;

    if ( ! ca_write_access ( chan ) ) {
        printf ( "skipping array test - no write access\n" );
    }

    showProgressBegin ();

    pRF = (dbr_double_t *) calloc ( ca_element_count (chan), sizeof (*pRF) );
    assert ( pRF != NULL );

    pWF = (dbr_double_t *) calloc ( ca_element_count (chan), sizeof (*pWF) );
    assert ( pWF != NULL );

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
    status = ca_pend_io ( 30.0 );
    SEVCHK ( status, "array read failed" );

    /*
     * verify read response matches values written
     */
    for ( i = 0; i < ca_element_count ( chan ); i++ ) {
        assert ( pWF[i] == pRF[i] );
    }

    /*
     * read back the array as strings
     */
    {
        char *pRS;

        pRS = malloc ( ca_element_count (chan) * MAX_STRING_SIZE );
        assert ( pRS );
        status = ca_array_get ( DBR_STRING, 
            ca_element_count (chan), chan, pRS ); 
        SEVCHK  ( status, "array read request failed" );
        status = ca_pend_io ( 30.0 );
        SEVCHK ( status, "array read failed" );
        free ( pRS );
    }

    /*
     * write some random numbers into the array
     */
    for ( i = 0; i < ca_element_count (chan); i++ ) {
        pWF[i] =  rand ();
        pRF[i] = - pWF[i];
    }
    status = ca_array_put_callback ( DBR_DOUBLE, ca_element_count ( chan ), 
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
     * a get request should fail when the array size is
     * too large
     */
    {
        status = ca_array_get ( DBR_DOUBLE, 
            ca_element_count (chan)+1, chan, pRF ); 
        assert ( status == ECA_BADCOUNT );
    }

    free ( pRF );
    free ( pWF );

    showProgressEnd ();
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
    assert ( fabs(accuracy) < 10.0 );
}

void caTaskExistTest ()
{
    int status;

    epicsTimeStamp end_time;
    epicsTimeStamp start_time;
    dbr_double_t delay;

    epicsTimeGetCurrent ( &start_time );
    printf ( "entering ca_task_exit()\n" );
    status = ca_task_exit ();
    SEVCHK ( status, NULL );
    epicsTimeGetCurrent ( &end_time );
    delay = epicsTimeDiffInSeconds ( &end_time, &start_time );
    printf ( "in ca_task_exit() for %f sec\n", delay );
}

void verifyDataTypeMacros ()
{
    int type;

    type = dbf_type_to_DBR ( DBF_SHORT );
    assert ( type == DBR_SHORT );
    type = dbf_type_to_DBR_STS ( DBF_SHORT );
    assert ( type == DBR_STS_SHORT );
    type = dbf_type_to_DBR_GR ( DBF_SHORT );
    assert ( type == DBR_GR_SHORT );
    type = dbf_type_to_DBR_CTRL ( DBF_SHORT );
    assert ( type == DBR_CTRL_SHORT );
    type = dbf_type_to_DBR_TIME ( DBF_SHORT );
    assert ( type == DBR_TIME_SHORT );
    assert ( strcmp ( dbr_type_to_text( DBR_SHORT ), "DBR_SHORT" )  == 0 );
    assert ( strcmp ( dbf_type_to_text( DBF_SHORT ), "DBF_SHORT" )  == 0 );
    assert ( dbr_type_is_SHORT ( DBR_SHORT ) );
    assert ( dbr_type_is_valid ( DBR_SHORT ) );
    assert ( dbf_type_is_valid ( DBF_SHORT ) );
    {
        int dataType;
        dbf_text_to_type ( "DBF_SHORT", dataType ); 
        assert ( dataType == DBF_SHORT );
        dbr_text_to_type ( "DBR_SHORT", dataType ); 
        assert ( dataType == DBR_SHORT );
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

dbr_float_t performMonitorUpdateTestPattern ( unsigned iter )
{
    return ( (float) iter ) * 10.12345f + 10.7f;
}


/*
 * performMonitorUpdateTest
 *
 * 1) verify we can add many monitors at once
 * 2) verify that under heavy load the last monitor
 *      returned is the last modification sent
 */
void performMonitorUpdateTest ( chid chan )
{
    eventTest       test[100];
    dbr_float_t     temp, getResp;
    unsigned        i, j;
    unsigned        flowCtrlCount = 0u;
    unsigned        prevPassCount;
    unsigned        tries;

    if ( ! ca_read_access ( chan ) ) {
        return;
    }
    
    showProgressBegin ();

    /*
     * set channel to known value
     */
    temp = 1.0;
    SEVCHK ( ca_put ( DBR_FLOAT, chan, &temp ), NULL );

    for ( i = 0; i < NELEMENTS(test); i++ ) {
        test[i].count = 0;
        test[i].lastValue = -1.0;
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
    SEVCHK ( ca_pend_io ( 1000.0 ) ,NULL );
            
    showProgress ();

    /*
     * pass the test only if we get the first monitor update
     */
    tries = 0;
    while ( 1 ) {
        unsigned nComplete = 0u;
        epicsThreadSleep ( 0.1 );
        ca_poll (); /* emulate typical GUI */
        for ( i = 0; i < NELEMENTS ( test ); i++ ) {
            if ( test[i].count > 0 ) {
                if ( test[i].lastValue == temp ) {
                    nComplete++;
                }
            }
        }
        if ( nComplete == NELEMENTS ( test ) ) {
            break;
        }
        printf ( "-" );
        fflush ( stdout );
        assert ( tries++ < 50 );
    }

    showProgress ();

    /*
     * attempt to uncover problems where the last event isnt sent
     * and hopefully get into a flow control situation
     */   
    prevPassCount = 0u;
    for ( i = 0; i < 10; i++ ) {
        for ( j = 0; j < NELEMENTS(test); j++ ) {
            SEVCHK ( ca_clear_event ( test[j].id ), NULL );
            test[j].count = 0;
            test[j].lastValue = -1.0;
            SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, updateTestEvent,
                &test[j], &test[j].id ) , NULL );
        } 

        for ( j = 0; j <= i; j++ ) {
            temp = performMonitorUpdateTestPattern ( j );
            SEVCHK ( ca_put ( DBR_FLOAT, chan, &temp ), NULL );
        }

        /*
         * wait for the above to complete
         */
        getResp = -1;
        SEVCHK ( ca_get ( DBR_FLOAT, chan, &getResp ), NULL );
        SEVCHK ( ca_pend_io ( 1000.0 ), NULL );

        assert ( getResp == temp );

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
                assert ( test[j].count <= i + 2 );
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
                assert ( tries++ < 500 );
                if ( tries % 50 == 0 ) {
                    for ( j = 0; j <= i; j++ ) {
                        dbr_float_t pat = performMonitorUpdateTestPattern ( j );
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

    showProgress ();

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
    SEVCHK ( ca_pend_io ( 1000.0 ), NULL );

    /* printf ( "flow control bypassed %u events\n", flowCtrlCount ); */

    showProgressEnd ();
} 

void verifyReasonableBeaconPeriod ( chid chan )
{
    if ( ca_get_ioc_connection_count () > 0 ) {
        double beaconPeriod, expectedBeaconPeriod, error, percentError;
        unsigned attempts = 0u;

        long status = envGetDoubleConfigParam ( 
            &EPICS_CA_BEACON_PERIOD, &expectedBeaconPeriod );
        assert ( status >=0 );
    
        while ( 1 ) {
            static const unsigned maxAttempts = 10u;
            const double delay = ( expectedBeaconPeriod * 4 ) / maxAttempts;

            beaconPeriod = ca_beacon_period ( chan );
            if ( beaconPeriod >= 0.0 ) {
                error = beaconPeriod - expectedBeaconPeriod;
                percentError = fabs ( error ) / expectedBeaconPeriod;
                if ( percentError < 0.1 ) {
                    break;
                }
                printf ( "Beacon period error estimate = %f sec.\n", error );
            }
            assert ( attempts++ < maxAttempts );
            printf ( "Waiting for a better beacon period estimate result (%f sec).\n",
                attempts * delay );
            epicsThreadSleep ( delay );
        }
    }
}

void verifyOldPend ()
{
    int status;
    /*
     * at least verify that the old ca_pend() is in the symbol table
     */
    status = ca_pend ( 100000.0, 1 );
    assert ( status = ECA_NORMAL );
    status = ca_pend ( 1e-12, 0 );
    assert ( status = ECA_TIMEOUT );
}

void verifyTimeStamps ( chid chan )
{
    struct dbr_time_double first, last;
    epicsTimeStamp localTime;
    char buf[128];
    size_t length;
    double diff;
    int status;

    status = epicsTimeGetCurrent ( &localTime );
    assert ( status >= 0 );

    status = ca_get ( DBR_TIME_DOUBLE, chan, &first );
    SEVCHK ( status, "fetch of dbr time double failed\n" );
    status = ca_pend_io ( 20.0 );
    assert ( status = ECA_NORMAL );

    status = ca_get ( DBR_TIME_DOUBLE, chan, &last );
    SEVCHK ( status, "fetch of dbr time double failed\n" );
    status = ca_pend_io ( 20.0 );
    assert ( status = ECA_NORMAL );

    length = epicsTimeToStrftime ( buf, sizeof ( buf ), 
        "%a %b %d %H:%M:%S %Y", &first.stamp );
    assert ( length );
    printf ("Processing time of channel \"%s\" was \"%s\"\n", ca_name ( chan ), buf );

    diff = epicsTimeDiffInSeconds ( &last.stamp, &first.stamp );
    printf ("Time difference between two successive reads was %g sec\n", diff );

    diff = epicsTimeDiffInSeconds ( &first.stamp, &localTime );
    printf ("Time difference between client and server %g sec\n", diff );
}

void verifyImmediateTearDown ()
{
    ca_task_initialize ();
    ca_task_exit ();
}

int acctst ( char *pName, unsigned channelCount, unsigned repetitionCount )
{
    chid chan;
    int status;
    unsigned i;
    appChan *pChans;
    unsigned connections;

    printf ( "CA Client V%s, channel name \"%s\"\n", ca_version (), pName );

    epicsEnvSet ( "EPICS_CA_MAX_ARRAY_BYTES", "10000000" ); 

    verifyImmediateTearDown ();

    verifyDataTypeMacros ();

    connections = ca_get_ioc_connection_count ();
    assert ( connections == 0u );

    status = ca_search ( pName, &chan );
    SEVCHK ( status, NULL );
    assert ( strcmp ( pName, ca_name (chan) ) == 0 );
    status = ca_pend_io ( 100.0 );
    SEVCHK ( status, NULL );

    connections = ca_get_ioc_connection_count ();
    assert ( connections == 1u || connections == 0u );
    if ( connections == 0u ) {
        printf ( "testing with a local channel\n" );
    }

    verifyTimeStamps ( chan );
    verifyOldPend ();
    exceptionTest ( chan );
    arrayTest ( chan ); 
    verifyMonitorSubscriptionFlushIO ( chan );
    monitorSubscriptionFirstUpdateTest ( pName, chan );
    performGrEnumTest ( chan );
    performCtrlDoubleTest ( chan );
    verifyBlockInPendIO ( chan );
    verifyAnalogIO ( chan, DBR_FLOAT, FLT_MIN, FLT_MAX, 
               FLT_MIN_EXP, FLT_MAX_EXP, FLT_EPSILON );
    verifyAnalogIO ( chan, DBR_DOUBLE, DBL_MIN, DBL_MAX, 
               DBL_MIN_EXP, DBL_MAX_EXP, DBL_EPSILON );
    verifyLongIO ( chan );
    verifyShortIO ( chan );
    verifyHighThroughputRead ( chan );
    verifyHighThroughputWrite ( chan );
    verifyHighThroughputReadCallback ( chan );
    verifyHighThroughputWriteCallback ( chan );
    verifyBadString ( chan );
    test_sync_groups ( chan );
    performDeleteTest ( chan );
    eventClearTest ( chan );
    performMonitorUpdateTest ( chan );

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
    ca_client_status ( 0u );

    pChans = calloc ( channelCount, sizeof ( *pChans ) );
    assert ( pChans );

    for ( i = 0; i < channelCount; i++ ) {
        strncpy ( pChans[ i ].name, pName, sizeof ( pChans[ i ].name ) );
        pChans[ i ].name[ sizeof ( pChans[i].name ) - 1 ] = '\0';
    }

    verifyConnectionHandlerConnect ( pChans, channelCount, repetitionCount );
    verifyBlockingConnect ( pChans, channelCount, repetitionCount );
    verifyClear ( pChans );

    verifyReasonableBeaconPeriod ( chan );

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
        SEVCHK ( ca_pend_io ( 2000.0 ), NULL );
    }
#endif

    /* test that ca_task_exit () works when there is still one channel remaining */
    /* status = ca_clear_channel ( chan ); */
    /* SEVCHK ( status, NULL ); */

    caTaskExistTest ();

    printf ( "\nTest Complete\n" );

    return 0;
}


