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

#ifndef netIOh  
#define netIOh

#include "nciu.h"
#include "compilerDependencies.h"

// SUN PRO generates multiply defined symbols if the baseNMIU
// destructor is virtual (therefore it is protected). 
// I assume that SUNPRO will fix this in future versions.
// With other compilers we get warnings (and 
// potential problems) if we dont make the baseNMIU
// destructor virtual.
#if defined ( __SUNPRO_CC ) && ( __SUNPRO_CC <= 0x540 )
#   define NETIO_VIRTUAL_DESTRUCTOR
#else
#   define NETIO_VIRTUAL_DESTRUCTOR virtual
#endif

class privateInterfaceForIO;

class baseNMIU : public tsDLNode < baseNMIU >,
        public chronIntIdRes < baseNMIU > {
public:
    virtual void destroy ( 
        epicsGuard < epicsMutex > &, class cacRecycle & ) = 0; // only called by cac
    virtual void completion ( 
        epicsGuard < epicsMutex > &, cacRecycle & ) = 0;
    virtual void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &, 
        int status, const char * pContext ) = 0;
    virtual void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext, unsigned type, 
        arrayElementCount count ) = 0;
    virtual void completion ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        unsigned type, arrayElementCount count, 
        const void * pData ) = 0;
    virtual void forceSubscriptionUpdate (
        epicsGuard < epicsMutex > & guard, nciu & chan ) = 0;
    virtual class netSubscription * isSubscription () = 0;
    virtual void show ( 
        unsigned level ) const = 0;
    virtual void show ( 
        epicsGuard < epicsMutex > &, 
        unsigned level ) const = 0;
protected:
    NETIO_VIRTUAL_DESTRUCTOR ~baseNMIU (); 
};

class netSubscription : public baseNMIU  {
public:
    static netSubscription * factory ( 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &, 
        class privateInterfaceForIO &, unsigned type, arrayElementCount count, 
        unsigned mask, cacStateNotify & );
    void show ( 
        unsigned level ) const;
    void show ( 
        epicsGuard < epicsMutex > &, unsigned level ) const;
    arrayElementCount getCount (
        epicsGuard < epicsMutex > &, bool allow_zero ) const;
    unsigned getType (
        epicsGuard < epicsMutex > & ) const;
    unsigned getMask (
        epicsGuard < epicsMutex > & ) const;
    void subscribeIfRequired (
        epicsGuard < epicsMutex > & guard, nciu & chan );
    void unsubscribeIfRequired ( 
        epicsGuard < epicsMutex > & guard, nciu & chan );
protected:
    netSubscription ( 
        class privateInterfaceForIO &, unsigned type, 
        arrayElementCount count, 
        unsigned mask, cacStateNotify & );
    ~netSubscription ();
private:
    const arrayElementCount count;
    class privateInterfaceForIO & privateChanForIO;
    cacStateNotify & notify;
    const unsigned type;
    const unsigned mask;
    bool subscribed;
    class netSubscription * isSubscription ();
    void operator delete ( void * );
    void * operator new ( size_t, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & ))
    void destroy ( 
        epicsGuard < epicsMutex > &, class cacRecycle & );
    void completion (
        epicsGuard < epicsMutex > &, cacRecycle & );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext );
    void completion ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        unsigned type, arrayElementCount count, const void * pData );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext, unsigned type, 
        arrayElementCount count );
    void forceSubscriptionUpdate (
        epicsGuard < epicsMutex > & guard, nciu & chan );
    netSubscription ( const netSubscription & );
    netSubscription & operator = ( const netSubscription & );
};

class netReadNotifyIO : public baseNMIU {
public:
    static netReadNotifyIO * factory ( 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > &, 
        privateInterfaceForIO &, cacReadNotify & );
    void show ( 
        unsigned level ) const;
    void show ( 
        epicsGuard < epicsMutex > &, unsigned level ) const;
protected:
    netReadNotifyIO ( privateInterfaceForIO &, cacReadNotify & );
    ~netReadNotifyIO ();
private:
    cacReadNotify & notify;
    class privateInterfaceForIO & privateChanForIO;
    void operator delete ( void * );
    void * operator new ( size_t, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & ))
    void destroy ( 
        epicsGuard < epicsMutex > &, class cacRecycle & );
    void completion (
        epicsGuard < epicsMutex > &, cacRecycle & );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext );
    void completion ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        unsigned type, arrayElementCount count,
        const void * pData );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext, 
        unsigned type, arrayElementCount count );
    class netSubscription * isSubscription ();
    void forceSubscriptionUpdate (
        epicsGuard < epicsMutex > & guard, nciu & chan );
    netReadNotifyIO ( const netReadNotifyIO & );
    netReadNotifyIO & operator = ( const netReadNotifyIO & );
};

