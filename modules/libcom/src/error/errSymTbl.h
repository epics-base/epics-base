/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file errSymTbl.h
 * \brief The module includes a symbol table for error messages and its handling.
 * 
 */

#ifndef INC_errSymTbl_H
#define INC_errSymTbl_H

#include <stddef.h>

#include "libComAPI.h"
#include "epicsTypes.h"

/** \brief entry in symbol table */
typedef struct {
    long errNum;        /**< \brief errMessage symbol number */
    const char *name;         /**< \brief pointer to symbol name */
} ERRSYMBOL;

/** \brief symbol table */
typedef struct {
    int nsymbols;       /**< \brief current number of symbols in table */
    ERRSYMBOL *symbols; /**< \brief ptr to array of symbol entries */
} ERRSYMTAB;

typedef ERRSYMTAB *ERRSYMTAB_ID;


#ifdef __cplusplus
extern "C" {
#endif
/** \brief Looks up an error message based on a given status code. */
LIBCOM_API void errSymLookup(long status, char *pBuf, size_t bufLength);
/** \brief Look up the error message associated with a given error status code and return a pointer to that error message. */
LIBCOM_API const char* errSymMsg(long status);
/** \brief The purpose of this function is to test the error symbol table by printing out a range of error messages.*/
LIBCOM_API void errSymTest(epicsUInt16 modnum, epicsUInt16 begErrNum,
    epicsUInt16 endErrNum);
/** \brief Look up the error message associated with a given error status code and print it to standard output. */
LIBCOM_API void errSymTestPrint(long errNum);
/** \brief Builds the error symbol table. */
LIBCOM_API int errSymBld(void);
/** \brief Adds symbols to the master errnumlist. */
LIBCOM_API int errSymbolAdd(long errNum, const char *name);
/** \brief pPrints the contents of the error symbol table. */
LIBCOM_API void errSymDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_errSymTbl_H */
