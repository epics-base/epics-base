/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2023 Chris Johns <chris@contemporary.software>
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Author: Chris Johns
 */
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>

#define EPICS_EXPOSE_LIBCOM_MONOTONIC_PRIVATE
#include <epicsStdio.h>
#include "epicsExit.h"
#include "epicsTime.h"
#include "errlog.h"
#include "osdTime.h"
#include "osiNTPTime.h"
#include "osiClockTime.h"
#include "generalTimeSup.h"
#include "iocsh.h"
#include "taskwd.h"

#include "epicsSimpleNtp.h"

#include <iocsh/initHooks.h>

#include <rtems.h>

/*
 * This file is built automatically by EPICS's build system
 */
#if __RTEMS_MAJOR__ < 6

/* Include the code from posix/osdTime.cpp. Better solutions welcome */

#include "../posix/osdTime.cpp"

#else /* __RTEMS_MAJOR__ < 6 */

#include <string.h>

#include <rtems/ntpd.h>
#include <rtems/ntpq.h>

extern "C" {

/*
 * NTPD is configured by a file /etc/ntp.conf. The RTEMS /etc/ directory
 * does not survive a reset so ntp.conf is created each boot.
 *
 * Users can configure ntpd by:
 *
 *  None:
 *     ntpd is not started
 *
 *  BOOTP/DHCP/Init:
 *     The BOOTP server IP address is used if there is a valid BOOTP
 *     record and variable rtemsInit_NTP_server_ip is empty and
 *     rtems_bsdnet_config.ntp_server is set. If the variable
 *     rtems_bsdnet_config.ntp_server is empty and rtemsInit_NTP_server_ip
 *     is set it is used.
 *
 *  NTP Server IP:
 *     The environment variable EPICS_TS_NTP_INET is used as the
 *     address if no configuration file is set. This option takes
 *     priority over BOOTP/DHCP/Init.
 *
 *  Configuration File:
 *     The environment variable EPICS_TS_NTP_CONF_FILE is a path to a
 *     configuration file that is used as is. This allows site specifc
 *     and even board specific configuration support at run time. It
 *     is recommended you provide a site specific configuration file
 *     if you need site specific control. This option takes
 *     priority over NTP Server IP.
 *
 *  The Configuration File option provides a system level means to
 *  configure a site specific ntpd configuration that can be loaded
 *  from a NFS file system when the system starts. This option
 *  provides the ability to implement a site specific configuration of
 *  ntpd.
 *
 *  Notes:
 *    1. ntpd can take a while to synchronize the realtime clock
 *    2. Changes in the environment variables not do reconfigure ntpd
 */

#define NTPD_CONFIG_NONE   0
#define NTPD_CONFIG_BOOTP  1
#define NTPD_CONFIG_NTP_IP 2
#define NTPD_CONFIG_FILE   3

#define osdNTP_NTPD_Fails  4

/*
 * NTPD Runner Interval
 *
 * The period in seconds the ntpd thread checks for a valid
 * configuration.
 */
#define EPICS_NTPD_RUNNER_INTERVAL 1.0

/*
 * NTPD Monitor Intervals
 *
 * The periods are in seconds. The first is the period used when
 * NtpD is not running. The second is the period when running.
 * configuration.
 */
#define EPICS_NTPD_MONITOR_IDLE_INTERVAL   1.0
#define EPICS_NTPD_MONITOR_ACTIVE_INTERVAL 10.0

/*
 * Environment variables
 */
#define EPICS_TS_NTP_INET      "EPICS_TS_NTP_INET"
#define EPICS_TS_NTP_CONF_FILE "EPICS_TS_NTP_CONF_FILE"

typedef ntp_sys_var_data ntpq_rl_data;

/*
 * Declared in rtems_init.c. No header.
 */
extern char rtemsInit_NTP_server_ip[16];

/*
 * For rtems_bsdnet_config.
 */
#ifdef RTEMS_LEGACY_STACK
#include <rtems/rtems_bsdnet.h>
extern struct rtems_bsdnet_config rtems_bsdnet_config;
#endif /* RTEMS_LEGACY_STACK */

 /*
  * NTPD Private data
  */
static struct {
    int          config_source;
    char         config_arg[256]; /* could be a long path */
    char         config_ip[32];
    int          active;
    int          synchronized;
    int          ntpd_active;
    int          ntpd_active_secs;
    int          ntpd_finished;
    float        runner_interval;
    float        monitor_interval;
    epicsEventId monitorEvent;
    epicsEventId runnerEvent;
    epicsMutexId lock;
    int          ntpq_open;
    ntpq_rl_data rl;
    char         last_updated[64];
    char         ntpq_output[2048];
} osdNTPPvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

static void osdNTP_Configure(void);
static void osdNTP_Runner(void *dummy);
static void osdNTP_Monitor(void *dummy);

void osdNTP_Shutdown(void *dummy);

static const char etc_services[] =
    "ntp                123/tcp      # Network Time Protocol  [Dave_Mills] [RFC5905]\n"
    "ntp                123/udp      # Network Time Protocol  [Dave_Mills] [RFC5905]\n";

static void set_rl_last_updated()
{
    struct timespec ts;
    int r = clock_gettime(CLOCK_REALTIME, &ts);
    if (r == 0) {
        struct tm tm;
        localtime_r(&ts.tv_sec, &tm);
        strftime(
            osdNTPPvt.last_updated, sizeof(osdNTPPvt.last_updated),
            "%Y-%m-%d %H:%M:%S", &tm);
    } else {
        strlcpy(osdNTPPvt.last_updated, "unknown", sizeof(osdNTPPvt.last_updated));
    }
}

static void ntpq_open(void)
{
    const char *argv[] = {
        "host",
        "127.0.0.1",
        NULL
    };
    const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
    rtems_ntpq_create(1024);
    int r = rtems_ntpq_query(
        argc, argv, osdNTPPvt.ntpq_output, sizeof(osdNTPPvt.ntpq_output));
    if (r != 0) {
        errlogPrintf("osdNTP: ntpq host 127.0.0.1 failed\n");
    }
}

static bool osdNTP_Get_Active(void)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    bool active = osdNTPPvt.ntpd_active;
    epicsMutexUnlock(osdNTPPvt.lock);
    return active;
}

