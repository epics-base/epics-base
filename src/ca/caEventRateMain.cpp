
#include <stdio.h>
#include <stdlib.h>

void caEventRate ( const char *pName );

int main ( int argc, char **argv )
{
    if ( argc < 2 || argc > 2 ) {
        printf ( "usage: %s < chan name >\n", argv[0] );
    }

    caEventRate ( argv[1] );

    return 0;
}
