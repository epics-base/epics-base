/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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

#include "epicsAssert.h"
#include "udpiiu.h"

int main()
{
    ca_repeater ();
    return ( 0 );
}

