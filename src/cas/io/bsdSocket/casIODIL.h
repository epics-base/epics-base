/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef casIODILh
#define casIODILh

//
// casDGIntfIO::getBCastFD()
//
inline int casDGIntfIO::getBCastFD() const
{
	return this->bcastRecvSock;
}

//
// casDGIntfIO::getBCastFD()
//
inline bool casDGIntfIO::validBCastFD() const
{
	return this->bcastRecvSock != INVALID_SOCKET;
}

inline void * ipIgnoreEntry::operator new ( size_t size )
{
    return ipIgnoreEntry::pFreeList->allocate ( size );
}

inline void ipIgnoreEntry::operator delete ( void * pCadaver, size_t size )
{
    ipIgnoreEntry::pFreeList->release ( pCadaver, size );
}

inline ipIgnoreEntry::ipIgnoreEntry ( unsigned ipAddrIn ) :
    ipAddr ( ipAddrIn )
{
}

inline void ipIgnoreEntry::destroy ()
{
    delete this;
}

#endif // casIODILh

