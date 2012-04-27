/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>

#include "chfPlugin.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "epicsUnitTest.h"
#include "testMain.h"

#define PATTERN 0x55555555

/* Expected call bit definitions */
#define e_alloc         0x00000001
#define e_free          0x00000002
#define e_error         0x00000004
#define e_ok            0x00000008
#define e_open          0x00000010
#define e_reg_pre_cb    0x00000020
#define e_report        0x00000040
#define e_close         0x00000080

unsigned int e, c;

#define e_any (e_alloc | e_free | e_error | e_ok | e_open | e_reg_pre_cb| e_report | e_close)

typedef struct myStruct {
    int sent1;
    char        flag;
    int sent2;
    epicsUInt32 ival;
    int sent3;
    double      dval;
    int sent4;
    int         enumval;
    int sent5;
    char        str[20];
    int sent6;
    char        c;
    char        c1[2];
} myStruct;

static const
chfPluginEnumType colorEnum[] = { {"R", 1}, {"G", 2}, {"B", 4}, {NULL,0} };

static const
chfPluginArgDef strictOpts[] = {
    chfInt32  (myStruct, ival,    "i" , 1, 0),
    chfBoolean(myStruct, flag,    "f" , 1, 0),
    chfDouble (myStruct, dval,    "d" , 1, 0),
    chfString (myStruct, str,     "s" , 1, 0),
    chfEnum   (myStruct, enumval, "c" , 1, 0, colorEnum),
    chfPluginArgEnd
};

static const
chfPluginArgDef noconvOpts[] = {
    chfInt32  (myStruct, ival,    "i" , 0, 0),
    chfBoolean(myStruct, flag,    "f" , 0, 0),
    chfDouble (myStruct, dval,    "d" , 0, 0),
    chfString (myStruct, str,     "s" , 0, 0),
    chfEnum   (myStruct, enumval, "c" , 0, 0, colorEnum),
    chfPluginArgEnd
};

static const
chfPluginArgDef sloppyOpts[] = {
    chfInt32  (myStruct, ival,    "i" , 0, 1),
    chfBoolean(myStruct, flag,    "f" , 0, 1),
    chfDouble (myStruct, dval,    "d" , 0, 1),
    chfString (myStruct, str,     "s" , 0, 1),
    chfEnum   (myStruct, enumval, "c" , 0, 1, colorEnum),
    chfPluginArgEnd
};

