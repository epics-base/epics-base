/*
 * Provide a dummy version of iocshRegister to be used with test applications.
 */

#include <iocsh.h>

void
iocshRegister(const iocshFuncDef *piocshFuncDef, iocshCallFunc func)
{
    printf ("No iocsh -- %s not registered\n", piocshFuncDef->name);
}
