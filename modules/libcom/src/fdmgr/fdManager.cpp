/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
//
//      File descriptor management C++ class library
//      (for multiplexing IO in a single threaded environment)
//
//      Author  Jeffrey O. Hill
//              johill@lanl.gov
//              505 665 1831
//
// NOTES:
// 1) This library is not thread safe
//

#define instantiateRecourceLib
#include "epicsAssert.h"
#include "epicsThread.h"
#include "fdManager.h"
#include "locationException.h"

#ifdef FDMGR_USE_POLL
#ifdef _WIN32
#define poll WSAPoll
#endif

static const short PollEvents[] = { // must match fdRegType
    POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR,
    POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR,
    POLLPRI};
#endif

#ifdef FDMGR_USE_SELECT
#include <algorithm>
using std :: max;
#endif

fdManager fileDescriptorManager;

static const unsigned mSecPerSec = 1000u;
static const unsigned uSecPerSec = 1000u * mSecPerSec;

//
// fdManager::fdManager()
//
// hopefully its a reasonable guess that poll()/select() and epicsThreadSleep()
// will have the same sleep quantum
//
LIBCOM_API fdManager::fdManager () :
    sleepQuantum ( epicsThreadSleepQuantum () ),
    pTimerQueue ( 0 ), processInProg ( false ),
#ifdef FDMGR_USE_SELECT
    fdSetsPtr ( new fd_set [fdrNEnums] ), maxFD ( 0 ),
#endif
    pCBReg ( 0 )
{
    int status = osiSockAttach ();
    assert (status);

#ifdef FDMGR_USE_SELECT
    for ( size_t i = 0u; i < fdrNEnums; i++ ) {
        FD_ZERO ( &fdSetsPtr[i] );
    }
#endif
}

//
// fdManager::~fdManager()
//
LIBCOM_API fdManager::~fdManager()
{
    fdReg   *pReg;

    while ( (pReg = this->regList.get()) ) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    while ( (pReg = this->activeList.get()) ) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    delete this->pTimerQueue;
#ifdef FDMGR_USE_SELECT
    delete [] this->fdSetsPtr;
#endif
    osiSockRelease();
}

