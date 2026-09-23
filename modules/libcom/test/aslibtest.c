/*************************************************************************\
* Copyright (c) 2018 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <testMain.h>
#include <epicsUnitTest.h>

#include <errSymTbl.h>
#include <epicsEvent.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <osiFileName.h>
#include <osiSock.h>
#include <errlog.h>

#include <asLib.h>
#include <as/asLibPvt.h>

static char *asUser,
            *asHost;
static int asAsl;

static epicsUInt64 hagNow;
static epicsUInt32 hagAddress;
static unsigned hagResolveCount;
static int hagResolveFailure;
static int hagResolveBlock;
static int hagUnexpectedName;
static epicsEventId hagResolverEntered;
static epicsEventId hagResolverRelease;

#define TEST_NSEC_PER_SEC 1000000000uLL

/**
 * @brief Test data with Host Access Groups (HAG)
 *
 * This includes a host access group (HAG) for localhost and a default Access Security Group (ASG)
 * with rules for read and write access to the HAG.
 */
static const char hostname_config[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n"

    "ASG(rw) {\n"
    "    RULE(1, WRITE) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

static const char refresh_config[] = ""
    "HAG(foo) {refresh.test}\n"
    "ASG(DEFAULT) { RULE(0, NONE) }\n"
    "ASG(ro) { RULE(1, READ) { HAG(foo) } }\n";

static const char duplicate_config[] = ""
    "HAG(foo) {refresh.test, 127.0.0.1}\n"
    "ASG(DEFAULT) { RULE(0, NONE) }\n"
    "ASG(ro) { RULE(1, READ) { HAG(foo) } }\n";

static const char numeric_config[] = ""
    "HAG(foo) {127.0.0.3}\n"
    "ASG(DEFAULT) { RULE(0, NONE) }\n"
    "ASG(ro) { RULE(1, READ) { HAG(foo) } }\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST)
 * - valid top level keyword with well-formed arg list
 */
static const char supported_config_1[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST)\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { WELL,FORMED,LIST }
 * - valid top level keyword with well-formed arg list and valid arg list body
 */
static const char supported_config_2[] = ""
    "HAG(foo) {localhost}\n"

    "SIMPLE(WELL, FORMED, ARG, LIST) {\n"
    "    WELL, FORMED, LIST\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) }
 * - valid top level keyword with well-formed arg list and valid recursive body
 * - includes quoted strings, integers, and floating point numbers
 */
static const char supported_config_3[] = ""
    "HAG(foo) {localhost}\n"

    "COMPLEX_ARGUMENTS(1, WELL, \"FORMED\", ARG, LIST) {\n"
    "    ALSO_GENERIC(WELL, FORMED, ARG, LIST, 2.0) \n"
    "    RULE(WELL, FORMED, ARG, LIST, 2.0) \n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) { AND_BODY } }
 * - valid top level keyword with well-formed arg list and valid recursive body, with a nested body
 * - includes floating point numbers, and an empty arg list
 */
static const char supported_config_4[] = ""
"HAG(foo) {localhost}\n"

"SUB_BLOCKS(1.0, ARGS) {\n"
"    ALSO_GENERIC() {\n"
"        AND_LIST_BODY\n"
"    }\n"
"    RULE() {\n"
"        BIGGER, LIST, BODY\n"
"    }\n"
"}\n"

"ASG(DEFAULT) {\n"
"    RULE(0, NONE)\n"
"}\n"

"ASG(ro) {\n"
"    RULE(0, NONE)\n"
"    RULE(1, READ) {\n"
"        HAG(foo)\n"
"    }\n"
"}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { recursive-body-keyword(WELL,FORMED,LIST) { AND_RECURSIVE_BODY() {LIST, LIST } }
 * - valid top level keyword with well-formed arg list and valid recursive body, with a nested recursion
 * - includes floating point numbers, and an empty arg list
 */
static const char supported_config_5[] = ""
    "HAG(foo) {localhost}\n"

    "RECURSIVE_SUB_BLOCKS(1.0, -2.3, +4.5, ARGS, +2.71828E-23, -2.71828e+23, +12, -13, +-14) {\n"
    "    ALSO_GENERIC() {\n"
    "        AND_RECURSIVE(FOO) {\n"
    "            LIST, BODY\n"
    "        }\n"
    "    }\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(+1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should be silently ignored, but the rest of the config is processed.
 *
 * top-unknown-keyword(KEYWORD) { KEYWORD(KEYWORD) }
 * - valid top level keyword with keyword for args, recursive body name, and arg list
 * - top level generic items referenced in RULES, then RULES are ignored
 */
static const char supported_config_6[] = ""
    "HAG(foo) {localhost}\n"

    "WITH_KEYWORDS(UAG) {\n"
    "    ASG(HAL, IMP, CALC, RULE)\n"
    "    HAL(USG, MAL) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ignored) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        WITH_KEYWORDS(UAG)\n"
    "    }\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "    RULE(2, WRITE) {\n"
    "        WITH_KEYWORDS(UAG)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported elements should be silently ignored, and the rule will not match,
 * but the rest of the config is processed.
 *
 * - RULE contains unsupported elements
 */
static const char supported_config_7[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "        BAD_PREDICATE(\"x509\")\n"
    "        BAD_PREDICATE_AS_WELL(\"EPICS Certificate Authority\")\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported elements should be silently ignored, and the rule will not match,
 * but the rest of the config is processed.
 *
 * - unexpected permission name in arg list for RULE element ignored
 */
static const char supported_config_8[] = ""
        "HAG(foo) {localhost}\n"

        "ASG(DEFAULT) {\n"
        "    RULE(0, NONE)\n"
        "}\n"

        "ASG(ro) {\n"
        "    RULE(0, NONE)\n"
        "    RULE(1, ADDITIONAL_PERMISSION) {\n"
        "        HAG(foo)\n"
        "    }\n"
        "}\n"
        ;

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword( a b )
 * - invalid arg list missing commas
 */
static const char unsupported_config_1[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(not well-formed arg list)\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { a b }
 * - invalid string list
 */
static const char unsupported_config_2[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "    NOT WELL-FORMED BODY\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword { a, b }
 * - missing parameters (must have at least an empty arg list)
 */
static const char unsupported_config_3[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC {\n"
    "    WELL, FORMED, LIST, BODY\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { X, Y(a b c) }
 * - bad arg list for recursive body
 */
static const char unsupported_config_4[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "    BODY(BAD ARG LIST)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Test data with unsupported elements.
 * The unsupported element should cause an error as the format is invalid.
 *
 * top-unknown-keyword(WELL,FORMED,LIST) { X, Y(a b c) }
 * - mix of list and recursive type bodies
 */
static const char unsupported_config_5[] = ""
    "HAG(foo) {localhost}\n"

    "GENERIC(WELL, FORMED, ARG, LIST) {\n"
    "    LIST, BODY, MIXED, WITH,\n"
    "    RECURSIVE_BODY(ARG, LIST)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for ASG element
 */
static const char unsupported_mod_1[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro BAD ARG LIST) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for HAG element
 */
static const char unsupported_mod_2[] = ""
    "HAG(BAD ARG LIST) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg list for RULE element
 */
static const char unsupported_mod_3[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0 BAD ARG LIST)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - bad arg count for ASG element
 */
static const char unsupported_mod_4[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro, UNKNOWN_PERMISSION) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected name in arg list for RULE element
 */
static const char unsupported_mod_5[] = ""
    "HAG(foo) {localhost}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE, UNKNOWN_FLAG)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected recursive body mixed in with HAG string list body
 */
static const char unsupported_mod_6[] = ""
    "HAG(foo) {\n"
    "    localhost,\n"
    "    NETWORK(\"127.0.0.1\")\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * The modification to a well known element should cause an error.
 *
 * - unexpected recursive body mixed in with UAG string list body
 */
static const char unsupported_mod_7[] = ""
    "UAG(foo) {\n"
    "    alice,\n"
    "    GROUP(admin)\n"
    "}\n"

    "ASG(DEFAULT) {\n"
    "    RULE(0, NONE)\n"
    "}\n"

    "ASG(ro) {\n"
    "    RULE(0, NONE)\n"
    "    RULE(1, READ) {\n"
    "        HAG(foo)\n"
    "    }\n"
    "}\n";

/**
 * Set the username for the authorization tests
 */
static void setUser(const char *name)
{
    free(asUser);
    asUser = epicsStrDup(name);
}

/**
 * Set the hostname for the authorization tests
 */
static void setHost(const char *name)
{
    free(asHost);
    asHost = epicsStrDup(name);
}

static epicsUInt64 testHagClock(void)
{
    return hagNow;
}

static int epicsStdCall testHagResolver(const char *name, unsigned short port,
    struct sockaddr_in *address)
{
    static const struct sockaddr_in emptyAddress = {0};

    if(strcmp(name, "refresh.test") != 0)
        hagUnexpectedName = 1;
    hagResolveCount++;
    if(hagResolveBlock) {
        epicsEventSignal(hagResolverEntered);
        epicsEventMustWait(hagResolverRelease);
    }
    if(hagResolveFailure)
        return -1;
    *address = emptyAddress;
    address->sin_family = AF_INET;
    address->sin_port = htons(port);
    address->sin_addr.s_addr = htonl(hagAddress);
    return 0;
}

static void countAccessCallback(ASCLIENTPVT client, asClientStatus status)
{
    unsigned *count = asGetClientPvt(client);

    if(status == asClientCOAR && count)
        (*count)++;
}

static void addPersistentClient(ASMEMBERPVT member, ASCLIENTPVT *client,
    char *host, unsigned *callbacks)
{
    long status = asAddClient(client, member, 0, "testing", host);

    testOk(status == 0, "add persistent client %s -> %s",
        host, errSymMsg(status));
    if(status)
        return;
    asPutClientPvt(*client, callbacks);
    status = asRegisterClientCallback(*client, countAccessCallback);
    testOk(status == 0, "register callback for %s -> %s",
        host, errSymMsg(status));
}

/**
 * Test the access control system with the given ASG, user, and hostname
 * This will test that the expected access given by mask is granted
 * when the Access Security Group (ASG) is interpreted in the
 * context of the configured user, host, and Level (asl).
 */
static void testAccess(const char *asg, unsigned mask)
{
    ASMEMBERPVT asp = 0; /* aka dbCommon::asp */
    ASCLIENTPVT client = 0;
    long ret;

    ret = asAddMember(&asp, asg);
    if(ret) {
        testFail("testAccess(ASG:%s, USER:%s, HOST:%s, ASL:%d) -> asAddMember error: %s",
                 asg, asUser, asHost, asAsl, errSymMsg(ret));
    } else {
        ret = asAddClient(&client, asp, asAsl, asUser, asHost);
    }
    if(ret) {
        testFail("testAccess(ASG:%s, USER:%s, HOST:%s, ASL:%d) -> asAddClient error: %s",
                 asg, asUser, asHost, asAsl, errSymMsg(ret));
    } else {
        unsigned actual = 0;
        actual |= asCheckGet(client) ? 1 : 0;
        actual |= asCheckPut(client) ? 2 : 0;
        testOk(actual==mask, "testAccess(ASG:%s, USER:%s, HOST:%s, ASL:%d) -> %x == %x",
               asg, asUser, asHost, asAsl, actual, mask);
    }
    if(client) asRemoveClient(&client);
    if(asp) asRemoveMember(&asp);
}

static void testSyntaxErrors(void)
{
    static const char empty[] = "\n#almost empty file\n\n";
    long ret;

    testDiag("testSyntaxErrors()");
    asCheckClientIP = 0;

    eltc(0);
    ret = asInitMem(empty, NULL);
    testOk(ret==S_asLib_badConfig, "load \"empty\" config -> %s", errSymMsg(ret));
    eltc(1);
}

static void testHostNames(void)
{
    testDiag("testHostNames()");
    asCheckClientIP = 0;

    testOk1(asInitMem(hostname_config, NULL)==0);

    setUser("testing");
    setHost("localhost");
    asAsl = 0;

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 1);
    testAccess("rw", 3);

    setHost("127.0.0.1");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);

    setHost("guaranteed.invalid.");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);
}

static void testUseIP(void)
{
    testDiag("testUseIP()");
    asCheckClientIP = 1;

    /* still host names in .acf */
    testOk1(asInitMem(hostname_config, NULL)==0);
    /* now resolved to IPs */

    setUser("testing");
    setHost("localhost"); /* will not match against resolved IP */
    asAsl = 0;

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);

    setHost("127.0.0.1");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 1);
    testAccess("rw", 3);

    setHost("guaranteed.invalid.");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);
}

static void testRefreshInactive(void)
{
    unsigned changed = 1;
    long status = asRefreshHag(&changed);

    testOk(status == S_asLib_asNotActive && changed == 0,
        "refresh reports inactive Access Security");
}

static void testHagRefresh(void)
{
    ASMEMBERPVT member = NULL;
    ASCLIENTPVT oldClient = NULL;
    ASCLIENTPVT newClient = NULL;
    char oldHost[] = "127.0.0.1";
    char newHost[] = "127.0.0.2";
    unsigned oldCallbacks = 0;
    unsigned newCallbacks = 0;
    unsigned changed;
    unsigned calls;
    long status;

    testDiag("testHagRefresh()");
    asCheckClientIP = 1;
    hagNow = 0;
    hagAddress = 0x7f000001u;
    hagResolveCount = 0;
    hagResolveFailure = 0;
    hagResolveBlock = 0;
    hagUnexpectedName = 0;
    asTestSetHagClock(testHagClock);
    asTestSetHagResolver(testHagResolver);

    status = asInitMem(refresh_config, NULL);
    testOk(status == 0, "load refreshable HAG -> %s", errSymMsg(status));
    testOk(hagResolveCount == 1 && !hagUnexpectedName,
        "initial load resolves the original hostname once");
    status = asAddMember(&member, "ro");
    testOk(status == 0, "add refresh test member -> %s", errSymMsg(status));
    addPersistentClient(member, &oldClient, oldHost, &oldCallbacks);
    addPersistentClient(member, &newClient, newHost, &newCallbacks);
    testOk(asCheckGet(oldClient) && !asCheckGet(newClient),
        "initial mapping grants only the original address");

    changed = 99;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 0 && hagResolveCount == 1,
        "poll before success interval performs no DNS work");

    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    changed = 99;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 0 && hagResolveCount == 2 &&
           oldCallbacks == 1 && newCallbacks == 1,
        "unchanged refresh reports no effective change or callbacks");

    hagAddress = 0x7f000002u;
    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    changed = 0;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 1,
        "changed address reports an effective mapping change");
    testOk(!asCheckGet(oldClient) && asCheckGet(newClient),
        "persistent client rights follow the changed address");
    testOk(oldCallbacks == 2 && newCallbacks == 2,
        "address change calls each affected client once");

    hagResolveFailure = 1;
    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    changed = 0;
    eltc(0);
    status = asRefreshHag(&changed);
    eltc(1);
    testOk(status == 0 && changed == 1,
        "resolution failure removes the effective mapping");
    testOk(!asCheckGet(oldClient) && !asCheckGet(newClient) &&
           oldCallbacks == 2 && newCallbacks == 3,
        "resolution failure is fail-closed for existing clients");

    calls = hagResolveCount;
    hagNow += 59uLL * TEST_NSEC_PER_SEC;
    changed = 99;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 0 && hagResolveCount == calls,
        "failed hostname is not retried before 60 seconds");

    hagResolveFailure = 0;
    hagAddress = 0x7f000001u;
    hagNow += TEST_NSEC_PER_SEC;
    changed = 0;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 1 && hagResolveCount == calls + 1,
        "failed hostname is retried after 60 seconds");
    testOk(asCheckGet(oldClient) && !asCheckGet(newClient) &&
           oldCallbacks == 3 && newCallbacks == 3,
        "successful retry restores existing client rights");

    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    status = asRefreshHag(NULL);
    testOk(status == 0, "changed output may be NULL");

    if(oldClient) asRemoveClient(&oldClient);
    if(newClient) asRemoveClient(&newClient);
    if(member) asRemoveMember(&member);
}

