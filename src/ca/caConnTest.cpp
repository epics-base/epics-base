
#include <stdio.h>
#include <stdlib.h>

#include "cadef.h"
#include "epicsAssert.h"
#include "osiTime.h"

static unsigned channelCount = 0u;
static unsigned connCount = 0u;
static bool subsequentConnect = false;

osiTime begin;

static void connHandler ( struct connection_handler_args args )
{
    if ( args.op == CA_OP_CONN_UP ) {
        if ( connCount == 0u ) {
            if ( subsequentConnect ) {
                printf ("the first channel connected\n");
                begin = osiTime::getCurrent ();
            }
        }
        connCount++;
        // printf  ( "." );
        // fflush ( stdout );
        if ( connCount == channelCount ) {
            osiTime current = osiTime::getCurrent ();
            double delay = current - begin;
            printf ( "all channels connected after %f sec ( %f sec per channel)\n",
                delay, delay / channelCount );
        }
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        if ( connCount == channelCount ) {
            printf ( "channels are disconnected\n" );
            subsequentConnect = true;
        }
        connCount--;
        if ( connCount == 0u ) {
            printf ( "all channels are disconnected\n" );
        }
    }
    else {
        assert ( 0 );
    }
}

void caConnTest ( const char *pNameIn, unsigned channelCountIn, double delayIn )
{
    unsigned iteration = 0u;
    int status;
	unsigned i;
	chid *pChans;

    channelCount = channelCountIn;

    pChans = new chid [channelCount];
    assert ( pChans );
	
	while ( 1 ) {
        connCount = 0u;
        subsequentConnect = false;
        begin = osiTime::getCurrent ();

        printf ( "initializing CA client library\n" );

		status = ca_task_initialize();
		SEVCHK ( status, "CA init failed" );

        printf ( "creating channels\n" );

		for ( i = 0u; i < channelCount; i++ ) {
			status = ca_search_and_connect ( pNameIn, &pChans[i], connHandler, 0 );
			SEVCHK ( status, "CA search problems" );
		}

        printf ( "all channels were created\n" );

		ca_pend_event ( delayIn );

        if ( iteration & 1 ) {
		    for ( i = 0u; i < channelCount; i++ ) {
			    status = ca_clear_channel ( pChans[i] );
			    SEVCHK ( status, "ca_clear_channel() problems" );
		    }
            printf ( "all channels were destroyed\n" );
        }

        printf ( "shutting down CA client library\n" );

		status = ca_task_exit ();
		SEVCHK ( status, "task exit problems" );

        iteration++;
	}

    delete [] pChans;
}
