/*************************************************************************\
* Copyright (c) 2026 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
\*************************************************************************/

#include <string.h>
#include <time.h>
#include <vector>

#include <testMain.h>
#include <epicsUnitTest.h>

#include "osiSock.h"
#include "caStatefulAddr.h"

static int resolverPhase;
static int badNowResolves;

static void setAddr(osiSockAddr *addr, unsigned a, unsigned b,
    unsigned c, unsigned d, unsigned short port)
{
    memset(addr, 0, sizeof(*addr));
    addr->ia.sin_family = AF_INET;
    addr->ia.sin_port = htons(port);
    addr->ia.sin_addr.s_addr =
        htonl((a << 24) | (b << 16) | (c << 8) | d);
}

static int fakeResolver(const char *token, unsigned short defaultPort,
    int ignoreNonDefaultPort, caStatefulAddr::ResolveResult *result)
{
    unsigned short port = defaultPort;

    memset(result, 0, sizeof(*result));
    result->ttl = 10u;
    if(strcmp(token, "dyn:6000")==0)
        port = 6000u;
    if(ignoreNonDefaultPort && port != defaultPort)
        return 1;
    if(strcmp(token, "bad")==0 && !badNowResolves)
        return -1;
    if(strcmp(token, "dyn")==0 || strcmp(token, "dyn:6000")==0) {
        setAddr(&result->addr, 10, 0, 0, resolverPhase ? 2u : 1u, port);
        return 0;
    }
    if(strcmp(token, "bad")==0) {
        setAddr(&result->addr, 10, 0, 0, 3, port);
        return 0;
    }
    return -1;
}

MAIN(caStatefulAddrTest)
{
    testPlan(16);

    caStatefulAddr::setResolverForTest(NULL);
    {
        caStatefulAddr addr("127.0.0.1", 5064u, 0);

        testOk(addr.resolved(), "numeric address resolves immediately");
        testOk(addr.isStatic(), "numeric address is static");
        testOk(ntohs(addr.addr().ia.sin_port)==5064u,
            "numeric address uses default port");
    }
    {
        caStatefulAddr addr("127.0.0.1:6000", 5064u, 1);

        testOk(addr.ignored(), "non-default numeric port is ignored when requested");
    }

    caStatefulAddr::setResolverForTest(fakeResolver);
    resolverPhase = 0;
    badNowResolves = 0;
    {
        time_t start = time(NULL);
        caStatefulAddr addr("dyn", 5064u, 0);

        if(start == (time_t)-1)
            start = 0;
        testOk(addr.resolved(), "hostname resolves initially");
        testOk(!addr.isStatic(), "hostname is refreshable");
        testOk(ntohl(addr.addr().ia.sin_addr.s_addr)==0x0a000001u,
            "hostname uses initial resolved address");
        resolverPhase = 1;
        testOk(!addr.refresh(start + 5), "hostname does not refresh before TTL");
        testOk(ntohl(addr.addr().ia.sin_addr.s_addr)==0x0a000001u,
            "hostname keeps cached address before TTL");
        testOk(addr.refresh(start + 20), "hostname refreshes after TTL");
        testOk(ntohl(addr.addr().ia.sin_addr.s_addr)==0x0a000002u,
            "hostname uses refreshed address after TTL");
    }
    {
        time_t start = time(NULL);
        caStatefulAddr addr("bad", 5064u, 0);

        if(start == (time_t)-1)
            start = 0;
        testOk(!addr.resolved(), "unresolved hostname fails closed");
        badNowResolves = 1;
        testOk(addr.refresh(start + 120), "unresolved hostname retries later");
        testOk(addr.resolved(), "retried hostname can become resolved");
    }
    {
        std::vector<osiSockAddr> seen;
        osiSockAddr addr;

        setAddr(&addr, 10, 0, 0, 4, 5064u);
        testOk(!caSockAddrSeen(seen, addr), "first address is not duplicate");
        testOk(caSockAddrSeen(seen, addr), "second address is duplicate");
    }

    caStatefulAddr::setResolverForTest(NULL);
    return testDone();
}
