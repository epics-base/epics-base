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
//  Example EPICS CA directory server
//
//
//  caServer
//  |
//  directoryServer
//
//


//
// ANSI C
//
#include <string.h>
#include <stdio.h>

//
// EPICS
//
#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"
#define caNetAddrSock
#include "casdef.h"
#include "epicsAssert.h"
#include "resourceLib.h"


#ifndef NELEMENTS
#   define NELEMENTS(A) (sizeof(A)/sizeof(A[0]))
#endif

class directoryServer;

//
// pvInfo
//
class pvInfo {
public:
    pvInfo (const char *pNameIn, sockaddr_in addrIn) : 
        addr(addrIn), pNext(pvInfo::pFirst)
    {
        pvInfo::pFirst = this;
        this->pName = new char [strlen(pNameIn)+1u];
        strcpy(this->pName, pNameIn);
    }
    
    const struct sockaddr_in getAddr() const { return this->addr; }
    const char *getName () const { return this->pName; }
    const pvInfo *getNext () const { return this->pNext; }
    static const pvInfo *getFirst () { return pvInfo::pFirst; }
private:
    struct sockaddr_in  addr;
    char * pName;
    const pvInfo * pNext;
    static const pvInfo * pFirst;
};

//
// pvEntry 
//
// o entry in the string hash table for the pvInfo
// o Since there may be aliases then we may end up
// with several of this class all referencing 
// the same pv info class (justification
// for this breaking out into a seperate class
// from pvInfo)
//
class pvEntry
    : public stringId, public tsSLNode<pvEntry> {
public:
    pvEntry (const pvInfo  &infoIn, directoryServer &casIn, 
            const char *pAliasName) : 
        stringId(pAliasName), info(infoIn), cas(casIn) 
    {
        assert(this->stringId::resourceName()!=NULL);
    }

    inline ~pvEntry();

    const pvInfo &getInfo() const { return this->info; }
    
    inline void destroy ();

private:
    const pvInfo &info;
    directoryServer &cas;
};

//
// directoryServer
//
class directoryServer : public caServer {
public:
    directoryServer ( const char * const pvPrefix, unsigned aliasCount );
    ~directoryServer();
    void show ( unsigned level ) const;

    void installAliasName ( const pvInfo &info, const char *pAliasName );
    void removeAliasName ( pvEntry &entry );

private:
    resTable < pvEntry, stringId > stringResTbl;

    pvExistReturn pvExistTest ( const casCtx&, 
        const char *pPVName );
    pvExistReturn pvExistTest ( const casCtx&, 
        const caNetAddr &, const char *pPVName );
};


//
// directoryServer::removeAliasName()
//
inline void directoryServer::removeAliasName ( pvEntry & entry )
{
    pvEntry * pE = this->stringResTbl.remove ( entry );
    assert ( pE == & entry );
}

//
// pvEntry::~pvEntry()
//
inline pvEntry::~pvEntry()
{
    this->cas.removeAliasName(*this);
}

//
// pvEntry::destroy()
//
inline void pvEntry::destroy ()
{
    //
    // always created with new
    //
    delete this;
}
 
