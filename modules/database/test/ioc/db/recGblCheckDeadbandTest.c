/*************************************************************************\
* Copyright (c) 2014 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@gmx.de>
 */

#include <string.h>

#include "recGbl.h"
#include "dbBase.h"
#include "epicsMath.h"
#include "epicsUnitTest.h"
#include "testMain.h"

/* Test parameters */

#define NO_OF_DEADBANDS 3
#define NO_OF_PATTERNS 19

void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);

/* Indices for deadband value, test number, val in sequence */
static int idbnd, itest;

/* Different deadbands to test with */
static double t_Deadband[NO_OF_DEADBANDS] = { -1, 0, 1.5 };
/* Value sequences for each of the 16 tests */
static double t_SetValues[NO_OF_PATTERNS][2];
/* Expected updates (1=yes) for each sequence of each test of each deadband */
static int t_ExpectedUpdates[NO_OF_DEADBANDS][NO_OF_PATTERNS] = {
    {   /* deadband = -1 */
        1, 1, 1, 1,
        1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
        1, 1, 1, 1,
    },
    {   /* deadband = 0 */
        1, 1, 0, 0,
        1, 1, 1,
        1, 0, 1, 1,
        1, 1, 0, 1,
        1, 1, 1, 0,
    },
    {   /* deadband = 1.5 */
        0, 1, 0, 0,
        1, 1, 1,
        1, 0, 1, 1,
        1, 1, 0, 1,
        1, 1, 1, 0,
    },
};

MAIN(recGblCheckDeadbandTest)
{
    unsigned mask;
    double oldval, newval;

    /* Test patterns:
     * 0: step less than deadband (of 1.5)
     * 1: step larger than deadband (of 1.5)
     * 2: no change
     * 3: -0.0 -> +0.0
     * ... all possible combinations of steps
     * between: finite / NaN / -inf / +inf
     */
    t_SetValues[ 0][0] =  1.0;      t_SetValues[ 0][1] = 2.0;
    t_SetValues[ 1][0] =  0.0;      t_SetValues[ 1][1] = 2.0;
    t_SetValues[ 2][0] =  0.0;      t_SetValues[ 2][1] = 0.0;
    t_SetValues[ 3][0] = -0.0;      t_SetValues[ 3][1] = 0.0;
    t_SetValues[ 4][0] =  1.0;      t_SetValues[ 4][1] = epicsNAN;
    t_SetValues[ 5][0] =  1.0;      t_SetValues[ 5][1] = epicsINF;
    t_SetValues[ 6][0] =  1.0;      t_SetValues[ 6][1] = -epicsINF;
    t_SetValues[ 7][0] =  epicsNAN; t_SetValues[ 7][1] = 1.0;
    t_SetValues[ 8][0] =  epicsNAN; t_SetValues[ 8][1] = epicsNAN;
    t_SetValues[ 9][0] =  epicsNAN; t_SetValues[ 9][1] = epicsINF;
    t_SetValues[10][0] =  epicsNAN; t_SetValues[10][1] = -epicsINF;
    t_SetValues[11][0] =  epicsINF; t_SetValues[11][1] = 1.0;
    t_SetValues[12][0] =  epicsINF; t_SetValues[12][1] = epicsNAN;
    t_SetValues[13][0] =  epicsINF; t_SetValues[13][1] = epicsINF;
    t_SetValues[14][0] =  epicsINF; t_SetValues[14][1] = -epicsINF;
    t_SetValues[15][0] = -epicsINF; t_SetValues[15][1] = 1.0;
    t_SetValues[16][0] = -epicsINF; t_SetValues[16][1] = epicsNAN;
    t_SetValues[17][0] = -epicsINF; t_SetValues[17][1] = epicsINF;
    t_SetValues[18][0] = -epicsINF; t_SetValues[18][1] = -epicsINF;

    testPlan(114);

    /* Loop over all tested deadband values */
    for (idbnd = 0; idbnd < NO_OF_DEADBANDS; idbnd++) {

        /* Loop over all test patterns */
        for (itest = 0; itest < NO_OF_PATTERNS; itest++) {
            oldval = t_SetValues[itest][0];
            newval = t_SetValues[itest][1];
            mask = 0;

            recGblCheckDeadband(&oldval, newval, t_Deadband[idbnd], &mask, 1);

            /* Check expected vs. actual test result */
            testOk(t_ExpectedUpdates[idbnd][itest] == mask,
                   "deadband=%2.1f: check for oldvalue=%f newvalue=%f (expected %d, got %d)",
                   t_Deadband[idbnd], t_SetValues[itest][0], t_SetValues[itest][1],
                   t_ExpectedUpdates[idbnd][itest], mask);

            if (mask) {
                testOk((oldval == newval) || (isnan(oldval) && isnan(newval)), "mask set, oldval equals newval");
            } else {
                testOk((oldval == t_SetValues[itest][0]) || (isnan(oldval) && isnan(t_SetValues[itest][0])),
                        "mask not set, oldval unchanged");
            }
        }
    }
    return testDone();
}
