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
#include <epicsString.h>
#include <osiFileName.h>
#include <errlog.h>

#include <asLib.h>

static char *asUser,
            *asHost,
            *asMethod,
            *asAuthority;
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

static void setMethod(const char *name)
{
    free(asMethod);
    asMethod = epicsStrDup(name);
}

static void setAuthority(const char *name)
{
    free(asAuthority);
    asAuthority = epicsStrDup(name);
}

static void testAccess(const char *asg, unsigned mask)
{
    ASMEMBERPVT asp = 0; /* aka dbCommon::asp */
    ASCLIENTPVT client = 0;
    long ret;

    ret = asAddMember(&asp, asg);
    if(ret) {
        testFail("testAccess(ASG:%s, USER:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, ASL:%d) -> asAddMember error: %s",
                 asg, asUser, (asMethod?asMethod:""), (asAuthority?asAuthority:""), asHost, asAsl, errSymMsg(ret));
    } else {
        ret = asAddClientX(&client, asp, asAsl, asUser, (asMethod?asMethod:""), (asAuthority?asAuthority:""), asHost);
    }
    if(ret) {
        testFail("testAccess(ASG:%s, USER:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, ASL:%d) -> asAddClient error: %s",
                 asg, asUser, asHost, (asMethod?asMethod:""), (asAuthority?asAuthority:""), asAsl, errSymMsg(ret));
    } else {
        unsigned actual = 0;
        actual |= asCheckGet(client) ? 1 : 0;
        actual |= asCheckPut(client) ? 2 : 0;
        actual |= asCheckRPC(client) ? 4 : 0;
        testOk(actual==mask, "testAccess(ASG:%s, USER:%s, METHOD:%s, AUTHORITY:%s, HOST:%s, ASL:%d) -> %x == %x",
               asg, asUser, (asMethod?asMethod:""), (asAuthority?asAuthority:""), asHost, asAsl, actual, mask);
    }
    if(client) asRemoveClient(&client);
    if(asp) asRemoveMember(&asp);
}

