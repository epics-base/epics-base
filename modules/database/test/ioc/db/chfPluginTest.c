/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     für Materialien und Energie GmbH.
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <string.h>
#include <stdio.h>

#include "chfPlugin.h"
#include "dbStaticLib.h"
#include "dbAccessDefs.h"
#include "errlog.h"
#include "registry.h"
#include "epicsUnitTest.h"
#include "testMain.h"
#include "osiFileName.h"

#define PATTERN 0x55555555
#define TYPE_START 0xAAA
#define R_LEVEL 42

/* Expected / actually run callback bit definitions */
#define e_alloc         0x00000001
#define e_free          0x00000002
#define e_error         0x00000004
#define e_ok            0x00000008
#define e_open          0x00000010
#define e_reg_pre       0x00000020
#define e_reg_post      0x00000040
#define e_report        0x00000080
#define e_close         0x00000100
#define e_pre           0x00000200
#define e_post          0x00000400
#define e_dtor          0x00000800

unsigned int e1, e2, c1, c2;
unsigned int offset;
int drop = -1;
db_field_log *dtorpfl;

#define e_any (e_alloc | e_free | e_error | e_ok | e_open \
| e_reg_pre | e_reg_post | e_pre | e_post | e_dtor | e_report | e_close)

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
    epicsUInt32 tval;
    int sent7;
    char        c;
    char        c1[2];
    int         offpre;
    int         offpost;
} myStruct;

static const
chfPluginEnumType colorEnum[] = { {"R", 1}, {"G", 2}, {"B", 4}, {NULL,0} };

static const
chfPluginArgDef sloppyTaggedOpts[] = {
    chfInt32     (myStruct, tval,    "t" , 0, 0),
    chfTagInt32  (myStruct, ival,    "I" , tval, 1, 0, 0),
    chfTagBoolean(myStruct, flag,    "F" , tval, 2, 0, 0),
    chfTagDouble (myStruct, dval,    "D" , tval, 3, 0, 0),
    chfTagString (myStruct, str,     "S" , tval, 4, 0, 0),
    chfTagEnum   (myStruct, enumval, "C" , tval, 5, 0, 0, colorEnum),
    chfPluginArgEnd
};

