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

#include "osiThread.h"
#include "dbAccess.h"
#include "errlog.h"
#include "dbTest.h"
#include "registryRecordType.h"
#include "ioccrf.h"
#include "ioccrfRegisterCommon.h"
#include "registerRecordDeviceDriverRegister.h"

int main(int argc,char *argv[])
{
    ioccrfRegisterCommon();
    registerRecordDeviceDriverRegister();
    if(argc>=2) {    
        ioccrf(argv[1]);
        threadSleep(.2);
    }
    ioccrf(NULL);
    threadExitMain();
    return(0);
}
