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
#include <epicsStdlib.h>

#include "caDiagnostics.h"

int main ( int argc, char **argv )
{
    double delay = 60.0 * 5.0;
    unsigned count = 2000;

    if ( argc < 2 || argc > 4 ) {
        printf ( "usage: %s < channel name > [ < count > ] [ < delay sec > ]\n", argv[0] );
        return -1;
    }

    if ( argc >= 3 ) {
        int nConverted = sscanf ( argv[2], "%u", &count );
        if ( nConverted != 1 ) {
            printf ( "conversion failed, changing channel count arg \"%s\" to %u\n",
                argv[1], count );
        }
    }

    if ( argc >= 4 ) {
        int nConverted = epicsScanDouble( argv[3], &delay );
        if ( nConverted != 1 ) {
            printf ( "conversion failed, changing delay arg \"%s\" to %f\n",
                argv[2], delay );
        }
    }

    caConnTest ( argv[1], count, delay );

    return 0;
}
