
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

#include "iocinf.h"

cacChannel::cacChannel () : pChannelIO (0) 
{
}

cacChannel::~cacChannel ()
{
    if ( this->pChannelIO ) {
        this->pChannelIO->destroy ();
    }
}

void cacChannel::attachIO (cacChannelIO &io)
{
    assert ( ! this->pChannelIO );
    this->pChannelIO = &io;
    this->ioAttachNotify ();
}

int cacChannel::read ( unsigned type, unsigned long count, cacNotify & notify )
{
    if ( this->pChannelIO ) {
        return pChannelIO->read (type, count, notify);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::read ( unsigned type, unsigned long count, void *pValue )
{
    if ( this->pChannelIO ) {
        return pChannelIO->read (type, count, pValue);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::write (unsigned type, unsigned long count, const void *pvalue )
{
    if ( this->pChannelIO ) {
        return pChannelIO->write (type, count, pvalue);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::write (unsigned type, unsigned long count, 
                       const void *pvalue, cacNotify & notify )
{
    if ( this->pChannelIO ) {
        return pChannelIO->write (type, count, pvalue, notify);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::subscribe ( unsigned type, unsigned long count, 
                           unsigned mask, cacNotify &notify )
{
    if ( this->pChannelIO ) {
        return pChannelIO->subscribe (type, count, mask, notify);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

void cacChannel::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        if ( this->pChannelIO ) {
            pChannelIO->hostName (pBuf, bufLength);
        }
        else {
            strncpy ( pBuf, "<not connected>", bufLength );
            pBuf[bufLength-1u] = '\0';
        }
    }
}

const char * cacChannel::pHostName () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->pHostName ();
    }
    else {
        return "<not connected>";
    }
}

short cacChannel::nativeType () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->nativeType ();
    }
    else {
        return TYPENOTCONN;
    }
}
 
unsigned long cacChannel::nativeElementCount () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->nativeElementCount ();
    }
    else {
        return 0ul;
    }
}

channel_state cacChannel::state () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->state ();
    }
    else {
        return cs_never_conn;
    }
}

bool cacChannel::readAccess () const
{
    if ( this->pChannelIO ) {
        caar ar = pChannelIO->accessRights ();
        return ar.read_access;
    }
    else {
        return false;
    }
}

bool cacChannel::writeAccess () const
{
    if ( this->pChannelIO ) {
        caar ar = pChannelIO->accessRights ();
        return ar.write_access;
    }
    else {
        return false;
    }
}

const char *cacChannel::pName () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->pName ();
    }
    else {
        return "<disconnected>";
    }
}

unsigned cacChannel::searchAttempts () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->searchAttempts ();
    }
    else {
        return 0u;
    }
}

bool cacChannel::ca_v42_ok () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->ca_v42_ok ();
    }
    else {
        return false;
    }
}

bool cacChannel::connected () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->connected ();
    }
    else {
        return false;
    }
}

caar cacChannel::accessRights () const
{
    if ( this->pChannelIO ) {
        return pChannelIO->accessRights ();
    }
    else {
        caar ar;
        ar.read_access = false;
        ar.write_access = false;
        return ar;
    }
}

void cacChannel::ioAttachNotify ()
{
}

void cacChannel::ioReleaseNotify ()
{
}

void cacChannel::connectNotify ()
{
}

void cacChannel::disconnectNotify ()
{
}

void cacChannel::accessRightsNotify ( caar )
{
}

void cacChannel::exceptionNotify ( int status, const char *pContext )
{
    ca_signal ( status, pContext );
}

void cacChannel::connectTimeoutNotify ()
{
}

unsigned cacChannel::readSequence () const
{
    if ( this->pChannelIO ) {
        return this->pChannelIO->readSequence ();
    }
    else {
        return 0u;
    }
}

void cacChannel::decrementOutstandingIO ()
{
    if ( this->pChannelIO ) {
        this->pChannelIO->decrementOutstandingIO ();
    }
}

void cacChannel::incrementOutstandingIO ()
{
    if ( this->pChannelIO ) {
        this->pChannelIO->incrementOutstandingIO ();
    }
}

void cacChannel::lock () const
{
    if ( this->pChannelIO ) {
        this->pChannelIO->lock ();
    }
}

void cacChannel::unlock () const
{
    if ( this->pChannelIO ) {
        this->pChannelIO->unlock ();
    }
}
