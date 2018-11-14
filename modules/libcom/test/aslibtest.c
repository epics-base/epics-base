/*************************************************************************\
* Copyright (c) 2018 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <testMain.h>
#include <epicsUnitTest.h>

#include <errSymTbl.h>
#include <epicsString.h>
#include <osiFileName.h>
#include <errlog.h>

#include <asLib.h>

static char *asUser,
            *asHost;
static int asAsl;

static void setUser(const char *name)
{
    free(asUser);
    asUser = epicsStrDup(name);
}

static void setHost(const char *name)
{
    free(asHost);
    asHost = epicsStrDup(name);
}

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

    eltc(0);
    ret = asInitMem(empty, NULL);
    testOk(ret==S_asLib_badConfig, "load \"empty\" config -> %s", errSymMsg(ret));
    eltc(1);
}
static const char hostname_config[] = ""
        "HAG(foo) {localhost}\n"
        "ASG(DEFAULT) {RULE(0, NONE)}\n"
        "ASG(ro) {RULE(0, NONE)RULE(1, READ) {HAG(foo)}}\n"
        "ASG(rw) {RULE(1, WRITE) {HAG(foo)}}\n"
        ;

static void testHostNames(void)
{

    testDiag("testHostNames()");

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

    setHost("nosuchhost");

    testAccess("invalid", 0);
    testAccess("DEFAULT", 0);
    testAccess("ro", 0);
    testAccess("rw", 0);
}
MAIN(aslibtest)
{
    testPlan(14);
    testSyntaxErrors();
    testHostNames();
    errlogFlush();
    return testDone();
}
