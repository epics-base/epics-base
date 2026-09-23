/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <epicsRtemsInit.h>

#include "errlog.h"

static int rtemsLogResetInitialize() {
    void rtems_bsp_reset_cause(char *buf, size_t capacity) __attribute__((weak));
    void (*reset_cause_p)(char *buf, size_t capacity) = rtems_bsp_reset_cause;

    if (reset_cause_p) {
        char buf[80];
        reset_cause_p(buf, sizeof(buf));
        errlogPrintf("Startup after %s\n", buf);
    }
    else {
        errlogPrintf("Startup\n");
    }
    errlogFlush();
    return 0;
}

static int logFlushErrorLog() {
    errlogFlush();
    return 0;
}

void epicRtemsInit_log() {
    epicsRtemsInitRegisterHandler(
        "system", "log.reset", rtemsInit_Order_pre_net_services + 100,
        true, rtemsLogResetInitialize);
    epicsRtemsInitRegisterHandler(
        "system", "log.err.flush", rtemsInit_Order_ioc - 1, true, logFlushErrorLog);
}
