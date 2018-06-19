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

void udpSockTest(void)
{
    SOCKET s;

    s = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    testOk(s != INVALID_SOCKET, "epicsSocketCreate INET, DGRAM, 0");

    udpBroadcast(s, 1);
    udpBroadcast(s, 0);

    multiCastLoop(s, 1);
    multiCastLoop(s, 0);

    epicsSocketDestroy(s);
}


MAIN(osiSockTest)
{
    int status;
    testPlan(10);

    status = osiSockAttach();
    testOk(status, "osiSockAttach");

    udpSockTest();

    osiSockRelease();
    return testDone();
}