static void osdNTP_Set_Active(bool active)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    osdNTPPvt.ntpd_active = active ? 1 : 0;
    epicsMutexUnlock(osdNTPPvt.lock);
}

static void osdNTP_Set_ConfigSource(int config_source)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    osdNTPPvt.config_source = config_source;
    epicsMutexUnlock(osdNTPPvt.lock);
}

static int osdNTP_Get_ConfigSource(void)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    int config_source = osdNTPPvt.config_source;
    epicsMutexUnlock(osdNTPPvt.lock);
    return config_source;
}

static bool osdNTP_Configured(void)
{
    return osdNTP_Get_ConfigSource() != NTPD_CONFIG_NONE;
}

static bool osdNTP_Synchronized(void)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    int synchronized = osdNTPPvt.synchronized;
    epicsMutexUnlock(osdNTPPvt.lock);
    return synchronized != 0;
}

static void osdNTP_Set_Synchronized(int synchronized)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    osdNTPPvt.synchronized = synchronized;
    epicsMutexUnlock(osdNTPPvt.lock);
}

static bool osdNTP_Update_ActiveSecs(int secs)
{
    const int active_before_ntpq_open = 15;
    epicsMutexMustLock(osdNTPPvt.lock);
    if (osdNTPPvt.active) {
        int active_secs = osdNTPPvt.ntpd_active_secs;
        osdNTPPvt.ntpd_active_secs += secs;
        /*
         * Has the active time been long enough to check the ntpd
         * port?  The ntpq host command checks.
         */
        if (active_secs <= active_before_ntpq_open
            && osdNTPPvt.ntpd_active_secs > active_before_ntpq_open) {
            epicsMutexUnlock(osdNTPPvt.lock);
            ntpq_open();
            epicsMutexMustLock(osdNTPPvt.lock);
            osdNTPPvt.ntpq_open = 1;
        }
    }
    bool ntpq_is_open = osdNTPPvt.ntpq_open != 0;
    epicsMutexUnlock(osdNTPPvt.lock);
    return ntpq_is_open;
}