static void testHagEffectiveMapping(void)
{
    ASMEMBERPVT member = NULL;
    ASCLIENTPVT client = NULL;
    char host[] = "127.0.0.1";
    unsigned callbacks = 0;
    unsigned changed = 99;
    long status;

    testDiag("testHagEffectiveMapping()");
    hagNow = 0;
    hagAddress = 0x7f000001u;
    hagResolveFailure = 0;
    hagResolveCount = 0;
    eltc(0);
    status = asInitMem(duplicate_config, NULL);
    eltc(1);
    testOk(status == 0, "load duplicate effective HAG mapping -> %s",
        errSymMsg(status));
    status = asAddMember(&member, "ro");
    testOk(status == 0, "add duplicate mapping member -> %s",
        errSymMsg(status));
    addPersistentClient(member, &client, host, &callbacks);
    testOk(asCheckGet(client) && callbacks == 1,
        "duplicate mapping initially grants access");

    hagResolveFailure = 1;
    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    eltc(0);
    status = asRefreshHag(&changed);
    eltc(1);
    testOk(status == 0 && changed == 0,
        "entry change with identical effective set reports unchanged");
    testOk(asCheckGet(client) && callbacks == 1,
        "unchanged effective set avoids client recomputation");

    if(client) asRemoveClient(&client);
    if(member) asRemoveMember(&member);
}

