
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
    epicsAutoMutex locker ( nciu::freeListMutex );
    return nciu::freeList.allocate ( size );
}

inline void nciu::operator delete ( void *pCadaver, size_t size )
{ 
    epicsAutoMutex locker ( nciu::freeListMutex );
    nciu::freeList.release ( pCadaver, size );
}

inline bool nciu::identifierEquivelence ( unsigned idToMatch )
{
    return idToMatch == this->id;
}

inline void nciu::resetRetryCount () 
{
    this->retry = 0u;
}

inline void nciu::accessRightsStateChange ( const caAccessRights &arIn )
{
    this->accessRightState = arIn;
    this->notify().accessRightsNotify ( *this, arIn );
}

inline ca_uint32_t nciu::getSID () const
{
    return this->sid;
}

inline ca_uint32_t nciu::getCID () const
{
    return this->id;
}

inline unsigned nciu::getRetrySeqNo () const
{
    return this->retrySeqNo;
}

// this is to only be used by early protocol revisions
inline void nciu::connect ()
{
    this->connect ( this->typeCode, this->count, this->sid );
}

inline void nciu::searchReplySetUp ( netiiu &iiu, unsigned sidIn, 
    unsigned typeIn, unsigned long countIn )
{
    this->piiu = &iiu;
    this->typeCode = typeIn;      
    this->count = countIn;
    this->sid = sidIn;
}

inline bool nciu::connected () const
{
    return this->f_connected;
}

inline bool nciu::previouslyConnected () const
{
    return this->f_previousConn;
}

inline bool nciu::isAttachedToVirtaulCircuit ( const osiSockAddr &addrIn )
{
    return this->piiu->isVirtaulCircuit ( this->pNameStr, addrIn );
}

inline netiiu * nciu::getPIIU ()
{
    return this->piiu;
}

inline cac & nciu::getClient ()
{
    return this->cacCtx;
}

inline void nciu::connectTimeoutNotify ()
{
    this->f_connectTimeOutSeen = true;
}





