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
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include "taskwd.h"

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
 *  BOOTP/DHCP:
 *     The BOOTP server IP address is used if there is a valid BOOTP
 *     record. This is currently not supported because it is not
 *     implemented with the libbsd stack.
 *
 *  NTP Server IP:
 *     The environment variable EPICS_TS_NTP_INET is used as the
 *     address if no configuration file is set.
 *
 *  Configuration File:
 *     The environment variable EPICS_TS_NTP_CONF_FILE is a path to a
 *     configuration file that is used as is. This allows site specifc
 *     and even board specific configuration support at run time. It
 *     is recommended you provide a site specific configuration file
 *     if you need site specific control.
 *
 *  The Configuration File option provides a system level means to
 *  configure a site specific ntpd configuration that can be loaded
 *  from a NFS file system when the system starts. This option
 *  provides the ability to implement a site specific configuration of
 *  ntpd.
 *
 *  Leap Seconds:
 *
 *  A leap second file can be specified with
 *  EPICS_TS_NTP_LEAP_SECONDS_FILE. If set the file is used else the
 *  default built into this code is used. A site can update the leap
 *  second file without rebuilding the software. It is recommended you
 *  provide a site specific leap seconds file.
 *
 *  Notes:
 *    1. ntpd can take a while to synchronize the realtime clock
 *    2. Changes in the environment variables restarts ntpd
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
#define EPICS_NTPD_MONITOR_IDLE_INTERVAL   5.0
#define EPICS_NTPD_MONITOR_ACTIVE_INTERVAL 60.0

/*
 * Environment variables
 */
#define EPICS_TS_NTP_INET              "EPICS_TS_NTP_INET"
#define EPICS_TS_NTP_CONF_FILE         "EPICS_TS_NTP_CONF_FILE"
#define EPICS_TS_NTP_LEAP_SECONDS_FILE "EPICS_TS_NTP_LEAP_SECONDS_FILE"

/*
 * The variable list as returned by the `rl` ntpq command. The output is:
 *
 * associd=0 status=0615 leap_none, sync_ntp, 1 event, clock_sync,
 * version="ntpd 4.2.8p12", processor="PowerPC/Generic (classic FPU)",
 * system="RTEMS/6.0.0", leap=00, stratum=5, precision=-19,
_* rootdelay=105.672, rootdisp=108.337, refid=10.1.5.8,
 * reftime=e822a1a2.38a4c49d  Wed, May 31 2023 18:48:34.221,
 * clock=e822a277.cfbbf886  Wed, May 31 2023 18:52:07.811, peer=56320, tc=6,
 * mintc=3, offset=-0.408622, frequency=+32.120, sys_jitter=0.000000,
 * clk_jitter=4.856, clk_wander=0.002, tai=37, leapsec=201701010000,
 * expire=202306280000
 */
typedef struct {
    char last_updated[32];
    unsigned int associd;
    unsigned int status;
    char leap_status[32];
    char sync_status[32];
    char clock_status[32];
    char version[32];
    char processor[32];
    char system[32];
    int leap;
    int stratum;
    float precision;
    float rootdelay;
    float rootdisp;
    char refid[32];
    uint64_t reftime_sec;
    uint64_t reftime_nsec;
    uint64_t clock_sec;
    uint64_t clock_nsec;
    int peer;
    int tc;
    int mintc;
    float offset;
    float frequency;
    float sys_jitter;
    float clk_jitter;
    float clk_wander;
    int tai;
    uint64_t leapsec;
    uint64_t expire;
} ntpq_rl_data;

 /*
  * NTPD Private data
  */
static struct {
    int          config_source;
    char         ev_leap_seconds[256]; /* could be a long path */
    char         ev_conf_file[256]; /* could be a long path */
    char         ev_ntp_ip[32];
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
    char         ntpq_output[2048];
    char         ntpq_report_output[2048];
    ntpq_rl_data rl;
} osdNTPPvt;

static epicsThreadOnceId onceId = EPICS_THREAD_ONCE_INIT;

