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

void caEventRate ( const char *pName );

int main ( int argc, char **argv )
{
    if ( argc < 2 || argc > 2 ) {
        printf ( "usage: %s < PV name >\n", argv[0] );
    }

    caEventRate ( argv[1] );

    return 0;
}
