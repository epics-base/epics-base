

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
    return this->dest;
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
    return this->fc;
}

inline void tcpiiu::flush ()
{
    if ( cacRingBufferWriteFlush ( &this->send ) ) {
        this->armSendWatchdog ();
    }
}

inline void tcpiiu::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        strncpy ( pBuf, this->host_name_str, bufLength );
        pBuf[bufLength - 1u] = '\0';
    }
}

// deprecated - please dont use
inline const char * tcpiiu::pHostName () const
{
    return this->host_name_str;
}

inline bool tcpiiu::ca_v42_ok () const
{
    return CA_V42 (CA_PROTOCOL_VERSION, this->minor_version_number);
}

inline bool tcpiiu::ca_v41_ok () const
{
    return CA_V41 (CA_PROTOCOL_VERSION, this->minor_version_number);
}

inline SOCKET tcpiiu::getSock () const
{
    return this->sock;
}
