/* SPDX-FileCopyrightText: 2005 Argonne National Laboratory */

/* SPDX-License-Identifier: EPICS */

/* Example showing how to register a new command with iocsh */

#include <stdio.h>

#include <epicsExport.h>
#include <iocsh.h>

/* This is the command, which the vxWorks shell will call directly */
void hello(const char *name) {
    if (name) {
        printf("Hello %s, from _APPNAME_\n", name);
    } else {
        puts("Hello from _APPNAME_");
    }
}

/* Information needed by iocsh */
static const iocshArg     helloArg0 = {"name", iocshArgString};
static const iocshArg    *helloArgs[] = {&helloArg0};
static const iocshFuncDef helloFuncDef = {
    "hello",
    1,
    helloArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
    "Greet the person whose name is given as first argument.\n"
    "If 'name' is unspecified, greet the world in general.\n\n"
    "Example: hello\n"
    "Example: hello stranger\n",
#endif
};

/* Wrapper called by iocsh, selects the argument types that hello needs */
static void helloCallFunc(const iocshArgBuf *args) {
    hello(args[0].sval);
}

/* Registration routine, runs at startup */
static void helloRegister(void) {
    iocshRegister(&helloFuncDef, helloCallFunc);
}
epicsExportRegistrar(helloRegister);