//
// fdManager::process()
//
LIBCOM_API void fdManager::process (double delay)
{
    this->lazyInitTimerQueue ();

    //
    // no recursion
    //
    if (this->processInProg) {
        return;
    }
    this->processInProg = true;

    //
    // One shot at expired timers prior to going into
    // poll/select. This allows zero delay timers to arm
    // fd writes. We will never process the timer queue
    // more than once here so that fd activity get serviced
    // in a reasonable length of time.
    //
    double minDelay = this->pTimerQueue->process(epicsTime::getCurrent());

    if ( minDelay >= delay ) {
        minDelay = delay;
    }

#ifdef FDMGR_USE_POLL
    pollfds.clear();
#endif

    int ioPending = 0;
    tsDLIter < fdReg > iter = this->regList.firstIter ();
    while ( iter.valid () ) {
        ++ioPending;

#ifdef FDMGR_USE_POLL
#if __cplusplus >= 201100L
        pollfds.emplace_back(pollfd{.fd = iter->getFD(), .events = PollEvents[iter->getType()]});
#else
        struct pollfd pollfd;
        pollfd.fd = iter->getFD();
        pollfd.events = PollEvents[iter->getType()];
        pollfd.revents = 0;
        pollfds.push_back(pollfd);
#endif
#endif

#ifdef FDMGR_USE_SELECT
        FD_SET(iter->getFD(), &this->fdSetsPtr[iter->getType()]);
#endif
        ++iter;
    }

    if ( ioPending ) {
#ifdef FDMGR_USE_POLL
        if (minDelay * mSecPerSec > INT_MAX)
            minDelay = INT_MAX / mSecPerSec;

        int status = poll(&pollfds.front(), // ancient C++ has no vector.data()
                    ioPending, static_cast<int>(minDelay * mSecPerSec));
        int i = 0;
#endif

#ifdef FDMGR_USE_SELECT
        struct timeval tv;
        tv.tv_sec = static_cast<time_t> ( minDelay );
        tv.tv_usec = static_cast<long> ( (minDelay-tv.tv_sec) * uSecPerSec );

        fd_set * pReadSet = & this->fdSetsPtr[fdrRead];
        fd_set * pWriteSet = & this->fdSetsPtr[fdrWrite];
        fd_set * pExceptSet = & this->fdSetsPtr[fdrException];
        int status = select (this->maxFD, pReadSet, pWriteSet, pExceptSet, &tv);
#endif

        this->pTimerQueue->process(epicsTime::getCurrent());

        if ( status > 0 ) {

            //
            // Look for activity
            //
            iter=this->regList.firstIter ();
            while ( iter.valid () && status > 0 ) {
                tsDLIter < fdReg > tmp = iter;
                tmp++;

#ifdef FDMGR_USE_POLL
                // In a single threaded application, nothing should have
                // changed the order of regList and pollfds by now.
                // But just in case...
                int isave = i;
                while (pollfds[i].fd != iter->getFD() || pollfds[i].events != PollEvents[iter->getType()])
                {
                    i++; // skip pollfd of removed items
                    if (i >= ioPending) { // skip unknown (inserted?) items
                        iter = tmp;
                        tmp++;
                        if (!iter.valid()) break;
                        i = isave;
                    }
                }
                if (i >= ioPending) break; // any unhandled item stays in regList for next time

                if (pollfds[i++].revents & PollEvents[iter->getType()]) {
#endif

#ifdef FDMGR_USE_SELECT
                if (FD_ISSET(iter->getFD(), &this->fdSetsPtr[iter->getType()])) {
                    FD_CLR(iter->getFD(), &this->fdSetsPtr[iter->getType()]);
#endif
                    this->regList.remove(*iter);
                    this->activeList.add(*iter);
                    iter->state = fdReg::active;
                    status--;
                }
                iter = tmp;
            }

            //
            // I am careful to prevent problems if they access the
            // above list while in a "callBack()" routine
            //
            fdReg * pReg;
            while ( (pReg = this->activeList.get()) ) {
                pReg->state = fdReg::limbo;

                //
                // Tag current fdReg so that we
                // can detect if it was deleted
                // during the call back
                //
                this->pCBReg = pReg;
                pReg->callBack();
                if (this->pCBReg != NULL) {
                    //
                    // check only after we see that it is non-null so
                    // that we don't trigger bounds-checker dangling pointer
                    // error
                    //
                    assert (this->pCBReg==pReg);
                    this->pCBReg = 0;
                    if (pReg->onceOnly) {
                        pReg->destroy();
                    }
                    else {
                        this->regList.add(*pReg);
                        pReg->state = fdReg::pending;
                    }
                }
            }
        }
        else if ( status < 0 ) {
            int errnoCpy = SOCKERRNO;

#ifdef FDMGR_USE_SELECT
            // don't depend on flags being properly set if
            // an error is returned from select
            for ( size_t i = 0u; i < fdrNEnums; i++ ) {
                FD_ZERO ( &fdSetsPtr[i] );
            }
#endif

            //
            // print a message if it's an unexpected error
            //
            if ( errnoCpy != SOCK_EINTR ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString (
                    sockErrBuf, sizeof ( sockErrBuf ) );
                errlogPrintf("fdManager: "
#ifdef FDMGR_USE_POLL
                    "poll()"
#endif
#ifdef FDMGR_USE_SELECT
                    "select()"
#endif
                    " failed because \"%s\"\n",
                    sockErrBuf);
            }
        }
    }
    else {
        /*
         * recover from subtle differences between
         * windows sockets and UNIX sockets implementation
         * of select()
         */
        epicsThreadSleep(minDelay);
        this->pTimerQueue->process(epicsTime::getCurrent());
    }
    this->processInProg = false;
}

//
// fdReg::destroy()
// (default destroy method)
//
void fdReg::destroy()
{
    delete this;
}

//
// fdReg::~fdReg()
//
fdReg::~fdReg()
{
    this->manager.removeReg(*this);
}

//
// fdReg::show()
//
void fdReg::show(unsigned level) const
{
    printf ("fdReg at %p\n", (void *) this);
    if (level>1u) {
        printf ("\tstate = %d, onceOnly = %d\n",
            this->state, this->onceOnly);
    }
    this->fdRegId::show(level);
}

//
// fdRegId::show()
//
void fdRegId::show ( unsigned level ) const
{
    printf ( "fdRegId at %p\n",
        static_cast <const void *> ( this ) );
    if ( level > 1u ) {
        printf ( "\tfd = %d, type = %d\n",
            int(this->fd), this->type );
    }
}

//
// fdManager::installReg ()
//
void fdManager::installReg (fdReg &reg)
{
#ifdef FDMGR_USE_SELECT
    this->maxFD = max ( this->maxFD, reg.getFD()+1 );
#endif
    // Most applications will find that it's important to push here to
    // the front of the list so that transient writes get executed
    // first allowing incoming read protocol to find that outgoing
    // buffer space is newly available.
    this->regList.push ( reg );
    reg.state = fdReg::pending;

    int status = this->fdTbl.add ( reg );
    if ( status != 0 ) {
        throwWithLocation ( fdInterestSubscriptionAlreadyExits () );
    }
}

//
// fdManager::removeReg ()
//
void fdManager::removeReg (fdReg &regIn)
{
    fdReg *pItemFound;

    pItemFound = this->fdTbl.remove (regIn);
    if (pItemFound!=&regIn) {
        errlogPrintf("fdManager::removeReg() bad fd registration object\n");
        return;
    }

    //
    // signal fdManager that the fdReg was deleted
    // during the call back
    //
    if (this->pCBReg == &regIn) {
        this->pCBReg = 0;
    }

    switch (regIn.state) {
    case fdReg::active:
        this->activeList.remove (regIn);
        break;
    case fdReg::pending:
        this->regList.remove (regIn);
        break;
    case fdReg::limbo:
        break;
    default:
        //
        // here if memory corrupted
        //
        assert(0);
    }
    regIn.state = fdReg::limbo;

#ifdef FDMGR_USE_SELECT
    FD_CLR(regIn.getFD(), &this->fdSetsPtr[regIn.getType()]);
#endif

}

//
// fdManager::reschedule ()
// NOOP - this only runs single threaded, and therefore they can only
// add a new timer from places that will always end up in a reschedule
//
void fdManager::reschedule ()
{
}

double fdManager::quantum ()
{
    return this->sleepQuantum;
}

//
// lookUpFD()
//
LIBCOM_API fdReg *fdManager::lookUpFD (const SOCKET fd, const fdRegType type)
{
    if (fd<0) {
        return NULL;
    }
    fdRegId id (fd,type);
    return this->fdTbl.lookup(id);
}

//
// fdReg::fdReg()
//
fdReg::fdReg (const SOCKET fdIn, const fdRegType typIn,
        const bool onceOnlyIn, fdManager &managerIn) :
    fdRegId (fdIn,typIn), state (limbo),
    onceOnly (onceOnlyIn), manager (managerIn)
{
#ifdef FDMGR_USE_SELECT
    if (!FD_IN_FDSET(fdIn)) {
        errlogPrintf("%s: fd > FD_SETSIZE ignored\n",
            __FILE__);
        return;
    }
#endif
    this->manager.installReg (*this);
}

template class resTable<fdReg, fdRegId>;
