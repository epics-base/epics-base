
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
#include "epicsAssert.h"
#include "tsStamp.h"

/*
 * CA 
 */
#include "cadef.h"

#include "caDiagnostics.h"

#ifndef min
#define min(A,B) ((A)>(B)?(B):(A))
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NELEMENTS
#define NELEMENTS(A) ( sizeof (A) / sizeof (A[0]) )
#endif

typedef struct appChan {
    char name[64];
    chid channel;
    evid subscription;
    unsigned char connected;
    unsigned subscriptionUpdateCount;
    unsigned accessUpdateCount;
    unsigned connectionUpdateCount;
    unsigned getCallbackCount;
} appChan;

unsigned subscriptionUpdateCount;
unsigned accessUpdateCount;
unsigned connectionUpdateCount;
unsigned getCallbackCount;

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

void monitorSubscriptionFirstUpdateTest ( chid chan )
{
    int status;
    unsigned eventCount = 0u;
    unsigned waitCount = 0u;
    evid id;

    /*
     * verify that the first event arrives
     */
    status = ca_add_event ( DBR_FLOAT, 
                chan, nUpdatesTester, &eventCount, &id );
    SEVCHK (status, 0);
    while ( eventCount < 1 && waitCount++ < 100 ) {
        ca_pend_event ( 0.01 );
    }
    assert ( eventCount > 0 );
    status = ca_clear_event ( id );
    SEVCHK (status, 0);

    showProgress ();
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
        assert ( pChan->accessUpdateCount > 0u );
        assert ( ! pChan->connected );
        pChan->connected = 1;
        status = ca_get_callback ( DBR_GR_STRING, args.chid, getCallbackStateChange, pChan );
        SEVCHK (status, 0);
        status = ca_flush_io ();
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

    subscriptionUpdateCount = 0u;
    accessUpdateCount = 0u;
    connectionUpdateCount = 0u;
    getCallbackCount = 0u;

    for ( i = 0; i < repetitionCount; i++ ) {
        for ( j = 0u; j < chanCount; j++ ) {

            pChans[j].subscriptionUpdateCount = 0u;
            pChans[j].accessUpdateCount = 0u;
            pChans[j].connectionUpdateCount = 0u;
            pChans[j].getCallbackCount = 0u;
            pChans[j].connected = 0u;

	        status = ca_search_and_connect ( pChans[j].name,
                &pChans[j].channel, connectionStateChange, &pChans[j]);
            SEVCHK ( status, NULL );

            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
            SEVCHK ( status, NULL );

            status = ca_add_event ( DBR_GR_STRING, pChans[j].channel,
                    subscriptionStateChange, &pChans[j], &pChans[j].subscription );
            SEVCHK ( status, NULL );

            assert ( ca_test_io () == ECA_IODONE );
        }

        while ( connectionUpdateCount < chanCount || 
            getCallbackCount < chanCount ) {
            ca_pend_event ( 1.0 );
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

        for ( j = 0u; j < chanCount; j += 2 ) {
            status = ca_clear_event ( pChans[j].subscription );
            SEVCHK ( status, NULL );
        }

        for ( j = 0u; j < chanCount; j++ ) {
            status = ca_clear_channel ( pChans[j].channel );
            SEVCHK ( status, NULL );
        }
    }
    showProgress ();
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
    unsigned connections;
    unsigned i, j;

    connections = ca_get_ioc_connection_count ();
    assert ( connections == 0u );

    for ( i = 0; i < repetitionCount; i++ ) {

        for ( j = 0u; j < chanCount; j++ ) {
            pChans[j].subscriptionUpdateCount = 0u;
            pChans[j].accessUpdateCount = 0u;
            pChans[j].connectionUpdateCount = 0u;
            pChans[j].getCallbackCount = 0u;
            pChans[j].connected = 0u;

            status = ca_search_and_connect ( pChans[j].name, &pChans[j].channel, NULL, &pChans[j] );
            SEVCHK ( status, NULL );

            if ( ca_state ( pChans[j].channel ) == cs_conn ) {
                assert ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) == TRUE );
            }
            else {
                /*
                 * its possible for the channel to connect while this test is going on
                 */
                unsigned ctr;
                for ( ctr = 0u; ctr < 100u; ctr ++ ) {
                    assert ( INVALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) == TRUE );
                    assert ( ca_test_io () == ECA_IOINPROGRESS );
                }
            }
            
            status = ca_replace_access_rights_event (
                    pChans[j].channel, accessRightsStateChange );
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

        assert ( ca_test_io () == ECA_IODONE );

        connections = ca_get_ioc_connection_count ();
        assert ( connections == 1u );

        for ( j = 0u; j < chanCount; j++ ) {
            assert ( VALID_DB_REQ ( ca_field_type ( pChans[j].channel ) ) == TRUE );
            assert ( ca_state ( pChans[j].channel ) == cs_conn );
            SEVCHK ( ca_clear_channel ( pChans[j].channel ), NULL );
        }

        /*
         * verify that connections to IOC's that are 
         * not in use are dropped
         */
        if ( ca_get_ioc_connection_count() != 0u ) {
            ca_pend_event ( 1.0 );
            j=0;
            while ( ca_get_ioc_connection_count() != 0u ) {
                printf ( "-" );
                ca_pend_event ( 1.0 );
                assert ( ++j < 100 );
                fflush ( stdout );
            }
            printf ("\n");
        }
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
            ca_pend_event ( 0.1 );
            if ( ca_state( pChans[0].channel ) != cs_conn ) {
                printf ( "waiting on pend io verify connect" );
                fflush ( stdout );
                while ( ca_state ( pChans[0].channel ) != cs_conn ) {
                    printf ( "." );
                    fflush ( stdout );
                    ca_pend_event ( 0.1 );
                }
                printf ( "done\n" );
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
    showProgress ();
}

/*
 * 1) verify that use of NULL evid does not cause problems
 * 2) verify clear before connect
 */
void verifyClear ( appChan *pChans )
{
    int status;

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
            pChans[0].channel, subscriptionStateChange, NULL, NULL );
    SEVCHK ( status, NULL );

    status = ca_clear_channel ( pChans[0].channel );
    SEVCHK ( status, NULL );
    showProgress ();
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
    showProgress ();
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
    status = ca_pend_io (20.0);
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
    showProgress ();
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
    }
    else {
        printf ("skipped pend IO block test - no read access\n");
    }
    showProgress ();
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
            printf ( "float test failed val written %f\n", fval );
            printf ( "float test failed val read %f\n", fretval );
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
    showProgress ();
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
        showProgress ();
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

    incr = ( cl.upper_ctrl_limit - cl.lower_ctrl_limit );
    if ( incr >= 1 ) {
        incr /= 1000;
        if ( incr == 0 ) {
            incr = 1;
        }
        for ( iter = cl.lower_ctrl_limit; 
            iter <= cl.upper_ctrl_limit; iter+=incr ) {

            status = ca_put ( DBR_SHORT, chan, &iter );
            status = ca_get ( DBR_SHORT, chan, &rdbk );
            status = ca_pend_io ( 10.0 );
            SEVCHK ( status, "get pend failed\n" );
            assert ( iter == rdbk );
        }
        showProgress ();
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
        for ( i=0; i<10000; i++ ) {
            status = ca_get ( DBR_FLOAT, chan, &temp );
            SEVCHK ( status ,NULL );
        }
        status = ca_pend_io (2000.0);
        SEVCHK ( status, NULL );
        showProgress ();
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
        for ( i=0; i<10000; i++ ) {
            dbr_double_t fval = 3.3;
            status = ca_put ( DBR_DOUBLE, chan, &fval );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_pend_io (2000.0), NULL );
        showProgress ();
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
        for ( i=0; i<10000; i++ ) {
            status = ca_array_get_callback (
                    DBR_FLOAT, 1, chan, nUpdatesTester, &count );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_flush_io (), NULL );
        while ( count < 10000u ) {
            ca_pend_event ( 0.1 );
        }
        showProgress ();
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
        for ( i=0; i<10000; i++ ) {
            dbr_float_t fval = 3.3F;
            status = ca_array_put_callback (
                    DBR_FLOAT, 1, chan, &fval,
                    nUpdatesTester, &count );
            SEVCHK ( status, NULL );
        }
        SEVCHK ( ca_flush_io (), NULL );
        while ( count < 10000u ) {
            ca_pend_event ( 0.1 );
        }
        showProgress ();
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
        printf (
"Test fails if stim \"%s\" isnt roughly equiv to resp \"%s\"\n",
            stimStr, respStr);
        showProgress ();
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

    showProgress ();
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
    
    for ( i=0; i < NELEMENTS (mid); i++ ) {
        SEVCHK ( ca_add_event ( DBR_GR_FLOAT, chan, noopSubscriptionStateChange,
            &count, &mid[i]) , NULL );
    }

    showProgress ();

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
     * without pausing begin deleting the event suvbscriptions 
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

    showProgress ();
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
}

