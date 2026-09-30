/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Optional host-only experiment.  All networking uses the existing CA API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asCa.h"
#include "asDbLib.h"
#include "cadef.h"
#include "epicsExit.h"
#include "epicsThread.h"
#include "epicsTime.h"
#include "iocsh.h"
#include "osiSock.h"

int asReloadProbeLoadDatabase(const char *dbd);

/* Input connection time is distinct from arrival of the first input value. */
static void reloadSample(const iocshArgBuf *args)
{
    epicsUInt64 begin = epicsMonotonicGet(), returned, connected;
    int status = asInit(), inputs, disconnected;
    returned = epicsMonotonicGet();
    do {
        ascaStats(&inputs, &disconnected);
        connected = epicsMonotonicGet();
        if(!disconnected || connected - returned >= 10000000000ull)
            break;
        epicsThreadSleep(0.001);
    } while(1);
    printf("ASRELOAD {\"event\":\"reload\",\"sample\":%d,\"status\":%d,"
           "\"reload_ns\":%llu,\"inputs_connected_ns\":%llu,"
           "\"inputs\":%d,\"disconnected\":%d}\n",
           args[0].ival, status, (unsigned long long)(returned - begin),
           (unsigned long long)(connected - begin), inputs, disconnected);
    iocshSetError(status || disconnected);
}

static const iocshArg sampleArg = {"sample identifier", iocshArgInt};
static const iocshArg * const sampleArgs[] = {&sampleArg};
static const iocshFuncDef sampleDef = {"asReloadSample", 1, sampleArgs};

static void resolveHost(const iocshArgBuf *args)
{
    struct sockaddr_in address;
    char buffer[48] = "";
    int status = aToIPAddr("hag-refresh.test", 0, &address);
    if(!status)
        ipAddrToDottedIP(&address, buffer, sizeof(buffer));
    printf("ASRELOAD {\"event\":\"resolve\",\"status\":%d,"
           "\"address\":\"%s\"}\n", status, buffer);
}

static const iocshFuncDef resolveDef = {"asReloadResolve", 0, NULL};

static void connectionChanged(struct connection_handler_args args)
{
    printf("ASRELOAD {\"event\":\"connection\",\"mono_ns\":%llu,"
           "\"connected\":%d}\n", (unsigned long long)epicsMonotonicGet(),
           args.op == CA_OP_CONN_UP);
}

static void accessChanged(struct access_rights_handler_args args)
{
    printf("ASRELOAD {\"event\":\"access\",\"mono_ns\":%llu,"
           "\"read\":%u,\"write\":%u}\n",
           (unsigned long long)epicsMonotonicGet(),
           args.ar.read_access, args.ar.write_access);
}

static void valueChanged(struct event_handler_args args)
{
    printf("ASRELOAD {\"event\":\"value\",\"mono_ns\":%llu,"
           "\"status\":%d,\"value\":%.17g}\n",
           (unsigned long long)epicsMonotonicGet(), args.status,
           args.status == ECA_NORMAL ? *(const double *)args.dbr : 0.0);
}

static int checkCA(int status)
{
    if(status == ECA_NORMAL)
        return 0;
    fprintf(stderr, "CA: %s\n", ca_message(status));
    return 1;
}

static int observe(const char *pv)
{
    chid channel;
    evid subscription;
    char command[64];
    if(checkCA(ca_context_create(ca_enable_preemptive_callback)))
        return 1;
    if(checkCA(ca_create_channel(pv, connectionChanged, NULL, 0, &channel)) ||
       checkCA(ca_replace_access_rights_event(channel, accessChanged)) ||
       checkCA(ca_create_subscription(DBR_DOUBLE, 1, channel,
            DBE_VALUE | DBE_ALARM, valueChanged, NULL, &subscription)) ||
       checkCA(ca_flush_io()))
        return 1;
    while(fgets(command, sizeof(command), stdin)) {
        if(!strcmp(command, "quit\n"))
            break;
        if(!strcmp(command, "state\n")) {
            printf("ASRELOAD {\"event\":\"state\",\"mono_ns\":%llu,"
                   "\"connected\":%d,\"read\":%u,\"write\":%u}\n",
                   (unsigned long long)epicsMonotonicGet(),
                   ca_state(channel) == cs_conn, ca_read_access(channel),
                   ca_write_access(channel));
        }
    }
    ca_clear_subscription(subscription);
    ca_clear_channel(channel);
    ca_context_destroy();
    return 0;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    if(argc == 3 && !strcmp(argv[1], "client"))
        return observe(argv[2]);
    if(argc == 4 && !strcmp(argv[1], "ioc")) {
        if(asReloadProbeLoadDatabase(argv[2]))
            return 1;
        iocshRegister(&sampleDef, reloadSample);
        iocshRegister(&resolveDef, resolveHost);
        if(iocsh(argv[3]))
            return 1;
        printf("ASRELOAD {\"event\":\"ready\"}\n");
        iocsh(NULL);
        asCaStop();
        epicsExit(0);
    }
    fprintf(stderr, "Usage: %s ioc DBD STARTUP | client PV\n", argv[0]);
    return 1;
}
