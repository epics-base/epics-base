/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "macLib.h"
#include "envDefs.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

int warn;

static void check(const char *str, const char *expect)
{
    char *got = macEnvExpand(str);
    int pass = -1;

    if (expect == NULL) ++warn;

    if (expect && !got) {
        testDiag("Got NULL, expected \"%s\".\n", expect);
        pass = 0;
    }
    else if (!expect && got) {
        testDiag("Got \"%s\", expected NULL.\n", got);
        pass = 0;
    }
    else if (expect && got && strcmp(got, expect)) {
        testDiag("Got \"%s\", expected \"%s\".\n", got, expect);
        pass = 0;
    }
    testOk(pass, "%s", str);
}

MAIN(macEnvExpandTest)
{
    warn = 0;
    testPlan(71);

    check("FOO", "FOO");

    check("${FOO}", NULL);
    check("${FOO,BAR}", NULL);
    check("${FOO,BAR=baz}", NULL);
    check("${FOO,BAR=$(FOO)}", NULL);
    check("${FOO,FOO}", NULL);
    check("${FOO,FOO=$(FOO)}", NULL);
    check("${FOO,BAR=baz,FUM}", NULL);

    check("${=}", "");
    check("x${=}y", "xy");

    check("${,=}", "");
    check("x${,=}y", "xy");

    check("${FOO=}", "");
    check("x${FOO=}y", "xy");

    check("${FOO=,}", "");
    check("x${FOO=,}y", "xy");

    check("${FOO,FOO=}", "");
    check("x${FOO,FOO=}y", "xy");

    check("${FOO=,BAR}", "");
    check("x${FOO=,BAR}y", "xy");

    check("${FOO=$(BAR=)}", "");
    check("x${FOO=$(BAR=)}y", "xy");

    check("${FOO=,BAR=baz}", "");
    check("x${FOO=,BAR=baz}y", "xy");

    check("${FOO=$(BAR),BAR=}", "");
    check("x${FOO=$(BAR),BAR=}y", "xy");

    check("${=BAR}", "BAR");
    check("x${=BAR}y", "xBARy");

    check("${FOO=BAR}", "BAR");
    check("x${FOO=BAR}y", "xBARy");

    epicsEnvSet("FOO","BLETCH");
    check("${FOO}", "BLETCH");
    check("${FOO,FOO}", "BLETCH");
    check("x${FOO}y", "xBLETCHy");
    check("x${FOO}y${FOO}z", "xBLETCHyBLETCHz");
    check("${FOO=BAR}", "BLETCH");
    check("x${FOO=BAR}y", "xBLETCHy");
    check("${FOO=${BAZ}}", "BLETCH");
    check("${FOO=${BAZ},BAR=$(BAZ)}", "BLETCH");
    check("x${FOO=${BAZ}}y", "xBLETCHy");
    check("x${FOO=${BAZ},BAR=$(BAZ)}y", "xBLETCHy");
    check("${BAR=${FOO}}", "BLETCH");
    check("x${BAR=${FOO}}y", "xBLETCHy");
    check("w${BAR=x${FOO}y}z", "wxBLETCHyz");

    check("${FOO,FOO=BAR}", "BAR");
    check("x${FOO,FOO=BAR}y", "xBARy");
    check("${BAR,BAR=$(FOO)}", "BLETCH");
    check("x${BAR,BAR=$(FOO)}y", "xBLETCHy");
    check("${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}", "GRIBBLE");
    check("x${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}y", "xGRIBBLEy");
    check("${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}", "GRIBBLE");
    check("x${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}y", "xGRIBBLEy");

    epicsEnvSet("BAR","GLEEP");
    check("${FOO}/${BAR}", "BLETCH/GLEEP");
    check("x${FOO}/${BAR}y", "xBLETCH/GLEEPy");
    check("${FOO,BAR}/${BAR}", "BLETCH/GLEEP");
    check("${FOO,BAR=x}/${BAR}", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR}/${BAR}", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR=x}/${BAR}", "BLETCH/GLEEP");

    epicsEnvSet("BLETCH","BAR");
    check("${${FOO}}", "BAR");
    check("x${${FOO}}y", "xBARy");
    check("${${FOO}=GRIBBLE}", "BAR");
    check("x${${FOO}=GRIBBLE}y", "xBARy");

    epicsEnvSet("BLETCH","${BAR}");
    check("${${FOO}}", "GLEEP");

    epicsEnvSet("FOO","${BAR}");
    check("${FOO}","GLEEP");

    epicsEnvSet("BAR","${BAZ}");
    check("${FOO}", NULL);

    epicsEnvSet("BAR","${BAZ=GRIBBLE}");
    check("${FOO}", "GRIBBLE");

    epicsEnvSet("BAR","${STR1}");
    epicsEnvSet("STR1","VAL1");
    epicsEnvSet("STR2","VAL2");
    check("${FOO}", "VAL1");

    epicsEnvSet("BAR","${STR2}");
    check("${FOO}", "VAL2");

    epicsEnvSet("BAR","${FOO}");
    check("${FOO}", NULL);
    check("${FOO,FOO=$(FOO)}", NULL);
    check("${FOO=$(FOO)}", NULL);
    check("${FOO=$(BAR),BAR=$(FOO)}", NULL);
    
    errlogFlush();
    testDiag("%d warning messages from macLib were expected above.\n", warn);
    return testDone();
}
