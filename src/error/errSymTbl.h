/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_errSymTbl_H
#define INC_errSymTbl_H

#include <stddef.h>

#include "shareLib.h"
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

epicsShareFunc void errSymLookup(long status, char *pBuf, size_t bufLength);
epicsShareFunc const char* errSymMsg(long status);
epicsShareFunc void errSymTest(epicsUInt16 modnum, epicsUInt16 begErrNum,
    epicsUInt16 endErrNum);
epicsShareFunc void errSymTestPrint(long errNum);
epicsShareFunc int errSymBld(void);
epicsShareFunc int errSymbolAdd(long errNum, const char *name);
epicsShareFunc void errSymDump(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_errSymTbl_H */
