
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