static void testStaticHagEntries(void)
{
    unsigned changed = 99;
    long status;

    testDiag("testStaticHagEntries()");
    hagResolveCount = 0;
    hagNow = 1000uLL * TEST_NSEC_PER_SEC;
    asCheckClientIP = 1;
    status = asInitMem(numeric_config, NULL);
    testOk(status == 0, "load numeric HAG -> %s", errSymMsg(status));
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 0 && hagResolveCount == 0,
        "numeric HAG entries never perform DNS refresh");
    setUser("testing");
    setHost("127.0.0.3");
    asAsl = 0;
    testAccess("ro", 1);

    asCheckClientIP = 0;
    status = asInitMem(refresh_config, NULL);
    testOk(status == 0, "load hostname-string HAG -> %s", errSymMsg(status));
    changed = 99;
    status = asRefreshHag(&changed);
    testOk(status == 0 && changed == 0 && hagResolveCount == 0,
        "asCheckClientIP=0 keeps hostname HAG entries static");
    setHost("refresh.test");
    testAccess("ro", 1);
}

typedef struct RefreshThreadInfo {
    epicsEventId done;
    long status;
    unsigned changed;
} RefreshThreadInfo;

static void runRefresh(void *raw)
{
    RefreshThreadInfo *info = raw;

    info->status = asRefreshHag(&info->changed);
    epicsEventSignal(info->done);
}

