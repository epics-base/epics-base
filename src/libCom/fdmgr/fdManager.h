/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      File descriptor management C++ class library
 *      (for multiplexing IO in a single threaded environment)
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef fdManagerH_included
#define fdManagerH_included

#include "shareLib.h" // reset share lib defines
#include "tsDLList.h"
#include "resourceLib.h"
#include "epicsTime.h"
#include "osiSock.h"
#include "epicsTimer.h"

enum fdRegType {fdrRead, fdrWrite, fdrException, fdrNEnums};

//
// fdRegId
//
// file descriptor interest id
//
class epicsShareClass fdRegId
{
public:

    fdRegId (const SOCKET fdIn, const fdRegType typeIn) :
        fd(fdIn), type(typeIn) {}

    SOCKET getFD () const
    {
        return this->fd;
    }

    fdRegType getType () const
    {
        return this->type;
    }

    bool operator == (const fdRegId &idIn) const
    {
        return this->fd == idIn.fd && this->type==idIn.type;
    }

    resTableIndex hash () const;

    virtual void show (unsigned level) const;

    virtual ~fdRegId() {}
private:
    SOCKET fd;
    fdRegType type;
};

//
// fdManager
//
// file descriptor manager
//
class fdManager : public epicsTimerQueueNotify {
public:
    //
    // exceptions
    //
    class fdInterestSubscriptionAlreadyExits {};

    epicsShareFunc fdManager ();
    epicsShareFunc virtual ~fdManager ();
    epicsShareFunc void process ( double delay ); // delay parameter is in seconds

    // returns NULL if the fd is unknown
    epicsShareFunc class fdReg *lookUpFD (const SOCKET fd, const fdRegType type);

    epicsTimer & createTimer ();

private:
    tsDLList < fdReg > regList;
    tsDLList < fdReg > activeList;
    resTable < fdReg, fdRegId > fdTbl;
    const double sleepQuantum;
    fd_set * fdSetsPtr;
    epicsTimerQueuePassive * pTimerQueue;
    SOCKET maxFD;
    bool processInProg;
    //
    // Set to fdreg when in call back
    // and nill otherwise
    //
    fdReg * pCBReg; 
    void reschedule ();
    double quantum ();
    void installReg (fdReg &reg);
    void removeReg (fdReg &reg);
    void lazyInitTimerQueue ();
    fdManager ( const fdManager & );
    fdManager & operator = ( const fdManager & );
    friend class fdReg;
};

//
// default file descriptor manager
//
epicsShareExtern fdManager fileDescriptorManager;

//
// fdReg
//
// file descriptor registration
//
class epicsShareClass fdReg :
    public fdRegId, public tsDLNode<fdReg>, public tsSLNode<fdReg> {
    friend class fdManager;

public:

    fdReg (const SOCKET fdIn, const fdRegType type, 
        const bool onceOnly=false, fdManager &manager = fileDescriptorManager);
    virtual ~fdReg ();

    virtual void show (unsigned level) const;
    
    //
    // Called by the file descriptor manager:
    // 1) If the fdManager is deleted and there are still 
    // fdReg objects attached
    // 2) Immediately after calling "callBack()" if
    // the constructor specified "onceOnly" 
    //
    // fdReg::destroy() does a "delete this"
    //
    virtual void destroy ();

private:
    enum state {active, pending, limbo};

    //
    // called when there is activity on the fd
    // NOTES
    // 1) the fdManager will call this only once during the
    // lifetime of a fdReg object if the constructor
    // specified "onceOnly"
    //
    virtual void callBack ()=0;

    unsigned char state; // state enums go here
    unsigned char onceOnly;
    fdManager &manager;

    fdReg ( const fdReg & );
    fdReg & operator = ( const fdReg & );
};

//
// fdRegId::hash()
//
inline resTableIndex fdRegId::hash () const
{
    const unsigned fdManagerHashTableMinIndexBits = 8;
    const unsigned fdManagerHashTableMaxIndexBits = sizeof(SOCKET)*CHAR_BIT;
    resTableIndex hashid;
        
    hashid = integerHash ( fdManagerHashTableMinIndexBits, 
        fdManagerHashTableMaxIndexBits, this->fd );
 
    //
    // also evenly distribute based on the type of fdRegType
    //
    hashid ^= this->type;

    //
    // the result here is always masked to the
    // proper size after it is returned to the resource class
    //
    return hashid;
}

inline void fdManager::lazyInitTimerQueue () 
{
    if ( ! this->pTimerQueue ) {
        this->pTimerQueue = & epicsTimerQueuePassive::create ( *this );
    }
}

inline epicsTimer & fdManager::createTimer () 
{
    this->lazyInitTimerQueue ();
    return this->pTimerQueue->createTimer ();
}

#endif // fdManagerH_included
 
