/*************************************************************************\
* Copyright (c) 2012 Brookhaven Science Associates as Operator of
*     Brookhaven National Lab.
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
\*************************************************************************/

#include "dbDefs.h"
#include "osiSock.h"

#include "epicsUnitTest.h"
#include "testMain.h"

#define DEFAULT_PORT 4000

typedef struct {
    const char *input;
    unsigned long IP;
    unsigned short port;
} testData;

static testData okdata[] = {
    {"127.0.0.1", 0x7f000001, DEFAULT_PORT},
    {"127.0.0.1:42", 0x7f000001, 42},
    {"localhost", 0x7f000001, DEFAULT_PORT},
    {"localhost:42", 0x7f000001, 42},
    {"2424", 2424, DEFAULT_PORT},
    {"2424:42", 2424, 42},
    {"255.255.255.255", 0xffffffff, DEFAULT_PORT},
    {"255.255.255.255:65535", 0xffffffff, 65535},
};

static const char * baddata[] = {
    "127.0.0.1:NaN",
    "127.0.0.test",
    "127.0.0.test:42",
    "16name.invalid",
    "16name.invalid:42",
    "1.2.3.4.5",
    "1.2.3.4.5:6",
    "1.2.3.4:5.6",
    "256.255.255.255",
    "255.256.255.255",
    "255.255.256.255",
    "255.255.255.256",
    "255.255.255.255:65536",
};

MAIN(epicsSockResolveTest)
{
    int i;

    testPlan(3*NELEMENTS(okdata) + NELEMENTS(baddata));
    osiSockAttach();

    {
        struct in_addr addr;

        /* See RFCs 2606 and 6761 */
        if (hostToIPAddr("guaranteed.invalid.", &addr) == 0) {
            testAbort("hostToIPAddr() is broken, testing not possible");
        }
    }

    testDiag("Tests of aToIPAddr");

    for (i=0; i<NELEMENTS(okdata); i++) {
        struct sockaddr_in addr;
        int ret;

        ret = aToIPAddr(okdata[i].input, DEFAULT_PORT, &addr);
        testOk(ret==0, "aToIPAddr(\"%s\", %u) -> %d",
               okdata[i].input, DEFAULT_PORT, ret);
        if (ret) {
            testSkip(2, "  aToIPAddr() failed");
        }
        else {
            testOk(addr.sin_addr.s_addr == htonl(okdata[i].IP), "  IP correct");
            testOk(addr.sin_port == htons(okdata[i].port), "  Port correct");
        }
    }

    for (i=0; i<NELEMENTS(baddata); i++) {
        struct sockaddr_in addr;
        int ret;

        ret = aToIPAddr(baddata[i], DEFAULT_PORT, &addr);
        testOk(ret!=0, "aToIPAddr(\"%s\", %u) -> %d",
               baddata[i], DEFAULT_PORT, ret);
        if (ret==0) {
            testDiag("  IP=0x%lx, port=%d",
                (unsigned long) ntohl(addr.sin_addr.s_addr),
                ntohs(addr.sin_port));
        }
    }

    osiSockRelease();
    return testDone();
}