static void osdNTP_InitOnce(void *not_used)
{
    (void) not_used;

    osdNTPPvt = { 0 };
    osdNTPPvt.config_source    = NTPD_CONFIG_NONE;
    osdNTPPvt.active           = 1;
    osdNTPPvt.synchronized     = 0;
    osdNTPPvt.ntpd_active      = 1;
    osdNTPPvt.ntpd_active_secs = 0;
    osdNTPPvt.ntpd_finished    = 0;
    osdNTPPvt.runner_interval  = EPICS_NTPD_RUNNER_INTERVAL;
    osdNTPPvt.monitor_interval = EPICS_NTPD_MONITOR_IDLE_INTERVAL;
    osdNTPPvt.monitorEvent     = epicsEventMustCreate(epicsEventEmpty);
    osdNTPPvt.runnerEvent      = epicsEventMustCreate(epicsEventEmpty);
    osdNTPPvt.lock             = epicsMutexCreate();
    osdNTPPvt.ntpq_open        = 0;

    epicsThreadCreate("osdNTP_Runner", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        osdNTP_Runner, nullptr);
    epicsThreadCreate("osdNTP_Monitor", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        osdNTP_Monitor, nullptr);
    epicsAtExit(osdNTP_Shutdown, nullptr);
}

static void osdNTP_Configure(void)
{
    int config_source = NTPD_CONFIG_NONE;
    const char* ev;
    int r;

    /*
     * Add NTP ports to /etc/services
     */
    r = rtems_ntpd_add_etc_services();
    if (r == 0) {
        /*
         * Configuration file? Set the leapfile field in the config file.
         */
        ev = getenv(EPICS_TS_NTP_CONF_FILE);
        if (ev != nullptr) {
            strlcpy(osdNTPPvt.config_arg, "--configfile=", sizeof(osdNTPPvt.config_arg));
            strlcat(osdNTPPvt.config_arg, ev, sizeof(osdNTPPvt.config_arg));
            config_source = NTPD_CONFIG_FILE;
        }

        /*
         * Server IP address?
         */
        if (config_source == NTPD_CONFIG_NONE) {
            ev = getenv(EPICS_TS_NTP_INET);
            if (ev != nullptr) {
                strlcpy(
                    osdNTPPvt.config_ip, "EPICS_TS_NTP_INET: ",
                    sizeof(osdNTPPvt.config_ip));
                strlcpy(osdNTPPvt.config_ip, ev, sizeof(osdNTPPvt.config_ip));
                r = rtems_ntpd_client_pool_config(ev);
                if (r == 0) {
                    config_source = NTPD_CONFIG_NTP_IP;
                }
            }
        }

        /*
         * BOOTP/DHCP/Init?
         */
        if (config_source == NTPD_CONFIG_NONE) {
            ev = nullptr;
#ifdef RTEMS_LEGACY_STACK
            if (rtems_bsdnet_config.ntp_server[0] != nullptr) {
                ev = rtems_bsdnet_config.ntp_server[0];
                strlcpy(
                    osdNTPPvt.config_ip, "BSDNET_CONFIG: ",
                    sizeof(osdNTPPvt.config_ip));
            }
#endif /* RTEMS_LEGACY_STACK */
            if (ev == nullptr && rtemsInit_NTP_server_ip[0] != '\0') {
                ev = &rtemsInit_NTP_server_ip[0];
                strlcpy(
                    osdNTPPvt.config_ip, "RTEMS_INIT: ",
                    sizeof(osdNTPPvt.config_ip));
            }
            if (ev != nullptr) {
                strlcat(osdNTPPvt.config_ip, ev, sizeof(osdNTPPvt.config_ip));
                r = rtems_ntpd_client_pool_config(ev);
                if (r == 0) {
                    config_source = NTPD_CONFIG_BOOTP;
                }
            }
        }
    }

    osdNTP_Set_ConfigSource(config_source);
}

void osdNTP_Shutdown(void *dummy)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    osdNTPPvt.config_source = NTPD_CONFIG_NONE;
    osdNTPPvt.active = 0;
    osdNTPPvt.synchronized = 0;
    osdNTPPvt.ntpq_open = 0;
    epicsMutexUnlock(osdNTPPvt.lock);
    rtems_ntpd_stop();
    rtems_ntpq_destroy();
    epicsEventSignal(osdNTPPvt.runnerEvent);
    epicsEventSignal(osdNTPPvt.monitorEvent);
}

static int osdNTP_RunCommand(const char* activity, int *fails, int argc, char** argv)
{
    osdNTP_Set_Active(true);
    int r = rtems_ntpd_run(argc, argv);
    osdNTP_Set_Active(false);
    if (r != 0) {
        *fails += 1;
        errlogPrintf("osdNTP: ntpd error (fails=%d): %d: %s\n", *fails, r, activity);
    } else {
        *fails = 0;
    }
    return r;
}

