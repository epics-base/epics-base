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

#include "cadef.h"
#include "dbDefs.h"
#include "epicsTime.h"
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
    unsigned eventCount = 0u;

    chid * pChidTable = new chid [ count ];

    {
        printf ( "Connecting to CA Channel \"%s\" %u times.", 
                    pName, count );
        fflush ( stdout );
    
        epicsTime begin = epicsTime::getCurrent ();
        for ( unsigned i = 0u; i < count; i++ ) {
            int status = ca_search ( pName,  & pChidTable[i] );
            SEVCHK ( status, NULL );
        }
    
        int status = ca_pend_io ( 10000.0 );
        if ( status != ECA_NORMAL ) {
            fprintf ( stderr, " not found.\n" );
            return;
        }
        epicsTime end = epicsTime::getCurrent ();
    
        printf ( " done(%f sec).\n", end - begin );
    }

    {
        printf ( "Subscribing %u times.", count );
        fflush ( stdout );
        
        epicsTime begin = epicsTime::getCurrent ();
        for ( unsigned i = 0u; i < count; i++ ) {
            int addEventStatus = ca_add_event ( DBR_FLOAT, 
                pChidTable[i], eventCallBack, &eventCount, NULL);
            SEVCHK ( addEventStatus, __FILE__ );
        }
    
        int status = ca_flush_io ();
        SEVCHK ( status, __FILE__ );
    
        epicsTime end = epicsTime::getCurrent ();
    
        printf ( " done(%f sec).\n", end - begin );
    }
        
    {
        printf ( "Waiting for initial value events." );
        fflush ( stdout );
    
        // let the first one go by 
        epicsTime begin = epicsTime::getCurrent ();
        while ( eventCount < count ) {
            int status = ca_pend_event ( 0.01 );
            if ( status != ECA_TIMEOUT ) {
                SEVCHK ( status, NULL );
            }
        }
        epicsTime end = epicsTime::getCurrent ();
    
        printf ( " done(%f sec).\n", end - begin );
    }

    double samplePeriod = initialSamplePeriod;
    double X = 0.0;
    double XX = 0.0;
    unsigned N = 0u;
    while ( true ) {
        unsigned nEvents, lastEventCount, curEventCount;

        epicsTime beginPend = epicsTime::getCurrent ();
        lastEventCount = eventCount;
        int status = ca_pend_event ( samplePeriod );
        curEventCount = eventCount;
        epicsTime endPend = epicsTime::getCurrent ();
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

        double period = endPend - beginPend;
        double Hz = nEvents / period;

        X += Hz;
        XX += Hz * Hz;

        double mean = X / N;
        double stdDev = sqrt ( XX / N - mean * mean );

        printf ( "CA Event Rate (Hz): current %g mean %g std dev %g\n", 
            Hz, mean, stdDev );

        if ( samplePeriod < maxSamplePeriod ) {
            samplePeriod += samplePeriod;
        }
    }
}

