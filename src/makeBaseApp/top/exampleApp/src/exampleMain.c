/* exampleMain.c */
/* Author:  Marty Kraimer Date:    17MAR2000 */

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

#include "epicsThread.h"
#include "ioccrf.h"

int main(int argc,char *argv[])
{
    if(argc>=2) {    
        ioccrf(argv[1]);
        epicsThreadSleep(.2);
    }
    ioccrf(NULL);
    return(0);
}
