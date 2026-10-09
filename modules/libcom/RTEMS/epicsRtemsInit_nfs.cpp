/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#include <epicsRtemsInit.h>

#include <rtems.h>
#include <rtems/libio.h>

static int rtemsNFSInitialize() {
    /*
     * Split argument string of form nfs_server:nfs_export:<path>
     * The nfs_export component will be used as:
     *      - the path to the directory exported from the NFS server
     *      - the local mount point
     *      - a prefix of <path>
     * For example, the argument string:
     *       romeo:/export/users:smith/ioc/iocexample/st.cmd
     * would:
     *       - mount /export/users from NFS server romeo on /export/users
     *       - chdir to /export/users/smith/ioc/iocexample
     *       - read commands from st.cmd
     */
    auto envp = getenv("RTEMS_NFS_MOUNT_PATH");
    if (envp != nullptr) {
        std::string env = envp;
        auto last_colon = env.find_last_of(':');
        auto nfs_source = env.substr(0, last_colon);
        auto first_colon = nfs_source.find_first_of(':');
        auto nfs_target = nfs_source.substr(first_colon + 1);
        std::cout << "mount: nfs: "
                  << nfs_source << " -> " << nfs_target
                  << std::endl;
        std::filesystem::create_directories(nfs_target);
        auto r = mount(
            nfs_source.c_str(), nfs_target.c_str(),
            "nfs", RTEMS_FILESYSTEM_READ_WRITE, "nfsv4,minorversion=1");
        if (r < 0) {
            std::cout << "error: mount: nfs: " << std::strerror(errno)
                      << std::endl;
            return 1;
        }
    }
    return 0;
}

void epicRtemsInit_nfs() {
    epicsRtemsInitRegisterHandler(
        "system", "nfs", rtemsInit_Order_net_filesystems, true, rtemsNFSInitialize);
}
