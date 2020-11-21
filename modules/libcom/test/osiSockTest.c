/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "epicsAssert.h"
#include "dbDefs.h"
#include "osiSock.h"
#include "epicsTime.h"
#include "epicsThread.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* This could easily be generalized to test more options */
static
void udpBroadcast(SOCKET s, int put)
{
    int status;
    int flag = put;
    osiSocklen_t len = sizeof(flag);

    status = setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&flag, len);
    testOk(status >= 0, "setsockopt BROADCAST := %d", put);

    status = getsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&flag, &len);
    testOk(status >= 0 && len == sizeof(flag) && !flag == !put,
        "getsockopt BROADCAST => %d", flag);
}

static
void multiCastLoop(SOCKET s, int put)
{
    int status;
    osiSockOptMcastLoop_t flag = put;
    osiSocklen_t len = sizeof(flag);

    status = setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP,
                   (char *)&flag, len);
    testOk(status >= 0, "setsockopt MULTICAST_LOOP := %d", put);

    status = getsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&flag, &len);
    testOk(status >= 0 && len == sizeof(flag) && !flag == !put,
        "getsockopt MULTICAST_LOOP => %d", (int) flag);
}

static
void multiCastTTL(SOCKET s, int put)
{
    int status;
    osiSockOptMcastTTL_t flag = put;
    osiSocklen_t len = sizeof(flag);

    status = setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL,
                   (char *)&flag, len);
    testOk(status >= 0, "setsockopt IP_MULTICAST_TTL := %d", put);

    status = getsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&flag, &len);
    testOk(status >= 0 && len == sizeof(flag) && !flag == !put,
        "getsockopt IP_MULTICAST_TTL => %d", (int) flag);
}

static
void udpSockTest(void)
{
    SOCKET s;

    testDiag("udpSockTest()");

    s = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    testOk(s != INVALID_SOCKET, "epicsSocketCreate INET, DGRAM, 0");

    udpBroadcast(s, 1);
    udpBroadcast(s, 0);

    multiCastLoop(s, 1);
    multiCastLoop(s, 0);

    /* Defaults to 1, setting to 0 makes no sense */
    multiCastTTL(s, 2);
    multiCastTTL(s, 1);

    epicsSocketDestroy(s);
}


static
int doBind(int expect, SOCKET S, unsigned* port)
{
    osiSockAddr addr;
    int ret;

    memset(&addr, 0, sizeof(addr));
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.ia.sin_port = htons(*port);

    ret = bind(S, &addr.sa, sizeof(addr.ia));
    if(ret) {
        testOk(expect==1, "bind() to %u error %d, %d", *port, ret, SOCKERRNO);
        return 1;
    } else {
        osiSocklen_t slen = sizeof(addr);
        ret = getsockname(S, &addr.sa, &slen);
        if(ret) {
            testFail("Unable to find sock name after binding");
            return 1;
        } else {
            *port = ntohs(addr.ia.sin_port);
            testOk(expect==0, "bind() to port %u", *port);
            return 0;
        }
    }
}

static
void tcpSockReuseBindTest(int reuse)
{
    SOCKET A, B;
    unsigned port=0; /* choose random port */

    testDiag("tcpSockReuseBindTest(%d)", reuse);

    A = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    B = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);

    if(A==INVALID_SOCKET || B==INVALID_SOCKET)
        testAbort("Insufficient sockets");

    if(reuse) {
        testDiag("epicsSocketEnableAddressReuseDuringTimeWaitState");
        epicsSocketEnableAddressReuseDuringTimeWaitState(A);
        epicsSocketEnableAddressReuseDuringTimeWaitState(B);
    }

    doBind(0, A, &port);
    if(listen(A, 4))
        testFail("listen(A) -> %d", (int)SOCKERRNO);
    doBind(1, B, &port);

    epicsSocketDestroy(A);
    epicsSocketDestroy(B);
}

static
void udpSockFanoutBindTest(void)
{
    SOCKET A, B, C;
    unsigned port=0; /* choose random port */

    testDiag("udpSockFanoutBindTest()");

    A = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    B = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    C = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);

    if(A==INVALID_SOCKET || B==INVALID_SOCKET || C==INVALID_SOCKET)
        testAbort("Insufficient sockets");

    /* not A */
    epicsSocketEnableAddressUseForDatagramFanout(B);
    epicsSocketEnableAddressUseForDatagramFanout(C);

    testDiag("First test if epicsSocketEnableAddressUseForDatagramFanout() is necessary");

    doBind(0, A, &port);
    doBind(1, B, &port); /* expect failure */

    epicsSocketDestroy(A);

    testDiag("Now the real test");
    doBind(0, B, &port);
    doBind(0, C, &port);

    epicsSocketDestroy(B);
    epicsSocketDestroy(C);
}