static int osdNTP_Run(int *fails)
{
    const char *argv[] = {
        "ntpd",
        "-g",
#if NTP_DEBUG
        "--set-debug-level=2",
#endif
        osdNTPPvt.config_arg,
        nullptr
    };
    const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
    return osdNTP_RunCommand("daemon", fails, argc, (char**) argv);
}

static void osdNTP_Runner(void *dummy)
{
    int ntpd_fails = 0;

    taskwdInsert(0, nullptr, nullptr);

    /*
     * Wait for a valid configuration mode to be set. Restart ntpd if
     * it exits limiting the number of attempts if there is a persistent
     * error.
     */
    for (epicsEventWaitWithTimeout(osdNTPPvt.runnerEvent, osdNTPPvt.runner_interval);
         osdNTPPvt.active && ntpd_fails < osdNTP_NTPD_Fails;
         epicsEventWaitWithTimeout(osdNTPPvt.runnerEvent, osdNTPPvt.runner_interval)) {

        if (osdNTP_Configured()) {
            int r = osdNTP_Run(&ntpd_fails);
            if (r == 0) {
                osdNTPPvt.monitor_interval = EPICS_NTPD_MONITOR_ACTIVE_INTERVAL;
            }
        }
    }

    epicsMutexMustLock(osdNTPPvt.lock);
    osdNTPPvt.ntpd_finished = 1;
    epicsMutexUnlock(osdNTPPvt.lock);

    taskwdRemove(0);
}

static void osdNTP_Monitor(void *dummy)
{
    taskwdInsert(0, nullptr, nullptr);

    for (epicsEventWaitWithTimeout(osdNTPPvt.monitorEvent, osdNTPPvt.monitor_interval);
         osdNTPPvt.active;
         epicsEventWaitWithTimeout(osdNTPPvt.monitorEvent, osdNTPPvt.monitor_interval)) {

        if (osdNTP_Configured()
            && osdNTP_Get_Active()
            && osdNTP_Update_ActiveSecs(osdNTPPvt.monitor_interval)) {
            epicsMutexMustLock(osdNTPPvt.lock);
            rtems_ntpd_get_sys_vars(&osdNTPPvt.rl);
            set_rl_last_updated();
            epicsMutexUnlock(osdNTPPvt.lock);
        }

        osdNTP_Set_Synchronized(rtems_ntpd_is_synchronized(&osdNTPPvt.rl));

        if (!osdNTP_Configured()) {
            osdNTP_Configure();
        }
    }

    osdNTP_Set_Synchronized(0);

    taskwdRemove(0);
}

void osdTimeRegister(void)
{
    /* Init NTP first so it can be used to sync ClockTime */
    NTPTime_Init(100);
    ClockTime_Init(CLOCKTIME_SYNC);

    osdMonotonicInit();
}

int osdNTPGet(struct timespec *ts)
{
    int r;
    if (osdNTP_Synchronized()) {
        r = clock_gettime(CLOCK_REALTIME, ts);
    } else {
        r = -1;
    }
    return r;
}

void osdNTPInit(void)
{
    printf("ntpd: RTEMS service init\n");
    epicsThreadOnce(&onceId, osdNTP_InitOnce, nullptr);
}

