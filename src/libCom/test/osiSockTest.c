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

void udpSockTest(void)
{
    SOCKET s;
    int status, one = 1, get = 0;
    osiSocklen_t len;
    osiSockOptMcastLoop_t flag = 1;

    s = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    testOk(s != INVALID_SOCKET, "epicsSocketCreate INET, DGRAM, 0");

    status = setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&one, sizeof(one));
    testOk(status >= 0, "setsockopt BROADCAST, 1");

    len = sizeof(get);
    status = getsockopt(s, SOL_SOCKET, SO_BROADCAST, (char *)&get, &len);
    testOk(status >= 0 && len == sizeof(get) && get == 1,
        "getsockopt BROADCAST == 1");

    status = setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP,
                   (char *)&flag, sizeof(flag));
    testOk(status >= 0, "setsockopt MULTICAST_LOOP, 1");

    flag = 0;
    len = sizeof(flag);
    status = getsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&flag, &len);
    testOk(status >= 0 && len == sizeof(flag) && flag == 1,
        "getsockopt MULTICAST_LOOP == 1");

    epicsSocketDestroy(s);
}


MAIN(osiSockTest)
{
    int status;
    testPlan(6);

    status = osiSockAttach();
    testOk(status, "osiSockAttach");

    udpSockTest();

    osiSockRelease();
    return testDone();
}
