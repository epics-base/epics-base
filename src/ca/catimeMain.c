
#include <stdio.h>

#include "caDiagnostics.h"

static const unsigned defaultIterations = 10000u;

int main(int argc, char **argv)
{
    const char *pUsage = "<channel name> [<channel count> [<if 2nd arg present append number to pv name>]]";

    if ( argc > 1 ) {
        char *pname = argv[1];
        if ( argc > 2 ) {
            int  iterations = atoi (argv[2]);
            if ( iterations > 0) {
                if ( argc > 3 ) {
                    if ( argc == 4 ) {
                        return catime ( pname, (unsigned) iterations, appendNumber );
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
