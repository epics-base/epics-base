
#include <stdio.h>
#include <stdlib.h>

#include <cadef.h>
#include <epicsAssert.h>

static unsigned channelCount = 0u;
static unsigned connCount = 0u;

static void connHandler ( struct connection_handler_args args )
{
    if ( args.op == CA_OP_CONN_UP ) {
        connCount++;
        if ( connCount == channelCount ) {
            printf ( "all channels connected\n" );
        }
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        if ( connCount == channelCount ) {
            printf ( "channels are disconnected\n" );
        }
        connCount--;
    }
    else {
        assert ( 0 );
    }
}

void caConnTest ( const char *pNameIn, unsigned channelCountIn, double delayIn )
{
    int status;
	unsigned i;
	chid *pChans;

    channelCount = channelCountIn;

    pChans = malloc ( channelCount * sizeof ( *pChans ) );
    assert ( pChans );
	
	while ( 1 ) {
		status = ca_task_initialize();
		SEVCHK ( status, "CA init failed" );

		for ( i = 0u; i < channelCount; i++ ) {
			status = ca_search_and_connect ( pNameIn, &pChans[i], connHandler, 0);
			SEVCHK ( status, "CA search problems" );
		}

		status = ca_pend_io ( 60.0 * 10.0 );
		SEVCHK ( status, "channels didnt connect" );

		status = ca_pend_event ( delayIn );
		SEVCHK ( status, "CA pend event failed" );

		status = ca_task_exit();
		SEVCHK ( status, "task exit problems" );
	}
}