struct CASearch {
    epicsUInt16 cmd, size, dtype, dcnt;
    epicsUInt32 p1, p2;
    char body[16];
};

STATIC_ASSERT(sizeof(struct CASearch)==32);

union CASearchU {
    struct CASearch msg;
    char bytes[sizeof(struct CASearch)];
};

static
unsigned nsuccess;

static
const unsigned nrepeat = 6u;

struct TInfo {
    SOCKET sock;
    unsigned id;
    epicsUInt32 key;
    epicsUInt8 rxmask;
    epicsUInt8 dupmask;
};

static
void udpSockFanoutTestRx(void* raw)
{
    struct TInfo *info = raw;
    epicsTimeStamp start, now;
    unsigned nremain = nrepeat;
#ifdef _WIN32
    /* ms */
    DWORD timeout = 10000;
#else
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
#endif

    (void)epicsTimeGetCurrent(&start);
    now = start;
    testDiag("RX%u start", info->id);

    if(setsockopt(info->sock, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout, sizeof(timeout))) {
        testFail("Unable to set socket timeout");
        return;
    }

    while(!epicsTimeGetCurrent(&now) && epicsTimeDiffInSeconds(&now, &start)<=5.0) {
        union CASearchU buf;
        osiSockAddr src;
        osiSocklen_t srclen = sizeof(src);

        int n = recvfrom(info->sock, buf.bytes, sizeof(buf.bytes), 0, &src.sa, &srclen);
        buf.bytes[sizeof(buf.bytes)-1] = '\0';

        if(n<0) {
            testDiag("recvfrom error (%d)", (int)SOCKERRNO);
            break;
        } else if((n==sizeof(buf.bytes)) && buf.msg.cmd==htons(6) && buf.msg.size==htons(16)
                  &&buf.msg.dtype==htons(5) && buf.msg.dcnt==0 && strcmp(buf.msg.body, "totallyinvalid")==0)
        {
            unsigned ord = ntohl(buf.msg.p1)-info->key;
            testDiag("RX%u success %u", info->id, ord);
            if(ord<8) {
                const epicsUInt8 mask = 1u<<ord;
                if(info->rxmask&mask)
                    info->dupmask|=mask;
                info->rxmask|=mask;
            }
            if(0==--nremain)
                break;
        } else {
            testDiag("RX ignore n=%d cmd=%d size=%d dtype=%d dcnt=%d body=%s",
                     n, ntohs(buf.msg.cmd), ntohs(buf.msg.size),
                     ntohs(buf.msg.dtype), ntohs(buf.msg.dcnt), buf.msg.body);
        }
    }
    testDiag("RX%u end", info->id);
}

