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
	struct putNotify;
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
}DBADDR;

typedef struct pnRestartNode {
        ELLNODE         node;
        struct putNotify *ppn;
        struct putNotify *ppnrestartList; /*ppn with restartList*/
}PNRESTARTNODE;
 
 
typedef struct putNotify{
        /*The following members MUST be set by user*/
#ifdef __STDC__
        void            (*userCallback)(struct putNotify *); /*callback provided by user*/
#else
        void            (*userCallback)(); /*callback provided by user*/
#endif
        struct dbAddr   *paddr;         /*dbAddr set by dbNameToAddr*/
        void            *pbuffer;       /*address of data*/
        long            nRequest;       /*number of elements to be written*/
        short           dbrType;        /*database request type*/
        void            *usrPvt;        /*for private use of user*/
        /*The following is status of request. Set by dbPutNotify*/
        long            status;
        /*The following are private to database access*/
        CALLBACK        callback;
        ELLLIST         waitList;       /*list of records for which to wait*/
        ELLLIST         restartList;    /*list of PUTNOTIFYs to restart*/
        PNRESTARTNODE   restartNode;
        short           restart;
        short           callbackState;
        void            *waitForCallback;
}PUTNOTIFY;

/*
 * old db access API
 * (included here because these routines use dbAccess.h and their
 * prototypes must also be included in db_access.h)
 */
#ifdef __STDC__
        int db_name_to_addr(const char *pname, DBADDR *paddr);
        int db_put_field(DBADDR *paddr, int src_type,
                        const void *psrc, int no_elements);
        int db_get_field(DBADDR *paddr, int dest_type,
                        void *pdest, int no_elements, void *pfl);
#else
        int db_name_to_addr();
        int db_put_field();
        int db_get_field();
#endif

#endif /* dbAddrh */