static const
chfPluginArgDef strictTaggedOpts[] = {
    chfInt32     (myStruct, tval,    "t" , 1, 0),
    chfBoolean   (myStruct, flag,    "f" , 1, 0),
    chfTagInt32  (myStruct, ival,    "I" , tval, 1, 0, 0),
    chfTagBoolean(myStruct, flag,    "F" , tval, 2, 0, 0),
    chfTagDouble (myStruct, dval,    "D" , tval, 3, 1, 0),
    chfTagDouble (myStruct, dval,    "D2", tval, 4, 1, 0),
    chfTagEnum   (myStruct, enumval, "C" , tval, 5, 0, 0, colorEnum),
    chfPluginArgEnd
};

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
    chfInt32 (myStruct,  c1, "i" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts2[] = {
    chfDouble(myStruct,  c1, "d" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts3[] = {
    chfString(myStruct, c1, "s" , 0, 1),
    chfPluginArgEnd
};

static const
chfPluginArgDef brokenOpts4[] = {
    chfEnum  (myStruct,  c1, "c" , 0, 1, colorEnum),
    chfPluginArgEnd
};

int p_ok_return = 0;
int c_open_return = 0;
void *puser1, *puser2;

static void clearStruct(void *p) {
    myStruct *my = (myStruct*) p;

    if (!my) return;
    memset(my, 0, sizeof(myStruct));
    my->sent1 = my->sent2 = my->sent3 = my->sent4 =
        my->sent5 = my->sent6 = my->sent7 = PATTERN;
    my->ival = 12;
    my->tval = 99;
    my->flag = 1;
    my->dval = 1.234e5;
    strcpy(my->str, "hello");
    my->enumval = 4;
}

static char inst(void* user) {
    return user == puser1 ? '1' : user == puser2 ? '2' : 'x';
}

static void * allocPvt(void)
{
    myStruct *my = (myStruct*) calloc(1, sizeof(myStruct));

    if (!puser1) {
        puser1 = my;
        testOk(e1 & e_alloc, "allocPvt (1) called");
        c1 |= e_alloc;
    } else if (!puser2) {
        puser2 = my;
        testOk(e2 & e_alloc, "allocPvt (2) called");
        c2 |= e_alloc;
    }
    clearStruct (my);
    return my;
}

static void * allocPvtFail(void)
{
    if (!puser1) {
        testOk(e1 & e_alloc, "allocPvt (1) called");
        c1 |= e_alloc;
    }
    return NULL;
}

static void freePvt(void *user)
{
    if (user == puser1) {
        testOk(e1 & e_free, "freePvt (1) called");
        c1 |= e_free;
        free(user);
        puser1 = NULL;
    } else if (user == puser2) {
        testOk(e2 & e_free, "freePvt (2) called");
        c2 |= e_free;
        free(user);
        puser2 = NULL;
    } else
        testFail("freePvt: user pointer invalid");
}

static void parse_error(void *user)
{
    if (user == puser1) {
        testOk(e1 & e_error, "parse_error (1) called");
        c1 |= e_error;
    } else if (user == puser2) {
        testOk(e2 & e_error, "parse_error (2) called");
        c2 |= e_error;
    } else
        testFail("parse_error: user pointer invalid");
}

static int parse_ok(void *user)
{
    if (user == puser1) {
        testOk(e1 & e_ok, "parse_ok (1) called");
        c1 |= e_ok;
    } else if (user == puser2) {
        testOk(e2 & e_ok, "parse_ok (2) called");
        c2 |= e_ok;
    } else
        testFail("parse_ok: user pointer invalid");

    return p_ok_return;
}

static long channel_open(dbChannel *chan, void *user)
{
    if (user == puser1) {
        testOk(e1 & e_open, "channel_open (1) called");
        c1 |= e_open;
    } else if (user == puser2) {
        testOk(e2 & e_open, "channel_open (2) called");
        c2 |= e_open;
    } else
        testFail("channel_open: user pointer invalid");

    return c_open_return;
}

static void dbfl_free1(db_field_log *pfl) {
    testOk(e1 & e_dtor, "dbfl_free (1) called");
    testOk(dtorpfl == pfl, "dbfl_free (1): db_field_log pointer correct");
    dtorpfl = NULL;
    c1 |= e_dtor;
}

static void dbfl_free2(db_field_log *pfl) {
    testOk(e2 & e_dtor, "dbfl_free (2) called");
    testOk(dtorpfl == pfl, "dbfl_free (2): db_field_log pointer correct");
    dtorpfl = NULL;
    c2 |= e_dtor;
}

static db_field_log * pre(void *user, dbChannel *chan, db_field_log *pLog) {
    myStruct *my = (myStruct*)user;
    dbfl_freeFunc *dtor = NULL;

    if (my == puser1) {
        testOk(e1 & e_pre, "pre (1) called");
        testOk(!(c2 & e_pre),
            "pre (2) was not called before pre (1)");
        c1 |= e_pre;
        dtor = dbfl_free1;
    } else if (my == puser2) {
        testOk(e2 & e_pre, "pre (2) called");
        testOk(!(e1 & e_pre) || c1 & e_pre,
            "pre (1) was called before pre (2)");
        c2 |= e_pre;
        dtor = dbfl_free2;
    } else {
        testFail("pre: user pointer invalid");
        testSkip(1, "Can't check order of pre(1)/pre(2)");
    }
    testOk(!(c1 & e_post),
        "post (1) was not called before pre (%c)", inst(user));
    testOk(!(c2 & e_post),
        "post (2) was not called before pre (%c)", inst(user));

    if (!testOk(pLog->field_type == TYPE_START + my->offpre,
            "pre (%c) got field log of expected type", inst(user)))
        testDiag("expected: %d, got %d",
            TYPE_START + my->offpre, pLog->field_type);
    pLog->field_type++;

    if (my->offpre == 0) {  /* The first one registers a dtor and saves pfl */
        pLog->u.r.dtor = dtor;
        dtorpfl = pLog;
    }

    if (my->offpre == drop) {
        testDiag("pre (%c) is dropping the field log", inst(user));
        return NULL;
    }
    return pLog;
}

static db_field_log * post(void *user, dbChannel *chan, db_field_log *pLog) {
    myStruct *my = (myStruct*)user;
    dbfl_freeFunc *dtor = NULL;

    if (my == puser1) {
        testOk(e1 & e_post, "post (1) called");
        testOk(!(c2 & e_post),
            "post (2) was not called before post (1)");
        c1 |= e_post;
        dtor = dbfl_free1;
    } else if (my == puser2) {
        testOk(e2 & e_post, "post (2) called");
        testOk(!(e1 & e_post) || c1 & e_post,
            "post (1) was called before post (2)");
        c2 |= e_post;
        dtor = dbfl_free2;
    } else {
        testFail("post: user pointer invalid");
        testSkip(1, "Can't check order of post(1)/post(2)");
    }
    testOk(!(e1 & e_pre) || c1 & e_pre,
        "pre (1) was called before post (%c)", inst(user));
    testOk(!(e2 & e_pre) || c2 & e_pre,
        "pre (2) was called before post (%c)", inst(user));

    if (!testOk(pLog->field_type == TYPE_START + my->offpost,
            "post (%c) got field log of expected type", inst(user)))
        testDiag("expected: %d, got %d",
            TYPE_START + my->offpost, pLog->field_type);
    pLog->field_type++;

    if (my->offpost == 0) { /* The first one registers a dtor and saves pfl */
        pLog->u.r.dtor = dtor;
        dtorpfl = pLog;
    }

    if (my->offpost == drop) {
        testDiag("post (%c) is dropping the field log", inst(user));
        return NULL;
    }
    return pLog;
}

static void channelRegisterPre(dbChannel *chan, void *user,
    chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    myStruct *my = (myStruct*)user;

    if (my == puser1) {
        testOk(e1 & e_reg_pre, "register_pre (1) called");
        testOk(!(c2 & e_reg_pre),
            "register_pre (2) was not called before register_pre (1)");
        c1 |= e_reg_pre;
    } else if (my == puser2) {
        testOk(e2 & e_reg_pre, "register_pre (2) called");
        testOk(!(e1 & e_reg_pre) || c1 & e_reg_pre,
            "register_pre (1) was called before register_pre (2)");
        c2 |= e_reg_pre;
    } else {
        testFail("register_pre: user pointer invalid");
        testSkip(1, "Can't check order of register_pre(1)/register_pre(2)");
    }
    testOk(!(c1 & e_reg_post),
        "register_post (1) was not called before register_pre (%c)", inst(user));
    testOk(!(c2 & e_reg_post),
        "register_post (2) was not called before register_pre (%c)", inst(user));

    my->offpre = offset++;
    probe->field_type++;
    *cb_out  = pre;
    *arg_out = user;
}

static void channelRegisterPost(dbChannel *chan, void *user,
    chPostEventFunc **cb_out, void **arg_out, db_field_log *probe)
{
    myStruct *my = (myStruct*)user;

    if (my == puser1) {
        testOk(e1 & e_reg_post, "register_post (1) called");
        testOk(!(c2 & e_reg_post),
            "register_post (2) was not called before register_post (1)");
        c1 |= e_reg_post;
    } else if (my == puser2) {
        testOk(e2 & e_reg_post, "register_post (2) called");
        testOk(!(e1 & e_reg_post) || c1 & e_reg_post,
            "register_post (1) was called before register_post (2)");
        c2 |= e_reg_post;
    } else {
        testFail("register_post: user pointer invalid");
        testSkip(1, "Can't check order of register_post(1)/register_post(2)");
    }
    testOk(!(e1 & e_reg_pre) || c1 & e_reg_pre,
        "register_pre (1) was called before register_post (%c)", inst(user));
    testOk(!(e2 & e_reg_pre) || c2 & e_reg_pre,
        "register_pre (2) was called before register_post (%c)", inst(user));

    my->offpost = offset++;
    probe->field_type++;
    *cb_out  = post;
    *arg_out = user;
}

static void channel_report(dbChannel *chan, void *user, int level,
    const unsigned short indent)
{
    testOk(level == R_LEVEL - 2, "channel_report: level correct %u == %u", level, R_LEVEL-2);
    if (user == puser1) {
        testOk(e1 & e_report, "channel_report (1) called");
        c1 |= e_report;
    } else if (user == puser2) {
        testOk(e2 & e_report, "channel_report (2) called");
        c2 |= e_report;
    } else
        testFail("channel_report: user pointer invalid");
}

static void channel_close(dbChannel *chan, void *user)
{
    if (user == puser1) {
        testOk(e1 & e_close, "channel_close (1) called");
        c1 |= e_close;
    } else if (user == puser2) {
        testOk(e2 & e_close, "channel_close (2) called");
        c2 |= e_close;
    } else
        testFail("channel_close: user pointer invalid");
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

static chfPluginIf prePif = {
    allocPvt,
    freePvt,

    parse_error,
    parse_ok,

    channel_open,
    channelRegisterPre,
    NULL,
    channel_report,
    channel_close
};

static chfPluginIf postPif = {
    allocPvt,
    freePvt,

    parse_error,
    parse_ok,

    channel_open,
    NULL,
    channelRegisterPost,
    channel_report,
    channel_close
};

static chfPluginIf allocFailPif = {
    allocPvtFail,
    freePvt,

    parse_error,
    parse_ok,

    channel_open,
    channelRegisterPre,
    channelRegisterPost,
    channel_report,
    channel_close
};

static int checkValues(myStruct *my,
    char t, epicsUInt32 i, int f, double d, char *s1, char *s2, int c) {
    int ret = 1;
    int s1fail, s2fail;
    int s2valid = (s2 && s2[0] != '\0');

    if (!my) return 0;
#define CHK(A,B,FMT) if((A)!=(B)) {testDiag("Fail: " #A " (" FMT ") != " #B " (" FMT")", A, B); ret=0;}
    CHK(my->sent1, PATTERN, "%08x")
    CHK(my->sent2, PATTERN, "%08x")
    CHK(my->sent3, PATTERN, "%08x")
    CHK(my->sent4, PATTERN, "%08x")
    CHK(my->sent5, PATTERN, "%08x")
    CHK(my->sent6, PATTERN, "%08x")
    CHK(my->sent7, PATTERN, "%08x")
    CHK(my->tval, t, "%08x")
    CHK(my->ival, i, "%08x")
    CHK(my->flag, f, "%02x")
    CHK(my->dval, d, "%f")
    CHK(my->enumval, c, "%d")
#undef CHK
    s2fail = s1fail = strcmp(s1, my->str);
    if (s2valid) s2fail = strcmp(s2, my->str);
    if (s1fail && s2fail) {
        if (s1fail)            testDiag("Fail: my->str (%s) != s (%s)", my->str, s1);
        if (s2valid && s2fail) testDiag("Fail: my->str (%s) != s (%s)", my->str, s2);
        ret = 0;
    }
    return ret;
}

static void testHead (char* title) {
    testDiag("--------------------------------------------------------");
    testDiag("%s", title);
    testDiag("--------------------------------------------------------");
}

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

MAIN(chfPluginTest)
{
    dbChannel *pch;
    db_field_log *pfl;

#ifdef _WIN32
#if (defined(_MSC_VER) && _MSC_VER < 1900) || \
    (defined(_MINGW) && defined(_TWO_DIGIT_EXPONENT))
    _set_output_format(_TWO_DIGIT_EXPONENT);
#endif
#endif

    testPlan(1433);

    dbChannelInit();
    db_init_events();

    /* Enum to string conversion */
    testHead("Enum to string conversion");
    testOk(strcmp(chfPluginEnumString(colorEnum, 1, "-"), "R") == 0,
        "Enum to string: R");
    testOk(strcmp(chfPluginEnumString(colorEnum, 2, "-"), "G") == 0,
        "Enum to string: G");
    testOk(strcmp(chfPluginEnumString(colorEnum, 4, "-"), "B") == 0,
        "Enum to string: B");
    testOk(strcmp(chfPluginEnumString(colorEnum, 3, "-"), "-") == 0,
        "Enum to string: invalid index");

    if (dbReadDatabase(&pdbbase, "dbTestIoc.dbd",
            "." OSI_PATH_LIST_SEPARATOR ".." OSI_PATH_LIST_SEPARATOR
            "../O.Common" OSI_PATH_LIST_SEPARATOR "O.Common", NULL))
        testAbort("Database description 'dbTestIoc.dbd' not found");

    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    if (dbReadDatabase(&pdbbase, "xRecord.db",
            "." OSI_PATH_LIST_SEPARATOR "..", NULL))
        testAbort("Test database 'xRecord.db' not found");

    testHead("Try to register buggy plugins");
    eltc(0);
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts1),
        "not enough storage for integer");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts2),
        "not enough storage for double");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts3),
        "not enough storage for string");
    testOk(!!chfPluginRegister("buggy", &myPif, brokenOpts4),
        "not enough storage for enum");
    errlogFlush();
    eltc(1);

    testHead("Register plugins");
    testOk(!chfPluginRegister("sloppy-tagged", &myPif, sloppyTaggedOpts),
        "register plugin sloppy-tagged");
    testOk(!chfPluginRegister("strict-tagged", &myPif, strictTaggedOpts),
        "register plugin strict-tagged");
    testOk(!chfPluginRegister("strict", &myPif, strictOpts),
        "register plugin strict");
    testOk(!chfPluginRegister("noconv", &myPif, noconvOpts),
        "register plugin noconv");
    testOk(!chfPluginRegister("sloppy", &myPif, sloppyOpts),
        "register plugin sloppy");
    testOk(!chfPluginRegister("pre",   &prePif, sloppyOpts),
        "register plugin pre");
    testOk(!chfPluginRegister("post", &postPif, sloppyOpts),
        "register plugin post");
    testOk(!chfPluginRegister("alloc-fail", &allocFailPif, sloppyOpts),
        "register plugin alloc-fail");

    /* Check failing allocation of plugin private structures */

    testHead("Failing allocation of plugin private structures");
    /* tag i */
    e1 = e_alloc; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"alloc-fail\":{\"i\":1}}")),
        "create channel for alloc-fail: allocPvt returning NULL");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    /* TAGGED parsing: shorthand for integer plus other parameter */

    /* STRICT TAGGED parsing: mandatory, no conversions */

    /* All perfect */
    testHead("STRICT TAGGED parsing: all ok");
    /* tag D (t and d) and f */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"strict-tagged\":{\"D\":1.2e15,\"f\":false}}")),
        "create channel for strict-tagged parsing: D (t and d) and f");
    testOk(checkValues(puser1, 3, 12, 0, 1.2e15, "hello", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag D2 (t and d) and f */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"strict-tagged\":{\"D2\":1.2e15,\"f\":false}}")),
        "create channel for strict-tagged parsing: D2 (t and d) and f");
    testOk(checkValues(puser1, 4, 12, 0, 1.2e15, "hello", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag F: (t and f), d missing) */
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict-tagged\":{\"F\":false}}")),
        "create channel for strict-tagged parsing: F (t and f), d missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag I: (t and i) and f, d missing) */
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict-tagged\":{\"I\":1,\"f\":false}}")),
        "create channel for strict-tagged parsing: I (t and i) and f, d missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    /* SLOPPY TAGGED parsing: optional, all others have defaults */

    testHead("SLOPPY TAGGED parsing: all ok");

    /* tag i */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"sloppy-tagged\":{\"I\":1}}")),
        "create channel for sloppy-tagged parsing: I");
    testOk(checkValues(puser1, 1, 1, 1, 1.234e5, "hello", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag f */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"sloppy-tagged\":{\"F\":false}}")),
        "create channel for sloppy-tagged parsing: F");
    testOk(checkValues(puser1, 2, 12, 0, 1.234e5, "hello", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag d */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"sloppy-tagged\":{\"D\":1.2e15}}")),
        "create channel for sloppy-tagged parsing: D");
    testOk(checkValues(puser1, 3, 12, 1, 1.2e15, "hello", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag s */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"sloppy-tagged\":{\"S\":\"bar\"}}")),
        "create channel for sloppy-tagged parsing: S");
    testOk(checkValues(puser1, 4, 12, 1, 1.234e5, "bar", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    /* tag c */
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"sloppy-tagged\":{\"C\":\"R\"}}")),
        "create channel for sloppy-tagged parsing: C");
    testOk(checkValues(puser1, 5, 12, 1, 1.234e5, "hello", 0, 1),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    /* STRICT parsing: mandatory, no conversion */

    /* All perfect */
    testHead("STRICT parsing: all ok");
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate("x.{\"strict\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")),
           "create channel for strict parsing: JSON correct");
    testOk(checkValues(puser1, 99, 1, 0, 1.2e15, "bar", 0, 1),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    /* Any one missing must fail */
    testHead("STRICT parsing: any missing parameter must fail");
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\"}}")),
        "create channel for strict parsing: c missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict\":{\"f\":false,\"i\":1,\"d\":1.2e15,\"c\":\"R\"}}")),
        "create channel for strict parsing: s missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict\":{\"i\":1,\"c\":\"R\",\"f\":false,\"s\":\"bar\"}}")),
        "create channel for strict parsing: d missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict\":{\"d\":1.2e15,\"c\":\"R\",\"i\":1,\"s\":\"bar\"}}")),
        "create channel for strict parsing: f missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_alloc | e_error | e_free; c1 = 0;
    testOk(!(pch = dbChannelCreate(
        "x.{\"strict\":{\"c\":\"R\",\"s\":\"bar\",\"f\":false,\"d\":1.2e15}}")),
        "create channel for strict parsing: i missing");
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    /* NOCONV parsing: optional, no conversion */

    /* Any one missing must leave the default intact */
    testHead("NOCONV parsing: missing parameters get default value");
    e1 = e_alloc | e_ok; c1 = 0;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"noconv\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"s\":\"bar\"}}")),
        "create channel for noconv parsing: c missing");
    testOk(checkValues(puser1, 99, 1, 0, 1.2e15, "bar", 0, 4),
        "guards intact, values correct");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);
    e1 = e_close | e_free; c1 = 0;
    if (pch) dbChannelDelete(pch);
    testOk(!puser1, "user part cleaned up");
    if (!testOk(c1 == e1, "all expected calls happened"))
        testDiag("expected %#x - called %#x", e1, c1);

    e1 = e_any;
    testOk(!!(pch = dbChannelCreate(
        "x.{\"noconv\":{\"i\":1,\"f\":false,\"d\":1.2e15,\"c\":\"R\"}}")),
        "create channel for noconv parsing: s missing");
    testOk(checkValues(puser1, 99, 1, 0, 1.2e15, "hello", 0, 1),
        "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate(
        "x.{\"noconv\":{\"i\":1,\"f\":false,\"s\":\"bar\",\"c\":\"R\"}}")),
        "create channel for noconv parsing: d missing");
    testOk(checkValues(puser1, 99, 1, 0, 1.234e5, "bar", 0, 1),
        "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate(
        "x.{\"noconv\":{\"i\":1,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")),
        "create channel for noconv parsing: f missing");
    testOk(checkValues(puser1, 99, 1, 1, 1.2e15, "bar", 0, 1),
        "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    testOk(!!(pch = dbChannelCreate(
        "x.{\"noconv\":{\"f\":false,\"d\":1.2e15,\"s\":\"bar\",\"c\":\"R\"}}")),
        "create channel for noconv parsing: i missing");
    testOk(checkValues(puser1, 99, 12, 0, 1.2e15, "bar", 0, 1),
        "guards intact, values correct");
    if (pch) dbChannelDelete(pch);

    /* Reject wrong types */
