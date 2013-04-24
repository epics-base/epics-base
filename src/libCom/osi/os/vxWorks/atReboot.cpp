/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* atReboot.cpp */

/* Author:  Marty Kraimer Date:  30AUG2003 */

#include <stdio.h>

#include "epicsDynLink.h"
#include "epicsExit.h"

extern "C" {

typedef int (*sysAtReboot_t)(void(func)(void));

void atRebootRegister(void)
{
    STATUS status;
    sysAtReboot_t sysAtReboot;
    SYM_TYPE type;

    status = symFindByNameEPICS(sysSymTbl, "_sysAtReboot",
                                (char **)&sysAtReboot, &type);
    if (status == OK) {
        status = sysAtReboot(epicsExitCallAtExits);
        if (status != OK) {
            printf("atReboot: sysAtReboot returned error %d\n", status);
        }
    } else {
        printf("BSP routine sysAtReboot() not found, epicsExit() will not be\n"
               "called by reboot.  For reduced functionality, call\n"
               "    rebootHookAdd(epicsExitCallAtExits)\n");
    }
}

}
