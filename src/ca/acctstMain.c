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

#include "cadef.h"
#include "caDiagnostics.h"

int main ( int argc, char **argv )
{
    unsigned progressLoggingLevel;
    unsigned channelCount;
    unsigned repetitionCount;
	enum ca_preemptive_callback_select preempt;
	int aBoolean;


    if ( argc < 2 || argc > 6 ) {
        printf ("usage: %s <PV name> [progress logging level] [channel count] "
                "[repetition count] [enable preemptive callback]\n", 
                argv[0] );
        return 1;        
    }

    if ( argc >= 3 ) {
        progressLoggingLevel = atoi ( argv[2] );
    }
    else {
        progressLoggingLevel = 0;
    }

    if ( argc >= 4 ) {
        channelCount = atoi ( argv[3] );
    }
    else {
        channelCount = 20000;
    }

    if ( argc >= 5 ) {
        repetitionCount = atoi ( argv[4] );
    }
    else {
        repetitionCount = 1;
    }

    if ( argc >= 6 ) {
        aBoolean = atoi ( argv[5] );
    }
    else {
        aBoolean = 0;
    }
	if ( aBoolean ) {
		preempt = ca_enable_preemptive_callback;
	}
	else {
		preempt = ca_disable_preemptive_callback;
	}

    acctst ( argv[1], progressLoggingLevel, channelCount, repetitionCount, preempt );

    return 0;
}
