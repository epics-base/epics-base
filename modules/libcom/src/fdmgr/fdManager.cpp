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

#if !defined(FDMGR_USE_POLL) && !defined(FDMGR_USE_SELECT)
#if defined(__linux__) || defined(darwin) || _WIN32_WINNT >= 0x600 || (defined(__rtems__) && !defined(RTEMS_LEGACY_STACK))
#define FDMGR_USE_POLL
#else
#define FDMGR_USE_SELECT
#endif
#endif

#ifdef FDMGR_USE_POLL
#include <vector>
#if !defined(_WIN32)
#include <poll.h>
#endif

static const short PollEvents[] = { // must match fdRegType
    POLLRDNORM | POLLRDBAND | POLLIN | POLLHUP | POLLERR,
    POLLWRBAND | POLLWRNORM | POLLOUT | POLLERR,
    POLLPRI };

#if defined(_WIN32)
#define poll WSAPoll
// Filter out PollEvents that Windows does not accept in events (only returns in revents)
#define WIN_POLLEVENT_FILTER(ev) static_cast<short>((ev) & (POLLIN | POLLOUT))
#else
// Linux, MacOS and RTEMS don't care
#define WIN_POLLEVENT_FILTER(ev) (ev)
#endif
#endif

#ifdef FDMGR_USE_SELECT
#include <algorithm>
#endif

struct fdManagerPrivate {
    tsDLList<fdReg> regList;
    tsDLList<fdReg> activeList;
    resTable<fdReg, fdRegId> fdTbl;
    const double sleepQuantum;
    epics::auto_ptr<epicsTimerQueuePassive> pTimerQueue;
    bool processInProg;

#ifdef FDMGR_USE_POLL
    std::vector<struct pollfd> pollfds;
#endif

#ifdef FDMGR_USE_SELECT
    fd_set fdSets[fdrNEnums];
    SOCKET maxFD;
#endif

    //
    // Set to fdreg when in call back
    // and nill otherwise
    //
    volatile fdReg* pCBReg;
    fdManager& owner;

    explicit fdManagerPrivate(fdManager& owner);
    void lazyInitTimerQueue();
};

fdManagerPrivate::fdManagerPrivate(fdManager& owner) :
    sleepQuantum(epicsThreadSleepQuantum()),
    processInProg(false),
    pCBReg(NULL), owner(owner)
{}

inline void fdManagerPrivate::lazyInitTimerQueue()
{
    if (!pTimerQueue.get()) {
        pTimerQueue.reset(&epicsTimerQueuePassive::create(owner));
    }
}

epicsTimer& fdManager::createTimer()
{
    priv->lazyInitTimerQueue();
    return priv->pTimerQueue->createTimer();
}

fdManager fileDescriptorManager;

static const unsigned mSecPerSec = 1000u;
#ifdef FDMGR_USE_SELECT
static const unsigned uSecPerSec = 1000u * mSecPerSec;
#endif

//
// fdManager::fdManager()
//
// hopefully its a reasonable guess that poll()/select() and epicsThreadSleep()
// will have the same sleep quantum
//
LIBCOM_API fdManager::fdManager() :
    priv(new fdManagerPrivate(*this))
{
    int status = osiSockAttach();
    assert(status);

#ifdef FDMGR_USE_SELECT
    priv->maxFD = 0;
    for (size_t i = 0u; i < fdrNEnums; i++) {
        FD_ZERO(&priv->fdSets[i]);
    }
#endif
}

//
// fdManager::~fdManager()
//
LIBCOM_API fdManager::~fdManager()
{
    fdReg* pReg;

    while ((pReg = priv->regList.get())) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    while ((pReg = priv->activeList.get())) {
        pReg->state = fdReg::limbo;
        pReg->destroy();
    }
    osiSockRelease();
}

