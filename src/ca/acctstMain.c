
#include <stdio.h>
#include <stdlib.h>

#include "cadef.h"
#include "caDiagnostics.h"

int main ( int argc, char **argv )
{
    unsigned channelCount;
    unsigned repetitionCount;
	enum ca_preemptive_callback_select preempt;
	int aBoolean;


    if ( argc < 2 || argc > 5 ) {
        printf ( 
"usage: %s <PV name> [channel count] [repetition count] [enable preemptive callback]\n", 
			argv[0] );
    }

    if ( argc >= 3 ) {
        channelCount = atoi ( argv[2] );
    }
    else {
        channelCount = 20000;
    }

    if ( argc >= 4 ) {
        repetitionCount = atoi ( argv[3] );
    }
    else {
        repetitionCount = 1;
    }

    if ( argc >= 5 ) {
        aBoolean = atoi ( argv[4] );
    }
    else {
        aBoolean = 0;
    }
	if ( aBoolean ) {
		preempt = ca_disable_preemptive_callback;
	}
	else {
		preempt = ca_enable_preemptive_callback;
	}

    acctst ( argv[1], channelCount, repetitionCount, preempt );

    return 0;
}
