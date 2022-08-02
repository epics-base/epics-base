/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef dbAddrh
#define dbAddrh

#include <ellLib.h>

struct dbCommon;
struct dbFldDes;
struct VFieldType;

typedef struct dbAddr {
        struct dbCommon *precord;   /* address of record                     */
        void    *pfield;            /* address of field                      */
        struct dbFldDes *pfldDes;   /* address of struct fldDes              */
        const ELLLIST* vfields;     /* list of VFieldTypeNode, vtypes to try with rset::get/put_vfield  */
        long    no_elements;        /* number of elements (arrays)           */
        unsigned ro:1;              /* dbPut permitted? */
        short   field_type;         /* type of database field                */
        short   field_size;         /* size of the field being accessed      */
        short   special;            /* special processing                    */
        short   dbr_field_type;     /* field type as seen by database request*/
                                    /* DBR_STRING,...,DBR_ENUM,DBR_NOACCESS  */
} dbAddr;

typedef dbAddr DBADDR;

#endif /* dbAddrh */
