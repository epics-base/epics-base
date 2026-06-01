/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <iostream>

#include <rtems/rtems-debugger.h>
#include <rtems/rtems-debugger-remote-tcp.h>

#include <epicsRtemsInit.h>

#include "iocsh.h"

static const iocshArg debuggerStartArg0 = { "port", iocshArgInt };
static const iocshArg * const debuggerStartArgs[1] = { &debuggerStartArg0 };
static const iocshFuncDef debuggerStartFuncDef = { "Debug_Start", 1, debuggerStartArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "Start the debugger to wait for a connection"
#endif
                                           };

static void debuggerStartFunc(const iocshArgBuf *args) {
    if (rtems_debugger_running()) {
        std::cout << "error: debugger already running" << std::endl;
        iocshSetError(1);
        return;
    }

    rtems_printer printer;
    const char* remote = "tcp";
    const char* device = "1122";
    int timeout = RTEMS_DEBUGGER_TIMEOUT;
    rtems_task_priority priority = 1;

    std::cout << "Starting debugger: " << remote << ':' << device << std::endl;

    rtems_print_printer_fprintf(&printer, stdout);

    int r = rtems_debugger_start(remote, device, timeout, priority, &printer);
    if (r < 0) {
        std::cout << "error: debugger start failed" << std::endl;
        iocshSetError(1);
        return;
    }
    iocshSetError(0);
}

static const iocshArg breakWaitArg0 = { "wait (0 or 1)", iocshArgInt };
static const iocshArg * const breakWaitArgs[1] = { &breakWaitArg0 };
static const iocshFuncDef breakWaitFuncDef = { "Debug_Break", 1, breakWaitArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "Break or wait for the debugger"
#endif
                                           };

static void breakWaitFunc(const iocshArgBuf *args) {
    const bool wait = args[0].ival != 0;
    std::cout << "Debugger: break" << std::endl;
    rtems_debugger_break(wait);
}

static int rtemsDebuggerInitialize() {
    rtems_debugger_register_tcp_remote();
    iocshRegister(&debuggerStartFuncDef, debuggerStartFunc);
    iocshRegister(&breakWaitFuncDef, breakWaitFunc);
    return 0;
}

void epicRtemsInit_debugger() {
    epicsRtemsInitRegisterHandler(
        "system", "debugger", rtemsInit_Order_post_net_services + 40,
        false, rtemsDebuggerInitialize);
}
