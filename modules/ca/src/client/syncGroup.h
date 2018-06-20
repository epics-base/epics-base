/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
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
#   define syncGrouph_restore_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "tsDLList.h"
#include "tsFreeList.h"
#include "resourceLib.h"
#include "epicsEvent.h"
#include "compilerDependencies.h"

#ifdef syncGrouph_restore_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#include "cadef.h"
#include "cacIO.h"

static const unsigned CASG_MAGIC = 0xFAB4CAFE;

class syncGroupNotify : public tsDLNode < syncGroupNotify > {
public:
    syncGroupNotify ();
    virtual void destroy (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard ) = 0;
    virtual bool ioPending (
        epicsGuard < epicsMutex > & guard ) = 0;
    virtual void cancel (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & mutualExclusionGuard ) = 0;
    virtual void show (
        epicsGuard < epicsMutex > &,
        unsigned level ) const = 0;
protected:
    virtual ~syncGroupNotify ();
    syncGroupNotify ( const syncGroupNotify & );
    syncGroupNotify & operator = ( const syncGroupNotify & );
};

struct CASG;

class syncGroupReadNotify : public syncGroupNotify, public cacReadNotify {
public:
    typedef void ( CASG :: * PRecycleFunc ) 
        ( epicsGuard < epicsMutex > &, syncGroupReadNotify & );
    static syncGroupReadNotify * factory (
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > &,
        CASG &, PRecycleFunc, chid, void *pValueIn );
    void destroy (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    bool ioPending (
        epicsGuard < epicsMutex > & guard );
    void begin (  epicsGuard < epicsMutex > &,
        unsigned type, arrayElementCount count );
    void cancel (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void show (  epicsGuard < epicsMutex > &, unsigned level ) const;
protected:
    syncGroupReadNotify ( CASG & sgIn, PRecycleFunc, chid, void * pValueIn );
    virtual ~syncGroupReadNotify ();
private:
    chid chan;
    PRecycleFunc pRecycleFunc;
    CASG & sg;
    void * pValue;
    const unsigned magic;
    cacChannel::ioid id;
    bool idIsValid;
    bool ioComplete;
    void operator delete ( void * );
    void * operator new ( size_t,
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > & ))
    void completion (
        epicsGuard < epicsMutex > &, unsigned type,
        arrayElementCount count, const void * pData );
    void exception (
        epicsGuard < epicsMutex > &, int status,
        const char * pContext, unsigned type, arrayElementCount count );
    syncGroupReadNotify ( const syncGroupReadNotify & );
    syncGroupReadNotify & operator = ( const syncGroupReadNotify & );
};

class syncGroupWriteNotify : public syncGroupNotify, public cacWriteNotify {
public:
    typedef void ( CASG :: * PRecycleFunc ) 
        ( epicsGuard < epicsMutex > &, syncGroupWriteNotify & );
    static syncGroupWriteNotify * factory (
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > &,
        CASG &, PRecycleFunc, chid );
    void destroy (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    bool ioPending (
        epicsGuard < epicsMutex > & guard );
    void begin (  epicsGuard < epicsMutex > &, unsigned type,
        arrayElementCount count, const void * pValueIn );
    void cancel (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void show (  epicsGuard < epicsMutex > &, unsigned level ) const;
protected:
    syncGroupWriteNotify  ( struct CASG &, PRecycleFunc, chid );
    virtual ~syncGroupWriteNotify (); // allocate only from pool
private:
    chid chan;
    PRecycleFunc pRecycleFunc;
    CASG & sg;
    const unsigned magic;
    cacChannel::ioid id;
    bool idIsValid;
    bool ioComplete;
    void operator delete ( void * );
    void * operator new ( size_t,
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > & ))
    void completion ( epicsGuard < epicsMutex > & );
    void exception (
        epicsGuard < epicsMutex > &, int status, const char *pContext,
		unsigned type, arrayElementCount count );
    syncGroupWriteNotify ( const syncGroupWriteNotify & );
    syncGroupWriteNotify & operator = ( const syncGroupWriteNotify & );
};

struct ca_client_context;

template < class T > class sgAutoPtr;

struct CASG : public chronIntIdRes < CASG > {
public:
    CASG ( epicsGuard < epicsMutex > &, ca_client_context & cacIn );
    void destructor (
        CallbackGuard &,
        epicsGuard < epicsMutex > & guard );
    bool ioComplete (
        CallbackGuard &,
        epicsGuard < epicsMutex > & guard );
    bool verify ( epicsGuard < epicsMutex > & ) const;
    int block ( epicsGuard < epicsMutex > * pcbGuard,
        epicsGuard < epicsMutex > & guard, double timeout );
    void reset ( CallbackGuard &, epicsGuard < epicsMutex > & );
    void show ( epicsGuard < epicsMutex > &, unsigned level ) const;
    void show ( unsigned level ) const;
    void get ( epicsGuard < epicsMutex > &, chid pChan, 
        unsigned type, arrayElementCount count, void * pValue );
    void put ( epicsGuard < epicsMutex > &, chid pChan, 
        unsigned type, arrayElementCount count, const void * pValue );
    void completionNotify (
        epicsGuard < epicsMutex > &, syncGroupNotify & );
    int printFormated ( const char * pFormat, ... );
    void exception (
         epicsGuard < epicsMutex > &, int status, const char * pContext,
        const char * pFileName, unsigned lineNo );
    void exception (
         epicsGuard < epicsMutex > &, int status, const char * pContext,
        const char * pFileName, unsigned lineNo, oldChannelNotify & chan,
        unsigned type, arrayElementCount count, unsigned op );
    void * operator new ( size_t size,
        tsFreeList < struct CASG, 128, epicsMutexNOOP  > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < struct CASG, 128, epicsMutexNOOP > & ))

private:
    tsDLList < syncGroupNotify > ioPendingList;
    tsDLList < syncGroupNotify > ioCompletedList;
    epicsEvent sem;
    ca_client_context & client;
    unsigned magic;
    tsFreeList < class syncGroupReadNotify, 128, epicsMutexNOOP > freeListReadOP;
    tsFreeList < class syncGroupWriteNotify, 128, epicsMutexNOOP > freeListWriteOP;