static void startRefreshThread(const char *name, RefreshThreadInfo *info)
{
    info->done = epicsEventMustCreate(epicsEventEmpty);
    info->status = -1;
    info->changed = 99;
    epicsThreadCreate(name, epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackSmall), runRefresh, info);
}

static void finishRefreshThread(RefreshThreadInfo *info)
{
    epicsEventMustWait(info->done);
    epicsEventDestroy(info->done);
}

static void testHagRefreshConcurrency(void)
{
    RefreshThreadInfo first;
    RefreshThreadInfo second;
    unsigned calls;
    long status;

    testDiag("testHagRefreshConcurrency()");
    asCheckClientIP = 1;
    hagNow = 0;
    hagAddress = 0x7f000001u;
    hagResolveFailure = 0;
    hagResolveBlock = 0;
    status = asInitMem(refresh_config, NULL);
    testOk(status == 0, "load policy for reload race -> %s", errSymMsg(status));

    hagResolverEntered = epicsEventMustCreate(epicsEventEmpty);
    hagResolverRelease = epicsEventMustCreate(epicsEventEmpty);
    hagResolveBlock = 1;
    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    startRefreshThread("hagRefreshReload", &first);
    epicsEventMustWait(hagResolverEntered);
    status = asInitMem(numeric_config, NULL);
    testOk(status == 0, "policy reload completes while DNS is pending -> %s",
        errSymMsg(status));
    hagResolveBlock = 0;
    epicsEventSignal(hagResolverRelease);
    finishRefreshThread(&first);
    testOk(first.status == 0 && first.changed == 0,
        "stale DNS result is discarded after policy generation changes");
    setUser("testing");
    setHost("127.0.0.3");
    asAsl = 0;
    testAccess("ro", 1);
    epicsEventDestroy(hagResolverEntered);
    epicsEventDestroy(hagResolverRelease);

    hagNow = 0;
    status = asInitMem(refresh_config, NULL);
    testOk(status == 0, "load policy for concurrent refresh -> %s",
        errSymMsg(status));
    hagResolverEntered = epicsEventMustCreate(epicsEventEmpty);
    hagResolverRelease = epicsEventMustCreate(epicsEventEmpty);
    hagResolveBlock = 1;
    hagNow += 300uLL * TEST_NSEC_PER_SEC;
    calls = hagResolveCount;
    startRefreshThread("hagRefreshOne", &first);
    epicsEventMustWait(hagResolverEntered);
    startRefreshThread("hagRefreshTwo", &second);
    epicsThreadSleep(0.02);
    hagResolveBlock = 0;
    epicsEventSignal(hagResolverRelease);
    finishRefreshThread(&first);
    finishRefreshThread(&second);
    testOk(first.status == 0 && second.status == 0,
        "concurrent refresh callers both complete successfully");
    testOk(hagResolveCount == calls + 1,
        "concurrent refresh calls serialize one due DNS lookup");
    testOk(first.changed == 0 && second.changed == 0,
        "serialized unchanged refreshes report no mapping change");
    epicsEventDestroy(hagResolverEntered);
    epicsEventDestroy(hagResolverRelease);
    hagResolveBlock = 0;
    asTestResetHagHooks();
}

