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
check(char *str, char *expect)
{
    char *got = macEnvExpand(str);
    if(expect && !got) {
        printf("Expand \"%s'\".  Got NULL, expected \"%s\".\n", str, expect);
        return 1;
    }
    if (!expect && got) {
        printf("Expand \"%s'\".  Got \"%s\", expected NULL.\n", str, got);
        return 1;
    }
    if(expect && got && strcmp(got, expect)) {
        printf("Expand \"%s'\".  Got \"%s\", expected \"%s\".\n", str, got, expect);
        return 1;
    }
    return 0;
}

int macEnvExpandTest(void)
{
    int bad = 0;

    printf ("Expect one message about failure to expand ${FOOBAR}.\n");
    epicsEnvSet("FOO","BLETCH");
    epicsEnvSet("BAR","GLEEP");
    bad |= check ("${FOO}", "BLETCH");
    bad |= check ("${FOO}/${BAR}", "BLETCH/GLEEP");
    bad |= !check ("${FOOBAR}", NULL);
    epicsEnvSet("FOO","${BAR}");
    epicsEnvSet("BAR","${STR1}");
    epicsEnvSet("STR1","VAL1");
    epicsEnvSet("STR2","VAL2");
    bad |= check ("${FOO}", "VAL1");
    epicsEnvSet("BAR","${STR2}");
    bad |= check ("${FOO}", "VAL2");
    
    errlogFlush();
    return bad;
}
