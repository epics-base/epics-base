
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

#include <float.h>

#include "iocinf.h"

cacChannel::cacChannel () : pChannelIO (0) 
{
}

cacChannel::~cacChannel ()
{
    cacChannelIO *pChanIO = this->pChannelIO;
    if ( pChanIO ) {
        pChanIO->destroy ();
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
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->read ( type, count, notify );
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::read ( unsigned type, unsigned long count, void *pValue )
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->read ( type, count, pValue );
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::write (unsigned type, unsigned long count, const void *pvalue )
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->write (type, count, pvalue);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::write (unsigned type, unsigned long count, 
                       const void *pvalue, cacNotify & notify )
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->write (type, count, pvalue, notify);
    }
    else {
        return ECA_DISCONNCHID;
    }
}

int cacChannel::subscribe ( unsigned type, unsigned long count, 
                           unsigned mask, cacNotify &notify )
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->subscribe ( type, count, mask, notify );
    }
    else {
        return ECA_DISCONNCHID;
    }
}

void cacChannel::hostName ( char *pBuf, unsigned bufLength ) const
{
    if ( bufLength ) {
        cacChannelIO *pIO = this->pChannelIO;
        if ( pIO ) {
            pIO->hostName (pBuf, bufLength);
        }
        else {
            strncpy ( pBuf, "<not connected>", bufLength );
            pBuf[bufLength-1u] = '\0';
        }
    }
}

const char * cacChannel::pHostName () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->pHostName ();
    }
    else {
        return "<not connected>";
    }
}

short cacChannel::nativeType () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->nativeType ();
    }
    else {
        return TYPENOTCONN;
    }
}
 
unsigned long cacChannel::nativeElementCount () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->nativeElementCount ();
    }
    else {
        return 0ul;
    }
}

channel_state cacChannel::state () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->state ();
    }
    else {
        return cs_never_conn;
    }
}

bool cacChannel::readAccess () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        caar ar = pIO->accessRights ();
        return ar.read_access;
    }
    else {
        return false;
    }
}

bool cacChannel::writeAccess () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        caar ar = pChannelIO->accessRights ();
        return ar.write_access;
    }
    else {
        return false;
    }
}

const char *cacChannel::pName () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->pName ();
    }
    else {
        return "<disconnected>";
    }
}

unsigned cacChannel::searchAttempts () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->searchAttempts ();
    }
    else {
        return 0u;
    }
}

double cacChannel::beaconPeriod () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->beaconPeriod ();
    }
    else {
        return - DBL_MAX;
    }
}

bool cacChannel::ca_v42_ok () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->ca_v42_ok ();
    }
    else {
        return false;
    }
}

bool cacChannel::connected () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->connected ();
    }
    else {
        return false;
    }
}

caar cacChannel::accessRights () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->accessRights ();
    }
    else {
        // static here avoids undefined memory read warning from
        // error checking codes
        static caar ar = { false, false };
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
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        return pIO->readSequence ();
    }
    else {
        return 0u;
    }
}

void cacChannel::decrementOutstandingIO ()
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        pIO->decrementOutstandingIO ();
    }
}

void cacChannel::incrementOutstandingIO ()
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        pIO->incrementOutstandingIO ();
    }
}

void cacChannel::lockOutstandingIO () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        pIO->lockOutstandingIO ();
    }
}

void cacChannel::unlockOutstandingIO () const
{
    cacChannelIO *pIO = this->pChannelIO;
    if ( pIO ) {
        pIO->unlockOutstandingIO ();
    }
}
