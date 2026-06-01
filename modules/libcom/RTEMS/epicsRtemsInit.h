/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef EPICS_RTEMS_INIT_H
#define EPICS_RTEMS_INIT_H

#include <functional>

/*
 * Initialise start order bands. A band starts at list order number.
 * You can create relative start orders by adding an offset.
 */
enum epicsRtemsInit_bands {
    rtemsInit_Order_early= 10,
    rtemsInit_Order_filesystems = 200,
    rtemsInit_Order_pre_net_services= 100,
    rtemsInit_Order_net = 500,
    rtemsInit_Order_net_filesystems = 600,
    rtemsInit_Order_post_net_services= 800,
    rtemsInit_Order_commands = 900,
    rtemsInit_Order_ioc = 1000,
};

/*
 * Returns an error code where zero is not error and any non-zero
 * value is an error.
 */
using epicsRtemsInitHandler = std::function<int()>;

/*
 * Static object registration.
 *
 * You can use this object in your IOC to add new RTEMS init modules
 * positioning them relative to system domain handler automatically
 * installed.
 *
 * You need to link an object file with a epicsRtemsInitRegister
 * static object into your IOC to be seen. This can be achieved by
 * linking the object file directly on the linker command line or
 * having your IOC reference a symbol in the same object file as the
 * static object.
 *
 * The domain field lets you override a system domain initialization
 * handler. System domain handlers reside in the `system` domain. If a
 * handler has the same name and start order as a system domain
 * handler it will replace it. Handlers with the same domain, name and
 * order as a registered handler will abort initialization early and
 * reboot the system. This is a development error.
 *
 * This example sets environment variables to configure a network
 * interface statically from an IOC:
 *
 * static int myInitialize() {
 *    setenv("RTEMS_NET_HOSTNAME", "myhost", 1);
 *    setenv("RTEMS_NET_IFACE_2", "cgem2", 1);
 *    setenv("RTEMS_NET_IF_2_IP_ADDR", "172.16.100.123", 1);
 *    setenv("RTEMS_NET_IF_2_NETMASK", "172.16.100.255", 1);
 *    return 0;
 * }
 *
 * static epicsRtemsInitRegister myNet("myioc", "myinit", 50, myInitialize);
 *
 * The example is a useful use case for unit testing initialization.
 */

struct epicsRtemsInitRegister {
    epicsRtemsInitRegister(
        const char* domain, const char* name, const size_t order, bool enabled,
        const epicsRtemsInitHandler& handler);
};

/*
 * Register a handler. This is only useful to the system domain init
 * modules.
 */
int epicsRtemsInitRegisterHandler(
    const char* domain, const char* name, const size_t order, bool enabled,
    const epicsRtemsInitHandler& handler);


/*
 * Enable and disable handlers
 */
void epicsRtemsInit_enable(const char* domain, const char* name);
void epicsRtemsInit_disable(const char* domain, const char* name);

/*
 * Module register
 */
void epicRtemsInit_cmds();
void epicRtemsInit_debugger();
void epicRtemsInit_filesys();
void epicRtemsInit_ioc();
void epicRtemsInit_log();
void epicRtemsInit_net();
void epicRtemsInit_nfs();
void epicRtemsInit_ntp();

/*
 * Hook to allow app specific FS setup using mem file system
 */
extern "C" int epicsRtemsMountLocalFilesystem(const char** argv);

#endif