static
void udpSockFanoutTestIface(const osiSockAddr* addr)
{
    SOCKET sender;
    struct TInfo rx1, rx2;
    epicsThreadId trx1, trx2;
    epicsThreadOpts topts = EPICS_THREAD_OPTS_INIT;
    int opt = 1;
    unsigned i;
    osiSockAddr any;
    epicsUInt32 key = 0xdeadbeef ^ ntohl(addr->ia.sin_addr.s_addr);
    union CASearchU buf;
    int ret;

    topts.joinable = 1;

    /* we bind to any for lack of a portable way to find the
     * interface address from the interface broadcast address
     */
    memset(&any, 0, sizeof(any));
    any.ia.sin_family = AF_INET;
    any.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    any.ia.sin_port = addr->ia.sin_port;

    buf.msg.cmd = htons(6);
    buf.msg.size = htons(16);
    buf.msg.dtype = htons(5);
    buf.msg.dcnt = htons(0); /* version 0, which newer servers should ignore */
    /* .p1 and .p2 set below */
    memcpy(buf.msg.body, "tota" "llyi" "nval" "id\0\0", 16);

    sender = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    rx1.sock = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    rx2.sock = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    if((sender==INVALID_SOCKET) || (rx1.sock==INVALID_SOCKET) || (rx2.sock==INVALID_SOCKET))
        testAbort("Unable to allocate test socket(s)");

    rx1.id = 1;
    rx2.id = 2;
    rx1.key = rx2.key = key;
    rx1.rxmask = rx2.rxmask = 0u;
    rx1.dupmask = rx2.dupmask = 0u;

    if(setsockopt(sender, SOL_SOCKET, SO_BROADCAST, (void*)&opt, sizeof(opt))!=0) {
        testFail("setsockopt SOL_SOCKET, SO_BROADCAST error -> %d", (int)SOCKERRNO);
    }

    epicsSocketEnableAddressUseForDatagramFanout(rx1.sock);
    epicsSocketEnableAddressUseForDatagramFanout(rx2.sock);

    if(bind(rx1.sock, &any.sa, sizeof(any)))
        testFail("Can't bind test socket rx1 %d", (int)SOCKERRNO);
    if(bind(rx2.sock, &any.sa, sizeof(any)))
        testFail("Can't bind test socket rx2 %d", (int)SOCKERRNO);

    /* test to see if send is possible (not EPERM) */
    ret = sendto(sender, buf.bytes, sizeof(buf.bytes), 0, &addr->sa, sizeof(*addr));
    if(ret!=(int)sizeof(buf.bytes)) {
        testDiag("test sendto() error %d (%d)", ret, (int)SOCKERRNO);
        goto cleanup;
    }

    trx1 = epicsThreadCreateOpt("rx1", &udpSockFanoutTestRx, &rx1, &topts);
    trx2 = epicsThreadCreateOpt("rx2", &udpSockFanoutTestRx, &rx2, &topts);

    for(i=0; i<nrepeat; i++) {
        /* don't spam */
        epicsThreadSleep(0.5);

        buf.msg.p1 = buf.msg.p2 = htonl(key + i);
        ret = sendto(sender, buf.bytes, sizeof(buf.bytes), 0, &addr->sa, sizeof(*addr));
        if(ret!=(int)sizeof(buf.bytes))
            testDiag("sendto() error %d (%d)", ret, (int)SOCKERRNO);
    }

    epicsThreadMustJoin(trx1);
    epicsThreadMustJoin(trx2);

    testDiag("Result: RX1 %x:%x RX2 %x:%x",
             rx1.rxmask, rx1.dupmask, rx2.rxmask, rx2.dupmask);
    /* success if any one packet was seen by both sockets */
    if(rx1.rxmask & rx2.rxmask)
        nsuccess++;

cleanup:
    epicsSocketDestroy(sender);
    epicsSocketDestroy(rx1.sock);
    epicsSocketDestroy(rx2.sock);
}

/* This test violates the principle of unittest isolation by broadcasting
 * on the well known CA search port on all interfaces.  There is no
 * portable way to avoid this.  (eg. 127.255.255.255 is Linux specific)
 */
static
void udpSockFanoutTest()
{
    ELLLIST ifaces = ELLLIST_INIT;
    ELLNODE *cur;
    SOCKET dummy;
    osiSockAddr match;
    int foundNotLo = 0;

    testDiag("udpSockFanoutTest()");

    memset(&match, 0, sizeof(match));
    match.ia.sin_family = AF_INET;
    match.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    if((dummy = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0))==INVALID_SOCKET)
        testAbort("Unable to allocate discovery socket");

    osiSockDiscoverBroadcastAddresses(&ifaces, dummy, &match);

    for(cur = ellFirst(&ifaces); cur; cur = ellNext(cur)) {
        char name[64];
        osiSockAddrNode* node = CONTAINER(cur, osiSockAddrNode, node);

        node->addr.ia.sin_port = htons(5064);
        (void)sockAddrToDottedIP(&node->addr.sa, name, sizeof(name));

        testDiag("Interface %s", name);
        if(node->addr.ia.sin_addr.s_addr!=htonl(INADDR_LOOPBACK)) {
            testDiag("Not LO");
            foundNotLo = 1;
        }

        udpSockFanoutTestIface(&node->addr);
    }

    ellFree(&ifaces);

    testOk(foundNotLo, "Found non-loopback interface");
    testOk(nsuccess>0, "Successes %u", nsuccess);

    epicsSocketDestroy(dummy);
}

MAIN(osiSockTest)
{
    int status;
    testPlan(24);

    status = osiSockAttach();
    testOk(status, "osiSockAttach");

    udpSockTest();
    udpSockFanoutBindTest();
    udpSockFanoutTest();
    tcpSockReuseBindTest(0);
    tcpSockReuseBindTest(1);

    osiSockRelease();
    return testDone();
}
