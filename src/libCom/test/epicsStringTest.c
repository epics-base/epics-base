/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsStdioTest.c
 *
 *      Author  Marty Kraimer
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "epicsString.h"

int epicsStringTest()
{
    if(epicsStrnCaseCmp("","",0)!=0) printf("case 1 failed\n");
    if(epicsStrnCaseCmp("","",1)!=0) printf("case 2 failed\n");
    if(epicsStrnCaseCmp(" ","",1)!=-1) printf("case 3 failed\n");
    if(epicsStrnCaseCmp(""," ",1)!=1) printf("case 4 failed\n");
    if(epicsStrnCaseCmp("a","A",1)!=0) printf("case 5 failed\n");
    if(epicsStrnCaseCmp("a","A",2)!=0) printf("case 6 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCD",2)!=0) printf("case 7 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCD",4)!=0) printf("case 8 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCD",1000)!=0) printf("case 9 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCDE",2)!=0) printf("case 10 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCDE",4)!=0) printf("case 11 failed\n");
    if(epicsStrnCaseCmp("abcd","ABCDE",1000)!=1) printf("case 12 failed\n");
    if(epicsStrnCaseCmp("abcde","ABCD",2)!=0) printf("case 13 failed\n");
    if(epicsStrnCaseCmp("abcde","ABCD",4)!=0) printf("case 14 failed\n");
    if(epicsStrnCaseCmp("abcde","ABCD",1000)!=-1) printf("case 15 failed\n");

    if(epicsStrCaseCmp("","")!=0) printf("case 16 failed\n");
    if(epicsStrCaseCmp("a","A")!=0) printf("case 17 failed\n");
    if(epicsStrCaseCmp("abcd","ABCD")!=0) printf("case 18 failed\n");
    if(epicsStrCaseCmp("abcd","ABCDE")==0) printf("case 19 failed\n");
    if(epicsStrCaseCmp("abcde","ABCD")==0) printf("case 20 failed\n");
    if(epicsStrCaseCmp("abcde","ABCDF")==0) printf("case 21 failed\n");

    printf("String comparison tests completed.\n");

    return(0);
}
