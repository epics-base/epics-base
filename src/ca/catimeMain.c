
#include <stdio.h>

#include "caDiagnostics.h"

int main(int argc, char **argv)
{
    char    *pname;

    if(argc <= 1 || argc>3){
printf("usage: %s <channel name> [<if 2nd arg present append number to pv name>]\n", 
    argv[0]);
        return -1;
    }
    else{
        pname = argv[1];    
        if (argc==3) {
            catime(pname, appendNumber);
        }
        else {
            catime(pname, dontAppendNumber);
        }
    }
    return 0;
}
