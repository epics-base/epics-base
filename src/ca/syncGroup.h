
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, 1986, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

#ifndef syncGrouph  
#define syncGrouph

#include "tsDLList.h"
#include "tsFreeList.h"
#include "resourceLib.h"
#include "epicsEvent.h"

#define epicsExportSharedSymbols
#include "cadef.h"
#include "cacIO.h"
#undef epicsExportSharedSymbols

static const unsigned CASG_MAGIC = 0xFAB4CAFE;

// used to control access to CASG's recycle routines which
// should only be indirectly invoked by CASG when its lock
// is applied
class casgRecycle {
public:
    virtual void recycleSyncGroupWriteNotify ( class syncGroupWriteNotify & io ) = 0;
    virtual void recycleSyncGroupReadNotify ( class syncGroupReadNotify & io ) = 0;
};

class syncGroupNotify : public tsDLNode < syncGroupNotify > {
public:
    syncGroupNotify  ( struct CASG &sgIn, chid );
    virtual void destroy ( casgRecycle & ) = 0;
    void show ( unsigned level ) const;
protected:
    chid chan;
    struct CASG &sg;
    const unsigned magic;
    cacChannel::ioid id;
    bool idIsValid;
    virtual ~syncGroupNotify ();  
};

class syncGroupReadNotify : public syncGroupNotify, public cacReadNotify {
public:
    static syncGroupReadNotify * factory ( 
        tsFreeList < class syncGroupReadNotify, 128 > &, 
        struct CASG &, chid, unsigned type, 
        arrayElementCount count, void *pValueIn );
    void destroy ( casgRecycle & );
    void show ( unsigned level ) const;
protected:
    virtual ~syncGroupReadNotify ();
private:
    void *pValue;
    syncGroupReadNotify ( struct CASG &sgIn, chid, 
        unsigned type, arrayElementCount count, void *pValueIn );
    void * operator new ( size_t, 
        tsFreeList < class syncGroupReadNotify, 128 > & );
#   if ! defined ( NO_PLACEMENT_DELETE )
    void operator delete ( void *, size_t, 
        tsFreeList < class syncGroupReadNotify, 128 > & );
#   endif
    void completion (
        unsigned type, arrayElementCount count, const void *pData );
    void exception (
        int status, const char *pContext, unsigned type, arrayElementCount count );
};

class syncGroupWriteNotify : public syncGroupNotify, public cacWriteNotify {
public:
    static syncGroupWriteNotify * factory ( 
        tsFreeList < class syncGroupWriteNotify, 128 > &, 
        struct CASG &, chid, unsigned type, 
        arrayElementCount count, const void *pValueIn );
    void destroy ( casgRecycle & );
    void show ( unsigned level ) const;
protected:
    virtual ~syncGroupWriteNotify (); // allocate only from pool
private:
    void *pValue;
    syncGroupWriteNotify  ( struct CASG &, chid, unsigned type, 
                       arrayElementCount count, const void *pValueIn );
    void * operator new ( size_t, 
        tsFreeList < class syncGroupWriteNotify, 128 > & );
#   if ! defined ( NO_PLACEMENT_DELETE )
    void operator delete ( void *, size_t, 
        tsFreeList < class syncGroupWriteNotify, 128 > & );
#   endif
    void completion ();
    void exception ( int status, const char *pContext, unsigned type, arrayElementCount count );
};

class oldCAC;

struct CASG : public chronIntIdRes < CASG >, private casgRecycle {
public:
    CASG ( oldCAC &cacIn );
    bool ioComplete () const;
    void destroy ();
    bool verify () const;
    int block ( double timeout );
    void reset ();
    void show ( unsigned level ) const;
    int get ( chid pChan, unsigned type, arrayElementCount count, void *pValue );
    int put ( chid pChan, unsigned type, arrayElementCount count, const void *pValue );
    void destroyIO ( syncGroupNotify & );
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    int printf ( const char * pFormat, ... );
    void exception ( int status, const char *pContext, 
        const char *pFileName, unsigned lineNo );
    void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
        unsigned type, arrayElementCount count, unsigned op );
protected:
    virtual ~CASG ();
private:
    tsDLList < syncGroupNotify > ioList;
    epicsMutex mutable mutex;
    epicsMutex serializeBlock;
    epicsEvent sem;
    oldCAC & client;
    unsigned magic;
    tsFreeList < class syncGroupReadNotify, 128 > freeListReadOP;
    tsFreeList < class syncGroupWriteNotify, 128 > freeListWriteOP;
    void recycleSyncGroupWriteNotify ( syncGroupWriteNotify &io );
    void recycleSyncGroupReadNotify ( syncGroupReadNotify &io );
    static tsFreeList < struct CASG, 128 > freeList;
    static epicsMutex freeListMutex;
};

#endif // ifdef syncGrouph
