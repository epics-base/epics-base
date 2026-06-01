/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <epicsRtemsInit.h>

#include "envDefs.h"

static std::string hostname;

/*
 * Set working directory (workdir)
 *
 * The working directory is evaluated in the following order.
 *
 * 1. If RTEMS_INIT_WORKDIR is present user it as is. An init module
 *    is setting the working directory bypassing any boot settings.
 *
 * 2. If RTEMS_BOOT_CMD_LINE is present it is `path/script` path and
 *    file name pair. The working directory is the path component.
 *
 * 3. If RTEMS_NFS_MOUNT_PATH is defined extract the boot command line
 *    and then following the processing in item 2 of this list.
 */
static void setWorkingDirectory() {

    std::string cmdline;
    auto envp = getenv("RTEMS_INIT_WORKDIR");
    if (envp != nullptr) {
        cmdline = envp;
    } else {
        envp = getenv("RTEMS_BOOT_CMD_LINE");
        if (envp != nullptr) {
            cmdline = envp;
        }
        if (cmdline.empty()) {
            envp = getenv("RTEMS_NFS_MOUNT_PATH");
            if (envp != nullptr) {
                cmdline = envp;
            }
            auto colon = cmdline.find_first_of(':');
            if (colon != std::string::npos) {
                cmdline.erase(0, colon + 1);
                colon = cmdline.find_first_of(':');
                if (colon != std::string::npos) {
                    cmdline[colon] = '/';
                }
            }
            setenv("RTEMS_BOOT_CMD_LINE", cmdline.c_str(), 1);
        }
    }
    std::filesystem::path workdir = cmdline;
    workdir.remove_filename();
    if (workdir.empty()) {
        workdir = "/";
    }
    std::cout << "IOC work directory: " << workdir << std::endl;
    std::filesystem::current_path(workdir);
}

static void iocShellPrompt() {
    char tmp[1024];
    std::memset(tmp, 0, sizeof(tmp));
    gethostname(tmp, sizeof(tmp) - 1);
    hostname = tmp;
    std::ostringstream oss;
    oss << hostname << " > ";
    epicsEnvSet("IOCSH_PS1", oss.str().c_str());
    epicsEnvSet("IOC_NAME", hostname.c_str());
}

static int rtemsIOCInitialize() {
    /*
     * Create a reasonable environment
     */
    setenv("TERM", "xterm", 1);
    setenv("IOCSH_HISTSIZE", "20", 1);
    setWorkingDirectory();
    iocShellPrompt();
    tzset();
    return 0;
}

void epicRtemsInit_ioc() {
    epicsRtemsInitRegisterHandler(
        "system", "ioc", rtemsInit_Order_ioc, true, rtemsIOCInitialize);
}
