
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

// does the local compiler support placement delete
#if defined (_MSC_VER)
#   if _MSC_VER >= 1200
#       define NETIO_PLACEMENT_DELETE
#   endif
#elif defined (__GNUC__) && 0
#   if __GNUC__>2 || ( __GNUC__==2 && __GNUC_MINOR_>=96 )
#	    define NETIO_PLACEMENT_DELETE
#   endif
#else
#   define NETIO_PLACEMENT_DELETE
#endif

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
    virtual void show ( unsigned level ) const;
    ca_uint32_t getID () const;
    nciu & channel () const;
protected:
    virtual ~baseNMIU ();
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
#   if defined ( NETIO_PLACEMENT_DELETE )
    void operator delete ( void *, 
        tsFreeList < class netSubscription, 1024, epicsMutexNOOP > & );
#   endif
    netSubscription ( const netSubscription & );
    netSubscription & operator = ( const netSubscription & );
    ~netSubscription ();
#   if defined (_MSC_VER) && _MSC_VER == 1300 
        void operator delete ( void * ); // avoid visual c++ 7 bug
#   endif
#   if __GNUC__==2 && __GNUC_MINOR_<96 
        void operator delete ( void *, size_t ); // avoid gnu g++ bug
#   endif
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
private:
    cacReadNotify & notify;
    netReadNotifyIO ( nciu & chan, cacReadNotify & notify );
    void * operator new ( size_t, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & );
#   if defined ( NETIO_PLACEMENT_DELETE )
    void operator delete ( void *, 
        tsFreeList < class netReadNotifyIO, 1024, epicsMutexNOOP > & );
#   endif
    ~netReadNotifyIO ();
    netReadNotifyIO ( const netReadNotifyIO & );
    netReadNotifyIO & operator = ( const netReadNotifyIO & );
#   if defined (_MSC_VER) && _MSC_VER == 1300
        void operator delete ( void * ); // avoid visual c++ 7 bug
#   endif
#   if __GNUC__==2 && __GNUC_MINOR_<96 
        void operator delete ( void *, size_t ); // avoid gnu g++ bug
#   endif
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
private:
    cacWriteNotify &notify;
    netWriteNotifyIO ( nciu &chan, cacWriteNotify &notify );
    void * operator new ( size_t, 
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & );
#   if defined ( NETIO_PLACEMENT_DELETE )
    void operator delete ( void *,
        tsFreeList < class netWriteNotifyIO, 1024, epicsMutexNOOP > & );
#   endif
    netWriteNotifyIO ( const netWriteNotifyIO & );
    netWriteNotifyIO & operator = ( const netWriteNotifyIO & );
    ~netWriteNotifyIO ();
#   if defined (_MSC_VER) && _MSC_VER == 1300
        void operator delete ( void * ); // avoid visual c++ 7 bug
#   endif
#   if __GNUC__==2 && __GNUC_MINOR_<96 
        void operator delete ( void *, size_t ); // avoid gnu g++ bug
#   endif
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

#endif // ifdef netIOh
