/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef dbAddrh
#define dbAddrh

#ifdef __cplusplus
	struct dbCommon;
#endif

typedef struct dbAddr{
        struct dbCommon *precord;/* address of record                   */
        void    *pfield;        /* address of field                     */
        void    *pfldDes;       /* address of struct fldDes             */
        void    *asPvt;         /* Access Security Private              */
        long    no_elements;    /* number of elements (arrays)          */
        short   field_type;     /* type of database field               */
        short   field_size;     /* size (bytes) of the field being accessed */
        short   special;        /* special processing                   */
        short   dbr_field_type; /* field type as seen by database request*/
                                /*DBR_STRING,...,DBR_ENUM,DBR_NOACCESS*/
}dbAddr;

typedef dbAddr DBADDR;

unsigned dbNameOfPV ( const dbAddr * paddr, char * pBuf, unsigned bufLen );
unsigned dbNameSizeOfPV ( const dbAddr * paddr );

#endif /* dbAddrh */
