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
#include <stdlib.h>

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include "cadef.h"
#include "epicsTime.h"

static unsigned channelCount = 0u;
static unsigned connCount = 0u;
static bool subsequentConnect = false;

epicsTime begin;

extern "C" void caConnTestConnHandler ( struct connection_handler_args args )
{
    if ( args.op == CA_OP_CONN_UP ) {
        if ( connCount == 0u ) {
            if ( subsequentConnect ) {
                printf ("the first channel connected\n");
                begin = epicsTime::getCurrent ();
            }
        }
        connCount++;
        // printf  ( "." );
        // fflush ( stdout );
        if ( connCount == channelCount ) {
            epicsTime current = epicsTime::getCurrent ();
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
	
	while ( 1 ) {
        connCount = 0u;
        subsequentConnect = false;
        begin = epicsTime::getCurrent ();

        printf ( "initializing CA client library\n" );

		status = ca_task_initialize();
		SEVCHK ( status, "CA init failed" );

        printf ( "creating channels\n" );

		for ( i = 0u; i < channelCount; i++ ) {
			status = ca_search_and_connect ( pNameIn, 
                &pChans[i], caConnTestConnHandler, 0 );
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

    //delete [] pChans;
}
