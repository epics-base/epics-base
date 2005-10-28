/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 * $Id$
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "macLib.h"
#include "envDefs.h"
#include "errlog.h"

int
check(const char *str, const char *expect)
{
    char *got = macEnvExpand(str);
    int bad = 0;
    static int count = 0;
    if (expect && !got) {
        printf("\t#Got NULL, expected \"%s\".\n", expect);
        bad = 1;
    }
    else if (!expect && got) {
        printf("\t#Got \"%s\", expected NULL.\n", got);
        bad = 1;
    }
    else if (expect && got && strcmp(got, expect)) {
        printf("\t#Got \"%s\", expected \"%s\".\n", got, expect);
        bad = 1;
    }
    /* This output format follows the perl standard: */
    printf("%sok %d - \"%s\".\n", (bad ? "not " : ""), ++count, str);
    return bad;
}

int macEnvExpandTest(void)
{
    int bad = 0;
    int warn = 0;

    /* Announce the number of calls to check() present in the code below: */
    printf("1..%d\n", 30);

    bad |= check ("FOO", "FOO");

    bad |= check ("${FOO}", NULL); warn++;
    bad |= check ("${FOO=}", "");
    bad |= check ("x${FOO=}y", "xy");
    bad |= check ("${FOO=BAR}", "BAR");
    bad |= check ("x${FOO=BAR}y", "xBARy");

    epicsEnvSet("FOO","BLETCH");
    bad |= check ("${FOO}", "BLETCH");
    bad |= check ("x${FOO}y", "xBLETCHy");
    bad |= check ("x${FOO}y${FOO}z", "xBLETCHyBLETCHz");
    bad |= check ("${FOO=BAR}", "BLETCH");
    bad |= check ("x${FOO=BAR}y", "xBLETCHy");
    bad |= check ("${FOO=${BAZ}}", "BLETCH");
    bad |= check ("x${FOO=${BAZ}}y", "xBLETCHy");
    bad |= check ("${BAR=${FOO}}", "BLETCH");
    bad |= check ("x${BAR=${FOO}}y", "xBLETCHy");
    bad |= check ("w${BAR=x${FOO}y}z", "wxBLETCHyz");

    epicsEnvSet("BAR","GLEEP");
    bad |= check ("${FOO}/${BAR}", "BLETCH/GLEEP");
    bad |= check ("x${FOO}/${BAR}y", "xBLETCH/GLEEPy");
    bad |= check ("${BAR=${FOO}}", "GLEEP");

    epicsEnvSet("BLETCH","BAR");
    bad |= check ("${${FOO}}", "BAR");
    bad |= check ("x${${FOO}}y", "xBARy");
    bad |= check ("${${FOO}=GRIBBLE}", "BAR");
    bad |= check ("x${${FOO}=GRIBBLE}y", "xBARy");

    epicsEnvSet("BLETCH","${BAR}");
    bad |= check ("${${FOO}}", "GLEEP");

    epicsEnvSet("FOO","${BAR}");
    bad |= check ("${FOO}","GLEEP");

    epicsEnvSet("BAR","${BAZ}");
    bad |= check ("${FOO}", NULL); warn++;

    epicsEnvSet("BAR","${BAZ=GRIBBLE}");
    bad |= check ("${FOO}", "GRIBBLE");

    epicsEnvSet("BAR","${STR1}");
    epicsEnvSet("STR1","VAL1");
    epicsEnvSet("STR2","VAL2");
    bad |= check ("${FOO}", "VAL1");

    epicsEnvSet("BAR","${STR2}");
    bad |= check ("${FOO}", "VAL2");

    epicsEnvSet("BAR","${FOO}");
    bad |= check ("${FOO}", NULL); warn++;
    
    printf("# Expect %d warning messages here:\n", warn);
    errlogFlush();
    return bad;
}
