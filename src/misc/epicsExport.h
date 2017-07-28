/*epicsExport.h */

/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
#ifndef INCepicsExporth
#define INCepicsExporth

#ifdef __cplusplus
extern "C" {
#endif

#define epicsExportSharedSymbols
#include <shareLib.h>

typedef void (*REGISTRAR)(void);

#define EPICS_EXPORT_POBJ(typ,obj) pvar_ ## typ ## _ ## obj
#define EPICS_EXPORT_PFUNC(obj) pvar_func_ ## obj

#define epicsExportAddress(typ,obj) \
epicsShareExtern typ *EPICS_EXPORT_POBJ(typ,obj); \
epicsShareDef typ *EPICS_EXPORT_POBJ(typ,obj) = (typ *)(char *)&obj

#define epicsExportRegistrar(func) \
epicsShareFunc REGISTRAR EPICS_EXPORT_PFUNC(func) = (REGISTRAR)(void*)&func

#define epicsRegisterFunction(func) \
static void register_func_ ## func(void) { \
    registryFunctionAdd(#func,(REGISTRYFUNCTION)func);} \
epicsExportRegistrar(register_func_ ## func)

#ifdef __cplusplus
}
#endif

#endif /* epicsExporth */
