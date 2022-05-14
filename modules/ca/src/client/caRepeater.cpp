/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *
 *  CA UDP repeater standalone executable
 *
 *  Author:      Jeff Hill
 *  Date:        3-27-90
 *
 *  PURPOSE:
 *  Broadcasts fan out over the LAN, but old IP kernels do not allow
 *  two processes on the same machine to get the same broadcast
 *  (and modern IP kernels do not allow two processes on the same machine
 *  to receive the same unicast).
 *
 *  This code fans out UDP messages sent to the CA repeater port
 *  to all CA client processes that have subscribed.
 *
 *  NOTES:
 *
 *  see repeater.c
 *
 */

#define epicsAssertAuthor "Jeff Hill johill@lanl.gov"

#include <stdio.h>

#if !defined(_WIN32) && !defined(__rtems__) && !defined(vxWorks)
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#define CAN_DETACH_STDINOUT
#endif

#include "epicsAssert.h"
#include "osiUnistd.h"
#include "epicsGetopt.h"
#include "udpiiu.h"

static void usage(char* argv[])
{
    fprintf(stderr, "Usage: %s -hv\n"
            "\n"
            " -h - Print this message\n"
            " -v - Do not replace stdin/out/err with /dev/null\n",
            argv[0]);
}

int main(int argc, char* argv[])
{
    bool detachinout = true;

    int opt;
    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        default:
            usage(argv);
            fprintf(stderr, "\nUnknown argument '%c'\n", opt);
            return 1;
        case 'h':
            usage(argv);
            return 0;
        case 'v':
            detachinout = false;
            break;
        }
    }

#ifdef CAN_DETACH_STDINOUT
    if(detachinout) {
        int readfd  = open("/dev/null", O_RDONLY);
        int writefd = open("/dev/null", O_WRONLY);

        dup2(readfd, 0);
        dup2(writefd, 1);
        dup2(writefd, 2);

        close(readfd);
        close(writefd);
    }
#else
    (void)detachinout;
#endif

    (void)! chdir ( "/" );
    ca_repeater ();
    return ( 0 );
}