class netWriteNotifyIO : public baseNMIU {
public:
    static netWriteNotifyIO * factory ( 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &, 
        privateInterfaceForIO &, cacWriteNotify & );
    void show ( 
        unsigned level ) const;
    void show ( 
        epicsGuard < epicsMutex > &, unsigned level ) const;
protected:
    netWriteNotifyIO ( privateInterfaceForIO &, cacWriteNotify & );
    ~netWriteNotifyIO ();
private:
    cacWriteNotify & notify;
    privateInterfaceForIO & privateChanForIO;
    void operator delete ( void * );
    void * operator new ( size_t, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & );
    epicsPlacementDeleteOperator (( void *,
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & ))
    class netSubscription * isSubscription ();
    void destroy ( 
        epicsGuard < epicsMutex > &, class cacRecycle & );
    void completion (
        epicsGuard < epicsMutex > &, cacRecycle & );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext );
    void completion ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        unsigned type, arrayElementCount count,
        const void * pData );
    void exception ( 
        epicsGuard < epicsMutex > &, cacRecycle &,
        int status, const char * pContext, unsigned type, 
        arrayElementCount count );
    void forceSubscriptionUpdate (
        epicsGuard < epicsMutex > & guard, nciu & chan );
    netWriteNotifyIO ( const netWriteNotifyIO & );
    netWriteNotifyIO & operator = ( const netWriteNotifyIO & );
};

inline void * netSubscription::operator new ( size_t size, 
    tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &freeList )
{
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
    inline void netSubscription::operator delete ( void *pCadaver, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &freeList ) 
    {
        freeList.release ( pCadaver );
    }
#endif

inline netSubscription * netSubscription::factory ( 
    tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & freeList, 
    class privateInterfaceForIO & chan, unsigned type, arrayElementCount count, 
    unsigned mask, cacStateNotify &notify )
{
    return new ( freeList ) netSubscription ( chan, type,
                                count, mask, notify );
}

inline arrayElementCount netSubscription::getCount (
    epicsGuard < epicsMutex > & guard, bool allow_zero ) const
{
    //guard.assertIdenticalMutex ( this->mutex );
    arrayElementCount nativeCount = this->privateChanForIO.nativeElementCount ( guard );
    if ( (this->count == 0u && !allow_zero) || this->count > nativeCount ) {
        return nativeCount;
    }
    else {
        return this->count;
    }
}

inline unsigned netSubscription::getType ( epicsGuard < epicsMutex > & ) const
{
    return this->type;
}

inline unsigned netSubscription::getMask ( epicsGuard < epicsMutex > & ) const
{
    return this->mask;
}

inline netReadNotifyIO * netReadNotifyIO::factory ( 
    tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & freeList, 
    privateInterfaceForIO & ioComplNotifIntf, cacReadNotify & notify )
{
    return new ( freeList ) netReadNotifyIO ( ioComplNotifIntf, notify );
}

inline void * netReadNotifyIO::operator new ( size_t size, 
    tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & freeList )
{
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
    inline void netReadNotifyIO::operator delete ( void *pCadaver, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & freeList ) 
    {
        freeList.release ( pCadaver );
    }
#endif

inline netWriteNotifyIO * netWriteNotifyIO::factory ( 
    tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & freeList, 
    privateInterfaceForIO & ioComplNotifyIntf, cacWriteNotify & notify )
{
    return new ( freeList ) netWriteNotifyIO ( ioComplNotifyIntf, notify );
}

inline void * netWriteNotifyIO::operator new ( size_t size, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & freeList )
{ 
    return freeList.allocate ( size );
}

#if defined ( CXX_PLACEMENT_DELETE )
    inline void netWriteNotifyIO::operator delete ( void *pCadaver, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & freeList ) 
    {
        freeList.release ( pCadaver );
    }
#endif

#endif // ifdef netIOh