/*
 * array test
 *
 * verify that we can at least write and read back the same array
 * if multiple elements are present
 */
void arrayTest ( chid chan )
{
    dbr_double_t *pRF, *pWF;
    unsigned i;
    int status;

    if ( ! ca_write_access ( chan ) ) {
        printf ( "skipping array test - no write access\n" );
    }

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
    for ( i = 0; i < ca_element_count (chan); i++ ) {
        assert ( pWF[i] == pRF[i] );
    }

    /*
     * read back the array as strings
     */
    {
        /* clip to 16k message buffer limit */
        unsigned maxElem = ( ( 1 << 14 ) - 16 ) / MAX_STRING_SIZE;
        unsigned nElem = min ( maxElem, ca_element_count (chan) );
        char *pRS = malloc ( nElem * MAX_STRING_SIZE );

        assert (pRS);
        status = ca_array_get ( DBR_STRING, nElem, chan, pRS ); 
        SEVCHK  ( status, "array read request failed" );
        status = ca_pend_io ( 30.0 );
        SEVCHK ( status, "array read failed" );
        free ( pRS );
    }

    free ( pRF );
    free ( pWF );

    showProgress ();
}

/*
 * pend_event_delay_test()
 */
void pend_event_delay_test(dbr_double_t request)
{
    int     status;
    TS_STAMP    end_time;
    TS_STAMP    start_time;
    dbr_double_t    delay;
    dbr_double_t    accuracy;

    tsStampGetCurrent(&start_time);
    status = ca_pend_event(request);
    if (status != ECA_TIMEOUT) {
        SEVCHK(status, NULL);
    }
    tsStampGetCurrent(&end_time);
    delay = tsStampDiffInSeconds(&end_time,&start_time);
    accuracy = 100.0*(delay-request)/request;
    printf("CA pend event delay = %f sec results in error = %f %%\n",
        request, accuracy);
    assert (fabs(accuracy) < 10.0);
}

