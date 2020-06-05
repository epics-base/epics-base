/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_epicsExport_H
#define INC_epicsExport_H

/** \file epicsExport.h
 *  \brief Exporting IOC objects.
 *
 * This header is unique, as it defines epicsExportSharedSymbols and thus
 * triggers a transition between importing declarations from other libraries,
 * to exporting symbols from our own library. The comments in shareLib.h
 * provide more information.
 *
 * This header should be included with a trailing comment to make it stand
 * out from other includes, something like this:
 \code
    #include <epicsExport.h>  // defines epicsExportSharedSymbols
 \endcode
 */

#define epicsExportSharedSymbols
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*REGISTRAR)(void);

#define EPICS_EXPORT_POBJ(typ, obj) pvar_ ## typ ## _ ## obj
#define EPICS_EXPORT_PFUNC(fun) EPICS_EXPORT_POBJ(func, fun)

/** \brief Declare an object for exporting.
 *
 * The epicsExportAddress() macro must be used to declare any IOC object
 * that is also named in a DBD file. For example a record support source
 * file must contain a statement like:
 \code
    epicsExportAddress(rset, myRSET);
 \endcode
 *
 * A device support source file must contain a statement like:
 \code
    epicsExportAddress(dset, devMyName);
 \endcode
 * Note that the \p typ parameter for a device support entry table must be
 * spelled \c dset even if the \p obj was actually declared as some other
 * type, say using \c typed_dset .
 *
 * A driver support source file must contain a statement like:
 \code
    epicsExportAddress(drvet, drvName);
 \endcode
 *
 * A variable named in a DBD \c variable statement must be declared with:
 \code
    int myDebug = 0;
    epicsExportAddress(int, myDebug);
 \endcode
 * Only \c int and \c double are currently supported for DBD variables.
 *
 * \param typ Object's data type.
 * \param obj Object's name.
 *
 * \note C++ code needs to wrap with @code extern "C" { } @endcode
 */
#define epicsExportAddress(typ, obj) \
    epicsShareExtern typ *EPICS_EXPORT_POBJ(typ,obj); \
    epicsShareDef typ *EPICS_EXPORT_POBJ(typ, obj) = (typ *) (char *) &obj

/** \brief Declare a registrar function for exporting.
 *
 * The epicsExportRegistrar() macro must be used to declare a registrar
 * function that is named in a DBD \c registrar statement. For example:
 \code
    static void myRegistrar(void) {
        ...
    }
    epicsExportRegistrar(myRegistrar);
 \endcode
 *
 * \param fun Registrar function's name.
 *
 * \note C++ code needs to wrap with @code extern "C" { } @endcode
 */
#define epicsExportRegistrar(fun) \
    epicsShareFunc REGISTRAR EPICS_EXPORT_PFUNC(fun) = (REGISTRAR) &fun

/** \brief Declare and register a function for exporting.
 *
 * The epicsRegisterFunction() macro must be used to declare and register
 * a function that is named in a DBD \c function statement and called by
 * one or more subroutine or aSub records. For example:
 \code
    epicsRegisterFunction(mySubInit);
    epicsRegisterFunction(mySubProcess);
 \endcode
*
 * \param fun Function's name
 *
 * \note C++ code needs to wrap with @code extern "C" { } @endcode
 */
#define epicsRegisterFunction(fun) \
    static void register_func_ ## fun(void) \
    { \
        registryFunctionAdd(#fun, (REGISTRYFUNCTION) fun); \
    } \
    epicsExportRegistrar(register_func_ ## fun)

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsExport_H */
