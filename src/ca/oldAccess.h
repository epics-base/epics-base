
/*  
 *  $Id$
 *
 *                              
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *                                  
 *  Copyright, The Regents of the University of California.
 *                                  
 *           
 *	Author Jeffrey O. Hill
 *	johill@lanl.gov
 *	505 665 1831
 */

struct oldChannelNotify : public cacChannelNotify {
public:
    oldChannelNotify ( cac &, const char *pName, caCh *pConnCallBackIn, void *pPrivateIn );
    void destroy ();
    bool ioAttachOK ();
    void setPrivatePointer ( void * );
    void * privatePointer () const;
    int changeConnCallBack ( caCh *pfunc );
    int replaceAccessRightsEvent ( caArh *pfunc );

    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );

    const char *pName () const;
    void show ( unsigned level ) const;
    void initiateConnect ();
    void read ( 
        unsigned type, unsigned long count, 
        cacDataNotify &notify, cacChannel::ioid *pId = 0 );
    void read ( 
        unsigned type, unsigned long count, 
        void *pValue );
    void write ( 
        unsigned type, unsigned long count, 
        const void *pValue );
    void write ( 
        unsigned type, unsigned long count, const void *pValue, 
        cacNotify &, cacChannel::ioid *pId = 0 );
    void subscribe ( 
        unsigned type, unsigned long count, unsigned mask, 
        cacDataNotify &, cacChannel::ioid & );
    void ioCancel ( const cacChannel::ioid & );
    void ioShow ( const cacChannel::ioid &, unsigned level ) const;
    short nativeType () const;
    unsigned long nativeElementCount () const;
    caAccessRights accessRights () const; // defaults to unrestricted access
    unsigned searchAttempts () const; // defaults to zero
    double beaconPeriod () const; // defaults to negative DBL_MAX
    bool ca_v42_ok () const; 
    bool connected () const; 
    bool previouslyConnected () const;
    void hostName ( char *pBuf, unsigned bufLength ) const; // defaults to local host name
    const char * pHostName () const; // deprecated - please do not use
protected:
    ~oldChannelNotify (); // must allocate from pool