void caTaskExistTest ()
{
    int status;

    TS_STAMP end_time;
    TS_STAMP start_time;
    dbr_double_t delay;

    tsStampGetCurrent ( &start_time );
    printf ( "entering ca_task_exit()\n" );
    status = ca_task_exit ();
    SEVCHK ( status, NULL );
    tsStampGetCurrent ( &end_time );
    delay = tsStampDiffInSeconds ( &end_time, &start_time );
    printf ( "in ca_task_exit() for %f sec\n", delay );
}

int acctst ( char *pName, unsigned channelCount, unsigned repetitionCount )
{
    chid chan;
    int status;
    unsigned i;
    appChan *pChans;

    printf ( "CA Client V%s, channel name \"%s\"\n", ca_version (), pName );

    pChans = calloc ( channelCount, sizeof ( *pChans ) );
    assert ( pChans );

    for ( i = 0; i < channelCount; i++ ) {
        strncpy ( pChans[ i ].name, pName, sizeof ( pChans[ i ].name ) );
        pChans[ i ].name[ sizeof ( pChans[i].name ) - 1 ] = '\0';
    }

    verifyBlockingConnect ( pChans, channelCount, repetitionCount );
    verifyConnectionHandlerConnect ( pChans, channelCount, repetitionCount );
    verifyClear ( pChans );

    status = ca_search ( pName, &chan );
    SEVCHK ( status, NULL );
    status = ca_pend_io ( 100.0 );
    SEVCHK ( status, NULL );

    monitorSubscriptionFirstUpdateTest ( chan );
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
    /* performMonitorUpdateTest ( chan ); */
    performDeleteTest ( chan );
    eventClearTest ( chan );
    arrayTest ( chan ); 

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

    {
        TS_STAMP    end_time;
        TS_STAMP    start_time;
        dbr_double_t    delay;
        dbr_double_t    request = 15.0;
        dbr_double_t    accuracy;

        tsStampGetCurrent(&start_time);
        printf ("waiting for events for %f sec\n", request);
        status = ca_pend_event (request);
        if ( status != ECA_TIMEOUT ) {
            SEVCHK ( status, NULL );
        }
        tsStampGetCurrent ( &end_time );
        delay = tsStampDiffInSeconds ( &end_time, &start_time );
        accuracy = 100.0 * ( delay - request ) / request;
        printf ( "CA pend event delay accuracy = %f %%\n", accuracy );
    }
#endif

    /*
     * CA pend event delay accuracy test
     * (CA asssumes that search requests can be sent
     * at least every 25 mS on all supported os)
     */
    pend_event_delay_test ( 1.0 );
    pend_event_delay_test ( 0.1 );
    pend_event_delay_test ( 0.25 ); 

    caTaskExistTest ();

    printf ( "\nTest Complete\n" );

    return 0;
}

