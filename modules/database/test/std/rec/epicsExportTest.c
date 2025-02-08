/*************************************************************************\
* Copyright (c) 2025 Dirk Zimoch
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Compile and link test for epicsExport.h
* 
* Test if macros eppicsExportAddr, epicsExportRegistrar and epicsRegisterFunction
* expand to valid C and C++ code, in the latter case regardless if
* wrapped with extern "C" {} or not.
* Also test that those macros have the intended effect in both cases.
*
* This file is compiled directly as C
* and included from epicsExportTestxx.cpp as C++
*/

#include <epicsUnitTest.h>
#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <longinRecord.h>
#include <aSubRecord.h>
#include <iocsh.h>
#include <registryFunction.h>
#include <epicsExport.h>

int i1, i2;
volatile int v1=10, v2=20;
const int c1=100, c2=200;

static long myReadLongin(struct dbCommon* prec)
{
    struct longinRecord* pli =
#ifdef __cplusplus
        reinterpret_cast<struct longinRecord*>(prec);
#else
        (struct longinRecord*)prec;
#endif
    pli->val=5;
    return 0;
}

/* Also test cast from user-specific const mydset to dset */
const struct mydset {
    long number;
    long (*report)(int);
    long (*init)(int);
    long (*init_record)(struct dbCommon*);
    long (*get_ioint_info)(int, struct dbCommon*, IOSCANPVT*);
    long (*process)(struct dbCommon*);
    long (*something_else)(struct dbCommon*);
} dset1 = {
    6,
    NULL,
    NULL,
    NULL,
    NULL,
    myReadLongin,
    NULL
}, dset2 = {
    6,
    NULL,
    NULL,
    NULL,
    NULL,
    myReadLongin,
    NULL
};

static void registrar1() {
    i1++;
    testPass("registrar1 executed");
}

static void registrar2() {
    i2++;
    testPass("registrar2 executed");
}

/* Also test cast from int(*)() to REGISTRAR */
static int registrar3() {
    i1++;
    testPass("registrar3 executed");
    return 0;
}

static int registrar4() {
    i2++;
    testPass("registrar4 executed");
    return 0;
}

/* Test both, native (potentially C++) and extern "C" functions */
static long aSubInit1(aSubRecord* prec) {
    *(epicsInt32*)prec->a = 1;
    return 0;
}

static long aSubProc1(aSubRecord* prec) {
    *(epicsInt32*)prec->b = 2;
    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
static long aSubInit2(aSubRecord* prec) {
    *(epicsInt32*)prec->a = 3;
    return 0;
}
static long aSubProc2(aSubRecord* prec) {
    *(epicsInt32*)prec->b = 4;
    return 0;
}
#ifdef __cplusplus
}
#endif

/* Test without wrapping */
epicsExportAddress(int, i1);
epicsExportAddress(int, v1);
epicsExportAddress(int, c1);
epicsExportAddress(dset, dset1);
epicsExportRegistrar(registrar1);
epicsExportRegistrar(registrar3);
epicsRegisterFunction(aSubInit1);
epicsRegisterFunction(aSubProc1);

/* In C++ test wrapped in extern "C" {} */
#ifdef __cplusplus
extern "C" {
#endif
epicsExportAddress(int, i2);
epicsExportAddress(int, v2);
epicsExportAddress(int, c2);
epicsExportAddress(dset, dset2);
epicsExportRegistrar(registrar2);
epicsExportRegistrar(registrar4);
epicsRegisterFunction(aSubInit2);
epicsRegisterFunction(aSubProc2);

#ifdef __cplusplus
}
#endif
