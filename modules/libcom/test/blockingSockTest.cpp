/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdexcept>

#include <string.h>
#include <stdio.h>

#include <osiSock.h>
#include <epicsThread.h>
#include <epicsEvent.h>
#include <epicsUnitTest.h>
#include <testMain.h>

namespace {

struct Socket {
    SOCKET sock;
    Socket() :sock(INVALID_SOCKET) {}
    explicit Socket(SOCKET sock)
        :sock(sock)
    {
        if(sock==INVALID_SOCKET)
            throw std::bad_alloc();
    }
    ~Socket()
    {
        clear();
    }
    void swap(Socket& o) {
        std::swap(sock, o.sock);
    }
    void create(int type) {
        Socket temp(epicsSocketCreate(AF_INET, type, 0));
        swap(temp);
    }
    void clear()
    {
        if(sock!=INVALID_SOCKET) {
            SOCKET temp = sock;
            sock = INVALID_SOCKET;
            testDiag("close() socket");
            epicsSocketDestroy(temp);
        }
    }
    void bind_and_listen()
    {
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(0);

        int ret = bind(sock, (sockaddr*)&addr, sizeof(addr));
        if(ret)
            testAbort("bind error %d", int(SOCKERRNO));

        ret = listen(sock, 1);
        if(ret)
            testAbort("listen error %d", int(SOCKERRNO));
    }
    void getname(sockaddr_in& addr) const {
        socklen_t alen = sizeof(addr);
        if(getsockname(sock, (sockaddr*)&addr, &alen))
            testAbort("getsockname errno=%d", int(SOCKERRNO));
    }
    void connect(const sockaddr_in& addr) {
        if(::connect(sock, (sockaddr*)&addr, sizeof(addr)))
            testAbort("connect errno=%d", int(SOCKERRNO));
    }

private:
    Socket(const Socket&);
    Socket& operator=(const Socket&);
};

struct CallRecvFrom : public epicsThreadRunable
{
    Socket sock;
    epicsEvent ready;

    virtual void run() {
        char buf = 0;
        testDiag("Worker Enter recvfrom()");
        ready.signal();
        long ret = ::recvfrom(sock.sock, &buf, 1, 0, 0, 0);
        testDiag("Worker Exit recvfrom() -> %ld (errno=%d)", ret, int(SOCKERRNO));
    }
};

void test_udp_recvfrom()
{
    testDiag("=== test_udp_recvfrom()");

    CallRecvFrom rx;
    rx.sock.create(SOCK_DGRAM);

    // delibrately unsafe storage to avoid trying to join stuck worker on error
    epicsThread* worker = new epicsThread(rx, "worker",
                       epicsThreadGetStackSize(epicsThreadStackBig),
                       epicsThreadPriorityMedium);

    worker->start();
    if(!rx.ready.wait(5.0))
        testAbort("never ready");

    // wait a bit longer to increase the chance of actually entering recvfrom()
    epicsThreadSleep(1.0);

    testDiag("shutdown()");
    int ret = ::shutdown(rx.sock.sock, SHUT_RDWR);
    testDiag("shutdown() -> %d (errno=%d)", ret, int(SOCKERRNO));

    bool interrupted = worker->exitWait(2.0);
#if defined(__linux__) || defined(__rtems__)
    testOk(!!interrupted, "shutdown() interrupted");
#else
    if(!interrupted) {
        testSkip(1, "shutdown() does NOT interrupt");
    } else {
        testSkip(1, "shutdown() Interrupted successfully");
    }
#endif

    rx.sock.clear();
    bool done = worker->exitWait(2.0);
    testOk(!!done, "worker join after close()");
    if(done)
        delete worker;
}

struct CallAcceptFrom : public epicsThreadRunable
{
    Socket sock;
    epicsEvent ready;