/* Options defs with not enough room provided */
static const
chfPluginArgDef brokenOpts1[] = {
    chfInt32 (myStruct,  c, "i" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts2[] = {
    chfDouble(myStruct,  c, "d" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts3[] = {
    chfString(myStruct, c1, "s" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts4[] = {
    chfEnum  (myStruct,  c, "c" , 0, 1, colorEnum),
    chfPluginArgEnd
};

int p_ok_return = 0;
int c_open_return = 0;
void *puser;

static void clearStruct(void *p) {
    myStruct *my = (myStruct*) p;

    if (!my) return;
    memset(my, 0, sizeof(myStruct));
    my->sent1 = my->sent2 = my->sent3 = my->sent4 = my->sent5 = my->sent6 = PATTERN;
    my->ival = 12;
    my->flag = 1;
    my->dval = 1.234e5;
    strcpy(my->str, "hello");
    my->enumval = 4;
}

static void * allocPvt(void)
{
    testOk(e & e_alloc, "allocPvt called");
    c |= e_alloc;

    myStruct *my = puser = (myStruct*) calloc(1, sizeof(myStruct));
    clearStruct (my);
    return my;
}

static void freePvt(void *user)
{
    testOk(e & e_free, "freePvt called");
    c |= e_free;
    testOk(user == puser, "user pointer correct");

    free(user);
    puser = NULL;
}

static void parse_error(void)
{
    testOk(e & e_error, "parse_error called");
    c |= e_error;
}

static int parse_ok(void *user)
{
    testOk(e & e_ok, "parse_ok called");
    c |= e_ok;
    testOk(user == puser, "user pointer correct");

    return p_ok_return;
}

static long channel_open(dbChannel *chan, void *user)
{
    testOk(e & e_open, "channel_open called");
    c |= e_open;
    testOk(user == puser, "user pointer correct");

    return c_open_return;
}

static void channelRegisterPre(dbChannel *chan, void *user,
                               chPostEventFunc **cb_out, void **arg_out)
{
    myStruct *my = (myStruct*)user;
}

static void channelRegisterPost(dbChannel *chan, void *user,
                                chPostEventFunc **cb_out, void **arg_out)
{
    myStruct *my = (myStruct*)user;
}

static void channel_report(dbChannel *chan, void *user, const char *intro, int level)
{
    testOk(e & e_report, "channel_report called");
    c |= e_report;
    testOk(user == puser, "user pointer correct");
}

static void channel_close(dbChannel *chan, void *user)
{
    testOk(e & e_close, "channel_close called");
    c |= e_close;
    testOk(user == puser, "user pointer correct");
}

static chfPluginIf myPif = {
    allocPvt,
    freePvt,

    parse_error,
    parse_ok,

    channel_open,
    channelRegisterPre,
    channelRegisterPost,
    channel_report,
    channel_close
};

static int checkValues(myStruct *my, epicsUInt32 i, int f, double d, char *s, int c) {
    if (!my) return 0;
    if (my->sent1 == PATTERN && my->sent2 == PATTERN && my->sent3 == PATTERN
        && my->sent4 == PATTERN && my->sent5 == PATTERN && my->sent6 == PATTERN
        && my->ival == i && my->flag == f && my->dval == d && my->enumval == c
        && (strcmp(s, my->str) == 0)) {
        return 1;
    } else {
        return 0;
    }
}

MAIN(chfPluginTest)
{
    dbChannel *pch;

    testPlan(667);

    /* Enum to string conversion */
    testOk(strcmp(chfPluginEnumString(colorEnum, 1, "-"), "R") == 0, "Enum to string: R");
    testOk(strcmp(chfPluginEnumString(colorEnum, 2, "-"), "G") == 0, "Enum to string: G");
    testOk(strcmp(chfPluginEnumString(colorEnum, 4, "-"), "B") == 0, "Enum to string: B");
    testOk(strcmp(chfPluginEnumString(colorEnum, 3, "-"), "-") == 0, "Enum to string: invalid index");

    testOk1(!dbReadDatabase(&pdbbase, "dbChannelTest.dbx", ".:..", NULL));
    testOk(!!pdbbase, "pdbbase was set");

    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts1), "not enough storage for integer");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts2), "not enough storage for double");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts3), "not enough storage for string");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts4), "not enough storage for enum");

    testOk(!chfPluginRegister("strict", &myPif, strictOpts), "register plugin strict");
    testOk(!chfPluginRegister("noconv", &myPif, noconvOpts), "register plugin noconv");
    testOk(!chfPluginRegister("sloppy", &myPif, sloppyOpts), "register plugin sloppy");

    /* STRICT parsing: mandatory, no conversion */

    /* All perfect */
    e = e_alloc | e_ok; c = 0;
    testOk(!!(pch = dbChannelCreate("x.{\"strict\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")), "strict parsing: JSON correct");
    testOk(checkValues(puser, 1, 0, 1.2e15, "bar", 1), "guards intact, values correct");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_close | e_free; c = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

    /* Any one missing must fail */
    e = e_alloc | e_error | e_free; c = 0;
    testOk(!(pch = dbChannelCreate("x.{\"strict\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\"}}")), "strict parsing: c missing");
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_alloc | e_error | e_free; c = 0;
    testOk(!(pch = dbChannelCreate("x.{\"strict\":{\"f\":false,\"i\":1,\"d\":1.2e15,\"c\":\"R\"}}")), "strict parsing: s missing");
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_alloc | e_error | e_free; c = 0;
    testOk(!(pch = dbChannelCreate("x.{\"strict\":{\"i\":1,\"c\":\"R\",\"f\":false,\"s\":\"bar\"}}")), "strict parsing: d missing");
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_alloc | e_error | e_free; c = 0;
    testOk(!(pch = dbChannelCreate("x.{\"strict\":{\"d\":1.2e15,\"c\":\"R\",\"i\":1,\"s\":\"bar\"}}")), "strict parsing: f missing");
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_alloc | e_error | e_free; c = 0;
    testOk(!(pch = dbChannelCreate("x.{\"strict\":{\"c\":\"R\",\"s\":\"bar\",\"f\":false,\"d\":1.2e15}}")), "strict parsing: i missing");
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

    /* NOCONV parsing: optional, no conversion */

    /* Any one missing must leave the default intact */
    e = e_alloc | e_ok; c = 0;
    testOk(!!(pch = dbChannelCreate("x.{\"noconv\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\"}}")), "noconv parsing: c missing");
    testOk(checkValues(puser, 1, 0, 1.2e15, "bar", 4), "guards intact, values correct");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);
    e = e_close | e_free; c = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser, "user part cleaned up");
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

    e = e_any;
    testOk(!!(pch = dbChannelCreate("x.{\"noconv\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"c\":\"R\"}}")), "noconv parsing: s missing");
    testOk(checkValues(puser, 1, 0, 1.2e15, "hello", 1), "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate("x.{\"noconv\":{\"i\":1,\"f\":false,\"s\":\"bar\",\"c\":\"R\"}}")), "noconv parsing: d missing");
    testOk(checkValues(puser, 1, 0, 1.234e5, "bar", 1), "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate("x.{\"noconv\":{\"i\":1,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")), "noconv parsing: f missing");
    testOk(checkValues(puser, 1, 1, 1.2e15, "bar", 1), "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate("x.{\"noconv\":{\"f\":false,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")), "noconv parsing: i missing");
    testOk(checkValues(puser, 12, 0, 1.2e15, "bar", 1), "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    /* Reject wrong types */
#define WRONGTYPETEST(Var, Val, Typ) \
    e = e_alloc | e_error | e_free; c = 0; \
    testOk(!(pch = dbChannelCreate("x.{\"noconv\":{\""#Var"\":"#Val"}}")), "noconv parsing: wrong type "#Typ" for "#Var); \
    testOk(!puser, "user part cleaned up"); \
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

    WRONGTYPETEST(i, 123.0, double);
    WRONGTYPETEST(i, true, boolean);
    WRONGTYPETEST(i, "1", string);
    WRONGTYPETEST(f, "false", string);
    WRONGTYPETEST(f, 0.0, double);
    WRONGTYPETEST(f, 1, integer);
    WRONGTYPETEST(d, "1.2", string);
    WRONGTYPETEST(d, true, boolean);
    WRONGTYPETEST(d, 123, integer);
    WRONGTYPETEST(s, 1.23, double);
    WRONGTYPETEST(s, true, boolean);
    WRONGTYPETEST(s, 123, integer);
    WRONGTYPETEST(c, 1.23, double);
    WRONGTYPETEST(c, true, boolean);
    WRONGTYPETEST(c, 2, integer);

    /* SLOPPY parsing: optional, with conversion */

#define CONVTESTGOOD(Var, Val, Typ, Ival, Fval, Dval, Sval, Cval) \
    e = e_alloc | e_ok; c = 0; \
    testOk(!!(pch = dbChannelCreate("x.{\"sloppy\":{\""#Var"\":"#Val"}}")), "sloppy parsing: "#Typ" (good) for "#Var); \
    testOk(checkValues(puser, Ival, Fval, Dval, Sval, Cval), "guards intact, values correct"); \
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c); \
    e = e_close | e_free; c = 0; \
    if (pch) dbChannelDelete(pch); \
    testOk(!puser, "user part cleaned up"); \
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

#define CONVTESTBAD(Var, Val, Typ) \
    e = e_alloc | e_error | e_free; c = 0; \
    testOk(!(pch = dbChannelCreate("x.{\"sloppy\":{\""#Var"\":"#Val"}}")), "sloppy parsing: "#Typ" (bad) for "#Var); \
    testOk(!puser, "user part cleaned up"); \
    if (!testOk(c == e, "all expected calls happened")) testDiag("expected %#x - called %#x", e, c);

    /* To integer */
    CONVTESTGOOD(i, "123e4", positive string, 123, 1, 1.234e5, "hello", 4);
    CONVTESTGOOD(i, "-12345", negative string, -12345, 1, 1.234e5, "hello", 4);
    CONVTESTBAD(i, "9234567890", out-of-range string);
    CONVTESTBAD(i, ".4", invalid string);
    CONVTESTGOOD(i, false, valid boolean, 0, 1, 1.234e5, "hello", 4);
    CONVTESTGOOD(i, 3456.789, valid double, 3456, 1, 1.234e5, "hello", 4);
    CONVTESTBAD(i, 34.7e14, out-of-range double);

    /* To boolean */
    CONVTESTGOOD(f, "false", valid string, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, "False", capital valid string, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, "0", 0 string, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, "15", 15 string, 12, 1, 1.234e5, "hello", 4);
    CONVTESTBAD(f, ".4", invalid .4 string);
    CONVTESTBAD(f, "Flase", misspelled invalid string);
    CONVTESTGOOD(f, 0, zero integer, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, 12, positive integer, 12, 1, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, -1234, negative integer, 12, 1, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, 0.4, positive non-zero double, 12, 1, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, 0.0, zero double, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, -0.0, minus-zero double, 12, 0, 1.234e5, "hello", 4);
    CONVTESTGOOD(f, -1.24e14, negative double, 12, 1, 1.234e5, "hello", 4);

    /* To double */
    CONVTESTGOOD(d, "123e4", positive double string, 12, 1, 1.23e6, "hello", 4);
    CONVTESTGOOD(d, "-7.89e-14", negative double string, 12, 1, -7.89e-14, "hello", 4);
    CONVTESTGOOD(d, "123", positive integer string, 12, 1, 123.0, "hello", 4);
    CONVTESTGOOD(d, "-1234567", negative integer string, 12, 1, -1.234567e6, "hello", 4);
    CONVTESTBAD(d, "1.67e407", out-of-range double string);
    CONVTESTBAD(d, "blubb", invalid blubb string);
    CONVTESTGOOD(d, 123, positive integer, 12, 1, 123.0, "hello", 4);
    CONVTESTGOOD(d, -12345, negative integer, 12, 1, -1.2345e4, "hello", 4);
    CONVTESTGOOD(d, true, true boolean, 12, 1, 1.0, "hello", 4);
    CONVTESTGOOD(d, false, false boolean, 12, 1, 0.0, "hello", 4);

    /* To string */
    CONVTESTGOOD(s, 12345, positive integer, 12, 1, 1.234e5, "12345", 4);
    CONVTESTGOOD(s, -1234567891, negative integer, 12, 1, 1.234e5, "-1234567891", 4);
    CONVTESTGOOD(s, true, true boolean, 12, 1, 1.234e5, "true", 4);
    CONVTESTGOOD(s, false, false boolean, 12, 1, 1.234e5, "false", 4);
    CONVTESTGOOD(s, 123e4, small positive double, 12, 1, 1.234e5, "1230000", 4);
    CONVTESTGOOD(s, -123e24, negative double, 12, 1, 1.234e5, "-1.23e+26", 4);
    CONVTESTGOOD(s, -1.23456789123456789e26, large rounded negative double, 12, 1, 1.234e5, "-1.234567891235e+26", 4);

    /* To Enum */
    CONVTESTGOOD(c, 2, valid integer choice, 12, 1, 1.234e5, "hello", 2);
    CONVTESTBAD(c, 3, invalid integer choice);
    CONVTESTBAD(c, 3.2, double);
    CONVTESTGOOD(c, "R", valid string choice, 12, 1, 1.234e5, "hello", 1);
    CONVTESTBAD(c, "blubb", invalid string choice);

    dbFreeBase(pdbbase);

    return testDone();
}
