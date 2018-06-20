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

void caEventRate ( const char *pName, unsigned count );

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
