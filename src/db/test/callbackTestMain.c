/* callbackTestMain.c */
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

void *pdbbase=NULL;

void callbackTest(void);

int main(int argc,char *argv[])
{
    callbackTest();
    printf("main terminating\n");
    return(0);
}