//
// fdManager::process()
//
LIBCOM_API void fdManager::process(double delay)
{
    priv->lazyInitTimerQueue();

    //
    // no recursion
    //
    if (priv->processInProg)
        return;
    priv->processInProg = true;

    //
    // One shot at expired timers prior to going into
    // poll/select. This allows zero delay timers to arm
    // fd writes. We will never process the timer queue
    // more than once here so that fd activity get serviced
    // in a reasonable length of time.
    //
    double minDelay = priv->pTimerQueue->process(epicsTime::getCurrent());

    if (minDelay >= delay) {
        minDelay = delay;
    }

#ifdef FDMGR_USE_POLL
    priv->pollfds.clear();
#endif

    int ioPending = 0;
    tsDLIter<fdReg> iter = priv->regList.firstIter();
    while (iter.valid()) {
        ++ioPending;

#ifdef FDMGR_USE_POLL
#if __cplusplus >= 201100L
        priv->pollfds.emplace_back(pollfd{
            .fd = iter->getFD(),
            .events = WIN_POLLEVENT_FILTER(PollEvents[iter->getType()])
        });
#else
        struct pollfd pollfd;
        pollfd.fd = iter->getFD();
        pollfd.events = WIN_POLLEVENT_FILTER(PollEvents[iter->getType()]);
        pollfd.revents = 0;
        priv->pollfds.push_back(pollfd);
#endif
#endif

#ifdef FDMGR_USE_SELECT
        FD_SET(iter->getFD(), &priv->fdSets[iter->getType()]);
#endif
        ++iter;
    }

    if (ioPending) {
#ifdef FDMGR_USE_POLL
        if (minDelay * mSecPerSec > INT_MAX)
            minDelay = INT_MAX / mSecPerSec;

        int status = poll(&priv->pollfds[0], // ancient C++ has no vector.data()
                    ioPending, static_cast<int>(minDelay * mSecPerSec));
        int i = 0;
#endif

#ifdef FDMGR_USE_SELECT
        struct timeval tv;
        tv.tv_sec = static_cast<time_t>(minDelay);
        tv.tv_usec = static_cast<long>((minDelay-tv.tv_sec) * uSecPerSec);

        int status = select(priv->maxFD,
                &priv->fdSets[fdrRead],
                &priv->fdSets[fdrWrite],
                &priv->fdSets[fdrException], &tv);
#endif

        priv->pTimerQueue->process(epicsTime::getCurrent());

        if (status > 0) {

            //
            // Look for activity
            //
            iter = priv->regList.firstIter();
            while (iter.valid() && status > 0) {
                tsDLIter<fdReg> tmp = iter;
                tmp++;

#ifdef FDMGR_USE_POLL
                // In a single threaded application, nothing should have
                // changed the order of regList and pollfds by now.
                // But just in case...
                int isave = i;
                while (priv->pollfds[i].fd != iter->getFD() ||
                    priv->pollfds[i].events != WIN_POLLEVENT_FILTER(PollEvents[iter->getType()]))
                {
                    errlogPrintf("fdManager: skipping (removed?) pollfd %d (expected %d)\n", priv->pollfds[i].fd, iter->getFD());
                    i++; // skip pollfd of removed items
                    if (i >= ioPending) { // skip unknown (inserted?) items
                        errlogPrintf("fdManager: skipping (inserted?) item %d\n", iter->getFD());
                        iter = tmp;
                        tmp++;
                        if (!iter.valid()) break;
                        i = isave;
                    }
                }
                if (i >= ioPending) break; // any unhandled item stays in regList for next time

                if (priv->pollfds[i++].revents & PollEvents[iter->getType()]) {
#endif

#ifdef FDMGR_USE_SELECT
                if (FD_ISSET(iter->getFD(), &priv->fdSets[iter->getType()])) {
                    FD_CLR(iter->getFD(), &priv->fdSets[iter->getType()]);
#endif
                    priv->regList.remove(*iter);
                    priv->activeList.add(*iter);
                    iter->state = fdReg::active;
                    status--;
                }
                iter = tmp;
            }

            //
            // I am careful to prevent problems if they access the
            // above list while in a "callBack()" routine
            //
            fdReg* pReg;
            while ((pReg = priv->activeList.get())) {
                pReg->state = fdReg::limbo;

                //
                // Tag current fdReg so that we
                // can detect if it was deleted
                // during the call back
                //
                priv->pCBReg = pReg;
                pReg->callBack();
                if (priv->pCBReg != NULL) {
                    //
                    // check only after we see that it is non-null so
                    // that we don't trigger bounds-checker dangling pointer
                    // error
                    //
                    assert(priv->pCBReg == pReg);
                    priv->pCBReg = NULL;
                    if (pReg->onceOnly) {
                        pReg->destroy();
                    }
                    else {
                        priv->regList.add(*pReg);
                        pReg->state = fdReg::pending;
                    }
                }
            }
        }
        else if (status < 0) {
            int errnoCpy = SOCKERRNO;

#ifdef FDMGR_USE_SELECT
            // don't depend on flags being properly set if
            // an error is returned from select
            for (size_t i = 0u; i < fdrNEnums; i++) {
                FD_ZERO(&priv->fdSets[i]);
            }
#endif

            //
            // print a message if it's an unexpected error
            //
            if (errnoCpy != SOCK_EINTR) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString(
                    sockErrBuf, sizeof(sockErrBuf));
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
        priv->pTimerQueue->process(epicsTime::getCurrent());
    }
    priv->processInProg = false;
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
    manager.removeReg(*this);
}

//
// fdReg::show()
//
void fdReg::show(unsigned level) const
{
    printf("fdReg at %p\n", this);
    if (level > 1u) {
        printf("\tstate = %d, onceOnly = %d\n",
            state, onceOnly);
    }
    fdRegId::show(level);
}

//
// fdRegId::show()
//
void fdRegId::show(unsigned level) const
{
    printf("fdRegId at %p\n", this);
    if (level > 1u) {
        printf("\tfd = %"
#if defined(_WIN32)
            "I"
#endif
        "d, type = %d\n",
            fd, type);
    }
}

//
// fdManager::installReg()
//
void fdManager::installReg(fdReg &reg)
{
#ifdef FDMGR_USE_SELECT
    priv->maxFD = std::max(priv->maxFD, reg.getFD()+1);
#endif
    // Most applications will find that it's important to push here to
    // the front of the list so that transient writes get executed
    // first allowing incoming read protocol to find that outgoing
    // buffer space is newly available.
    priv->regList.push(reg);
    reg.state = fdReg::pending;

    int status = priv->fdTbl.add(reg);
    if (status != 0) {
        throwWithLocation(fdInterestSubscriptionAlreadyExits());
    }
//    errlogPrintf("fdManager::adding fd %d\n", reg.getFD());
}

//
// fdManager::removeReg()
//
void fdManager::removeReg(fdReg &regIn)
{
    fdReg* pItemFound;

    pItemFound = priv->fdTbl.remove(regIn);
    if (pItemFound != &regIn) {
        errlogPrintf("fdManager::removeReg() bad fd registration object\n");
        return;
    }

    //
    // signal fdManager that the fdReg was deleted
    // during the call back
    //
    if (priv->pCBReg == &regIn) {
        priv->pCBReg = NULL;
    }

    switch (regIn.state) {
    case fdReg::active:
        priv->activeList.remove(regIn);
        break;
    case fdReg::pending:
        priv->regList.remove(regIn);
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
    FD_CLR(regIn.getFD(), &priv->fdSets[regIn.getType()]);
#endif

//    errlogPrintf("fdManager::removing fd %d\n", regIn.getFD());
}

//
// fdManager::reschedule()
// NOOP - this only runs single threaded, and therefore they can only
// add a new timer from places that will always end up in a reschedule
//
void fdManager::reschedule()
{
}

double fdManager::quantum()
{
    return priv->sleepQuantum;
}

//
// lookUpFD()
//
LIBCOM_API fdReg* fdManager::lookUpFD(const SOCKET fd, const fdRegType type)
{
    if (fd < 0) {
        return NULL;
    }
    fdRegId id(fd,type);
    return priv->fdTbl.lookup(id);
}

//
// fdReg::fdReg()
//
fdReg::fdReg(const SOCKET fdIn, const fdRegType typIn,
        const bool onceOnlyIn, fdManager &managerIn) :
    fdRegId(fdIn,typIn), state(limbo),
    onceOnly(onceOnlyIn), manager(managerIn)
{
#ifdef FDMGR_USE_SELECT
    if (!FD_IN_FDSET(fdIn)) {
        errlogPrintf("%s: fd > FD_SETSIZE ignored\n",
            __FILE__);
        return;
    }
#endif
    manager.installReg(*this);
}

template class resTable<fdReg, fdRegId>;
