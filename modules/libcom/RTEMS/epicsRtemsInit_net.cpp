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

#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/route.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <epicsRtemsInit.h>

#include <rtems.h>

#include <rtems/bsd/bsd.h>
#include <machine/rtems-bsd-rc-conf.h>
#include <machine/rtems-bsd-rc-conf-env.h>

static constexpr bool net_verbose = false;

/*
 * Block until the named interface reports link up via an RTM_IFINFO
 * routing message, or until timeout_secs elapses.
 *
 * We check the media state before using route_sock must the
 * RTM_IFINFO event has been missed.
 *
 * Returns 0 if link came up, -1 on timeout or an error code.
 */
static int wait_for_link_up(int sock, const char *ifname, int timeout_secs) {
    std::cout << ifname << ": waiting for link up (timeout " << timeout_secs << "s)... "
              << std::flush;

    /*
     * See if the link is already up?
     */
    struct ifmediareq ifmr;
    memset(&ifmr, 0, sizeof(ifmr));
    strlcpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));
    int r = ioctl(sock, SIOCGIFMEDIA, (caddr_t)&ifmr);
    if (r < 0) {
        std::cout << "error: " << ifname << ": socket ioctl: "
                  << std::strerror(errno) << std::endl;
        return -1;
    }
    if (((ifmr.ifm_status & IFM_AVALID) != 0) &&
        ((ifmr.ifm_status & IFM_ACTIVE) != 0)) {
        std::cout << "up" << std::endl;
        return 0;
    }

    struct timeval tv = { .tv_sec = timeout_secs, .tv_usec = 0 };
    r = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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
                    return 0;
                }
            }
        }
    }

    /* recv returned <= 0: SO_RCVTIMEO expired (EAGAIN) or socket error */
    std::cout << "timeout" << std::endl;
    return -1;
}

/*
 * Block until the named interface reports an IP address.
 */
static int wait_for_link_ip_addr(int sock, const char *ifname, int timeout_secs) {
    std::cout << ifname << ": waiting for link address (timeout "
              << timeout_secs << "s)... "
              << std::flush;
    int timeout_50msec = timeout_secs * (1000 / 50);
    while (timeout_50msec > 0) {
        /*
         * See if the interface as a valid address. The state of the media
         * does not concern us here as EPICS will be able to handle that.
         */
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        strlcpy(ifr.ifr_name, ifname, IFNAMSIZ);
        int r = ioctl(sock, SIOCGIFADDR, &ifr);
        if (r == 0) {
            std::cout << "up" << std::endl;
            return 0;
        }
        usleep(50 * 1000);
        --timeout_50msec;
    }
    std::cout << "timeout" << std::endl;
    return -1;
}

static int wait_for_link_up_and_ip_addr(const char *ifname, int timeout_secs) {
    int sock = socket(PF_ROUTE, SOCK_RAW, 0);
    if (sock < 0) {
        std::cout << "error: net: route sock open: " << std::strerror(errno)
                  << std::endl;
        return -1;
    }
    int r = wait_for_link_up(sock, ifname, timeout_secs);
    if (r == 0) {
        r = wait_for_link_ip_addr(sock, ifname, timeout_secs);
    }
    close(sock);
    return r;
}

static void rtemsNetMakeHosts() {
    std::ofstream hosts("/etc/hosts", std::ios::binary);
    hosts << "127.0.0.1       localhost" << std::endl;
}

static int rtemsNetInitialize() {
    auto r = rtems_bsd_rc_conf_from_env(true, 210, 20, net_verbose);
    if (r < 0) {
        std::cout << "error: net: initialize networking: creating /etc/rc.conf"
                  << std::endl;
        return 1;
    }
    rtems_bsd_resolv_conf_from_env(net_verbose);
    rtemsNetMakeHosts();
    auto sc = rtems_bsd_initialize();
    if (sc != RTEMS_SUCCESSFUL) {
        std::cout << "error: net: initialize networking: " << rtems_status_text(sc)
                  << std::endl;
        return 2;
    }
    r = rtems_bsd_run_etc_rc_conf(30, net_verbose);
    if (r < 0) {
        std::cout << "error: net: start networking: " << std::strerror(errno)
                  << std::endl;
        return 3;
    }
    for (int unit = 1; unit <= 4; ++unit) {
        std::ostringstream oss;
        oss << "RTEMS_NET_IFACE_" << unit;
        auto env = getenv(oss.str().c_str());
        if (env != nullptr && env[0] != '\0') {
            wait_for_link_up_and_ip_addr(env, 20);
        }
    }
    return 0;
}

void epicRtemsInit_net() {
    epicsRtemsInitRegisterHandler(
        "system", "net", rtemsInit_Order_net, true, rtemsNetInitialize);
}
