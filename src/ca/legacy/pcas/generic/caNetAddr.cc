
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

//
//      Author: Jeffrey O. Hill
//              johill@lanl.gov
//

#include <string>
#include <stdexcept>
#include <stdio.h>
#include <string.h>

#define epicsExportSharedSymbols
#define caNetAddrSock
#include "caNetAddr.h"

static class caNetAddrSelfTest {
public:
    caNetAddrSelfTest (); 
} caNetAddrSelfTestDuringLoad;

//
// caNetAddr::stringConvert ()
//
void caNetAddr::stringConvert ( char *pString, unsigned stringLength ) const
{
    if ( this->type == casnaInet ) {
        ipAddrToA (&this->addr.ip, pString, stringLength);
        return;
    }
    if ( stringLength ) {
        strncpy ( pString, "<Undefined Address>", stringLength );
        pString[stringLength-1] = '\n';
    }
}

// 
// caNetAddr::clear()
//
void caNetAddr::clear ()
{
    this->type = casnaUDF;
}

//
// caNetAddr::caNetAddr()
//
caNetAddr::caNetAddr ()
{   
    this->clear();
}

bool caNetAddr::isInet () const
{
    return this->type == casnaInet;
}

bool caNetAddr::isValid () const
{
    return this->type != casnaUDF;
}

bool caNetAddr::operator == (const caNetAddr &rhs) const
{
    if ( this->type != rhs.type ) {
        return false;
    }
    if ( this->type == casnaInet ) {
        return ( this->addr.ip.sin_addr.s_addr == rhs.addr.ip.sin_addr.s_addr ) && 
            ( this->addr.ip.sin_port == rhs.addr.ip.sin_port );
    }
    else {
        return false;
    }
}

bool caNetAddr::operator != ( const caNetAddr & rhs ) const
{
    return ! this->operator == (rhs);
}

//
// This is specified so that compilers will not use one of 
// the following assignment operators after converting to a 
// sockaddr_in or a sockaddr first.
//
// caNetAddr caNetAddr::operator =(const struct sockaddr_in&)
// caNetAddr caNetAddr::operator =(const struct sockaddr&)
//
caNetAddr caNetAddr::operator = ( const caNetAddr &naIn )
{
    this->addr = naIn.addr;
    this->type = naIn.type;
    return *this;
}   

void caNetAddr::setSockIP ( unsigned long inaIn, unsigned short portIn )
{
    this->type = casnaInet;
    this->addr.ip.sin_family = AF_INET;
    this->addr.ip.sin_addr.s_addr = inaIn;
    this->addr.ip.sin_port = portIn;
}   

void caNetAddr::setSockIP ( const struct sockaddr_in & sockIPIn )
{
    if ( sockIPIn.sin_family != AF_INET ) {
        throw std::logic_error ( "caNetAddr::setSockIP (): address wasnt IP" );
    }
    this->type = casnaInet;
    this->addr.ip = sockIPIn;
}   

void caNetAddr::setSock ( const struct sockaddr & sock )
{
    if ( sock.sa_family != AF_INET ) {
        throw std::logic_error ( "caNetAddr::setSock (): address wasnt IP" );
    }
    this->type = casnaInet;
    const struct sockaddr_in *psip = 
        reinterpret_cast <const struct sockaddr_in*> ( & sock );
    this->addr.ip = *psip;
}   

caNetAddr::caNetAddr ( const struct sockaddr_in & sockIPIn )
{
    this->setSockIP ( sockIPIn );
}

caNetAddr caNetAddr::operator = ( const struct sockaddr_in & sockIPIn )
{
    this->setSockIP ( sockIPIn );
    return *this;
}           

caNetAddr caNetAddr::operator = ( const struct sockaddr & sockIn )
{
    this->setSock (sockIn);
    return *this;
}       

struct sockaddr_in caNetAddr::getSockIP() const
{
    if ( this->type != casnaInet ) {
        throw std::logic_error ( "caNetAddr::getSockIP (): address wasnt IP" );
    }
    return this->addr.ip;
}

struct sockaddr caNetAddr::getSock() const
{
    if ( this->type != casnaInet ) {
        throw std::logic_error ( "caNetAddr::getSock (): address wasnt IP" );
    }

    osiSockAddr addr;
    addr.ia = this->addr.ip;
    return addr.sa;
}

caNetAddr::operator sockaddr_in () const
{
    return this->getSockIP ();
}

caNetAddr::operator sockaddr () const
{
    return this->getSock ();
}

void caNetAddr::selfTest ()
{
    // the dummy field must be greater than or equal to the size of
    // each of the other entries in the union
    if ( sizeof ( this->addr ) != sizeof ( this->addr.pad ) ) {
        fprintf ( stderr, "caNetAddr::selfTest ():self test failed in %s at line %d\n", 
            __FILE__, __LINE__ );
        throw std::logic_error ( "caNetAddr::selfTest (): failed self test" );
    }
}

caNetAddrSelfTest::caNetAddrSelfTest ()
{
    caNetAddr tmp;
    tmp.selfTest ();
}


