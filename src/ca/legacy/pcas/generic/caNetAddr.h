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
// caNetAddr
//
// special cas specific network address class so:
// 1) we dont drag BSD socket headers into the server tool
// 2) we are not prevented from using other networking services
//      besides sockets in the future
//

#ifndef caNetAddrH
#define caNetAddrH

#ifdef caNetAddrSock
#   ifdef epicsExportSharedSymbols
#       define epicsExportSharedSymbols_caNetAddrH
#       undef epicsExportSharedSymbols
#   endif
#   include "osiSock.h"
#   ifdef epicsExportSharedSymbols_caNetAddrH
#       define epicsExportSharedSymbols
#       include "shareLib.h"
#   endif
#else
#   include "shareLib.h"
#endif

class epicsShareClass caNetAddr {
public:
    void clear ();
    caNetAddr ();
    bool isValid () const;
    bool operator == ( const caNetAddr & rhs ) const;
    bool operator != ( const caNetAddr & rhs) const;
    caNetAddr operator = ( const caNetAddr & naIn );
    void stringConvert ( char *pString, unsigned stringLength ) const;
    void selfTest ();

    // support for socket addresses
    // (other address formats may be supported in the future)
    bool isInet () const;
    void setSockIP ( unsigned long inaIn, unsigned short portIn );
    void setSockIP ( const struct sockaddr_in & sockIPIn );
    void setSock ( const struct sockaddr & sock );
    caNetAddr ( const struct sockaddr_in & sockIPIn );
    caNetAddr operator = ( const struct sockaddr & sockIn );
    caNetAddr operator = ( const struct sockaddr_in & sockIPIn );
    struct sockaddr getSock() const;
    struct sockaddr_in getSockIP() const;
    operator struct sockaddr () const;
    operator struct sockaddr_in () const;

private:
    enum caNetAddrType { casnaUDF, casnaInet } type;
    union { 
        unsigned char pad[16];
#       ifdef caNetAddrSock     
            struct sockaddr_in ip;
#       endif
    } addr;
};

#endif // caNetAddrH
