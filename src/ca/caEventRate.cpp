
#include <stdio.h>
#include <limits.h>

#include "cadef.h"
#include "dbDefs.h"
#include "epicsTime.h"

/*
 * event_handler()
 */
extern "C" void eventCallBack ( struct event_handler_args args )
{
    unsigned *pCount = static_cast < unsigned * > ( args.usr );
    (*pCount)++;
}


/*
 * cam ()
 */
void caEventRate ( const char *pName )
{
    static const double initialSamplePeriod = 1.0;
    static const double maxSamplePeriod = 60.0;

    chid chan;
    int status = ca_search ( pName,  &chan );
    SEVCHK(status, NULL);

    unsigned eventCount = 0u;
    status = ca_add_event ( DBR_FLOAT, chan, eventCallBack, &eventCount, NULL);
    SEVCHK ( status, __FILE__ );

    status = ca_pend_io ( 10.0 );
    if ( status != ECA_NORMAL ) {
        printf ( "%s not found\n", pName );
        return;
    }

    // let the first one go by 
    while ( eventCount < 1u ) {
        status = ca_pend_event ( 0.01 );
        if ( status != ECA_TIMEOUT ) {
            SEVCHK ( status, NULL );
        }
    }

    double samplePeriod = initialSamplePeriod;
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
            nEvents = ( UINT_MAX - lastEventCount ) + curEventCount +1u;
        }

        printf ( "CA Channel \"%s\" is producing %g subscription update events per second.\n", 
            pName, nEvents / samplePeriod );

        if ( samplePeriod < maxSamplePeriod ) {
            samplePeriod += samplePeriod;
        }
    }
}

