/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <assert.h>

#include "cadef.h"
#include "dbDefs.h"
#include "errlog.h"

/*
 * event_handler()
 */
extern "C" void eventCallBack ( struct event_handler_args args )
{
    unsigned *pCount = static_cast < unsigned * > ( args.usr );
    (*pCount)++;
}

/*
 * caEventRate ()
 */
void caEventRate ( const char *pName, unsigned count )
{
    static const double initialSamplePeriod = 1.0;
    static const double maxSamplePeriod = 60.0 * 5.0;

    printf ( "Connecting to CA Channel \"%s\" %u times.", 
                pName, count );
    fflush ( stdout );

    chid * pChidTable = new chid [ count ];
    assert ( pChidTable );
    for ( unsigned i = 0u; i < count; i++ ) {
        int status = ca_search ( pName,  & pChidTable[i] );
        SEVCHK ( status, NULL );
    }

    int status = ca_pend_io ( 10000.0 );
    if ( status != ECA_NORMAL ) {
        fprintf ( stderr, " not found.\n" );
        return;
    }

    printf ( " done.\n" );

    printf ( "Subscribing %u times.", count );
    fflush ( stdout );

    unsigned eventCount = 0u;
    for ( i = 0u; i < count; i++ ) {
        status = ca_add_event ( DBR_FLOAT, 
            pChidTable[i], eventCallBack, &eventCount, NULL);
        SEVCHK ( status, __FILE__ );
    }

    status = ca_flush_io ();
    SEVCHK ( status, __FILE__ );

    printf ( " done.\n" );
        
    printf ( "Waiting for initial value events." );
    fflush ( stdout );

    // let the first one go by 
    while ( eventCount < count ) {
        status = ca_pend_event ( 0.01 );
        if ( status != ECA_TIMEOUT ) {
            SEVCHK ( status, NULL );
        }
    }

    printf ( " done.\n" );

    double samplePeriod = initialSamplePeriod;
    double X = 0.0;
    double XX = 0.0;
    unsigned N = 0u;
    while ( true ) {
        unsigned nEvents, lastEventCount, curEventCount;

        lastEventCount = eventCount;
        status = ca_pend_event ( samplePeriod );
        curEventCount = eventCount;
        if ( status != ECA_TIMEOUT ) {
            SEVCHK ( status, NULL );
        }

        if ( curEventCount >= lastEventCount ) {
            nEvents = curEventCount - lastEventCount;
        }
        else {
            nEvents = ( UINT_MAX - lastEventCount ) + curEventCount + 1u;
        }

        N++;

        if ( samplePeriod < maxSamplePeriod ) {
            samplePeriod += samplePeriod;
        }
    }
}

int main ( int argc, char **argv )
{
    if ( argc < 2 || argc > 3 ) {
        fprintf ( stderr, "usage: %s < PV name > [subscription count]\n", argv[0] );
        return 0;
    }

    unsigned count;
    if ( argc == 3 ) {
        int status = sscanf ( argv[2], " %u ", & count );
        if ( status != 1 ) {
            fprintf ( stderr, "expected unsigned integer 2nd argument\n" );
            return 0;
        }
    }
    else {
        count = 1;
    }

    caEventRate ( argv[1], count );

    return 0;
}

