/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * CA client library diagnostics IOC shell registration
 * Authors:
 * Jeff Hill
 */

#include <iocsh.h>
#include "caDiagnostics.h"

/* Information needed by iocsh */
static const iocshArg     acctstArg0 = { "channel name", iocshArgString };
static const iocshArg     acctstArg1 = { "interest level", iocshArgInt };
static const iocshArg     acctstArg2 = { "channel count", iocshArgInt };
static const iocshArg     acctstArg3 = { "repetition count", iocshArgInt };
static const iocshArg     acctstArg4 = { "preemptive callback select", iocshArgInt };

static const iocshArg    *acctstArgs[] =
{
    &acctstArg0,
    &acctstArg1,
    &acctstArg2,
    &acctstArg3,
    &acctstArg4
};
static const iocshFuncDef acctstFuncDef = {"acctst", 5, acctstArgs};


/* Wrapper called by iocsh, selects the argument types that print needs */
static void acctstCallFunc(const iocshArgBuf *args) {
    if ( args[1].ival <  0 ) {
        printf ( "negative interest level not allowed\n" );
        return;
    }
    if ( args[2].ival <  0 ) {
        printf ( "negative channel count not allowed\n" );
        return;
    }
    if ( args[3].ival <  0 ) {
        printf ( "negative repetition count not allowed\n" );
        return;
    }
    acctst (
        args[0].sval, /* channel name */
        ( unsigned ) args[1].ival, /* interest level */
        ( unsigned ) args[2].ival, /* channel count */
        ( unsigned ) args[3].ival, /* repetition count */
        ( ca_preemptive_callback_select ) args[4].ival );  /* preemptive callback select */
}

struct AutoInit {
    AutoInit ();
};

AutoInit :: AutoInit () 
{
    iocshRegister ( &acctstFuncDef, acctstCallFunc );
}

AutoInit autoInit;

