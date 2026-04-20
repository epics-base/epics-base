/*************************************************************************\
* Copyright (c) 2026 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exception>
#include <vector>

#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || defined(__sun)
#  include <arpa/nameser.h>
#  include <resolv.h>
#  define CA_USE_DNS_TTL 1
#endif

#include "dbDefs.h"
#include "epicsStdio.h"
#include "epicsString.h"
#include "errlog.h"
#include "osiSock.h"

#include "caStatefulAddr.h"

#define CA_DNS_TTL_FALLBACK 300u
#define CA_DNS_RETRY 60u
#define CA_ADDR_LIST_ENV_MAX 65536u

static caStatefulAddr::Resolver caResolverForTest = NULL;

static int caSetIPAddr(epicsUInt32 rawAddr, unsigned short port,
    struct sockaddr_in *pIP)
{
    static const struct sockaddr_in emptyAddr = {0};

    *pIP = emptyAddr;
    pIP->sin_family = AF_INET;
    pIP->sin_port = htons(port);
    pIP->sin_addr.s_addr = htonl(rawAddr);
    return 0;
}

static int caParseNumericIPAddr(const char *host, unsigned short defaultPort,
    struct sockaddr_in *pIP)
{
    int status;
    unsigned addr[4];
    unsigned long rawAddr;
    unsigned port;
    char dummy[8];

    status = sscanf(host, " %u . %u . %u . %u %7s ",
        addr, addr+1u, addr+2u, addr+3u, dummy);
    if(status == 4) {
        if(addr[0] <= 0xff && addr[1] <= 0xff &&
           addr[2] <= 0xff && addr[3] <= 0xff) {
            return caSetIPAddr(((epicsUInt32)addr[0] << 24) |
                               ((epicsUInt32)addr[1] << 16) |
                               ((epicsUInt32)addr[2] << 8) |
                               (epicsUInt32)addr[3], defaultPort, pIP);
        }
        return -1;
    }

    status = sscanf(host, " %u . %u . %u . %u : %u %7s ",
        addr, addr+1u, addr+2u, addr+3u, &port, dummy);
    if(status == 5 && port <= 0xffff) {
        if(addr[0] <= 0xff && addr[1] <= 0xff &&
           addr[2] <= 0xff && addr[3] <= 0xff) {
            return caSetIPAddr(((epicsUInt32)addr[0] << 24) |
                               ((epicsUInt32)addr[1] << 16) |
                               ((epicsUInt32)addr[2] << 8) |
                               (epicsUInt32)addr[3], (unsigned short)port, pIP);
        }
        return -1;
    }

    status = sscanf(host, " %lu %7s ", &rawAddr, dummy);
    if(status == 1 && rawAddr <= 0xfffffffful) {
        return caSetIPAddr((epicsUInt32)rawAddr, defaultPort, pIP);
    }

    status = sscanf(host, " %lu : %u %7s ", &rawAddr, &port, dummy);
    if(status == 2 && rawAddr <= 0xfffffffful && port <= 0xffff) {
        return caSetIPAddr((epicsUInt32)rawAddr, (unsigned short)port, pIP);
    }

    return -1;
}

static std::string caHostPart(const char *token)
{
    const char *colon = strchr(token, ':');

    if(colon)
        return std::string(token, colon-token);
    return std::string(token);
}

#ifdef CA_USE_DNS_TTL
static int caDnsTTL(const char *host, const struct in_addr *addr,
    unsigned *ttl)
{
    unsigned char answer[4096];
    ns_msg handle;
    int len;
    int count;
    int i;
    int found = 0;
    unsigned best = 0;

    len = res_query(host, ns_c_in, ns_t_a, answer, sizeof(answer));
    if(len < 0 || len > (int)sizeof(answer) || ns_initparse(answer, len, &handle))
        return -1;

    count = ns_msg_count(handle, ns_s_an);
    for(i = 0; i < count; i++) {
        ns_rr rr;

        if(ns_parserr(&handle, ns_s_an, i, &rr))
            continue;
        if(ns_rr_type(rr) != ns_t_a || ns_rr_class(rr) != ns_c_in)
            continue;
        if(ns_rr_rdlen(rr) != sizeof(addr->s_addr))
            continue;
        if(memcmp(ns_rr_rdata(rr), &addr->s_addr, sizeof(addr->s_addr)) != 0)
            continue;

        if(!found || ns_rr_ttl(rr) < best)
            best = ns_rr_ttl(rr);
        found = 1;
    }
    if(found) {
        *ttl = best;
        return 0;
    }
    return -1;
}
#endif

static int caResolveDefault(const char *token, unsigned short defaultPort,
    int ignoreNonDefaultPort, caStatefulAddr::ResolveResult *result)
{
    struct sockaddr_in addr;

    result->ttl = CA_DNS_TTL_FALLBACK;
    result->isStatic = 0;
    if(caParseNumericIPAddr(token, defaultPort, &addr)==0) {
        if(ignoreNonDefaultPort && ntohs(addr.sin_port) != defaultPort)
            return 1;
        result->addr.ia = addr;
        result->isStatic = 1;
        return 0;
    }

    if(aToIPAddr(token, defaultPort, &addr)) {
        errlogPrintf("CAC: Unable to resolve address list host '%s'\n", token);
        return -1;
    }
    if(ignoreNonDefaultPort && ntohs(addr.sin_port) != defaultPort)
        return 1;

#ifdef CA_USE_DNS_TTL
    {
        std::string host(caHostPart(token));

        if(caDnsTTL(host.c_str(), &addr.sin_addr, &result->ttl))
            result->ttl = CA_DNS_TTL_FALLBACK;
    }
#endif
    result->addr.ia = addr;
    return 0;
}

caStatefulAddr::caStatefulAddr() :
    _defaultPort(0u),
    _ignoreNonDefaultPort(0),
    _resolved(false),
    _ignored(false),
    _staticAddr(false),
    _expires(0),
    _addr()
{
}

caStatefulAddr::caStatefulAddr(const std::string &token,
    unsigned short defaultPort, int ignoreNonDefaultPort) :
    _token(token),
    _defaultPort(defaultPort),
    _ignoreNonDefaultPort(ignoreNonDefaultPort),
    _resolved(false),
    _ignored(false),
    _staticAddr(false),
    _expires(0),
    _addr()
{
    refreshIfDue();
}

bool caStatefulAddr::refreshIfDue()
{
    time_t now = time(NULL);

    if(now == (time_t)-1)
        now = 0;
    return refresh(now);
}

bool caStatefulAddr::refresh(time_t now)
{
    caStatefulAddr::ResolveResult result = {};
    caStatefulAddr::Resolver resolver = caResolverForTest;
    bool oldResolved;
    osiSockAddr oldAddr;
    int status;

    if(_ignored || _staticAddr)
        return false;
    if(_expires && difftime(now, _expires) < 0.0)
        return false;

    oldResolved = _resolved;
    oldAddr = _addr;
    if(!resolver)
        resolver = caResolveDefault;

    status = (*resolver)(_token.c_str(), _defaultPort, _ignoreNonDefaultPort,
        &result);
    if(status == 1) {
        _ignored = true;
        _resolved = false;
        return oldResolved;
    }
    if(status) {
        _resolved = false;
        _staticAddr = false;
        _expires = now + CA_DNS_RETRY;
        return oldResolved;
    }

    _addr = result.addr;
    _resolved = true;
    _staticAddr = !!result.isStatic;
    _expires = _staticAddr ? 0 : now + result.ttl;
    if(!oldResolved)
        return true;
    return !caSockAddrEqual(oldAddr, _addr);
}

bool caStatefulAddr::resolved() const
{
    return _resolved;
}

bool caStatefulAddr::ignored() const
{
    return _ignored;
}

bool caStatefulAddr::isStatic() const
{
    return _staticAddr;
}

const osiSockAddr &caStatefulAddr::addr() const
{
    return _addr;
}

const std::string &caStatefulAddr::token() const
{
    return _token;
}

void caStatefulAddr::setResolverForTest(Resolver resolver)
{
    caResolverForTest = resolver;
}

void caParseStatefulAddrList(std::vector<caStatefulAddr> &out,
    const ENV_PARAM *pEnv, unsigned short defaultPort,
    int ignoreNonDefaultPort)
{
    const char *pStr = envGetConfigParamPtr(pEnv);

    if(!pStr)
        return;

    try {
        size_t len = epicsStrnLen(pStr, CA_ADDR_LIST_ENV_MAX);
        std::vector<char> scratch;
        char *save = NULL;
        const char *pToken;

        if(len == CA_ADDR_LIST_ENV_MAX) {
            errlogPrintf("CAC: address list '%s' exceeds %u bytes\n",
                pEnv->name, (unsigned)CA_ADDR_LIST_ENV_MAX);
            return;
        }

        scratch.resize(len + 1u, '\0');
        memcpy(&scratch[0], pStr, len);

        for(pToken = epicsStrtok_r(&scratch[0], " \t\n\r", &save);
            pToken;
            pToken = epicsStrtok_r(NULL, " \t\n\r", &save)) {
            caStatefulAddr addr(pToken, defaultPort, ignoreNonDefaultPort);

            if(!addr.ignored())
                out.push_back(addr);
        }
    } catch(std::exception &) {
    }
}

bool caSockAddrEqual(const osiSockAddr &lhs, const osiSockAddr &rhs)
{
    if(lhs.sa.sa_family != rhs.sa.sa_family)
        return false;
    if(lhs.sa.sa_family != AF_INET)
        return false;
    return lhs.ia.sin_addr.s_addr == rhs.ia.sin_addr.s_addr &&
           lhs.ia.sin_port == rhs.ia.sin_port;
}

bool caSockAddrSeen(std::vector<osiSockAddr> &seen, const osiSockAddr &addr)
{
    std::vector<osiSockAddr>::const_iterator it;

    for(it = seen.begin(); it != seen.end(); ++it) {
        if(caSockAddrEqual(*it, addr))
            return true;
    }
    seen.push_back(addr);
    return false;
}
