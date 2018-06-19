/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
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


static void checkMac(MAC_HANDLE *handle, const char *name, const char *value)
{
    char buf[20];

    buf[19]='\0';
    if(macGetValue(handle, name, buf, 19)<0) {
        if(value)
            testFail("Macro %s undefined, expected %s", name, value);
        else
            testPass("Macro %s undefined", name);
    } else {
        if(!value)
            testFail("Macro %s is %s, expected undefined", name, buf);
        else if(strcmp(value, buf)==0)
            testPass("Macro %s is %s", name, value);
        else
            testFail("Macro %s is %s, expected %s", name, buf, value);
    }
}

static void macEnvScope(void)
{
    MAC_HANDLE *handle;
    char **defines;
    static const char *pairs[] = { "", "environ", NULL, NULL };

    epicsEnvSet("C","3");
    epicsEnvSet("D","4");
    epicsEnvSet("E","5");

    macCreateHandle(&handle, pairs);
    macParseDefns(NULL, "A=1,B=2,E=15", &defines);
    macInstallMacros(handle, defines);

    checkMac(handle, "A", "1");
    checkMac(handle, "B", "2");
    checkMac(handle, "C", "3");
    checkMac(handle, "D", "4");
    checkMac(handle, "E", "15");
    checkMac(handle, "F", NULL);

    {
        macPushScope(handle);

        macParseDefns(NULL, "A=11,C=13,D=14,G=7", &defines);
        macInstallMacros(handle, defines);

        checkMac(handle, "A", "11");
        checkMac(handle, "B", "2");
        checkMac(handle, "C", "13");
        checkMac(handle, "D", "14");
        checkMac(handle, "E", "15");
        checkMac(handle, "F", NULL);
        checkMac(handle, "G", "7");

        epicsEnvSet("D", "24");
        macPutValue(handle, "D", NULL); /* implicit when called through in iocshBody */
        epicsEnvSet("F", "6");
        macPutValue(handle, "F", NULL); /* implicit */
        epicsEnvSet("G", "17");
        macPutValue(handle, "G", NULL); /* implicit */

        checkMac(handle, "D", "24");
        checkMac(handle, "F", "6");
        checkMac(handle, "G", "17");

        macPopScope(handle);
    }

    checkMac(handle, "A", "1");
    checkMac(handle, "B", "2");
    checkMac(handle, "C", "3");
    checkMac(handle, "D", "24");
    checkMac(handle, "E", "15");
    checkMac(handle, "F", "6");
    checkMac(handle, "G", "17");

    {
        macPushScope(handle);

        macParseDefns(NULL, "D=34,G=27", &defines);
        macInstallMacros(handle, defines);

        checkMac(handle, "D", "34");
        checkMac(handle, "G", "27");

        macPopScope(handle);
    }

    checkMac(handle, "D", "24");

    macDeleteHandle(handle);
}

static void check(const char *str, const char *macros, const char *expect)
{
    MAC_HANDLE *handle;
    char **defines;
    static const char *pairs[] = { "", "environ", NULL, NULL };
    char *got;
    int pass = 1;
    
    macCreateHandle(&handle, pairs);
    macParseDefns(NULL, macros, &defines);
    macInstallMacros(handle, defines);
    
    got = macDefExpand(str, handle);

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
    
    macDeleteHandle(handle);
}

