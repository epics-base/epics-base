/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdio.h>

#include "osiSock.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* This could easily be generalized to test more options */
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

void udpSockTest(void)
{
    SOCKET s;

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

void udpSockFanoutTest(void)
{
    SOCKET A, B, C;
    unsigned port=0; /* choose random port */

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

MAIN(osiSockTest)
{
    int status;
    testPlan(18);

    status = osiSockAttach();
    testOk(status, "osiSockAttach");

    udpSockTest();
    udpSockFanoutTest();

    osiSockRelease();
    return testDone();
}
