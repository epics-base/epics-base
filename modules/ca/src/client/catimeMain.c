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

#include "caDiagnostics.h"

static const unsigned defaultIterations = 10000u;

int main ( int argc, char **argv )
{
    const char *pUsage = "<PV name> [<channel count> [<append number to pv name if true>]]";

    if ( argc > 1 ) {
        char *pname = argv[1];
        if ( argc > 2 ) {
            int  iterations = atoi (argv[2]);
            if ( iterations > 0) {
                if ( argc > 3 ) {
                    if ( argc == 4 ) {
                        int status;
                        unsigned appendNumberBool;
                        status = sscanf ( argv[3], " %u ", &appendNumberBool );
                        if ( status == 1 ) {
                            if ( appendNumberBool ) {
                                return catime ( pname, (unsigned) iterations, appendNumber );
                            }
                            else {
                                return catime ( pname, (unsigned) iterations, dontAppendNumber );
                            }
                        }
                    }
                }
                else {
                    return catime ( pname, (unsigned) iterations, dontAppendNumber );
                }
            }
        }
        else {
            return catime ( pname, defaultIterations, dontAppendNumber );
        }
    }
    printf ( "usage: %s %s\n", argv[0], pUsage);
    return -1;
}