static void osdNTP_Runner(void *dummy);
static void osdNTP_Monitor(void *dummy);

void osdNTP_Shutdown(void *dummy);

/*
 * Default ntp.conf template.
 *
 * It is recommended you provide a site specific configuration file if
 * you need site specific control.
 */
static const char etc_ntp_conf[] =
    "tos minclock 3 maxclock 6\n"
    "restrict default limited kod nomodify notrap noquery nopeer\n"
    "restrict source  limited kod nomodify notrap noquery\n"
    "restrict 10.0.0.0 mask 255.0.0.0\n"
    "restrict 172.16.0.0 mask 255.240.0.0\n"
    "restrict 192.168.0.0 mask 255.255.0.0\n"
    "restrict 127.0.0.1\n"
    "restrict ::1\n"
    "leapfile \"/etc/leap-seconds\"\n";

/*
 * Leap second default fallback file.
 *
 * It is recommended you provide a current version in a site specific
 * configuration.
 */
static const char etc_leap_seconds[] =
    "#       Updated through IERS Bulletin C64\n"
    "#       File expires on:  28 June 2023\n"
    "#\n"
    "#@      3896899200\n"
    "#\n"
    "2272060800      10      # 1 Jan 1972\n"
    "2287785600      11      # 1 Jul 1972\n"
    "2303683200      12      # 1 Jan 1973\n"
    "2335219200      13      # 1 Jan 1974\n"
    "2366755200      14      # 1 Jan 1975\n"
    "2398291200      15      # 1 Jan 1976\n"
    "2429913600      16      # 1 Jan 1977\n"
    "2461449600      17      # 1 Jan 1978\n"
    "2492985600      18      # 1 Jan 1979\n"
    "2524521600      19      # 1 Jan 1980\n"
    "2571782400      20      # 1 Jul 1981\n"
    "2603318400      21      # 1 Jul 1982\n"
    "2634854400      22      # 1 Jul 1983\n"
    "2698012800      23      # 1 Jul 1985\n"
    "2776982400      24      # 1 Jan 1988\n"
    "2840140800      25      # 1 Jan 1990\n"
    "2871676800      26      # 1 Jan 1991\n"
    "2918937600      27      # 1 Jul 1992\n"
    "2950473600      28      # 1 Jul 1993\n"
    "2982009600      29      # 1 Jul 1994\n"
    "3029443200      30      # 1 Jan 1996\n"
    "3076704000      31      # 1 Jul 1997\n"
    "3124137600      32      # 1 Jan 1999\n"
    "3345062400      33      # 1 Jan 2006\n"
    "3439756800      34      # 1 Jan 2009\n"
    "3550089600      35      # 1 Jul 2012\n"
    "3644697600      36      # 1 Jul 2015\n"
    "3692217600      37      # 1 Jan 2017\n";

static const char etc_services[] =
    "ntp                123/tcp      # Network Time Protocol  [Dave_Mills] [RFC5905]\n"
    "ntp                123/udp      # Network Time Protocol  [Dave_Mills] [RFC5905]\n";

static bool get_field(const char* field, const char* str, char* buf, size_t buflen)
{
    strlcpy(buf, field, buflen - 2);
    strlcat(buf, "=", buflen);
    const char* c = strstr(str, buf);
    if (c == NULL) {
        return false;
    }
    c += strlen(buf);
    size_t i = 0;
    while (i != buflen - 1 && *c != '\0' && *c != '=') {
        if (*c == '\n') {
            buf[i++] = ' ';
        } else {
            buf[i++] = *c;
        }
        ++c;
    }
    if (i != buflen - 1) {
        while (--i != 0) {
            if (buf[i] == ' ') {
                break;
            }
        }
    }
    buf[i--] = '\0';
    if (i > 0 && buf[i] == ',') {
        buf[i] = '\0';
    }
    return true;
}

static const char* copy_result(const char* src, char* dst, size_t dstlen, char delimiter)
{
    size_t i = 0;
    while (i < dstlen - 1 && *src != '\0' && *src != delimiter) {
        dst[i++] = *src++;
    }
    dst[i] = 0;
    if (*src == delimiter) {
        ++src;
    }
    return src;
}