MAIN(macDefExpandTest)
{
    eltc(0);
    testPlan(97);

    check("FOO", "", "FOO");

    check("${FOO}", "", NULL);
    check("${FOO,BAR}", "", NULL);
    check("${FOO,BAR=baz}", "", NULL);
    check("${FOO,BAR=$(FOO)}", "", NULL);
    check("${FOO,FOO}", "", NULL);
    check("${FOO,FOO=$(FOO)}", "", NULL);
    check("${FOO,BAR=baz,FUM}", "", NULL);

    check("${=}", "", "");
    check("x${=}y", "", "xy");

    check("${,=}", "", "");
    check("x${,=}y", "", "xy");

    check("${FOO=}", "", "");
    check("x${FOO=}y", "", "xy");

    check("${FOO=,}", "", "");
    check("x${FOO=,}y", "", "xy");

    check("${FOO,FOO=}", "", "");
    check("x${FOO,FOO=}y", "", "xy");

    check("${FOO=,BAR}", "", "");
    check("x${FOO=,BAR}y", "", "xy");

    check("${FOO=$(BAR=)}", "", "");
    check("x${FOO=$(BAR=)}y", "", "xy");

    check("${FOO=,BAR=baz}", "", "");
    check("x${FOO=,BAR=baz}y", "", "xy");

    check("${FOO=$(BAR),BAR=}", "", "");
    check("x${FOO=$(BAR),BAR=}y", "", "xy");

    check("${=BAR}", "", "BAR");
    check("x${=BAR}y", "", "xBARy");

    check("${FOO=BAR}", "", "BAR");
    check("x${FOO=BAR}y", "", "xBARy");

    epicsEnvSet("FOO","BLETCH");
    check("${FOO}", "", "BLETCH");
    check("${FOO,FOO}", "", "BLETCH");
    check("x${FOO}y", "", "xBLETCHy");
    check("x${FOO}y${FOO}z", "", "xBLETCHyBLETCHz");
    check("${FOO=BAR}", "", "BLETCH");
    check("x${FOO=BAR}y", "", "xBLETCHy");
    check("${FOO=${BAZ}}", "", "BLETCH");
    check("${FOO=${BAZ},BAR=$(BAZ)}", "", "BLETCH");
    check("x${FOO=${BAZ}}y", "", "xBLETCHy");
    check("x${FOO=${BAZ},BAR=$(BAZ)}y", "", "xBLETCHy");
    check("${BAR=${FOO}}", "", "BLETCH");
    check("x${BAR=${FOO}}y", "", "xBLETCHy");
    check("w${BAR=x${FOO}y}z", "", "wxBLETCHyz");

    check("${FOO,FOO=BAR}", "", "BAR");
    check("x${FOO,FOO=BAR}y", "", "xBARy");
    check("${BAR,BAR=$(FOO)}", "", "BLETCH");
    check("x${BAR,BAR=$(FOO)}y", "", "xBLETCHy");
    check("${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}", "", "GRIBBLE");
    check("x${BAR,BAR=$($(FOO)),BLETCH=GRIBBLE}y", "", "xGRIBBLEy");
    check("${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}", "", "GRIBBLE");
    check("x${$(BAR,BAR=$(FOO)),BLETCH=GRIBBLE}y", "", "xGRIBBLEy");

    check("${FOO}/${BAR}", "BAR=GLEEP", "BLETCH/GLEEP");
    check("x${FOO}/${BAR}y", "BAR=GLEEP", "xBLETCH/GLEEPy");
    check("${FOO,BAR}/${BAR}", "BAR=GLEEP", "BLETCH/GLEEP");
    check("${FOO,BAR=x}/${BAR}", "BAR=GLEEP", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR}/${BAR}", "BAR=GLEEP", "BLETCH/GLEEP");
    check("${BAZ=BLETCH,BAR=x}/${BAR}", "BAR=GLEEP", "BLETCH/GLEEP");
    
    check("${${FOO}}", "BAR=GLEEP,BLETCH=BAR", "BAR");
    check("x${${FOO}}y", "BAR=GLEEP,BLETCH=BAR", "xBARy");
    check("${${FOO}=GRIBBLE}", "BAR=GLEEP,BLETCH=BAR", "BAR");
    check("x${${FOO}=GRIBBLE}y", "BAR=GLEEP,BLETCH=BAR", "xBARy");

    check("${${FOO}}", "BAR=GLEEP,BLETCH=${BAR}", "GLEEP");

    epicsEnvSet("FOO","${BAR}");
    check("${FOO}", "BAR=GLEEP,BLETCH=${BAR}" ,"GLEEP");

    check("${FOO}", "BAR=${BAZ},BLETCH=${BAR}", NULL);

    check("${FOO}", "BAR=${BAZ=GRIBBLE},BLETCH=${BAR}", "GRIBBLE");

    check("${FOO}", "BAR=${STR1},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", "VAL1");

    check("${FOO}", "BAR=${STR2},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", "VAL2");

    check("${FOO}", "BAR=${FOO},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO,FOO=$(FOO)}", "BAR=${FOO},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO=$(FOO)}", "BAR=${FOO},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);
    check("${FOO=$(BAR),BAR=$(FOO)}", "BAR=${FOO},BLETCH=${BAR},STR1=VAL1,STR2=VAL2", NULL);

    macEnvScope();
    
    errlogFlush();
    eltc(1);
    return testDone();
}
