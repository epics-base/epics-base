/* threadTestMain.c */

/* Author:  Marty Kraimer Date:    26JAN2000 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "osiThread.h"
void threadTest(int nthreads);

int main(int argc,char *argv[])
{
    int nthreads = 2;

    if(argc>1) {
        if(isdigit(*argv[1])) {
            sscanf(argv[1],"%d",&nthreads);
            printf("nthreads %d\n",nthreads);
        } else {
            printf("Illegal argument %s\n",argv[0]);
        }
    }
    if(nthreads>0) threadTest(nthreads);
    printf("main terminating\n");
    return(0);
}
