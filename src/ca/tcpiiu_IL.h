

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

inline osiSockAddr tcpiiu::address () const
{
    return this->ipToA.address ();
}

inline void * tcpiiu::operator new (size_t size)
{ 
    return tcpiiu::freeList.allocate (size);
}

inline void tcpiiu::operator delete (void *pCadaver, size_t size)
{ 
    tcpiiu::freeList.release (pCadaver,size);
}

inline bool tcpiiu::fullyConstructed () const
{
    return this->fullyConstructedFlag;
}

inline void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    this->ipToA.hostName ( pBuf, bufLength );
}

// deprecated - please dont use
inline const char * tcpiiu::pHostName () const
{
    static char nameBuf [128];
    this->ipToA.hostName ( nameBuf, sizeof ( nameBuf ) );
    return nameBuf; // ouch !!
}

inline SOCKET tcpiiu::getSock () const
{
    return this->sock;
}

inline void tcpiiu::flush ()
{
    this->flushPending = true;
    semBinaryGive ( this->sendThreadFlushSignal );
}

inline bool tcpiiu::ca_v44_ok () const
{
    return CA_V44 ( CA_PROTOCOL_VERSION, this->minorProtocolVersionNumber );
}

inline bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 ( CA_PROTOCOL_VERSION, this->minorProtocolVersionNumber );
}

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 ( CA_PROTOCOL_VERSION, this->minorProtocolVersionNumber );
}

inline bool tcpiiu::alive () const
{
    if ( this->state == iiu_connecting || 
        this->state == iiu_connected ) {
        return true;
    }
    else {
        return false;
    }
}