static void testFutureProofParser(void)
{
    long ret;

    testDiag("testFutureProofParser()");
    asCheckClientIP = 0;

    eltc(0);  /* Suppress error messages during test */

    /* Test parsing should reject unsupported elements badly placed or formed */
    ret = asInitMem(unsupported_config_1, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects invalid arg list missing commas -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_2, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects invalid string list -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_3, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects missing parameters (must have at least an empty arg list) -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_4, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for recursive body -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_config_5, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects mix of list and recursive type bodies -> %s", errSymMsg(ret));


    /* Test supported elements badly modified should be rejected */
    ret = asInitMem(unsupported_mod_1, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for ASG element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_2, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for HAG element-> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_3, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg list for RULE element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_4, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects bad arg count for ASG element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_5, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected name in arg list for RULE element -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_6, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected recursive body in HAG element body -> %s", errSymMsg(ret));

    ret = asInitMem(unsupported_mod_7, NULL);
    testOk(ret==S_asLib_badConfig, "parsing rejects unexpected recursive body in UAG element body -> %s", errSymMsg(ret));


    eltc(1);

    /* Test supported for known elements containing unsupported elements, well-formed and ignored */
    setUser("testing");
    setHost("localhost");

    ret = asInitMem(supported_config_1, NULL);
    testOk(ret==0, "unknown elements ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_2, NULL);
    testOk(ret==0, "unknown elements with body ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_3, NULL);
    testOk(ret==0, "unknown elements with string and double args and a body, ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_4, NULL);
    testOk(ret==0, "unknown elements with recursive body ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_5, NULL);
    testOk(ret==0, "unknown elements with recursive body with recursion ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_6, NULL);
    testOk(ret==0, "unknown elements with keywords arguments and body names ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ignored", 0);
        testAccess("ro", 1);
    }

    ret = asInitMem(supported_config_7, NULL);
    testOk(ret==0, "rules with unknown elements ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 0);
    }

    ret = asInitMem(supported_config_8, NULL);
    testOk(ret==0, "rules with unknown permission names ignored -> %s", errSymMsg(ret));
    if (!ret) {
        asAsl = 0;
        testAccess("DEFAULT", 0);
        testAccess("ro", 0);
    }
}

MAIN(aslibtest)
{
    testPlan(105);
    testRefreshInactive();
    testSyntaxErrors();
    testFutureProofParser();
    testHostNames();
    testUseIP();
    testHagRefresh();
    testHagEffectiveMapping();
    testStaticHagEntries();
    testHagRefreshConcurrency();
    errlogFlush();
    return testDone();
}