static void testSyntaxErrors(void)
{
    static const char empty[]                         = "\n#almost empty file\n\n";
    static const char duplicateMethod[]               = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"        )  METHOD   (\"x509\"        )}}\n\n";
    static const char duplicateAuthority[]            = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org CA\")  AUTHORITY(\"Epics Org CA\")}}\n\n";
    static const char notDuplicateMethod[]            = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"        )} RULE(1, RPC            ) {METHOD   (\"x509\"        )}}\n\n";
    static const char notDuplicateAuthority[]         = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org CA\")} RULE(1, WRITE,TRAPWRITE) {AUTHORITY(\"Epics Org CA\")}}\n\n";
    static const char anotherNotDuplicatedMethod[]    = "\nASG(foo) {RULE(0, NONE) {METHOD   (\"x509\"        )  METHOD   (\"ca\"          )}}\n\n";
    static const char anotherNotDuplicatedAuthority[] = "\nASG(foo) {RULE(0, NONE) {AUTHORITY(\"Epics Org CA\")  AUTHORITY(\"ORNL CA\"     )}}\n\n";
    long ret;

    testDiag("testSyntaxErrors()");

    eltc(0);
    ret = asInitMem(empty, NULL);
    testOk(ret==S_asLib_badConfig, "load \"empty\" config -> %s", errSymMsg(ret));

    ret = asInitMem(duplicateMethod, NULL);
    testOk(ret==S_asLib_badConfig, "load \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(duplicateAuthority, NULL);
    testOk(ret==S_asLib_badConfig, "load \"duplicate authority rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(notDuplicateMethod, NULL);
    testOk(ret==0, "load non \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(notDuplicateAuthority, NULL);
    testOk(ret==0, "load non \"duplicate authority rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(anotherNotDuplicatedMethod, NULL);
    testOk(ret==0, "load another non \"duplicate method rule\" config -> %s", errSymMsg(ret));

    ret = asInitMem(anotherNotDuplicatedAuthority, NULL);
    testOk(ret==0, "load another non \"duplicate authority rule\" config -> %s", errSymMsg(ret));
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

static const char method_auth_config[] = ""
        "UAG(foo) {\"testing\"}\n"
        "UAG(bar) {\"boss\"}\n"
        "UAG(ops) {\"geek\"}\n"
        "ASG(DEFAULT) {RULE(0, NONE)}\n"
        "ASG(ro) {RULE(0, NONE) RULE(1, READ) {UAG(foo) UAG(ops) METHOD(\"ca\")}}\n"
        "ASG(rw) {RULE(0, NONE) RULE(1, WRITE, TRAPWRITE) {UAG(foo) METHOD(\"x509\") AUTHORITY(\"Epics Org CA\")}}\n"
        "ASG(rwx) {RULE(0, NONE) RULE(1, RPC) {UAG(bar) METHOD(\"x509\", \"ignored\") METHOD(\"ignored_too\") AUTHORITY(\"Epics Org CA\", \"ignored\") AUTHORITY(\"ORNL Org CA\")}}\n"
        ;

static void testMethodAndAuth(void)
{
    testDiag("testMethodAndAuth()");
    asCheckClientIP = 0;

    testOk1(asInitMem(method_auth_config, NULL)==0);

    asAsl = 0;
    testAccess("DEFAULT", 0);

    setHost("localhost");
    setUser("boss");
    setMethod("ca");

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setUser("testing");

    testAccess("ro", 1);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setMethod("x509");

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setAuthority("Epics Org CA");

    testAccess("ro", 0);
    testAccess("rw", 3);
    testAccess("rwx", 0);

    setAuthority("ORNL Org CA");
    testAccess("ro", 0);
    testAccess("rw", 0);

    setUser("boss");
    testAccess("rwx", 7);
}

static const char method_auth_config_yaml[] = ""
"# EPICS YAML\n"
"version: 1.0\n"
"\n"
"# user access groups\n"
"uags:\n"
"  - name: foo\n"
"    users:\n"
"      - testing\n"
"  - name: bar\n"
"    users:\n"
"      - boss\n"
"  - name: ops\n"
"    users:\n"
"      - geek\n"
"\n"
"# host access groups\n"
"hags:\n"
"  - name: local\n"
"    hosts:\n"
"      - 127.0.0.1\n"
"      - localhost\n"
"      - 192.168.0.11\n"
"  - name: admin\n"
"    hosts:\n"
"      - admin.intranet.com\n"
"\n"
"# Access security group definitions\n"
"asgs:\n"
"  # no access by default\n"
"  - name: DEFAULT\n"
"    rules: \n"
"      - level: 0\n"
"        access: NONE\n"
"        trapwrite: false\n"
"\n"
"  # read only access for non-secure connections for foo and ops\n"
"  - name: ro\n"
"    rules: \n"
"      - level: 0\n"
"        access: NONE\n"
"      - level: 1\n"
"        access: READ\n"
"        trapwrite: false\n"
"        uags:\n"
"          - foo\n"
"          - ops\n"
"        methods:\n"
"          - ca\n"
"\n"
"  # read write access for foo with a secure connection authenticated by Epics Org CA\n"
"  - name: rw\n"
"    links:\n"
"      - INPA: ACC-CT{}Prmt:Remote-Sel\n"
"      - INPB: ACC-CT{}Prmt:Remote-Sel\n"
"    rules:\n"
"      - level: 0\n"
"        access: NONE\n"
"      - level: 1\n"
"        access: WRITE\n"
"        trapwrite: true\n"
//"        calc: VAL>=0\n"
"        uags:\n"
"          - foo\n"
"        methods:\n"
"          - x509\n"
"        authorities:\n"
"          - Epics Org CA\n"
"\n"
"  # RPC access for localhost user bar with a secure EPICS Org CA authenticated connection\n"
"  - name: rwx\n"
"    rules:\n"
"      - level: 0\n"
"        access: NONE\n"
"      - level: 1\n"
"        access: RPC\n"
"        trapwrite: true\n"
"        uags:\n"
"          - bar\n"
"        hags:\n"
"          - local\n"
"        methods:\n"
"          - x509\n"
"          - ignored\n"
"          - ignored_too\n"
"        authorities:\n"
"          - Epics Org CA\n"
"          - ORNL Org CA\n"
;

static void testMethodAndAuthYaml(void)
{
    testDiag("testMethodAndAuthYaml()");
    asCheckClientIP = 0;

    testOk1(asInitMem(method_auth_config_yaml, NULL)==0);

    asAsl = 0;
    setHost("localhost");
    setUser("foobar");
    setMethod("ca");

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setUser("testing");

    testAccess("ro", 1);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setMethod("x509");

    testAccess("ro", 0);
    testAccess("rw", 0);
    testAccess("rwx", 0);

    setAuthority("Epics Org CA");

    testAccess("ro", 0);
    testAccess("rw", 3);
    testAccess("rwx", 0);

    setAuthority("Epics Org CA");

    testAccess("ro", 0);
    testAccess("rw", 3);

    setUser("boss");
    testAccess("rwx", 7);
}

static const char *expected_method_auth_config =
  "UAG(bar) {boss}\n"
  "UAG(foo) {testing}\n"
  "UAG(ops) {geek}\n"
  "ASG(DEFAULT) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "}\n"
  "ASG(ro) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,READ,NOTRAPWRITE) {\n"
  "\t\tUAG(foo,ops)\n"
  "\t\tMETHOD(\"ca\")\n"
  "\t}\n"
  "}\n"
  "ASG(rw) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,WRITE,TRAPWRITE) {\n"
  "\t\tUAG(foo)\n"
  "\t\tMETHOD(\"x509\")\n"
  "\t\tAUTHORITY(\"Epics Org CA\")\n"
  "\t}\n"
  "}\n"
  "ASG(rwx) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,RPC,NOTRAPWRITE) {\n"
  "\t\tUAG(bar)\n"
  "\t\tMETHOD(\"x509\",\"ignored\",\"ignored_too\")\n"
  "\t\tAUTHORITY(\"Epics Org CA\",\"ignored\",\"ORNL Org CA\")\n"
  "\t}\n"
  "}\n";

static void testDumpOutput(void)
{
    testDiag("testDumpOutput()");
    testOk1(asInitMem(method_auth_config, NULL)==0);

    char *buf = NULL;
    size_t size = 0;
    FILE *fp = open_memstream(&buf, &size);

    testOk(fp != NULL, "Created memory stream");
    if (!fp) return;

    asDumpFP(fp, NULL, NULL, 0);
    fclose(fp);

    testOk(strcmp(expected_method_auth_config, buf) == 0,
           "asDumpFP output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_method_auth_config, buf);

    free(buf);
}
static const char *expected_DEFAULT_rules_config =
  "ASG(DEFAULT) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "}\n";

static const char *expected_ro_rules_config =
  "ASG(ro) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,READ,NOTRAPWRITE) {\n"
  "\t\tUAG(foo,ops)\n"
  "\t\tMETHOD(\"ca\")\n"
  "\t}\n"
  "}\n";

static const char *expected_rw_rules_config =
  "ASG(rw) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,WRITE,TRAPWRITE) {\n"
  "\t\tUAG(foo)\n"
  "\t\tMETHOD(\"x509\")\n"
  "\t\tAUTHORITY(\"Epics Org CA\")\n"
  "\t}\n"
  "}\n";

static const char *expected_rwx_rules_config =
  "ASG(rwx) {\n"
  "\tRULE(0,NONE,NOTRAPWRITE)\n"
  "\tRULE(1,RPC,NOTRAPWRITE) {\n"
  "\t\tUAG(bar)\n"
  "\t\tMETHOD(\"x509\",\"ignored\",\"ignored_too\")\n"
  "\t\tAUTHORITY(\"Epics Org CA\",\"ignored\",\"ORNL Org CA\")\n"
  "\t}\n"
  "}\n";

static void testRulesDumpOutput(void)
{
    testDiag("testRulesDumpOutput()");
    testOk1(asInitMem(method_auth_config, NULL)==0);

    char *buf = NULL;
    size_t size = 0;

    // DEFAULT
    FILE *fp = open_memstream(&buf, &size);
    testOk(fp != NULL, "Created DEFAULT memory stream");
    if (!fp) return;
    asDumpRulesFP(fp, "DEFAULT");
    fclose(fp);
    testOk(strcmp(expected_DEFAULT_rules_config, buf) == 0,
           "asDumpFP DEFAULT output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_DEFAULT_rules_config, buf);
    free(buf);

    // ro
    fp = open_memstream(&buf, &size);
    testOk(fp != NULL, "Created ro memory stream");
    if (!fp) return;
    asDumpRulesFP(fp, "ro");
    fclose(fp);
    testOk(strcmp(expected_ro_rules_config, buf) == 0,
           "asDumpFP ro output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_ro_rules_config, buf);
    free(buf);

    // rw
    fp = open_memstream(&buf, &size);
    testOk(fp != NULL, "Created rw memory stream");
    if (!fp) return;
    asDumpRulesFP(fp, "rw");
    fclose(fp);
    testOk(strcmp(expected_rw_rules_config, buf) == 0,
           "asDumpFP rw output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_rw_rules_config, buf);
    free(buf);

    // rwx
    fp = open_memstream(&buf, &size);
    testOk(fp != NULL, "Created rwx memory stream");
    if (!fp) return;
    asDumpRulesFP(fp, "rwx");
    fclose(fp);
    testOk(strcmp(expected_rwx_rules_config, buf) == 0,
           "asDumpFP rwx output matches expected\nExpected:\n%s\nGot:\n%s",
           expected_rwx_rules_config, buf);
    free(buf);
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

MAIN(aslibtest)
{
    testPlan(78);
    testSyntaxErrors();
    testHostNames();
    testMethodAndAuth();
    testMethodAndAuthYaml();
    testDumpOutput();
    testRulesDumpOutput();
    testUseIP();
    errlogFlush();
    return testDone();
}
