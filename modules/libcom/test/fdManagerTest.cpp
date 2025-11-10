/*************************************************************************\
* Copyright (c) 2025 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <algorithm>

#include <string.h>

#include <osiSock.h>
#include <fdManager.h>
#include <epicsTime.h>
#include <epicsAtomic.h>
#include <epicsThread.h>

#include <epicsUnitTest.h>
#include <testMain.h>

#if __cplusplus<201103L
#  define final
#  define override
#endif

namespace {

void set_non_blocking(SOCKET sd)
{
    osiSockIoctl_t yes = true;
    int status = socket_ioctl ( sd,
                                FIONBIO, & yes);
    if(status)
        testFail("set_non_blocking fails : %d", SOCKERRNO);
}

// RAII for epicsTimer
struct ScopedTimer {
    epicsTimer& timer;
    explicit
    ScopedTimer(epicsTimer& t) :timer(t) {}
    ~ScopedTimer() { timer.destroy(); }
};
struct ScopedFDReg {
    fdReg * const reg;
    explicit
    ScopedFDReg(fdReg* reg) :reg(reg) {}
    ~ScopedFDReg() { reg->destroy(); }
};

// RAII for socket
struct Socket {
    SOCKET sd;
    Socket() :sd(INVALID_SOCKET) {}
    explicit
    Socket(SOCKET sd) :sd(sd) {}
    Socket(int af, int type)
        :sd(epicsSocketCreate(af, type, 0))
    {
        if(sd==INVALID_SOCKET)
            testAbort("failed to allocate socket %d %d", af, type);
    }
private:
    Socket(const Socket&);
    Socket& operator=(const Socket&);
public:
    ~Socket() {
        if(sd!=INVALID_SOCKET)
            epicsSocketDestroy(sd);
    }
    void swap(Socket& o) {
        std::swap(sd, o.sd);
    }
    osiSockAddr bind()
    {
        osiSockAddr addr = {{0}};
        addr.ia.sin_family = AF_INET;
        addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if(::bind(sd, &addr.sa, sizeof(addr)))
            testAbort("Unable to bind lo : %d", SOCKERRNO);

        osiSockAddr ret;
        osiSocklen_t addrlen = sizeof(ret);
        if(getsockname(sd, &ret.sa, &addrlen))
            testAbort("Unable to getsockname : %d", SOCKERRNO);
        (void)addrlen;

        if(listen(sd, 1))
            testAbort("Unable to listen : %d", SOCKERRNO);

        return ret;
    }
};

struct DoConnect final : public epicsThreadRunable {
    const SOCKET sd;
    osiSockAddr to;
    DoConnect(SOCKET sd, const osiSockAddr& to)
        :sd(sd)
        ,to(to)
    {}

    void run() override final {
        int err = connect(sd, &to.sa, sizeof(to));
        testOk(err==0, "connect() %d %d", err, SOCKERRNO);
    }
};

struct DoAccept final : public epicsThreadRunable {
    const SOCKET sd;
    Socket peer;
    osiSockAddr peer_addr;
    explicit
    DoAccept(SOCKET sd) :sd(sd) {}
    void run() override final {
        osiSocklen_t len(sizeof(peer_addr));
        Socket temp(accept(sd, &peer_addr.sa, &len));
        if(temp.sd==INVALID_SOCKET)
            testFail("accept() -> %d", SOCKERRNO);
        temp.swap(peer);
    }
};

struct DoRead final : public epicsThreadRunable {
    const SOCKET sd;
    char* buf;
    unsigned buflen;
    int n;
    DoRead(SOCKET sd, char* buf, unsigned buflen): sd(sd), buf(buf), buflen(buflen), n(0) {}
    void run() override final {
        n = recv(sd, buf, buflen, 0);
        if(n<0)
            testFail("read() -> %d, %d", n, SOCKERRNO);
    }
};

struct DoWriteAll final : public epicsThreadRunable {
    const SOCKET sd;
    const char* buf;
    unsigned buflen;
    DoWriteAll(SOCKET sd, const char* buf, unsigned buflen): sd(sd), buf(buf), buflen(buflen) {}
    void run() override final {
        unsigned nsent = 0;
        while(nsent<buflen) {
            int n = send(sd, nsent+buf, buflen-nsent, 0);
            if(n<0) {
                testFail("send() -> %d, %d", n, SOCKERRNO);
                break;
            }
            nsent += n;
        }
    }
};

struct Expire final : public epicsTimerNotify {
    bool expired;

    Expire() :expired(false) {}
    virtual ~Expire() {}
    virtual expireStatus expire(const epicsTime &) override final
    {
        if(!expired) {
            expired = true;
            testPass("expired");
        } else {
            testFail("re-expired?");
        }
        return noRestart;
    }
};

struct Event final : public fdReg {
    int* ready;

    Event(fdManager& mgr, fdRegType evt, SOCKET sd, int* ready)
        :fdReg(sd, evt, false, mgr)
        ,ready(ready)
    {}
    virtual ~Event() {}

    virtual void callBack() override final {
        epics::atomic::set(*ready, 1);
    }
};

struct OneShot final : public fdReg {
    int *mask;

    OneShot(fdManager& mgr, fdRegType evt, SOCKET sd, int *mask)
        :fdReg(sd, evt, true, mgr)
        ,mask(mask)
    {}
    virtual ~OneShot() {
        epics::atomic::add(*mask, 2);
    }

    virtual void callBack() override final {
        epics::atomic::add(*mask, 1);
    }
};

void testEmpty()
{
    fdManager empty;
    empty.process(0.1); // ca-gateway always passes 0.01
    testPass("Did nothing");
}

void testOnlyTimer()
{
    fdManager mgr;
    Expire trig, never;
    ScopedTimer trig_timer(mgr.createTimer()),
                never_timer(mgr.createTimer());
    epicsTime now(epicsTime::getCurrent());
    trig_timer.timer.start(trig, now+0.1);
    never_timer.timer.start(never, now+9999999.0);
    for(unsigned i=0; i<10 && !trig.expired; i++)
        mgr.process(0.2);
    testOk1(trig.expired);
    testOk1(!never.expired);
}

void testSockIO()
{
    fdManager mgr;
    Socket listener(AF_INET, SOCK_STREAM);
    set_non_blocking(listener.sd);
    osiSockAddr servAddr(listener.bind());

    Socket client(AF_INET, SOCK_STREAM);
    Socket server;
    // listen() / connect()
    {
        int readable = 0;
        Event evt(mgr, fdrRead, listener.sd, &readable);
        DoConnect conn(client.sd, servAddr);
        epicsThread connector(conn, "connect", 0);
        connector.start();

        mgr.process(5.0);

        testOk1(readable);

        DoAccept acc(listener.sd);
        acc.run();
        server.swap(acc.peer);
    }
    set_non_blocking(server.sd);
    // writeable
    {
        int mask = 0;
        new OneShot(mgr, fdrWrite, server.sd, &mask);

        mgr.process(5.0);

        testOk(mask==3, "OneShot event mask %x", mask);
    }
    // read
    {
        const char msg[] = "testing";
        int readable = 0;
        Event evt(mgr, fdrRead, server.sd, &readable);
        DoWriteAll op(client.sd, msg, sizeof(msg)-1);
        epicsThread writer(op, "writer", 0);
        writer.start();

        mgr.process(5.0);

        testOk1(readable);

        char buf[sizeof(msg)] = "";
        DoRead(server.sd, buf, sizeof(buf)-1).run();
        buf[sizeof(buf)-1] = '\0';
        testOk(strcmp(msg, buf)==0, "%s == %s", msg, buf);
    }
    // timer while unreadable
    {

        int readable = 0;
        Event evt(mgr, fdrRead, server.sd, &readable);
        Expire tmo;
        ScopedTimer timer(mgr.createTimer());
        timer.timer.start(tmo, epicsTime::getCurrent()); // immediate

        mgr.process(1.0);

        testOk1(!readable);
        testOk1(tmo.expired);
    }
    // notification on close()
    {
        int readable = 0;
        Event evt(mgr, fdrRead, server.sd, &readable);

        shutdown(client.sd, SHUT_RDWR);
        //Socket().swap(client);

        mgr.process(1.0);

        testOk1(readable);
    }
}

} // namespace

MAIN(fdManagerTest)
{
    testPlan(13);
    osiSockAttach();
    testEmpty();
    testOnlyTimer();
    testSockIO();
    osiSockRelease();
    return testDone();
}