    void destroyPendingIO (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void destroyCompletedIO (
        CallbackGuard & cbGuard,
        epicsGuard < epicsMutex > & guard );
    void recycleReadNotifyIO ( epicsGuard < epicsMutex > &, 
                                syncGroupReadNotify & );
    void recycleWriteNotifyIO ( epicsGuard < epicsMutex > &, 
                                syncGroupWriteNotify & );

    CASG ( const CASG & );
    CASG & operator = ( const CASG & );

    void operator delete ( void * );

    ~CASG ();

    friend class sgAutoPtr < syncGroupWriteNotify >;
    friend class sgAutoPtr < syncGroupReadNotify >;
};

class boolFlagManager {
public:
    boolFlagManager ( bool & flag );
    ~boolFlagManager ();
    void release ();
private:
    bool * pBool;
};

inline boolFlagManager::boolFlagManager ( bool & flagIn ) :
    pBool ( & flagIn )
{
    *this->pBool = true;
}

inline boolFlagManager::~boolFlagManager ()
{
    if ( this->pBool ) {
        *this->pBool = false;
    }
}

inline void boolFlagManager::release ()
{
    this->pBool = 0;
}

inline void * CASG::operator new ( size_t size,
    tsFreeList < struct CASG, 128, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
inline void CASG::operator delete ( void * pCadaver,
    tsFreeList < struct CASG, 128, epicsMutexNOOP > & freeList )
{
    freeList.release ( pCadaver );
}
#endif

inline bool syncGroupWriteNotify::ioPending (
    epicsGuard < epicsMutex > & /* guard */ )
{
    return ! this->ioComplete;
}

inline bool syncGroupReadNotify::ioPending (
    epicsGuard < epicsMutex > & /* guard */ )
{
    return ! this->ioComplete;
}

#endif // ifdef syncGrouph
