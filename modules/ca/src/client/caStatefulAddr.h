/*************************************************************************\
* Copyright (c) 2026 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
\*************************************************************************/

#ifndef INC_caStatefulAddr_H
#define INC_caStatefulAddr_H

#include <string>
#include <vector>
#include <time.h>

#include "envDefs.h"
#include "ellLib.h"
#include "osiSock.h"

class caStatefulAddr {
public:
    struct ResolveResult {
        osiSockAddr addr;
        unsigned ttl;
        int isStatic;
    };

    typedef int (*Resolver)(const char *token, unsigned short defaultPort,
        int ignoreNonDefaultPort, ResolveResult *result);

    caStatefulAddr();
    caStatefulAddr(const std::string &token, unsigned short defaultPort,
        int ignoreNonDefaultPort);

    bool refreshIfDue();
    bool refresh(time_t now);

    bool resolved() const;
    bool ignored() const;
    bool isStatic() const;
    const osiSockAddr &addr() const;
    const std::string &token() const;

    static void setResolverForTest(Resolver resolver);

private:
    std::string _token;
    unsigned short _defaultPort;
    int _ignoreNonDefaultPort;
    bool _resolved;
    bool _ignored;
    bool _staticAddr;
    time_t _expires;
    osiSockAddr _addr;
};

void caParseStatefulAddrList(std::vector<caStatefulAddr> &out,
    const ENV_PARAM *pEnv, unsigned short defaultPort,
    int ignoreNonDefaultPort);

bool caSockAddrEqual(const osiSockAddr &lhs, const osiSockAddr &rhs);
bool caSockAddrSeen(std::vector<osiSockAddr> &seen, const osiSockAddr &addr);
void caConfigureChannelAccessAutoAddressList(ELLLIST *pList, SOCKET sock,
    unsigned short port);

#endif /* INC_caStatefulAddr_H */
