
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
    this->ar = arIn;
    this->accessRightsNotify ( this->ar );
}

inline ca_uint32_t nciu::getSID () const
{
    return this->sid;
}

inline ca_uint32_t nciu::getCID () const
{
    return this->id;
}


//
// this routine is used to verify that the channel's
// iiu pointer and connection state has not changed 
// before the iiu's mutex was applied
//

inline bool nciu::verifyConnected ( netiiu &iiuIn )
{
    return ( this->piiu == &iiuIn ) && this->f_connected;
}

inline bool nciu::verifyIIU ( netiiu &iiuIn )
{
    return ( this->piiu == &iiuIn );
}

inline unsigned nciu::getRetrySeqNo () const
{
    return this->retrySeqNo;
}

inline void nciu::subscriptionCancelMsg ( netSubscription &subsc )
{
    this->piiu->subscriptionCancelRequest ( subsc );
}

// this is to only be used by early protocol revisions
inline void nciu::connect ()
{
    this->connect ( this->typeCode, this->count, this->sid );
}

inline void nciu::searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
    unsigned typeIn, unsigned long countIn )
{
    this->lock ();
    this->piiu = &iiu;
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

inline bool nciu::isAttachedToVirtaulCircuit ( const osiSockAddr &addrIn )
{
    return this->piiu->isVirtaulCircuit ( this->pNameStr, addrIn );
}

inline netiiu * nciu::getPIIU ()
{
    return this->piiu;
}