    virtual void run() {
        testDiag("Worker Enter accept()");
        ready.signal();
        long ret = ::accept(sock.sock, 0, 0);
        testDiag("Worker Exit accept() -> %ld (errno=%d)", ret, int(SOCKERRNO));
    }
};

void test_tcp_accept()
{
    testDiag("=== test_tcp_accept()");

    CallAcceptFrom rx;
    rx.sock.create(SOCK_STREAM);
    rx.sock.bind_and_listen();

    epicsThread* worker = new epicsThread(rx, "worker",
                       epicsThreadGetStackSize(epicsThreadStackBig),
                       epicsThreadPriorityMedium);
    worker->start();
    if(!rx.ready.wait(5.0))
        testAbort("never ready");

    // wait a bit longer to increase the chance of actually entering recvfrom()
    epicsThreadSleep(1.0);

    testDiag("shutdown()");
    int ret = ::shutdown(rx.sock.sock, SHUT_RDWR);
    testDiag("shutdown() -> %d (errno=%d)", ret, int(SOCKERRNO));

    bool interrupted = worker->exitWait(2.0);
#if defined(__linux__) || defined(__rtems__)
    testOk(!!interrupted, "shutdown() interrupted");
#else
    if(!interrupted) {
        testSkip(1, "shutdown() does NOT interrupt");
    } else {
        testSkip(1, "shutdown() Interrupted successfully");
    }
#endif

    rx.sock.clear();
    bool done = worker->exitWait(2.0);
    testOk(!!done, "worker join after close()");
    if(done)
        delete worker;
}

struct CallRecv : public epicsThreadRunable
{
    Socket sock;
    Socket server;
    epicsEvent ready;
    sockaddr_in addr;

    virtual void run() {
        char buf = 0;
        socklen_t alen = sizeof(addr);
        testDiag("Worker accept()");
        long ret = epicsSocketAccept(sock.sock, (sockaddr*)&addr, &alen);
        testOk(ret!=-1, "accept() -> errno=%d", int(SOCKERRNO));
        if(ret>=0) {
            Socket(ret).swap(server);
        }
        ready.signal();
        if(ret>=0) {
            testDiag("Worker Enter recv()");
            ret = ::recv(server.sock, &buf, 1, 0);
            testDiag("Worker Exit recv() -> %ld (errno=%d)", ret, int(SOCKERRNO));
        }
    }
};

void test_tcp_recv()
{
    testDiag("=== test_tcp_recv()");

    CallRecv rx;
    rx.sock.create(SOCK_STREAM);
    rx.sock.bind_and_listen();

    Socket client;
    client.create(SOCK_STREAM);

    epicsThread* worker = new epicsThread(rx, "worker",
                       epicsThreadGetStackSize(epicsThreadStackBig),
                       epicsThreadPriorityMedium);
    worker->start();

    {
        sockaddr_in addr;
        rx.sock.getname(addr);
        testDiag("client connecting");
        client.connect(addr);
        testDiag("client connected");
    }

    if(!rx.ready.wait(5.0))
        testAbort("never ready");

    // wait a bit longer to increase the chance of actually entering recvfrom()
    epicsThreadSleep(1.0);

    testDiag("shutdown()");
    int ret = ::shutdown(rx.server.sock, SHUT_RDWR);
    testDiag("shutdown() -> %d (errno=%d)", ret, int(SOCKERRNO));

    bool interrupted = worker->exitWait(2.0);
#if !defined(_WIN32)
    testOk(!!interrupted, "shutdown() interrupted");
#else
    if(!interrupted) {
        testSkip(1, "shutdown() does NOT interrupt");
    } else {
        testSkip(1, "shutdown() Interrupted successfully");
    }
#endif

    rx.server.clear();
    bool done = worker->exitWait(2.0);
    testOk(!!done, "worker join after close()");
    if(done)
        delete worker;
}

} // namespace

MAIN(blockingSockTest)
{
    testPlan(7);
    osiSockAttach();

    test_tcp_recv();
    test_tcp_accept();
    test_udp_recvfrom();

    osiSockRelease();
    return testDone();
}

