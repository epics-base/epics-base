
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

#ifndef netIOh  
#define netIOh

#include "nciu.h"

class baseNMIU : public tsDLNode < baseNMIU >, 
        public chronIntIdRes < baseNMIU > {
public:
    baseNMIU ( nciu &chan );
    virtual void destroy ( class cacRecycle & ) = 0; // only called by cac
    virtual void completion () = 0;
    virtual void exception ( int status, 
        const char *pContext ) = 0;
    virtual void exception ( int status, 
        const char *pContext, unsigned type, 
        arrayElementCount count ) = 0;
    virtual void completion ( unsigned type, 
        arrayElementCount count, const void *pData ) = 0;
    virtual class netSubscription * isSubscription ();
    void show ( unsigned level ) const;
    ca_uint32_t getID () const;
    nciu & channel () const;
protected:
    virtual ~baseNMIU () = 0;
//
// perhaps we should not store the channel here and instead fetch it out of the 
// notify 
//
    nciu & chan;
private:
	baseNMIU ( const baseNMIU & );
	baseNMIU & operator = ( const baseNMIU & );
};

class netSubscription : public baseNMIU  {
public:
    static netSubscription * factory ( 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &, 
        nciu &chan, unsigned type, arrayElementCount count, 
        unsigned mask, cacStateNotify &notify );
    void show ( unsigned level ) const;
    arrayElementCount getCount () const;
    unsigned getType () const;
    unsigned getMask () const;
    void destroy ( class cacRecycle & );
    void completion ();
    void exception ( int status, 
        const char *pContext );
    void completion ( unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( int status, 
        const char *pContext, unsigned type, 
        arrayElementCount count );
protected:
    ~netSubscription ();
private:
    const arrayElementCount count;
    cacStateNotify &notify;
    const unsigned type;
    const unsigned mask;
    netSubscription ( nciu &chan, unsigned type, arrayElementCount count, 
        unsigned mask, cacStateNotify &notify );
    class netSubscription * isSubscription ();
    void * operator new ( size_t, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & );
#   if ! defined ( NO_PLACEMENT_DELETE )
    void operator delete ( void *, size_t, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & );
#   endif
	netSubscription ( const netSubscription & );
	netSubscription & operator = ( const netSubscription & );
};

class netReadNotifyIO : public baseNMIU {
public:
    static netReadNotifyIO * factory ( 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > &, 
        nciu &chan, cacReadNotify &notify );
    void show ( unsigned level ) const;
    void destroy ( class cacRecycle & );
    void completion ();
    void exception ( int status, const char *pContext );
    void completion ( unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( int status, const char *pContext, 
        unsigned type, arrayElementCount count );
protected:
    ~netReadNotifyIO ();
private:
    cacReadNotify &notify;
    netReadNotifyIO ( nciu &chan, cacReadNotify &notify );
    void * operator new ( size_t, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & );
#   if ! defined ( NO_PLACEMENT_DELETE )
    void operator delete ( void *, size_t, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & );
#   endif
	netReadNotifyIO ( const netReadNotifyIO & );
	netReadNotifyIO & operator = ( const netReadNotifyIO & );
};

class netWriteNotifyIO : public baseNMIU {
public:
    static netWriteNotifyIO * factory ( 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &, 
        nciu &chan, cacWriteNotify &notify );
    void show ( unsigned level ) const;
    void destroy ( class cacRecycle & );
    void completion ();
    void exception ( int status, const char *pContext );
    void completion ( unsigned type, 
        arrayElementCount count, const void *pData );
    void exception ( int status, const char *pContext, 
        unsigned type, arrayElementCount count );
protected:
    ~netWriteNotifyIO ();
private:
    cacWriteNotify &notify;
    netWriteNotifyIO ( nciu &chan, cacWriteNotify &notify );
    void * operator new ( size_t, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & );
#   if ! defined ( NO_PLACEMENT_DELETE )
    void operator delete ( void *, size_t, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & );
#   endif
	netWriteNotifyIO ( const netWriteNotifyIO & );
	netWriteNotifyIO & operator = ( const netWriteNotifyIO & );
};

inline ca_uint32_t baseNMIU::getID () const
{
    return this->id;
}

inline class nciu & baseNMIU::channel () const
{
    return this->chan;
}

inline void * netSubscription::operator new ( size_t size, 
    tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &freeList )
{
    return freeList.allocate ( size );
}

// NOTE: The constructor for netSubscription::netSubscription() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netSubscription when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netSubscription::netSubscription()
#if ! defined ( NO_PLACEMENT_DELETE )
inline void netSubscription::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &freeList )
{
    freeList.release ( pCadaver, size );
}
#endif

inline netSubscription * netSubscription::factory ( 
    tsFreeList < class netSubscription, 1024, epicsMutexNOOP > &freeList, 
    nciu &chan, unsigned type, arrayElementCount count, 
    unsigned mask, cacStateNotify &notify )
{
    return new ( freeList ) netSubscription ( chan, type, 
                                count, mask, notify );
}

inline arrayElementCount netSubscription::getCount () const // X aCC 361
{
    arrayElementCount nativeCount = this->chan.nativeElementCount ();
    if ( this->count == 0u || this->count > nativeCount ) {
        return nativeCount;
    }
    else {
        return this->count;
    }
}

inline unsigned netSubscription::getType () const
{
    return this->type;
}

inline unsigned netSubscription::getMask () const
{
    return this->mask;
}

inline netReadNotifyIO * netReadNotifyIO::factory ( 
    tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > &freeList, 
    nciu &chan, cacReadNotify &notify )
{
    return new ( freeList ) netReadNotifyIO ( chan, notify );
}

inline void * netReadNotifyIO::operator new ( size_t size, 
    tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > &freeList )
{
    return freeList.allocate ( size );
}

// NOTE: The constructor for netReadNotifyIO::netReadNotifyIO() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netReadNotifyIO when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netReadNotifyIO::netReadNotifyIO()
#if ! defined ( NO_PLACEMENT_DELETE )
inline void netReadNotifyIO::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > &freeList ) {
    freeList.release ( pCadaver, size );
}
#endif

inline netWriteNotifyIO * netWriteNotifyIO::factory ( 
    tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &freeList, 
    nciu &chan, cacWriteNotify &notify )
{
    return new ( freeList ) netWriteNotifyIO ( chan, notify );
}

inline void * netWriteNotifyIO::operator new ( size_t size, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &freeList )
{ 
    return freeList.allocate ( size );
}

// NOTE: The constructor for netWriteNotifyIO::netWriteNotifyIO() currently does
// not throw an exception, but we should eventually have placement delete
// defined for class netWriteNotifyIO when compilers support this so that 
// there is no possibility of a leak if there was an exception in
// a future version of netWriteNotifyIO::netWriteNotifyIO()
#if ! defined ( NO_PLACEMENT_DELETE )
inline void netWriteNotifyIO::operator delete ( void *pCadaver, size_t size, 
    tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > &freeList )
{
    freeList.release ( pCadaver, size );
}
#endif

#endif // ifdef netIOh
