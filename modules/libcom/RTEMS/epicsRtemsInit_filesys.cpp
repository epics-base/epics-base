/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <filesystem>
#include <fstream>
#include <iostream>

#include <epicsRtemsInit.h>

#include "epicsMemFs.h"

static int rtemsFilesysLocalInitialize() {
    const char* argv[3] = { nullptr, nullptr, nullptr };
    auto r = epicsRtemsMountLocalFilesystem(argv);
    if (r == 0 && argv[1] != nullptr) {
        setenv("RTEMS_INIT_WORKDIR", argv[1], 1);
    }
    return 0;
}

static int rtemsFilesysRootInitialize() {
    try {
        std::filesystem::create_directory("/etc");
        std::ofstream passwd("/etc/passwd");
        passwd << "root::0:0::::" << std::endl;
        passwd.close();
        std::ofstream group("/etc/group");
        group << "root::0:" << std::endl;
        group.close();
    } catch (std::exception& e) {
        std::cout << "error: init: root fs: " << e.what() << std::endl;
    }
    std::cout << "File System: root set up" << std::endl;
    return 0;
}

void epicRtemsInit_filesys() {
    epicsRtemsInitRegisterHandler(
        "system", "filesys.root", rtemsInit_Order_filesystems + 10,
        true, rtemsFilesysRootInitialize);
    epicsRtemsInitRegisterHandler(
        "system", "filesys.local", rtemsInit_Order_filesystems + 50,
        true, rtemsFilesysLocalInitialize);
}
