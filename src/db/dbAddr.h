
#ifndef dbAddrh
#define dbAddrh

#ifdef __cplusplus
	//
	// for brain dead C++ compilers
	//
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
}DBADDR;

/*
 * old db access API
 * (included here because these routines use dbAccess.h and their
 * prototypes must also be included in db_access.h)
 */
epicsShareFunc int epicsShareAPI db_name_to_addr(const char *pname, DBADDR *paddr);
epicsShareFunc int epicsShareAPI db_put_field(DBADDR *paddr, int src_type,
                        const void *psrc, int no_elements);
epicsShareFunc int epicsShareAPI db_get_field(DBADDR *paddr, int dest_type,
                        void *pdest, int no_elements, void *pfl);
#endif /* dbAddrh */
