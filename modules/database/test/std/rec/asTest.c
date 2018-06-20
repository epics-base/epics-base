/*************************************************************************\
* Copyright (c) 2015 Brookhaven Science Assoc. as operator of Brookhaven
*               National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 *
 * Test the hooks that autosave uses during initialization
 */

#include "string.h"

#include "epicsString.h"
#include "dbUnitTest.h"
#include "epicsThread.h"
#include "iocInit.h"
#include "dbBase.h"
#include "link.h"
#include "recSup.h"
#include "dbAccess.h"
#include "dbConvert.h"
#include "dbStaticLib.h"
#include "registry.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "osiFileName.h"
#include "initHooks.h"
#include "devSup.h"
#include "errlog.h"

#include "aoRecord.h"
#include "waveformRecord.h"

#include "testMain.h"

epicsShareFunc void testRestore(void);

#include "epicsExport.h"

MAIN(asTest)
{
    testPlan(42);
    testRestore();
    return testDone();
}
