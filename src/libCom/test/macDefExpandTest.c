/*************************************************************************\
* Copyright (c) 2006 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * $Revision-Id$
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "macLib.h"
#include "envDefs.h"
#include "errlog.h"
#include "epicsUnitTest.h"
#include "testMain.h"

static void check(const char *str, const char *macros, const char *expect)
{
    char *got = macDefExpand(str, macros);
    int pass = -1;

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
    eltc(0);
    testPlan(71);

    check("FOO", NULL, "FOO");

    check("${FOO}", NULL, NULL);
    check("${FOO,BAR}", NULL, NULL);
    check("${FOO,BAR=baz}", NULL, NULL);
    check("${FOO,BAR=$(FOO)}", NULL, NULL);
    check("${FOO,FOO}", NULL, NULL);
    check("${FOO,FOO=$(FOO)}", NULL, NULL);
    check("${FOO,BAR=baz,FUM}", NULL, NULL);

    check("${=}", NULL, "");
    check("x${=}y", NULL, "xy");

    check("${,=}", NULL, "");
    check("x${,=}y", NULL, "xy");

    check("${FOO=}", NULL, "");
    check("x${FOO=}y", NULL, "xy");

    check("${FOO=,}", NULL, "");
    check("x${FOO=,}y", NULL, "xy");

    check("${FOO,FOO=}", NULL, "");
    check("x${FOO,FOO=}y", NULL, "xy");

    check("${FOO=,BAR}", NULL, "");
    check("x${FOO=,BAR}y", NULL, "xy");

    check("${FOO=$(BAR=)}", NULL, "");
    check("x${FOO=$(BAR=)}y", NULL, "xy");

    check("${FOO=,BAR=baz}", NULL, "");
    check("x${FOO=,BAR=baz}y", NULL, "xy");

    check("${FOO=$(BAR),BAR=}", NULL, "");
    check("x${FOO=$(BAR),BAR=}y", NULL, "xy");

    check("${=BAR}", NULL, "BAR");
    check("x${=BAR}y", NULL, "xBARy");

    check("${FOO=BAR}", NULL, "BAR");
    check("x${FOO=BAR}y", NULL, "xBARy");

    check("${FOO}", "FOO=BLETCH", "BLETCH");
    check("${FOO,FOO}", "FOO=BLETCH", "BLETCH");
    check("x${FOO}y", "FOO=BLETCH", "xBLETCHy");
    check("x${FOO}y${FOO}z", "FOO=BLETCH", "xBLETCHyBLETCHz");
    check("${FOO=BAR}", "FOO=BLETCH", "BLETCH");
    check("x${FOO=BAR}y", "FOO=BLETCH", "xBLETCHy");
    check("${FOO=${BAZ}}", "FOO=BLETCH", "BLETCH");
    check("${FOO=${BAZ},BAR=$(BAZ)}", "FOO=BLETCH", "BLETCH");
    check("x${FOO=${BAZ}}y", "FOO=BLETCH", "xBLETCHy");
    check("x${FOO=${BAZ},BAR=$(BAZ)}y", "FOO=BLETCH", "xBLETCHy");
    check("${BAR=${FOO}}", "FOO=BLETCH", "BLETCH");
    check("x${BAR=${FOO}}y", "FOO=BLETCH", "xBLETCHy");
    check("w${BAR=x${FOO}y}z", "FOO=BLETCH", "wxBLETCHyz");

    check("${FOO,FOO=BAR}", "FOO=BLETCH", "BAR");
    check("x${FOO,FOO=BAR}y", "FOO=BLETCH", "xBARy");
    check("${BAR,BAR=$(FOO)}", "FOO=BLETCH", "BLETCH");
    check("x${BAR,BAR=$(FOO)}y", "FOO=BLETCH", "xBLETCHy");
    check("${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}", "FOO=BLETCH", "GRIBBLE");
    check("x${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}y", "FOO=BLETCH", "xGRIBBLEy");
    check("${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}", "FOO=BLETCH", "GRIBBLE");
    check("x${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}y", "FOO=BLETCH", "xGRIBBLEy");

    check("${FOO}/${BAR}", "BAR=GLEEP,FOO=BLETCH", "BLETCH/GLEEP");
    check("x${FOO}/${BAR}y", "BAR=GLEEP,FOO=BLETCH", "xBLETCH/GLEEPy");
    check("${FOO,BAR}/${BAR}", "BAR=GLEEP,FOO=BLETCH", "BLETCH/GLEEP");
    check("${FOO,BAR=x}/${BAR}", "BAR=GLEEP,FOO=BLETCH", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR}/${BAR}", "BAR=GLEEP,FOO=BLETCH", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR=x}/${BAR}", "BAR=GLEEP,FOO=BLETCH", "BLETCH/GLEEP");
    
    check("${${FOO}}", "BAR=GLEEP,FOO=BLETCH,BLETCH=BAR", "BAR");
    check("x${${FOO}}y", "BAR=GLEEP,FOO=BLETCH,BLETCH=BAR", "xBARy");
    check("${${FOO}=GRIBBLE}", "BAR=GLEEP,FOO=BLETCH,BLETCH=BAR", "BAR");
    check("x${${FOO}=GRIBBLE}y", "BAR=GLEEP,FOO=BLETCH,BLETCH=BAR", "xBARy");

    check("${${FOO}}", "BAR=GLEEP,FOO=BLETCH,BLETCH=${BAR}", "GLEEP");

    check("${FOO}", "BAR=GLEEP,FOO=${BAR},BLETCH=${BAR}" ,"GLEEP");

    check("${FOO}", "BAR=${BAZ},FOO=${BAR},BLETCH=${BAR}", NULL);

    check("${FOO}", "BAR=${BAZ=GRIBBLE},FOO=${BAR},BLETCH=${BAR}", "GRIBBLE");

    check("${FOO}", "BAR=${STR1},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", "VAL1");

    check("${FOO}", "BAR=${STR2},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", "VAL2");

    check("${FOO}", "BAR=${FOO},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO,FOO=$(FOO)}", "BAR=${FOO},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO=$(FOO)}", "BAR=${FOO},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO=$(BAR),BAR=$(FOO)}", "BAR=${FOO},FOO=${BAR},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    
    errlogFlush();
    eltc(1);
    return testDone();
}
