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

#ifdef epicsExportSharedSymbols
#define syncGrouph_restore_epicsExportSharedSymbols
#undef epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "tsDLList.h"
#include "tsFreeList.h"
#include "epicsSingleton.h"
#include "resourceLib.h"
#include "epicsEvent.h"

#ifdef syncGrouph_restore_epicsExportSharedSymbols
#define epicsExportSharedSymbols
#endif

#include "shareLib.h"

#include "cadef.h"
#include "cacIO.h"

// does the local compiler support placement delete
#if defined (_MSC_VER) 
#   if _MSC_VER >= 1200
#	    define CASG_PLACEMENT_DELETE
#   endif
#else
#	define CASG_PLACEMENT_DELETE
#endif

static const unsigned CASG_MAGIC = 0xFAB4CAFE;

// used to control access to CASG's recycle routines which
// should only be indirectly invoked by CASG when its lock
// is applied
class casgRecycle {             // X aCC 655
public:
    virtual void recycleSyncGroupWriteNotify ( class syncGroupWriteNotify & io ) = 0;
    virtual void recycleSyncGroupReadNotify ( class syncGroupReadNotify & io ) = 0;
};

class syncGroupNotify : public tsDLNode < syncGroupNotify > {
public:
    syncGroupNotify  ( struct CASG &sgIn, chid );
    virtual void destroy ( casgRecycle & ) = 0;
    void show ( unsigned level ) const;
    bool ioInitiated () const;
protected:
    chid chan;
    struct CASG & sg;
    const unsigned magic;
    cacChannel::ioid id;
    bool idIsValid;
    virtual ~syncGroupNotify (); 
	syncGroupNotify ( const syncGroupNotify & );
	syncGroupNotify & operator = ( const syncGroupNotify & );
};

class syncGroupReadNotify : public syncGroupNotify, public cacReadNotify {
public:
    static syncGroupReadNotify * factory ( 
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > &, 
        struct CASG &, chid, void *pValueIn );
    void begin ( unsigned type, arrayElementCount count );
    void destroy ( casgRecycle & );
    void show ( unsigned level ) const;
protected:
    virtual ~syncGroupReadNotify ();
private:
    void *pValue;
    syncGroupReadNotify ( struct CASG &sgIn, chid, void *pValueIn );
    void * operator new ( size_t );
    void operator delete ( void * );
    void * operator new ( size_t, 
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & );
#   if defined ( CASG_PLACEMENT_DELETE )
    void operator delete ( void *, 
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & );
#   endif
    void completion (
        unsigned type, arrayElementCount count, const void *pData );
    void exception (
        int status, const char *pContext, unsigned type, arrayElementCount count );
	syncGroupReadNotify ( const syncGroupReadNotify & );
	syncGroupReadNotify & operator = ( const syncGroupReadNotify & );
};

class syncGroupWriteNotify : public syncGroupNotify, public cacWriteNotify {
public:
    static syncGroupWriteNotify * factory ( 
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &, 
        struct CASG &, chid );
    void begin ( unsigned type, arrayElementCount count, 
                            const void * pValueIn );
    void destroy ( casgRecycle & );
    void show ( unsigned level ) const;
protected:
    virtual ~syncGroupWriteNotify (); // allocate only from pool
private:
    void *pValue;
    syncGroupWriteNotify  ( struct CASG &, chid );
    void * operator new ( size_t );
    void operator delete ( void * );
    void * operator new ( size_t, 
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > & );
#   if defined ( CASG_PLACEMENT_DELETE )
    void operator delete ( void *, 
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > & );
#   endif
    void completion ();
    void exception ( int status, const char *pContext, 
		unsigned type, arrayElementCount count );
	syncGroupWriteNotify ( const syncGroupWriteNotify & );
	syncGroupWriteNotify & operator = ( const syncGroupWriteNotify & );
};

struct oldCAC;

class casgMutex {
public:
    void lock ();
    void unlock ();
    void show ( unsigned level ) const;
private:
    epicsMutex mutex;
};

struct CASG : public chronIntIdRes < CASG >, private casgRecycle {
public:
    CASG ( oldCAC & cacIn );
    bool ioComplete ();
    void destroy ();
    bool verify () const;
    int block ( double timeout );
    void reset ();
    void show ( unsigned level ) const;
    int get ( chid pChan, unsigned type, arrayElementCount count, void * pValue );
    int put ( chid pChan, unsigned type, arrayElementCount count, const void * pValue );
    void completionNotify ( syncGroupNotify & );
    void * operator new ( size_t size );
    void operator delete ( void * pCadaver, size_t size );
    int printf ( const char * pFormat, ... );
    void exception ( int status, const char *pContext, 
        const char *pFileName, unsigned lineNo );
    void exception ( int status, const char *pContext,
        const char *pFileName, unsigned lineNo, oldChannelNotify &chan, 
        unsigned type, arrayElementCount count, unsigned op );
protected:
    virtual ~CASG ();
private:
    tsDLList < syncGroupNotify > ioPendingList;
    tsDLList < syncGroupNotify > ioCompletedList;
    casgMutex mutable mutex;
    epicsEvent sem;
    oldCAC & client;
    unsigned magic;
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > freeListReadOP;
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > freeListWriteOP;
    void recycleSyncGroupWriteNotify ( syncGroupWriteNotify &io );
    void recycleSyncGroupReadNotify ( syncGroupReadNotify &io );
    static epicsSingleton < tsFreeList < struct CASG, 128 > > pFreeList;

    void destroyPendingIO ( syncGroupNotify * );
    void destroyCompletedIO ();
    void destroyPendingIO ();

	CASG ( const CASG & );
	CASG & operator = ( const CASG & );
};

inline bool syncGroupNotify::ioInitiated () const
{
    return this->idIsValid;
}

inline void casgMutex::lock ()
{
    this->mutex.lock ();
}

inline void casgMutex::unlock ()
{
    this->mutex.unlock ();
}

inline void casgMutex::show ( unsigned level ) const
{
    this->mutex.show ( level );
}

#endif // ifdef syncGrouph