static const char* skip_ws(const char* buf)
{
    while (*buf == ' ' && *buf != '\0') {
        ++buf;
    }
    return buf;
}

static const char* skip_until_ws(const char* buf)
{
    while (*buf != ' ' && *buf != '\0') {
        ++buf;
    }
    return buf;
}

#define EXIT_ON_NULL(_p) if ((_p) == NULL) { return; }

static void clear_rl(ntpq_rl_data* rl)
{
    struct timespec ts;
    memset(rl, 0, sizeof(*rl));
    int r = clock_gettime(CLOCK_REALTIME, &ts);
    if (r == 0) {
        struct tm tm;
        localtime_r(&ts.tv_sec, &tm);
        strftime(
            rl->last_updated, sizeof(rl->last_updated), "%Y-%m-%d %H:%M:%S", &tm);
    } else {
        strlcpy(rl->last_updated, "unknown", sizeof(rl->last_updated));
    }
}

void parse_rl(const char* resp, ntpq_rl_data* rl)
{
    char buf[128];
    clear_rl(rl);
    if (get_field("associd", resp, buf, sizeof(buf))) {
        rl->associd = strtol(buf, NULL, 0);
    }
    if (get_field("status", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        rl->status = strtol(p, NULL, 16);
        EXIT_ON_NULL(p = skip_until_ws(p));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = copy_result(p, &rl->leap_status[0], sizeof(rl->leap_status), ','));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = copy_result(p, &rl->sync_status[0], sizeof(rl->sync_status), ','));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = skip_until_ws(p));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = skip_until_ws(p));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = copy_result(p, &rl->clock_status[0], sizeof(rl->clock_status), ','));
    }
    if (get_field("version", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        if (*p == '"') {
            ++p;
        }
        EXIT_ON_NULL(p = skip_until_ws(p));
        EXIT_ON_NULL(p = skip_ws(p));
        EXIT_ON_NULL(p = copy_result(p, &rl->version[0], sizeof(rl->version), '"'));
    }
    if (get_field("processor", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        if (*p == '"') {
            ++p;
        }
        EXIT_ON_NULL(p = copy_result(p, &rl->processor[0], sizeof(rl->processor), '"'));
    }
    if (get_field("system", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        if (*p == '"') {
            ++p;
        }
        EXIT_ON_NULL(p = copy_result(p, &rl->system[0], sizeof(rl->system), '"'));
    }
    if (get_field("leap", resp, buf, sizeof(buf))) {
        rl->leap = strtol(buf, NULL, 0);
    }
    if (get_field("stratum", resp, buf, sizeof(buf))) {
        rl->stratum = strtol(buf, NULL, 0);
    }
    if (get_field("precision", resp, buf, sizeof(buf))) {
        rl->precision = strtof(buf, NULL);
    }
    if (get_field("rootdelay", resp, buf, sizeof(buf))) {
        rl->rootdelay = strtof(buf, NULL);
    }
    if (get_field("rootdisp", resp, buf, sizeof(buf))) {
        rl->rootdisp = strtof(buf, NULL);
    }
    if (get_field("refid", resp, buf, sizeof(buf))) {
        if (buf[0] == '0' && buf[1] == 'x') {
            uint32_t ip_as_num = strtoul(buf, NULL, 0);
            snprintf(
                &rl->refid[0], sizeof(rl->refid),
                "%d.%d.%d.%d",
                (int)(ip_as_num >> 24) & 0xff, (int)(ip_as_num >> 16)  & 0xff,
                (int)(ip_as_num >> 8) & 0xff, (int)(ip_as_num >> 0)  & 0xff);
        } else {
            copy_result(buf, &rl->refid[0], sizeof(rl->refid), ' ');
        }
    }
    if (get_field("reftime", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        char ref_buf[16];
        EXIT_ON_NULL(p = copy_result(p, &ref_buf[0], sizeof(ref_buf), '.'));
        rl->reftime_sec = strtoull(ref_buf, NULL, 16);
        ++p;
        EXIT_ON_NULL(p = copy_result(p, &ref_buf[0], sizeof(ref_buf), ' '));
        rl->reftime_nsec = strtoull(ref_buf, NULL, 16);
    }
    if (get_field("clock", resp, buf, sizeof(buf))) {
        const char* p = &buf[0];
        char ref_buf[16];
        EXIT_ON_NULL(p = copy_result(p, &ref_buf[0], sizeof(ref_buf), '.'));
        rl->clock_sec = strtoull(ref_buf, NULL, 16);
        ++p;
        EXIT_ON_NULL(p = copy_result(p, &ref_buf[0], sizeof(ref_buf), ' '));
        rl->clock_nsec = strtoull(ref_buf, NULL, 16);
    }
    if (get_field("peer", resp, buf, sizeof(buf))) {
        rl->peer = strtol(buf, NULL, 0);
    }
    if (get_field("tc", resp, buf, sizeof(buf))) {
        rl->tc = strtol(buf, NULL, 0);
    }
    if (get_field("mintc", resp, buf, sizeof(buf))) {
        rl->mintc = strtol(buf, NULL, 0);
    }
    if (get_field("offset", resp, buf, sizeof(buf))) {
        rl->offset = strtof(buf, NULL);
    }
    if (get_field("frequency", resp, buf, sizeof(buf))) {
        rl->frequency = strtof(buf, NULL);
    }
    if (get_field("sys_jitter", resp, buf, sizeof(buf))) {
        rl->sys_jitter = strtof(buf, NULL);
    }
    if (get_field("clk_jitter", resp, buf, sizeof(buf))) {
        rl->clk_jitter = strtof(buf, NULL);
    }
    if (get_field("clk_wander", resp, buf, sizeof(buf))) {
        rl->clk_wander = strtof(buf, NULL);
    }
    if (get_field("tai", resp, buf, sizeof(buf))) {
        rl->tai = strtol(buf, NULL, 10);
    }
    if (get_field("leapsec", resp, buf, sizeof(buf))) {
        rl->leapsec = strtoull(buf, NULL, 10);
    }
    if (get_field("expire", resp, buf, sizeof(buf))) {
        rl->expire = strtoull(buf, NULL, 10);
    }
}

static int copy_file(const char* src, const char* dst)
{
    unsigned char buf[256];
    struct stat sb;
    ssize_t size;
    int sfd;
    int dfd;
    int r;

    r = stat(src, &sb);
    if (r != 0) {
        errlogPrintf(
            "osdNTP: file stat error: %s: %d %s\n", src, errno, strerror(errno));
        return -1;
    }
    if (!S_ISREG(sb.st_mode)) {
        errlogPrintf("osdNTP: not a file: %s\n", src);
        return -1;
    }

    size = sb.st_size;

    sfd = open(src, O_RDONLY);
    if (sfd < 0) {
        errlogPrintf(
            "osdNTP: file open error: %s: %d %s\n", src, errno, strerror(errno));
        return -1;
    }

    dfd = open(dst, O_WRONLY | O_TRUNC | O_CREAT,
               S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (dfd < 0) {
        errlogPrintf(
            "osdNTP: file open error: %s: %d %s\n", dst, errno, strerror(errno));
        close(sfd);
        return -1;
    }

    while (size > 0) {
        ssize_t len = size > (ssize_t) sizeof(buf) ? sizeof(buf) : size;
        ssize_t s = read(sfd, buf, len);
        if (s != len) {
            errlogPrintf(
                "osdNTP: file read error: %s: %d %s\n", src, errno, strerror(errno));
            r = -1;
            break;
        }
        r = write(dfd, buf, len);
        if (r != len) {
            errlogPrintf(
                "osdNTP: file write error: %s: %d %s\n", src, errno, strerror(errno));
            r = -1;
            break;
        }
        size -= len;
    }

    close(sfd);
    close(dfd);
    return 0;
}

static int copy_text_to_file(const char* dst, const char* text, int append)
{
    const char* mode = "w";
    FILE* fp;
    size_t len;
    size_t w;
    int r = 0;
    if (append) {
        mode = "a";
    }
    fp = fopen(dst, mode);
    if (fp == NULL) {
        errlogPrintf(
            "osdNTP: file open error: %s: %d %s\n", dst, errno, strerror(errno));
        return -1;
    }
    len = strlen(text);
    w = fwrite(text, len, 1, fp);
    if (w != 1) {
        errlogPrintf(
            "osdNTP: file write error: %s: %d %s\n", dst, errno, strerror(errno));
        r = -1;
    }
    fclose(fp);
    return r;
}

static int make_etc_path(void)
{
    const char* etc = "/etc";
    struct stat sb;
    int r;
    r = stat(etc, &sb);
    if (r == 0) {
        if (!S_ISDIR(sb.st_mode)) {
            errlogPrintf("osdNTP: /etc exists and not a directory\n");
            return -1;
        }
        return 0;
    }
    if (r < 0 && errno != ENOENT) {
        errlogPrintf(
            "osdNTP: /etc stat failed: %d %s\n", errno, strerror(errno));
        return -1;
    }
    r = mkdir(etc, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (r != 0) {
        errlogPrintf(
            "osdNTP: /etc mkdir failed: %d %s\n", errno, strerror(errno));
        return -1;
    }
    return 0;
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
    const int active_before_ntpq_open = 30;
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
    clear_rl(&osdNTPPvt.rl);

    int r;

    r = make_etc_path();
    if (r < 0) {
        errlogPrintf("osdNTP: start failure\n");
        return;
    }
    r = copy_text_to_file("/etc/services", etc_services, TRUE);
    if (r < 0) {
        errlogPrintf("osdNTP: start failure\n");
        return;
    }
    r = copy_text_to_file("/etc/leap-seconds", etc_leap_seconds, FALSE);
    if (r < 0) {
        errlogPrintf("osdNTP: start failure\n");
        return;
    }

    epicsThreadCreate("osdNTP_Runner", epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        osdNTP_Runner, NULL);
    epicsThreadCreate("osdNTP_Monitor", epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackSmall),
        osdNTP_Monitor, NULL);

    epicsAtExit(osdNTP_Shutdown, NULL);
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

static void osdNTP_Runner(void *dummy)
{
    int ntpd_fails = 0;

    taskwdInsert(0, NULL, NULL);

    /*
     * Wait for a valid configuration mode to be set. Restart ntpd if
     * it exits limiting the number of attempts if there is a persistent
     * error.
     */
    for (epicsEventWaitWithTimeout(osdNTPPvt.runnerEvent, osdNTPPvt.runner_interval);
         osdNTPPvt.active && ntpd_fails < osdNTP_NTPD_Fails;
         epicsEventWaitWithTimeout(osdNTPPvt.runnerEvent, osdNTPPvt.runner_interval)) {

        if (osdNTP_Configured()) {
            const char *argv[] = {
                "ntpd",
                "-g",
#if NTP_DEBUG
                "--set-debug-level=1",
#endif
                NULL
            };
            const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
            osdNTP_Set_Active(true);
            int r = rtems_ntpd_run(argc, (char**) argv);
            osdNTP_Set_Active(false);
            if (r != 0) {
                errlogPrintf("osdNTP: ntps error (fails=%d): %d\n", ntpd_fails, r);
                ntpd_fails++;
            } else {
                ntpd_fails = 0;
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
    taskwdInsert(0, NULL, NULL);

    for (epicsEventWaitWithTimeout(osdNTPPvt.monitorEvent, osdNTPPvt.monitor_interval);
         osdNTPPvt.active;
         epicsEventWaitWithTimeout(osdNTPPvt.monitorEvent, osdNTPPvt.monitor_interval)) {
        int config_source = NTPD_CONFIG_NONE;
        int ntpd_restart = FALSE;
        const char* ev;

        /*
         * User defined Leap seconds file? Replace the default is set.
         */
        ev = getenv(EPICS_TS_NTP_LEAP_SECONDS_FILE);
        if (ev != NULL) {
            int match = strncmp(
                osdNTPPvt.ev_leap_seconds, ev, sizeof(osdNTPPvt.ev_leap_seconds)) == 0;
            if (!match) {
                int r = copy_file(ev, "/etc/leap-seconds");
                if (r == 0) {
                    strlcpy(osdNTPPvt.ev_leap_seconds, ev, sizeof(osdNTPPvt.ev_leap_seconds));
                    ntpd_restart = TRUE;
                }
            }
        }

        /*
         * Should a hard error with a setting stop further
         * configuraiton option checks?
         */

        /*
         * Configuration file?
         */
        ev = getenv(EPICS_TS_NTP_CONF_FILE);
        if (ev != NULL) {
            int match = strncmp(
                osdNTPPvt.ev_conf_file, ev, sizeof(osdNTPPvt.ev_conf_file)) == 0;
            if (!match) {
                int r = copy_file(ev, "/etc/ntp.conf");
                if (r == 0) {
                    strlcpy(osdNTPPvt.ev_conf_file, ev, sizeof(osdNTPPvt.ev_conf_file));
                    config_source = NTPD_CONFIG_FILE;
                    ntpd_restart = TRUE;
                }
            } else {
                config_source = NTPD_CONFIG_FILE;
            }
        }

        /*
         * Service IP address?
         */
        if (config_source == NTPD_CONFIG_NONE) {
            ev = getenv(EPICS_TS_NTP_INET);
            if (ev != NULL) {
                int match = strncmp(
                    osdNTPPvt.ev_ntp_ip, ev, sizeof(osdNTPPvt.ev_ntp_ip)) == 0;
                if (!match) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "pool %s iburst\n", ev);
                    if (copy_text_to_file("/etc/ntp.conf", buf, FALSE) == 0 &&
                        copy_text_to_file("/etc/ntp.conf", etc_ntp_conf, TRUE) == 0) {
                        strlcpy(osdNTPPvt.ev_ntp_ip, ev, sizeof(osdNTPPvt.ev_ntp_ip));
                        config_source = NTPD_CONFIG_NTP_IP;
                        ntpd_restart = TRUE;
                    }
                } else {
                    config_source = NTPD_CONFIG_NTP_IP;
                }
            }
        }

        osdNTP_Set_ConfigSource(config_source);

        int monitor_interval = osdNTPPvt.monitor_interval;

        if (ntpd_restart) {
            osdNTPPvt.monitor_interval = EPICS_NTPD_MONITOR_ACTIVE_INTERVAL;
            if (rtems_ntpd_running()) {
                rtems_ntpd_stop();
            }
        }

        /*
         * Query the status if active and we have been open for the
         * required active period. This ensures ntpq is open.
         */
        ntpq_rl_data rl;
        if (osdNTP_Get_Active()
            && osdNTP_Update_ActiveSecs(monitor_interval)) {
            /*
             * The rl query.
             */
            const char *argv[] = {
                "rl",
                NULL
            };
            const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
            ntpq_rl_data rl;
            int r = rtems_ntpq_query(
                argc, argv, osdNTPPvt.ntpq_output, sizeof(osdNTPPvt.ntpq_output));
            if (r == 0) {
                parse_rl(osdNTPPvt.ntpq_output, &rl);
            } else {
                clear_rl(&rl);
            }
        } else {
            clear_rl(&rl);
        }

        if (strcmp("sync_ntp", rl.sync_status) == 0) {
            osdNTP_Set_Synchronized(1);
        } else {
            osdNTP_Set_Synchronized(0);
        }

        epicsMutexMustLock(osdNTPPvt.lock);
        osdNTPPvt.rl = rl;
        epicsMutexUnlock(osdNTPPvt.lock);
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
    epicsThreadOnce(&onceId, osdNTP_InitOnce, NULL);
}

void osdNTPReport(void)
{
    epicsMutexMustLock(osdNTPPvt.lock);
    int config_source = osdNTPPvt.config_source;
    int synchronized = osdNTPPvt.synchronized;
    bool ntpd_active = osdNTPPvt.ntpd_active == 1;
    int ntpd_active_secs = osdNTPPvt.ntpd_active_secs;
    bool ntpq_open = osdNTPPvt.ntpq_open == 1;
    ntpq_rl_data rl = osdNTPPvt.rl;
    epicsMutexUnlock(osdNTPPvt.lock);
    printf("RTEMS NTP Status:\n");
    printf(" config type = %d\n", config_source);
    printf("  leap seconds file = %s\n", osdNTPPvt.ev_leap_seconds);
    printf("  conf file = %s\n", osdNTPPvt.ev_conf_file);
    printf("  server ip = %s\n", osdNTPPvt.ev_ntp_ip);
    printf(" ntpd active = %s\n", ntpd_active ? "yes" : "no");
    printf(" ntpd active sec = %d\n", ntpd_active_secs);
    printf(" ntpq open = %s\n", ntpq_open ? "yes" : "no");
    printf(" status last updated = %s\n", rl.last_updated);
    printf(" synchronized = %s\n", synchronized ? "yes" : "no");
    printf(" associd = %u\n", rl.associd);
    printf(" status = %04x\n", rl.status);
    printf(" leap_status = %s\n", rl.leap_status);
    printf(" sync_status = %s\n", rl.sync_status);
    printf(" clock_status = %s\n", rl.clock_status);
    printf(" version = %s\n", rl.version);
    printf(" processor = %s\n", rl.processor);
    printf(" system = %s\n", rl.system);
    printf(" leap = %d\n", rl.leap);
    printf(" stratum = %d\n", rl.stratum);
    printf(" precision = %f\n", rl.precision);
    printf(" rootdelay = %f\n", rl.rootdelay);
    printf(" rootdisp = %f\n", rl.rootdisp);
    printf(" refid = %s\n", rl.refid);
    printf(" reftime = %" PRIu64 ".%" PRIu64"\n", rl.reftime_sec, rl.reftime_nsec);
    printf(" clock = %" PRIu64 ".%" PRIu64 "\n", rl.clock_sec, rl.clock_nsec);
    printf(" peer = %d\n", rl.peer);
    printf(" tc = %d\n", rl.tc);
    printf(" mintc = %d\n", rl.mintc);
    printf(" offset = %f\n", rl.offset);
    printf(" frequency = %f\n", rl.frequency);
    printf(" sys_jitter = %f\n", rl.sys_jitter);
    printf(" clk_jitter = %f\n", rl.clk_jitter);
    printf(" clk_wander = %f\n", rl.clk_wander);
    printf(" tai = %d\n", rl.tai);
    printf(" leapsec = %" PRIu64 "\n", rl.leapsec);
    printf(" expire = %" PRIu64 "\n", rl.expire);
    /*
     * Query the peers. We need to be able to see what ntpd is using
     * as a time source.
     */
    printf("Peers:\n");
    if (ntpq_open) {
        const char *argv[] = {
            "peers",
            NULL
        };
        const int argc = ((sizeof(argv) / sizeof(argv[0])) - 1);
        int r = rtems_ntpq_query(
            argc, argv, osdNTPPvt.ntpq_report_output,
            sizeof(osdNTPPvt.ntpq_report_output));
        if (r == 0) {
            printf(osdNTPPvt.ntpq_report_output);
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

/*
 * Static constructors are run too early in a standalone binary
 * to be able to initialize the NTP time provider (the network
 * is not available yet), so the RTEMS standalone startup code
 * explicitly calls osdTimeRegister() at the appropriate time.
 * However if we are loaded dynamically we *do* register our
 * standard time providers at static constructor time; in this
 * case the network is available already.
 */
static int staticTimeRegister(void)
{
    /* If networking is already up at the time static constructors
     * are executed then we are probably run-time loaded and it's
     * OK to osdTimeRegister() at this point.
     */
    osdTimeRegister();

    return 1;
}
static int done = staticTimeRegister();

#endif /* __RTEMS_MAJOR__ < 6 */
