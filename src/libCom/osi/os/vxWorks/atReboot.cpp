/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* atReboot.cpp */

/* Author:  Marty Kraimer Date:  30AUG2003 */

#include <stdio.h>

#include "epicsDynLink.h"
#include "epicsExit.h"

/* osdThread references atRebootExtern just to make this module load*/
int atRebootExtern;

typedef int (*sysAtReboot_t)(void(func)(void));

class atRebootRegister {
public:
    atRebootRegister();
};

atRebootRegister::atRebootRegister()
{
    STATUS status;
    sysAtReboot_t sysAtReboot;
    SYM_TYPE type;

    status = symFindByNameEPICS(sysSymTbl, "_sysAtReboot",
				(char **)&sysAtReboot, &type);
    if(status==OK) {
        status = sysAtReboot(epicsExitCallAtExits);
        if(status!=OK) {
            printf("atReboot: sysAtReboot returned error %d\n", status);
        } else {
            printf("epicsExit will be called by reboot.\n");
        }
    } else {
	printf("BSP routine sysAtReboot() not found, epicsExit() will not be\n"
	       "called by reboot.  For reduced functionality, call\n"
	       "    rebootHookAdd(epicsExitCallAtExits)\n");
    }
}

static atRebootRegister atRebootRegisterObj;