private:
    cacChannel &io;
    caCh *pConnCallBack;
    void *pPrivate;
    caArh *pAccessRightsFunc;
    void connectNotify ( cacChannel &chan );
    void disconnectNotify ( cacChannel &chan );
    void accessRightsNotify ( cacChannel &chan, const caAccessRights & );
    bool includeFirstConnectInCountOfOutstandingIO () const;
    static tsFreeList < struct oldChannelNotify, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCopy : public cacDataNotify {
public:
    getCopy ( cac &cacCtx, unsigned type, 
        unsigned long count, void *pValue );
    void destroy ();
    void show ( unsigned level ) const;
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~getCopy (); // allocate only out of pool
private:
    unsigned long count;
    cac &cacCtx;
    void *pValue;
    unsigned readSeq;
    unsigned type;
    void completion (
        unsigned type, unsigned long count, const void *pData);
    void exception (
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class getCopy, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class getCallback : public cacDataNotify {
public:
    getCallback ( oldChannelNotify &chanIn, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~getCallback (); // allocate only out of pool
private:
    oldChannelNotify &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completion (
        unsigned type, unsigned long count, const void *pData);
    void exception (
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < class getCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

class putCallback : public cacNotify {
public:
    putCallback ( oldChannelNotify &, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
protected:
    ~putCallback (); // allocate only out of pool
private:
    oldChannelNotify &chan;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    void completion ();
    void exception ( 
        int status, const char *pContext );
    static tsFreeList < class putCallback, 1024 > freeList;
    static epicsMutex freeListMutex;
};

struct oldSubscription : public cacDataNotify {
public:
    oldSubscription ( 
        oldChannelNotify &,
        unsigned type, unsigned long nElem, unsigned mask, 
        caEventCallBackFunc *pFunc, void *pPrivate );
    bool ioAttachOK ();
    void destroy ();
    void * operator new ( size_t size );
    void operator delete ( void *pCadaver, size_t size );
    oldChannelNotify &channel () const;
protected:
    ~oldSubscription (); // must allocate from pool
private:
    oldChannelNotify &chan;
    cacChannel::ioid id;
    caEventCallBackFunc *pFunc;
    void *pPrivate;
    bool subscribed;
    void completion (
        unsigned type, unsigned long count, const void *pData );
    void exception (
        int status, const char *pContext, unsigned type, unsigned long count );
    static tsFreeList < struct oldSubscription, 1024 > freeList;
    static epicsMutex freeListMutex;
};

inline bool oldChannelNotify::ioAttachOK ()
{
    return &this->io ? true : false;
}

inline void oldChannelNotify::destroy ()
{
    delete this;
}

inline const char *oldChannelNotify::pName () const 
{
    return this->io.pName ();
}

inline void oldChannelNotify::show ( unsigned level ) const
{
    this->io.show ( level );
}

inline void oldChannelNotify::initiateConnect ()
{
    this->io.initiateConnect ();
}

inline void oldChannelNotify::read ( unsigned type, unsigned long count, 
                        cacDataNotify &notify, cacChannel::ioid *pId )
{
    this->io.read ( type, count, notify, pId );
}

inline void oldChannelNotify::write ( unsigned type, 
    unsigned long count, const void *pValue )
{
    this->io.write ( type, count, pValue );
}

inline void oldChannelNotify::write ( unsigned type, unsigned long count, 
                 const void *pValue, cacNotify &notify, cacChannel::ioid *pId )
{
    this->io.write ( type, count, pValue, notify, pId );
}

inline void oldChannelNotify::subscribe ( unsigned type, 
    unsigned long count, unsigned mask, cacDataNotify &notify,
    cacChannel::ioid &idOut)
{
    this->io.subscribe ( type, count, mask, notify, &idOut );
}

inline void oldChannelNotify::ioCancel ( const cacChannel::ioid &id )
{
    this->io.ioCancel ( id );
}

inline void oldChannelNotify::ioShow ( const cacChannel::ioid &id, unsigned level ) const
{
    this->io.ioShow ( id, level );
}

inline short oldChannelNotify::nativeType () const
{
    return this->io.nativeType ();
}

inline unsigned long oldChannelNotify::nativeElementCount () const
{
    return this->io.nativeElementCount ();
}

inline caAccessRights oldChannelNotify::accessRights () const
{
    return this->io.accessRights ();
}

inline unsigned oldChannelNotify::searchAttempts () const
{
    return this->io.searchAttempts ();
}

inline double oldChannelNotify::beaconPeriod () const
{
    return this->io.beaconPeriod ();
}

inline bool oldChannelNotify::ca_v42_ok () const
{
    return this->io.ca_v42_ok ();
}

inline bool oldChannelNotify::connected () const
{
    return this->io.connected ();
}

inline bool oldChannelNotify::previouslyConnected () const
{
    return this->io.previouslyConnected ();
}

inline void oldChannelNotify::hostName ( char *pBuf, unsigned bufLength ) const
{
    this->io.hostName ( pBuf, bufLength );
}

inline const char * oldChannelNotify::pHostName () const
{
    return this->io.pHostName ();
}

inline oldSubscription::oldSubscription  (
        oldChannelNotify &chanIn,
        unsigned type, unsigned long nElem, unsigned mask, 
        caEventCallBackFunc *pFuncIn, void *pPrivateIn ) :
    chan ( chanIn ), id ( 0 ), pFunc ( pFuncIn ), 
        pPrivate ( pPrivateIn ), subscribed ( false )
{
    this->chan.subscribe ( type, nElem, mask, *this, this->id );
    this->subscribed = true;
}

inline void oldSubscription::destroy ()
{
    if ( this->subscribed ) {
        this->chan.ioCancel ( this->id );
    }
    delete this;
}

inline void * oldSubscription::operator new ( size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    return oldSubscription::freeList.allocate ( size );
}

inline void oldSubscription::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( oldSubscription::freeListMutex );
    oldSubscription::freeList.release ( pCadaver, size );
}

inline oldChannelNotify & oldSubscription::channel () const
{
    return this->chan;
}

inline void getCopy::destroy ()
{
    delete this;
}

inline void * getCopy::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    return getCopy::freeList.allocate ( size );
}

inline void getCopy::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCopy::freeListMutex );
    getCopy::freeList.release ( pCadaver, size );
}

inline void putCallback::destroy ()
{
    delete this;
}

inline void * putCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    return putCallback::freeList.allocate ( size );
}

inline void putCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( putCallback::freeListMutex );
    putCallback::freeList.release ( pCadaver, size );
}

inline void getCallback::destroy ()
{
    delete this;
}

inline void * getCallback::operator new ( size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    return getCallback::freeList.allocate ( size );
}

inline void getCallback::operator delete ( void *pCadaver, size_t size )
{
    epicsAutoMutex locker ( getCallback::freeListMutex );
    getCallback::freeList.release ( pCadaver, size );
}
