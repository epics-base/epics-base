
#include <stdio.h>
#include <stdlib.h>

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
        int nConverted = sscanf ( argv[3], "%lf", &delay );
        if ( nConverted != 1 ) {
            printf ( "conversion failed, changing delay arg \"%s\" to %f\n",
                argv[2], delay );
        }
    }

    caConnTest ( argv[1], count, delay );

    return 0;
}
