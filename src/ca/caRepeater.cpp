/*
 *  $Id$
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

#include "epicsAssert.h"
#include "shareLib.h"

epicsShareFunc void epicsShareAPI ca_repeater (void);

int main()
{
    ca_repeater ();
    assert (0);
    return (0);
}

