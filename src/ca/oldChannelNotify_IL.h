
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

#ifndef oldChannelNotifyILh
#define oldChannelNotifyILh

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

#endif // ifndef oldChannelNotifyILh
