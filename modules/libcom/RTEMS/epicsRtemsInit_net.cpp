/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/socket.h>

#include <epicsRtemsInit.h>

#include <rtems.h>

#include <rtems/bsd/bsd.h>
#include <machine/rtems-bsd-rc-conf.h>
#include <machine/rtems-bsd-rc-conf-env.h>

static constexpr bool net_verbose = false;

/*
 * Block until the named interface reports link up via an RTM_IFINFO routing
 * message, or until timeout_secs elapses.
 *
 * route_sock must already be open (opened before the interface was configured
 * so that no RTM_IFINFO event can be missed).
 *
 * This function was modeled after the RTM_IFINFO handling in
 * rtems-libbsd/dhcpcd/if-bsd.c (manage_link).  There's a
 * simpler implementation in a test that uses a sleep loop in
 * rtems-libbsd/testsuite/include/rtems/bsd/test/default-init.h, but
 * we chose this more responsive event based implementation.
 *
 * Returns 0 if link came up, -1 on timeout or an error code.
 */
static int wait_for_link_up(const char *ifname, int timeout_secs) {
    int sock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (sock < 0) {
        std::cout << "error: net: route sock open: " << std::strerror(errno)
                  << std::endl;
        return 3;
    }

    std::cout << ifname << ": waiting for link (timeout " << timeout_secs << "s)... "
              << std::flush;

    struct timeval tv = { .tv_sec = timeout_secs, .tv_usec = 0 };
    int r = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    if (r == 0) {
        while (true) {
            char buf[sizeof(struct if_msghdr) + sizeof(struct sockaddr_dl)];
            auto n = recv(sock, buf, sizeof(buf), 0);
            if (n <= 0) {
                break;
            }
            struct rt_msghdr* rtm = reinterpret_cast<struct rt_msghdr*>(buf);
            if (rtm->rtm_type == RTM_IFINFO) {
                struct if_msghdr* ifm = reinterpret_cast<struct if_msghdr*>(buf);
                char name[IFNAMSIZ];
                if (if_indextoname(ifm->ifm_index, name) != nullptr &&
                    strcmp(name, ifname) == 0 &&
                    ifm->ifm_data.ifi_link_state == LINK_STATE_UP) {
                    std::cout << "up" << std::endl;
                    close(sock);
                    return 0;
                }
            }
        }
    }

    /* recv returned <= 0: SO_RCVTIMEO expired (EAGAIN) or socket error */
    std::cout << "timeout" << std::endl;
    close(sock);
    return -1;
}

static void rtemsNetMakeHosts() {
    std::ofstream hosts("/etc/hosts", std::ios::binary);
    hosts << "127.0.0.1       localhost" << std::endl;
}

static int rtemsNetInitialize() {
    rtems_bsd_rc_conf_from_env(true, 210, 20, net_verbose);
    rtems_bsd_resolv_conf_from_env(net_verbose);
    rtemsNetMakeHosts();
    auto sc = rtems_bsd_initialize();
    if (sc != RTEMS_SUCCESSFUL) {
        std::cout << "error: net: initialize networking: " << rtems_status_text(sc)
                  << std::endl;
        return 1;
    }
    auto r = rtems_bsd_run_etc_rc_conf(30, net_verbose);
    if (r < 0) {
        std::cout << "error: net: start networking: " << std::strerror(errno)
                  << std::endl;
        return 2;
    }
    for (int unit = 1; unit <= 4; ++unit) {
        std::ostringstream oss;
        oss << "RTEMS_NET_IFACE_" << unit;
        auto env = getenv(oss.str().c_str());
        if (env != nullptr && env[0] != '\0') {
            wait_for_link_up(env, 20);
        }
    }
    return 0;
}

void epicRtemsInit_net() {
    epicsRtemsInitRegisterHandler(
        "system", "net", rtemsInit_Order_net, true, rtemsNetInitialize);
}
