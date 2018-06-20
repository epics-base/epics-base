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
 *  CA performance test 
 *
 *  History 
 *  joh 09-12-89 Initial release
 *  joh 12-20-94 portability
 *
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <limits.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "epicsAssert.h"
#include "epicsTime.h"
#include "cadef.h"
#include "caProto.h"

#include "caDiagnostics.h"

#ifndef NULL
#define NULL 0
#endif

#define WAIT_FOR_ACK

typedef struct testItem {
    chid                chix;
    char                name[128];
    int                 type;
    int                 count;
    void *              pValue;	
} ti;

typedef void tf ( ti *pItems, unsigned iterations, unsigned *pInlineIter );

/*
 * test_pend()
 */
static void test_pend(
ti          *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    unsigned    i;
    int         status;

    for (i=0; i<iterations; i++) {
        status = ca_pend_event(1e-9);
        if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
            SEVCHK(status, NULL);
        }
        status = ca_pend_event(1e-9);
        if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
            SEVCHK(status, NULL);
        }
        status = ca_pend_event(1e-9);
        if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
            SEVCHK(status, NULL);
        }
        status = ca_pend_event(1e-9);
        if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
            SEVCHK(status, NULL);
        }
        status = ca_pend_event(1e-9);
        if (status != ECA_TIMEOUT && status != ECA_NORMAL) {
            SEVCHK(status, NULL);
        }
    }
    *pInlineIter = 5;
}

/*
 * test_search ()
 */
static void test_search (
ti      *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    unsigned i;
    int status;

    for ( i = 0u; i < iterations; i++ ) {
        status = ca_search ( pItems[i].name, &pItems[i].chix );
        SEVCHK (status, NULL);
    }
    status = ca_pend_io ( 0.0 );
        SEVCHK ( status, NULL );

    *pInlineIter = 1;
}

/*
 * test_sync_search()
 */
#if 0
static void test_sync_search(
ti      *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    unsigned i;
    int status;

    for (i=0u; i<iterations; i++) {
        status = ca_search (pItems[i].name, &pItems[i].chix);
        SEVCHK (status, NULL);
        status = ca_pend_io(0.0);
        SEVCHK (status, NULL);
    }

    *pInlineIter = 1;
}
#endif

/*
 * test_free ()
 */
static void test_free(
ti      *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    int status;
    unsigned i;
        
    for (i=0u; i<iterations; i++) {
        status = ca_clear_channel (pItems[i].chix);
        SEVCHK (status, NULL);
    }
    status = ca_flush_io();
    SEVCHK (status, NULL);
    *pInlineIter = 1;
}

/*
 * test_put ()
 */