void osdNTPReport(void)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    int config_source = osdNTPPvt.config_source;
    int synchronized = osdNTPPvt.synchronized;
    bool ntpd_active = osdNTPPvt.ntpd_active == 1;
    int ntpd_active_secs = osdNTPPvt.ntpd_active_secs;
    bool ntpq_open = osdNTPPvt.ntpq_open == 1;
    epicsMutexUnlock(osdNTPPvt.lock);
    printf("RTEMS NTP Status:\n");
    printf(" config type = ");
    if (config_source <= NTPD_CONFIG_FILE) {
        static const char* config_str[4] = {
            "NTPD_CONFIG_NONE",
            "NTPD_CONFIG_BOOTP",
            "NTPD_CONFIG_NTP_IP",
            "NTPD_CONFIG_FILE"
        };
        printf("%s\n", config_str[config_source]);

    } else {
        printf("INVALID (%i)\n", config_source);
    }
    if (osdNTPPvt.config_arg[0] != '\0') {
        printf(" config arg = %s\n", osdNTPPvt.config_arg);
    }
    if (osdNTPPvt.config_ip[0] != '\0') {
        printf(" config ip = %s\n", osdNTPPvt.config_ip);
    }
    printf(" ntpd active = %s\n", ntpd_active ? "yes" : "no");
    printf(" ntpd active sec = %d\n", ntpd_active_secs);
    printf(" ntpq open = %s\n", ntpq_open ? "yes" : "no");
    printf(" status last updated = %s\n", osdNTPPvt.last_updated);
    printf(" synchronized = %s\n", synchronized ? "yes" : "no");
    printf(" status = (%04x) %s\n", osdNTPPvt.rl.status, osdNTPPvt.rl.status_str);
    printf(" version = %s\n", osdNTPPvt.rl.version);
    printf(" processor = %s\n", osdNTPPvt.rl.processor);
    printf(" system = %s\n", osdNTPPvt.rl.system);
    printf(" leap = %d\n", (int) osdNTPPvt.rl.leap);
    printf(" stratum = %d\n", (int) osdNTPPvt.rl.stratum);
    printf(" precision = %i\n", (int) osdNTPPvt.rl.precision);
    printf(" rootdelay = %f\n", osdNTPPvt.rl.rootdelay);
    printf(" rootdisp = %f\n", osdNTPPvt.rl.rootdisp);
    printf(" refid = %s\n", osdNTPPvt.rl.refid);
    printf(" reftime = %" PRIu64 ".%" PRIu64"\n",
           osdNTPPvt.rl.reftime_sec, osdNTPPvt.rl.reftime_nsec);
    printf(" clock = %" PRIu64 ".%" PRIu64 "\n",
           osdNTPPvt.rl.clock_sec, osdNTPPvt.rl.clock_nsec);
    printf(" peer = %d\n", osdNTPPvt.rl.peer);
    printf(" tc = %d\n", osdNTPPvt.rl.tc);
    printf(" mintc = %d\n", osdNTPPvt.rl.mintc);
    printf(" offset = %f\n", osdNTPPvt.rl.offset);
    printf(" frequency = %f\n", osdNTPPvt.rl.frequency);
    printf(" sys_jitter = %f\n", osdNTPPvt.rl.sys_jitter);
    printf(" clk_jitter = %f\n", osdNTPPvt.rl.clk_jitter);
    printf(" clk_wander = %f\n", osdNTPPvt.rl.clk_wander);
    printf(" tai = %d\n", osdNTPPvt.rl.tai);
    printf(" leapsec = %" PRIu64 "\n", osdNTPPvt.rl.leapsec);
    printf(" expire = %" PRIu64 "\n", osdNTPPvt.rl.expire);
    /*
     * Query the peers. We need to be able to see what ntpd is using
     * as a time source.
     */
    printf("Peers:\n");
    if (ntpq_open) {
        const char *argv[] = {
            "peers",
            nullptr
        };
        const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
        int r = rtems_ntpq_query(
            argc, argv, osdNTPPvt.ntpq_output,
            sizeof(osdNTPPvt.ntpq_output));
        if (r == 0) {
            printf(osdNTPPvt.ntpq_output);
        } else {
            printf("%s\n", rtems_ntpq_error_text());
        }
    } else {
        printf("ntpq: not open\n");
    }
}

int osdTickGet(void)
{
    return rtems_clock_get_ticks_since_boot();
}

int osdTickRateGet(void)
{
    return rtems_clock_get_ticks_per_second();
}

/*
 * Use reentrant versions of time access
 */
int epicsTime_gmtime ( const time_t *pAnsiTime, struct tm *pTM )
{
    struct tm * pRet = gmtime_r ( pAnsiTime, pTM );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

int epicsTime_localtime ( const time_t *clock, struct tm *result )
{
    struct tm * pRet = localtime_r ( clock, result );
    if ( pRet ) {
        return epicsTimeOK;
    }
    else {
        return errno;
    }
}

} // extern "C"

static const iocshArg ntpStartArg0 = { "command (ip or config)", iocshArgString };
static const iocshArg ntpStartArg1 = { "value (addr or file)", iocshArgString };
static const iocshArg * const ntpStartArgs[2] = { &ntpStartArg0, &ntpStartArg1 };
static const iocshFuncDef ntpStartFuncDef = { "NTPTime_Start", 2, ntpStartArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "Start NTP with IP or configuration file"
#endif
                                           };

