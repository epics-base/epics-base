/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * Provide a dummy version of iocshRegister to be used with test applications.
 */

#include <iocsh.h>

void
iocshRegister(const iocshFuncDef *piocshFuncDef, iocshCallFunc func)
{
    printf ("No iocsh -- %s not registered\n", piocshFuncDef->name);
}
