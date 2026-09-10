/*************************************************************************\
* Copyright (c) 2026 Chris Johns
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <iostream>

#include <string.h>

#include <epicsRtemsInit.h>

char rtemsInit_NTP_server_ip[16] = "";

static int rtemsNTPInitialize() {
    auto envp = getenv("RTEMS_NET_NTP_IP");
    if (envp != nullptr) {
        ::strlcpy(
            rtemsInit_NTP_server_ip, envp, sizeof(rtemsInit_NTP_server_ip));
        std::cout << "NTP IP address: " << rtemsInit_NTP_server_ip
                  << std::endl;
    }
    return 0;
}

void epicRtemsInit_ntp() {
    epicsRtemsInitRegisterHandler(
        "system", "ntp.ip", rtemsInit_Order_post_net_services + 50,
        false, rtemsNTPInitialize);
}