static void ntpStartFunc(const iocshArgBuf *args) {
    if (osdNTP_Configured()) {
        printf("error: NTP: start: already configured\n");
        iocshSetError(1);
        return;
    }
    std::string arg0 = args[0].sval;
    const char* env = nullptr;
    if (arg0 == "ip") {
        env = EPICS_TS_NTP_INET;
    } else if (arg0 == "config") {
        env = EPICS_TS_NTP_CONF_FILE;
    } else {
        printf("error: NTP: start: invalid command (ip or config)\n");
        iocshSetError(1);
    }
    printf("NTP: setenv %s -> %s\n", args[1].sval, env);
    auto r = setenv(env, args[1].sval, 1);
    if (r != 0) {
        printf("error: NTP: start: cannot set env: %s\n", env);
        iocshSetError(1);
    }
    osdNTP_Configure();
    iocshSetError(0);
}

static const iocshArg ntpWaitForSyncArg0 = { "seconds", iocshArgInt };
static const iocshArg * const ntpWaitForSyncArgs[1] = { &ntpWaitForSyncArg0 };
static const iocshFuncDef ntpWaitForSyncFuncDef = { "NTPTime_SyncWait", 1, ntpWaitForSyncArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "Wait for NTP to synchronize with timeout, 0 is forever"
#endif
                                           };

static void ntpWaitForSyncFunc(const iocshArgBuf *args) {
    if (!osdNTP_Configured()) {
        printf("error: NTP: not configured\n");
        return;
    }
    constexpr int wait_for = 500;
    int msecs = args[0].ival * 1000;
    printf("RTEMS NTP: waiting %i seconds for sync\n", args[0].ival);
    while (!osdNTP_Synchronized()) {
        epicsThreadSleep(wait_for / 1000.0);
        if (msecs > 0) {
            if (msecs > wait_for) {
                msecs -= wait_for;
            } else {
                msecs = 0;
            }
            if (msecs == 0) {
                printf("error: NTP: timeout, not synchronized\n");
                iocshSetError(1);
                return;
            }
        }
    }
    printf("NTP is synchronized\n");
    iocshSetError(0);
}

static const iocshArg ntpUpdateArg0 = { "host ip", iocshArgString };
static const iocshArg * const ntpUpdateArgs[1] = { &ntpUpdateArg0 };
static const iocshFuncDef ntpUpdateFuncDef = { "NTPTime_Update", 1, ntpUpdateArgs
#ifdef IOCSHFUNCDEF_HAS_USAGE
                                            , "Update the time via NTP."
#endif
                                           };

static void ntpUpdateFunc(const iocshArgBuf *args) {
    if (osdNTP_Synchronized()) {
        printf("error: NTP: no update as NTP has synchronized the time\n");
        iocshSetError(2);
        return;
    }
    if (osdNTP_Configured()) {
        printf("warning: NTP: NTP is configured but not synchronized\n");
    }
    if (args[0].sval == nullptr) {
        printf("error: NTP: no host ip address provided\n");
        iocshSetError(1);
    }
    struct timespec now;
    int r = epicsSimpleNtpGetTime(args[0].sval, &now);
    if (r != 0) {
        printf("error: NTP: update from %s failed\n", args[0].sval);
        iocshSetError(1);
        return;
    }
    r = clock_settime(CLOCK_REALTIME, &now);
    if (r < 0) {
        printf ("error: NTP: clock set time failed: %s\n", strerror(errno));
        iocshSetError(1);
        return;
    }
    char buffer[100];
    strftime(buffer, sizeof buffer, "%D %T", gmtime(&now.tv_sec));
    printf("NTP update: time is %s.%09ld UTC\n", buffer, now.tv_nsec);
    iocshSetError(0);
}

static int staticTimeRegister(void)
{
    osdTimeRegister();
    iocshRegister(&ntpStartFuncDef, ntpStartFunc);
    iocshRegister(&ntpWaitForSyncFuncDef, ntpWaitForSyncFunc);
    iocshRegister(&ntpUpdateFuncDef, ntpUpdateFunc);
    return 0;

}
static int done = staticTimeRegister();

#endif /* __RTEMS_MAJOR__ < 6 */
