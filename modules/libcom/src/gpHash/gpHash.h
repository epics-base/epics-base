/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file gpHash.h
 * \brief provides a general purpose directory accessed via a hash table
 * \author Marty Kraimer
 * \date 04-07-94
 * 
 * describes a general purpose hash table for character strings. The hash table contains tableSize entries.
 * Each entry is a list of members that hash to the same value. The user can maintain separate directories
 * which share the same table by having a different pvt value for each directory.
 * \warning tableSize must be power of 2 in range 256 to 65536
 */

#ifndef INC_gpHash_H
#define INC_gpHash_H

#include "libComAPI.h"

#include "ellLib.h"

/** \brief An Entrynode in the Hash table. */
typedef struct{
    ELLNODE     node;
    const char  *name;          /**< \brief address of name placed in directory */
    void        *pvtid;         /**< \brief private name for subsystem user */
    void        *userPvt;       /**< \brief private for user */
} GPHENTRY;

struct gphPvt;

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Initializes a new instance of gphPvt, which is a structure used to hold a hash table for name lookups. */
LIBCOM_API void epicsStdCall
    gphInitPvt(struct gphPvt **ppvt, int tableSize);
/** \brief Calls gphFindParse with the length of the name set to the string length of the name parameter */
LIBCOM_API GPHENTRY * epicsStdCall
    gphFind(struct gphPvt *pvt, const char *name, void *pvtid);
/** \brief Calculates a hash value using the pvtid and the name, then searches the appropriate ELLLIST for a matching GPHENTRY node. */
LIBCOM_API GPHENTRY * epicsStdCall
    gphFindParse(struct gphPvt *pvt, const char *name, size_t len, void *pvtid);
/** \brief Adds a node to the hash table. */
LIBCOM_API GPHENTRY * epicsStdCall
    gphAdd(struct gphPvt *pvt, const char *name, void *pvtid);
/** \brief Deletes a node from the hash table. */
LIBCOM_API void epicsStdCall
    gphDelete(struct gphPvt *pvt, const char *name, void *pvtid);
/** \brief Free the memory allocated for a gphPvt structure and all its contained entries. */
LIBCOM_API void epicsStdCall gphFreeMem(struct gphPvt *pvt);
/** \brief Prints the hash table. */
LIBCOM_API void epicsStdCall gphDump(struct gphPvt *pvt);
/** \brief Prints the hash table. */
LIBCOM_API void epicsStdCall gphDumpFP(FILE *fp, struct gphPvt *pvt);

#ifdef __cplusplus
}
#endif

#endif /* INC_gpHash_H */
