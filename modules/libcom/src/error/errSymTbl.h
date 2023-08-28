/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_errSymTbl_H
#define INC_errSymTbl_H

#include <stddef.h>

#include "libComAPI.h"
#include "epicsTypes.h"

/* ERRSYMBOL - entry in symbol table */
typedef struct {
    long errNum;        /* errMessage symbol number */
    const char *name;         /* pointer to symbol name */
} ERRSYMBOL;

/* ERRSYMTAB - symbol table */
typedef struct {
    int nsymbols;       /* current number of symbols in table */
    ERRSYMBOL *symbols; /* ptr to array of symbol entries */
} ERRSYMTAB;

typedef ERRSYMTAB *ERRSYMTAB_ID;


#ifdef __cplusplus
extern "C" {
#endif

/** \brief Lookup message from error/status code.
 * \param status Input code
 * \param pBuf Output string buffer
 * \param bufLength Length of output buffer in bytes.  Must be non-zero.
 *
 * Handles EPICS message codes, and "errno" codes.
 *
 * Copies in a mesage for any status code.  Unknown status codes
 * are printed numerically.
 */
LIBCOM_API void errSymLookup(long status, char *pBuf, size_t bufLength);
/** \brief Lookup message from error/status code.
 * \param status Input code
 * \returns A statically allocated message string.  Never NULL.
 *
 * Handles EPICS message codes, and "errno" codes.
 *
 * For any unknown status codes, a generic "Unknown code" message is returned.
 *
 * \since 3.16.1
 */
LIBCOM_API const char* errSymMsg(long status);
LIBCOM_API void errSymTest(epicsUInt16 modnum, epicsUInt16 begErrNum,
    epicsUInt16 endErrNum);
LIBCOM_API void errSymTestPrint(long errNum);
LIBCOM_API int errSymBld(void);
/** @brief Define new custom error code and associate message string.
 *  @param errNum New error code.  Caller is reponsible for avoiding reuse of existing codes.
 *  @param message New message.  Pointer stored.  Caller must not free pointed storage.
 *  @return 0 on success
 */
LIBCOM_API int errSymbolAdd(long errNum, const char *message);
LIBCOM_API void errSymDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_errSymTbl_H */