static void test_put(
ti      *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    ti      *pi;
    int     status;
    dbr_int_t   val;
  
    for (pi=pItems; pi < &pItems[iterations]; pi++) {
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_put(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
    }
#ifdef WAIT_FOR_ACK
    status = ca_array_get (DBR_INT, 1, pItems[0].chix, &val);
    SEVCHK (status, NULL);
    ca_pend_io(100.0);
#endif
    status = ca_array_put(
            pItems[0].type,
            pItems[0].count,
            pItems[0].chix,
            pItems[0].pValue);
        SEVCHK (status, NULL);
    status = ca_flush_io();
        SEVCHK (status, NULL);

    *pInlineIter = 10;
}

/*
 * test_get ()
 */
static void test_get(
ti          *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    ti  *pi;
    int status;
  
    for (pi=pItems; pi<&pItems[iterations]; pi++) {
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
        SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
    }
    status = ca_pend_io(1e20);
        SEVCHK (status, NULL);

    *pInlineIter = 10;
}

/*
 * test_wait ()
 */
static void test_wait (
ti      *pItems,
unsigned    iterations,
unsigned    *pInlineIter
)
{
    ti  *pi;
    int status;
  
    for (pi=pItems; pi<&pItems[iterations]; pi++) {
        status = ca_array_get(
                pi->type,
                pi->count,
                pi->chix,
                pi->pValue);
            SEVCHK (status, NULL);
        status = ca_pend_io(100.0);
        SEVCHK (status, NULL);
    }

    *pInlineIter = 1;
}

/*
 * measure_get_latency
 */
static void measure_get_latency (ti *pItems, unsigned iterations)
{
    epicsTimeStamp end_time;
    epicsTimeStamp start_time;
    double delay;
    double X = 0u;
    double XX = 0u; 
    double max = DBL_MIN; 
    double min = DBL_MAX; 
    double mean;
    double stdDev;
    ti *pi;
    int status;

    for ( pi = pItems; pi < &pItems[iterations]; pi++ ) {
        epicsTimeGetCurrent ( &start_time );
        status = ca_array_get ( pi->type, pi->count, 
                        pi->chix, pi->pValue );
        SEVCHK ( status, NULL );
        status = ca_pend_io ( 100.0 );
        SEVCHK ( status, NULL );

        epicsTimeGetCurrent ( &end_time );

        delay = epicsTimeDiffInSeconds ( &end_time,&start_time );

        X += delay;
        XX += delay*delay;

        if ( delay > max ) {
            max = delay;
        }

        if ( delay < min ) {
            min = delay;
        }
    }

    mean = X/iterations;
    stdDev = sqrt ( XX/iterations - mean*mean );
    printf ( 
        "Get Latency - "
        "mean = %3.1f uS, "
        "std dev = %3.1f uS, "
        "min = %3.1f uS "
        "max = %3.1f uS\n",
        mean * 1e6, stdDev * 1e6, 
        min * 1e6, max * 1e6 );
}

/*
 * printSearchStat()
 */
static void printSearchStat ( const ti * pi, unsigned iterations )
{
    unsigned i;
    double  X = 0u;
    double  XX = 0u; 
    double  max = DBL_MIN; 
    double  min = DBL_MAX; 
    double  mean;
    double  stdDev;

    for ( i = 0; i < iterations; i++ ) {
        double retry = ca_search_attempts ( pi[i].chix );
        X += retry;
        XX += retry * retry;
        if ( retry > max ) {
            max = retry;
        }
        if ( retry < min ) {
            min = retry;
        }
    }

    mean = X / iterations;
    stdDev = sqrt( XX / iterations - mean * mean );
    printf ( 
        "Search tries per chan - "
        "mean = %3.1f "
        "std dev = %3.1f "
        "min = %3.1f "
        "max = %3.1f\n",
        mean, stdDev, min, max);
}

/*
 * timeIt ()
 */
void timeIt ( tf *pfunc, ti *pItems, unsigned iterations,
    unsigned nBytesSent, unsigned nBytesRecv )
{
    epicsTimeStamp      end_time;
    epicsTimeStamp      start_time;
    double              delay;
    unsigned            inlineIter;

    epicsTimeGetCurrent ( &start_time );
    (*pfunc) ( pItems, iterations, &inlineIter );
    epicsTimeGetCurrent ( &end_time );
    delay = epicsTimeDiffInSeconds ( &end_time, &start_time );
    if ( delay > 0.0 ) {
        double freq = ( iterations * inlineIter ) / delay;
        printf ( "Per Op, %8.4f uS ( %8.4f MHz )", 
            1e6 / freq, freq / 1e6 );
        if ( pItems != NULL ) {
            printf(", %8.4f snd Mbps, %8.4f rcv Mbps\n", 
                (inlineIter*nBytesSent*CHAR_BIT)/(delay*1e6),
                (inlineIter*nBytesRecv*CHAR_BIT)/(delay*1e6) );
        }
        else {
            printf ("\n");
        }
    }
}

/*
 * test ()
 */
static void test ( ti *pItems, unsigned iterations )
{
    unsigned payloadSize, dblPayloadSize;
    unsigned nBytesSent, nBytesRecv;

    payloadSize =     
        dbr_size_n ( pItems[0].type, pItems[0].count );
    payloadSize = CA_MESSAGE_ALIGN ( payloadSize );

    dblPayloadSize = dbr_size [ DBR_DOUBLE ];
    dblPayloadSize = CA_MESSAGE_ALIGN ( dblPayloadSize );
    
    if ( payloadSize > dblPayloadSize ) {
        unsigned factor = payloadSize / dblPayloadSize;
        while ( factor ) {
            if ( iterations > 10 * factor ) {
                iterations /= factor;
                break;
            }
            factor /= 2;
        }
    }

    printf ( "\t### async put test ###\n");
    nBytesSent = sizeof ( caHdr ) + CA_MESSAGE_ALIGN( payloadSize );
    nBytesRecv = 0u;
    timeIt ( test_put, pItems, iterations, 
        nBytesSent * iterations, 
        nBytesRecv * iterations );

    printf ( "\t### async get test ###\n");
    nBytesSent = sizeof ( caHdr );
    nBytesRecv = sizeof ( caHdr ) + CA_MESSAGE_ALIGN ( payloadSize );
    timeIt ( test_get, pItems, iterations, 
        nBytesSent * ( iterations ), 
        nBytesRecv * ( iterations ) );

    printf ("\t### synch get test ###\n");
    nBytesSent = sizeof ( caHdr );
    nBytesRecv = sizeof ( caHdr ) + CA_MESSAGE_ALIGN ( payloadSize );
    if ( iterations > 100 ) {
        iterations /= 100;
    }
    else if ( iterations > 10 ) {
        iterations /= 10;
    }
    timeIt ( test_wait, pItems, iterations, 
        nBytesSent * iterations,
        nBytesRecv * iterations );
}

/*
 * catime ()
 */
int catime ( const char * channelName, 
    unsigned channelCount, enum appendNumberFlag appNF )
{
    unsigned i;
    int j;
    unsigned strsize;
    unsigned nBytesSent, nBytesRecv;
    ti *pItemList;
    
    if ( channelCount == 0 ) {
        printf ( "channel count was zero\n" );
        return 0;
    }

    pItemList = calloc ( channelCount, sizeof (ti) );
    if ( ! pItemList ) {
        return -1;
    }

    SEVCHK ( ca_context_create ( ca_disable_preemptive_callback ), 
        "Unable to initialize" );

    if ( appNF == appendNumber ) {
        printf ( "Testing with %u channels named %snnn\n", 
            channelCount, channelName );
    }
    else {
        printf ( "Testing with %u channels named %s\n", 
             channelCount, channelName );
    }

    strsize = sizeof ( pItemList[0].name ) - 1;
    nBytesSent = 0;
    nBytesRecv = 0;
    for ( i=0; i < channelCount; i++ ) {
        if ( appNF == appendNumber ) {
            sprintf ( pItemList[i].name,"%.*s%.6u",
                (int) (strsize - 15u), channelName, i );
        }
        else {
            strncpy ( pItemList[i].name, channelName, strsize);
        }
        pItemList[i].name[strsize]= '\0';
        pItemList[i].count = 0;
        pItemList[i].pValue = 0;
        nBytesSent += 2 * ( CA_MESSAGE_ALIGN ( strlen ( pItemList[i].name ) ) 
                            + sizeof (caHdr) );
        nBytesRecv += 2 * sizeof (caHdr);
    }

    printf ( "Channel Connect Test\n" );
    printf ( "--------------------\n" );
    timeIt ( test_search, pItemList, channelCount, nBytesSent, nBytesRecv );
    printSearchStat ( pItemList, channelCount );
    
    for ( i = 0; i < channelCount; i++ ) {
        size_t count = ca_element_count ( pItemList[i].chix );
        size_t size = sizeof ( dbr_string_t ) * count;
        pItemList[i].count = count;
        pItemList[i].pValue = malloc ( size );
        assert ( pItemList[i].pValue );
    }

    printf (
        "channel name=%s, native type=%d, native count=%u\n",
        ca_name (pItemList[0].chix),
        ca_field_type (pItemList[0].chix),
        pItemList[0].count );

    printf ("Pend Event Test\n");
    printf ( "----------------\n" );
    timeIt ( test_pend, NULL, 100, 0, 0 );

    for ( i = 0; i < channelCount; i++ ) {
        dbr_float_t * pFltVal = ( dbr_float_t * ) pItemList[i].pValue;
        double val = i;
        val /= channelCount;
        for ( j = 0; j < pItemList[i].count; j++ ) {
            pFltVal[j] = (dbr_float_t) val;
        }
        pItemList[i].type = DBR_FLOAT; 
    }
    printf ( "DBR_FLOAT Test\n" );
    printf ( "--------------\n" );
    test ( pItemList, channelCount );

    for ( i = 0; i < channelCount; i++ ) {
        dbr_double_t * pDblVal = ( dbr_double_t * ) pItemList[i].pValue;
        double val = i;
        val /= channelCount;
        for ( j = 0; j < pItemList[i].count; j++ ) {
            pDblVal[j] = (dbr_double_t) val;
        }
        pItemList[i].type = DBR_DOUBLE; 
    }
    printf ( "DBR_DOUBLE Test\n" );
    printf ( "---------------\n" );
    test ( pItemList, channelCount );

    
    for ( i = 0; i < channelCount; i++ ) {
        dbr_string_t * pStrVal = ( dbr_string_t * ) pItemList[i].pValue;
        double val = i;
        val /= channelCount;
        for ( j = 0; j < pItemList[i].count; j++ ) {
            sprintf ( pStrVal[j], "%f", val );
        }
        pItemList[i].type = DBR_STRING; 
    }
    printf ( "DBR_STRING Test\n" );
    printf ( "---------------\n" );
    test ( pItemList, channelCount );

    for ( i = 0; i < channelCount; i++ ) {
        dbr_int_t * pIntVal = ( dbr_int_t * ) pItemList[i].pValue;
        double val = i;
        val /= channelCount;
        for ( j = 0; j < pItemList[i].count; j++ ) {
            pIntVal[j] = (dbr_int_t) val;
        }
        pItemList[i].type = DBR_INT; 
    }
    printf ( "DBR_INT Test\n" );
    printf ( "------------\n" );
    test ( pItemList, channelCount );

    printf ( "Get Latency Test\n" );
    printf ( "----------------\n" );
    for ( i = 0; i < channelCount; i++ ) {
        dbr_double_t * pDblVal = ( dbr_double_t * ) pItemList[i].pValue;
        for ( j = 0; j < pItemList[i].count; j++ ) {
            pDblVal[j] = 0;
        }
        pItemList[i].type = DBR_DOUBLE; 
    }   
    measure_get_latency ( pItemList, channelCount );

    printf ( "Free Channel Test\n" );
    printf ( "-----------------\n" );
    timeIt ( test_free, pItemList, channelCount, 0, 0 );

    SEVCHK ( ca_task_exit (), "Unable to free resources at exit" );
    
    for ( i = 0; i < channelCount; i++ ) {
        free ( pItemList[i].pValue );
    }   

    free ( pItemList );

    return CATIME_OK;
}