/*
 * write_event ()
 */
void write_event (struct event_handler_args args)
{
    static unsigned iterations = 100;
    int status;
    dbr_float_t *pFloat = (dbr_float_t *) args.dbr;
    dbr_float_t a;

    if ( ! args.dbr ) {
        return;
    }

    if ( iterations > 0 ) {
        iterations--;

        a = *pFloat;
        a += 10.1F;

        status = ca_array_put ( DBR_FLOAT, 1, args.chid, &a);
        SEVCHK ( status, NULL );
        SEVCHK ( ca_flush_io (), NULL );
    }
}

/*
 * accessSecurity_cb()
 */
void accessSecurity_cb(struct access_rights_handler_args args)
{
#   ifdef DEBUG
        printf( "%s on %s has %s/%s access\n",
            ca_name(args.chid),
            ca_host_name(args.chid),
            ca_read_access(args.chid)?"read":"noread",
            ca_write_access(args.chid)?"write":"nowrite");
#   endif
}


typedef struct {
    evid            id;
    dbr_float_t     lastValue;
    unsigned        count;
} eventTest;

/*
 * updateTestEvent ()
 */
void updateTestEvent (struct event_handler_args args)
{
    eventTest   *pET = (eventTest *) args.usr;
    pET->lastValue = * (dbr_float_t *) args.dbr;
    pET->count++;
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
    eventTest       test[1000];
    dbr_float_t     temp, getResp;
    unsigned        i, j;
    unsigned        flowCtrlCount = 0u;
    unsigned        tries;
    
    if ( ! ca_read_access(chan) ) {
        return;
    }
    
    printf ( "Performing event subscription update test..." );
    fflush ( stdout );

    for ( i=0; i<NELEMENTS(test); i++ ) {
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
    SEVCHK (ca_get(DBR_FLOAT,chan,&getResp),NULL);
    SEVCHK (ca_pend_io(1000.0),NULL);
            
    /*
     * attempt to uncover problems where the last event isnt sent
     * and hopefully get into a flow control situation
     */   
    for (i=0; i<NELEMENTS(test); i++) {
        for (j=0; j<=i; j++) {
            temp = (float) j;
            SEVCHK ( ca_put (DBR_FLOAT, chan, &temp), NULL);
        }

        /*
         * wait for the above to complete
         */
        SEVCHK ( ca_get (DBR_FLOAT,chan,&getResp), NULL);
        SEVCHK ( ca_pend_io (1000.0), NULL);

        assert (getResp==temp);

        /*
         * wait for all of the monitors to have correct values
         */
        tries = 0;
        while (1) {
            unsigned passCount = 0;
            for (j=0; j<NELEMENTS(test); j++) {
                assert (test[i].count<=i);
                if (test[i].lastValue==temp) {
                    if (test[i].count<i) {
                        flowCtrlCount++;
                    }
                    test[i].lastValue = -1.0;
                    test[i].count = 0;
                    passCount++;
                }
            }
            if ( passCount==NELEMENTS(test) ) {
                break;
            }
            SEVCHK ( ca_pend_event (0.1), 0);
            printf (".");
            fflush (stdout);

            assert (tries<50);
        }
    }

    /*
     * delete the event subscriptions 
     */
    for ( i=0; i<NELEMENTS(test); i++ ) {
        SEVCHK ( ca_clear_event (test[i].id), NULL);
    }
        
    /*
     * force all of the clear event requests to
     * complete
     */
    SEVCHK ( ca_get (DBR_FLOAT,chan,&temp), NULL);
    SEVCHK ( ca_pend_io (1000.0), NULL);

    printf ("done.\n");

    printf ("flow control bypassed %u events\n", flowCtrlCount);
} 



