
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


//
// nciu inline member functions
//

inline void * nciu::operator new ( size_t size )
{ 
    return nciu::freeList.allocate ( size );
}

inline void nciu::operator delete ( void *pCadaver, size_t size )
{ 
    nciu::freeList.release ( pCadaver, size );
}

inline void nciu::lock () const
{
    this->cacCtx.nciuPrivate::mutex.lock ();
}

inline void nciu::unlock () const
{
    this->cacCtx.nciuPrivate::mutex.unlock ();
}

inline bool nciu::fullyConstructed () const
{
    return this->f_fullyConstructed;
}

inline bool nciu::identifierEquivelence ( unsigned idToMatch )
{
    return idToMatch == this->id;
}

inline void nciu::resetRetryCount () 
{
    this->retry = 0u;
}

inline void nciu::accessRightsStateChange ( const caar &arIn )
{
    this->ar = ar;
    this->accessRightsNotify ( this->ar );
}

inline unsigned nciu::getSID () const
{
    return this->sid;
}

inline unsigned nciu::getRetrySeqNo () const
{
    return this->retrySeqNo;
}

inline void nciu::lockPIIU () const
{
    this->lock ();
    assert ( this->ptrLockCount < USHRT_MAX );
    this->ptrLockCount++;
    this->unlock ();
}

inline void nciu::unlockPIIU () const
{
    bool signalNeeded;

    this->lock ();
    assert ( this->ptrLockCount > 0 );
    this->ptrLockCount--;
    if ( this->ptrLockCount == 0u && this->ptrUnlockWaitCount > 0u ) {
        this->ptrUnlockWaitCount--;
        signalNeeded = true;
    }
    else {
        signalNeeded = false;
    }
    this->unlock ();

    if ( signalNeeded ) {
        this->cacCtx.nciuPrivate::ptrLockReleaseWakeup.signal ();
    }
}

inline void nciu::subscriptionCancelMsg ( ca_uint32_t clientId )
{
    this->lockPIIU ();

    if ( this->piiu ) {
        this->piiu->subscriptionCancelRequest ( clientId, this->sid, this->typeCode, this->count );
    }

    this->unlockPIIU ();
}

inline void nciu::connect ( tcpiiu &iiu )
{
    this->connect ( iiu, this->typeCode, this->count, this->sid );
}

inline void nciu::searchReplySetUp ( unsigned sidIn, 
    unsigned typeIn, unsigned long countIn )
{
    this->lock ();

    this->typeCode = typeIn;      
    this->count = countIn;
    this->sid = sidIn;
    this->ar.read_access = true;
    this->ar.write_access = true;

    this->unlock ();
}

inline bool nciu::connected () const
{
    return this->f_connected;
}

inline bool nciu::connectionInProgress ( const osiSockAddr &addrIn )
{
    bool status;

    this->lockPIIU ();

    if ( this->piiu ) {
        status = this->piiu->connectionInProgress ( this->pNameStr, addrIn );
    }
    else { 
        status = false;
    }

    this->unlockPIIU ();

    return status;
}

inline void nciu::ioInstall ( class baseNMIU &nmiu )
{
    this->cacCtx.ioInstall ( *this, nmiu );
}

inline void nciu::ioDestroy ( unsigned id )
{
    this->cacCtx.ioDestroy ( id );
}

