
#include <stdio.h>
#include <stdlib.h>

#include "caDiagnostics.h"

int main ( int argc, char **argv )
{
    unsigned channelCount;
    unsigned repetitionCount;

    if ( argc < 2 || argc > 4 ) {
        printf ( "usage: %s <PV name> [channel count] [repetition count]\n", argv[0] );
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

    acctst ( argv[1], channelCount, repetitionCount );

    return 0;
}
