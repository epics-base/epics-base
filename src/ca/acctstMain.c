
#include <stdio.h>

#include "caDiagnostics.h"

int main(int argc, char **argv)
{
    if(argc == 2){
        acctst(argv[1]);
    }
    else{
        printf("usage: %s <chan name>\n", argv[0]);
    }
    return 0;
}
