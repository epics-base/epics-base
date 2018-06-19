/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef dbAddrh
#define dbAddrh

struct dbCommon;
struct dbFldDes;

typedef struct dbAddr {
        struct dbCommon *precord;   /* address of record                     */
        void    *pfield;            /* address of field                      */
        struct dbFldDes *pfldDes;   /* address of struct fldDes              */
        long    no_elements;        /* number of elements (arrays)           */
        short   field_type;         /* type of database field                */
        short   field_size;         /* size of the field being accessed      */
        short   special;            /* special processing                    */
        short   dbr_field_type;     /* field type as seen by database request*/
                                    /* DBR_STRING,...,DBR_ENUM,DBR_NOACCESS  */
} dbAddr;

typedef dbAddr DBADDR;

#endif /* dbAddrh */
