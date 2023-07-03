/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsFindSymbol.h
 * \brief Functions for dynamic loading of Libraries.
 * \author likely Marty Kraimer
 */

#ifndef epicsFindSymbolh
#define epicsFindSymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "libComAPI.h"

/** \brief Loads a dynamic library with the specified name and returns a
 * handle to the library. */
LIBCOM_API void * epicsLoadLibrary(const char *name);
/** \brief Returns a string describing the last error that
 * occurred during library loading. */
LIBCOM_API const char *epicsLoadError(void);
/** \brief Looks up a symbol with the specified name in the
 *  loaded libraries and returns a pointer to the symbol. */
LIBCOM_API void * epicsStdCall epicsFindSymbol(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* epicsFindSymbolh */