#define WRONGTYPETEST(Var, Val, Typ) \
    e1 = e_alloc | e_error | e_free; c1 = 0; \
    testOk(!(pch = dbChannelCreate("x.{\"noconv\":{\""#Var"\":"#Val"}}")), \
        "create channel for noconv parsing: wrong type "#Typ" for "#Var); \
    testOk(!puser1, "user part cleaned up"); \
    if (!testOk(c1 == e1, "all expected calls happened")) \
        testDiag("expected %#x - called %#x", e1, c1);

    testHead("NOCONV parsing: rejection of wrong parameter types");

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

#define CONVTESTGOOD(Var, Val, Typ, Ival, Fval, Dval, Sval1, Sval2, Cval) \
    e1 = e_alloc | e_ok; c1 = 0; \
    testDiag("Calling dbChannelCreate x.{\"sloppy\":{\""#Var"\":"#Val"}}"); \
    testOk(!!(pch = dbChannelCreate("x.{\"sloppy\":{\""#Var"\":"#Val"}}")), \
        "create channel for sloppy parsing: "#Typ" (good) for "#Var); \
    testOk(checkValues(puser1, 99, Ival, Fval, Dval, Sval1, Sval2, Cval), \
        "guards intact, values correct"); \
    if (!testOk(c1 == e1, "create channel: all expected calls happened")) \
        testDiag("expected %#x - called %#x", e1, c1); \
    e1 = e_close | e_free; c1 = 0; \
    if (pch) dbChannelDelete(pch); \
    testOk(!puser1, "user part cleaned up"); \
    if (!testOk(c1 == e1, "delete channel: all expected calls happened")) \
        testDiag("expected %#x - called %#x", e1, c1);

#define CONVTESTBAD(Var, Val, Typ) \
    e1 = e_alloc | e_error | e_free; c1 = 0; \
    testDiag("Calling dbChannelCreate x.{\"sloppy\":{\""#Var"\":"#Val"}}"); \
    testOk(!(pch = dbChannelCreate("x.{\"sloppy\":{\""#Var"\":"#Val"}}")), \
        "create channel for sloppy parsing: "#Typ" (bad) for "#Var); \
    testOk(!puser1, "user part cleaned up"); \
    if (!testOk(c1 == e1, "create channel: all expected calls happened")) \
        testDiag("expected %#x - called %#x", e1, c1);

    /* To integer */
    testHead("SLOPPY parsing: conversion to integer");
    CONVTESTGOOD(i, "123e4", positive string, 123, 1, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(i, "-12345", negative string, -12345, 1, 1.234e5, "hello", 0, 4);
    CONVTESTBAD(i, "9234567890", out-of-range string);
    CONVTESTBAD(i, ".4", invalid string);
    CONVTESTGOOD(i, false, valid boolean, 0, 1, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(i, 3456.789, valid double, 3456, 1, 1.234e5, "hello", 0, 4);
    CONVTESTBAD(i, 34.7e14, out-of-range double);

    /* To boolean */
    testHead("SLOPPY parsing: conversion to boolean");
    CONVTESTGOOD(f, "false", valid string, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, "False", capital valid string, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, "0", 0 string, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, "15", 15 string, 12, 1, 1.234e5, "hello", 0, 4);
    CONVTESTBAD(f, ".4", invalid .4 string);
    CONVTESTBAD(f, "Flase", misspelled invalid string);
    CONVTESTGOOD(f, 0, zero integer, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, 12, positive integer, 12, 1, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, -1234, negative integer, 12, 1, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, 0.4, positive non-zero double, 12, 1, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, 0.0, zero double, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, -0.0, minus-zero double, 12, 0, 1.234e5, "hello", 0, 4);
    CONVTESTGOOD(f, -1.24e14, negative double, 12, 1, 1.234e5, "hello", 0, 4);

    /* To double */
    testHead("SLOPPY parsing: conversion to double");
    CONVTESTGOOD(d, "123e4", positive double string, 12, 1, 1.23e6, "hello", 0, 4);
    CONVTESTGOOD(d, "-7.89e-14", negative double string, 12, 1, -7.89e-14, "hello", 0, 4);
    CONVTESTGOOD(d, "123", positive integer string, 12, 1, 123.0, "hello", 0, 4);
    CONVTESTGOOD(d, "-1234567", negative integer string, 12, 1, -1.234567e6, "hello", 0, 4);
    CONVTESTBAD(d, "1.67e407", out-of-range double string);
    CONVTESTBAD(d, "blubb", invalid blubb string);
    CONVTESTGOOD(d, 123, positive integer, 12, 1, 123.0, "hello", 0, 4);
    CONVTESTGOOD(d, -12345, negative integer, 12, 1, -1.2345e4, "hello", 0, 4);
    CONVTESTGOOD(d, true, true boolean, 12, 1, 1.0, "hello", 0, 4);
    CONVTESTGOOD(d, false, false boolean, 12, 1, 0.0, "hello", 0, 4);

    /* To string */
    testHead("SLOPPY parsing: conversion to string");
    CONVTESTGOOD(s, 12345, positive integer, 12, 1, 1.234e5, "12345", 0, 4);
    CONVTESTGOOD(s, -1234567891, negative integer, 12, 1, 1.234e5, "-1234567891", 0, 4);
    CONVTESTGOOD(s, true, true boolean, 12, 1, 1.234e5, "true", 0, 4);
    CONVTESTGOOD(s, false, false boolean, 12, 1, 1.234e5, "false", 0, 4);
    CONVTESTGOOD(s, 123e4, small positive double, 12, 1, 1.234e5, "1230000", 0, 4);
    CONVTESTGOOD(s, -123e24, negative double, 12, 1, 1.234e5, "-1.23e+26", "-1.23e+026", 4);
    CONVTESTGOOD(s, -1.23456789123e26, large negative double, 12, 1, 1.234e5, "-1.23456789123e+26", "-1.23456789123e+026", 4);

    /* To Enum */
    testHead("SLOPPY parsing: conversion to enum");
    CONVTESTGOOD(c, 2, valid integer choice, 12, 1, 1.234e5, "hello", 0, 2);
    CONVTESTBAD(c, 3, invalid integer choice);
    CONVTESTBAD(c, 3.2, double);
    CONVTESTGOOD(c, "R", valid string choice, 12, 1, 1.234e5, "hello", 0, 1);
    CONVTESTBAD(c, "blubb", invalid string choice);

    /* Registering and running filter callbacks */

#define CHAINTEST1(Type, Json, ExpReg, ExpRun, DType) \
    testHead("Filter chain test, "Type" filter"); \
    offset = 0; \
    e1 = e_alloc | e_ok; c1 = 0; \
    testOk(!!(pch = dbChannelCreate("x."Json)), "filter chains: create channel with "Type" filter"); \
    if (!testOk(c1 == e1, "create channel: all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    e1 = e_open | ExpReg; c1 = 0; \
    testOk(!dbChannelOpen(pch), "dbChannelOpen returned channel"); \
    if (!testOk(c1 == e1, "open channel: all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    e1 = ExpRun; c1 = 0; \
    testOk(!!(pfl = db_create_read_log(pch)), "create db_field_log"); \
    pfl->type = dbfl_type_ref; \
    pfl->field_type = TYPE_START; \
    testOk(!!(pfl = dbChannelRunPreChain(pch, pfl)), "run pre eventq chain"); \
    testOk(!!(pfl = dbChannelRunPostChain(pch, pfl)), "run post eventq chain"); \
    testOk(pfl->field_type == TYPE_START + DType, "final data type is correct"); \
    db_delete_field_log(pfl); \
    if (!testOk(c1 == e1, "run filter chains: all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    e1 = e_report; c1 = 0; \
    dbChannelShow(pch, R_LEVEL, 0); \
    if (!testOk(c1 == e1, "report: all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    e1 = e_close | e_free; c1 = 0; \
    if (pch) dbChannelDelete(pch); \
    if (!testOk(c1 == e1, "delete channel: all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1);

#define CHAINTEST2(Type, Json, ExpReg1, ExpRun1, ExpReg2, ExpRun2, DType) \
    testHead("Filter chain test, "Type" filters"); \
    offset = 0; \
    e1 = e_alloc | e_ok; c1 = 0; \
    e2 = e_alloc | e_ok; c2 = 0; \
    testOk(!!(pch = dbChannelCreate("x."Json)), "filter chains: create channel with "Type" filters"); \
    if (!testOk(c1 == e1, "create channel (1): all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    if (!testOk(c2 == e2, "create channel (2): all expected calls happened")) testDiag("expected %#x - called %#x", e2, c2); \
    e1 = e_open | ExpReg1; c1 = 0; \
    e2 = e_open | ExpReg2; c2 = 0; \
    if (pch) testOk(!dbChannelOpen(pch), "dbChannelOpen returned channel"); \
    if (!testOk(c1 == e1, "open channel (1): all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    if (!testOk(c2 == e2, "open channel (2): all expected calls happened")) testDiag("expected %#x - called %#x", e2, c2); \
    e1 = ExpRun1; c1 = 0; \
    e2 = ExpRun2; c2 = 0; \
    if (pch) testOk(!!(pfl = db_create_read_log(pch)), "create db_field_log"); \
    pfl->type = dbfl_type_ref; \
    pfl->field_type = TYPE_START; \
    if (pch) testOk(!!(pfl = dbChannelRunPreChain(pch, pfl)) || (drop >=0 && drop <= 1), "run pre eventq chain"); \
    if (pch && (drop < 0 || drop >= 2)) testOk(!!(pfl = dbChannelRunPostChain(pch, pfl)) || drop >=2, "run post eventq chain"); \
    if (pfl) testOk(pfl->field_type == TYPE_START + DType, "final data type is correct"); \
    if (pfl) db_delete_field_log(pfl); \
    if (!testOk(c1 == e1, "run filter chains (1): all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    if (!testOk(c2 == e2, "run filter chains (2): all expected calls happened")) testDiag("expected %#x - called %#x", e2, c2); \
    e1 = e_report; c1 = 0; \
    e2 = e_report; c2 = 0; \
    dbChannelShow(pch, R_LEVEL, 0); \
    if (!testOk(c1 == e1, "report (1): all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    if (!testOk(c2 == e2, "report (2): all expected calls happened")) testDiag("expected %#x - called %#x", e2, c2); \
    e1 = e_close | e_free; c1 = 0; \
    e2 = e_close | e_free; c2 = 0; \
    if (pch) dbChannelDelete(pch); \
    if (!testOk(c1 == e1, "delete channel (1): all expected calls happened")) testDiag("expected %#x - called %#x", e1, c1); \
    if (!testOk(c2 == e2, "delete channel (2): all expected calls happened")) testDiag("expected %#x - called %#x", e2, c2);

    CHAINTEST1("1 pre", "{\"pre\":{}}", e_reg_pre, e_pre | e_dtor, 1);                                        /* One filter, pre chain */
    CHAINTEST1("1 post", "{\"post\":{}}", e_reg_post, e_post | e_dtor, 1);                                   /* One filter, post chain */
    CHAINTEST1("1 both", "{\"sloppy\":{}}", e_reg_pre | e_reg_post, e_pre | e_post | e_dtor, 2);                   /* One, both chains */
    CHAINTEST2("2 pre", "{\"pre\":{},\"pre\":{}}", e_reg_pre, e_pre | e_dtor, e_reg_pre, e_pre, 2);          /* Two filters, pre chain */
    CHAINTEST2("2 post", "{\"post\":{},\"post\":{}}", e_reg_post, e_post | e_dtor, e_reg_post, e_post, 2);  /* Two filters, post chain */
    CHAINTEST2("2 both", "{\"sloppy\":{},\"sloppy\":{}}",                                                          /* Two, both chains */
               e_reg_pre | e_reg_post, e_pre | e_post | e_dtor, e_reg_pre | e_reg_post, e_pre | e_post, 4);
    CHAINTEST2("1 pre, 1 post", "{\"pre\":{},\"post\":{}}", e_reg_pre, e_pre | e_dtor, e_reg_post, e_post, 2);   /* Two, pre then post */
    CHAINTEST2("1 post, 1 pre", "{\"post\":{},\"pre\":{}}", e_reg_post, e_post, e_reg_pre, e_pre | e_dtor, 2);   /* Two, post then pre */
    CHAINTEST2("1 pre, 1 both", "{\"pre\":{},\"sloppy\":{}}",                                                    /* Two, pre then both */
               e_reg_pre, e_pre | e_dtor, e_reg_pre | e_reg_post, e_pre | e_post, 3);
    CHAINTEST2("1 both, 1 pre", "{\"sloppy\":{},\"pre\":{}}",                                                    /* Two, both then pre */
               e_reg_pre | e_reg_post, e_pre | e_post | e_dtor, e_reg_pre, e_pre, 3);
    CHAINTEST2("1 post, 1 both", "{\"post\":{},\"sloppy\":{}}",                                                 /* Two, post then both */
               e_reg_post, e_post, e_reg_pre | e_reg_post, e_pre | e_post | e_dtor, 3);
    CHAINTEST2("1 both, 1 post", "{\"sloppy\":{},\"post\":{}}",                                                 /* Two, both then post */
               e_reg_pre | e_reg_post, e_pre | e_post | e_dtor, e_reg_post, e_post, 3);

    /* Plugins dropping updates */
    drop = 0;
    CHAINTEST2("2 both (drop at 0)", "{\"sloppy\":{},\"sloppy\":{}}",                            /* Two, both chains, drop at filter 0 */
               e_reg_pre | e_reg_post, e_pre, e_reg_pre | e_reg_post, 0, -1);
    drop = 1;
    CHAINTEST2("2 both (drop at 1)", "{\"sloppy\":{},\"sloppy\":{}}",                            /* Two, both chains, drop at filter 1 */
               e_reg_pre | e_reg_post, e_pre, e_reg_pre | e_reg_post, e_pre, -1);
    drop = 2;
    CHAINTEST2("2 both (drop at 2)", "{\"sloppy\":{},\"sloppy\":{}}",                            /* Two, both chains, drop at filter 2 */
               e_reg_pre | e_reg_post, e_pre | e_post, e_reg_pre | e_reg_post, e_pre, -1);
    drop = 3;
    CHAINTEST2("2 both (drop at 3)", "{\"sloppy\":{},\"sloppy\":{}}",                            /* Two, both chains, drop at filter 3 */
               e_reg_pre | e_reg_post, e_pre | e_post, e_reg_pre | e_reg_post, e_pre | e_post, -1);
    drop = -1;

    dbFreeBase(pdbbase);
    registryFree();
    pdbbase = NULL;

    return testDone();
}
