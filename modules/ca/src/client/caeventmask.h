/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file caeventmask.h
 * \brief event selections
 * 
 * If any more than 8 of these are needed then update the
 * select field in the event_block struct in db_event.c from
 * unsigned char to unsigned short.
 */
 
#ifndef INCLcaeventmaskh
#define INCLcaeventmaskh

/** Trigger an event when a significant change in the channel's value
 * occurs. Relies on the monitor deadband field under DCT. */
#define DBE_VALUE    (1<<0)
/** Trigger an event when an archive significant change in the channel's
 * value occurs. Relies on the archiver monitor deadband field under DCT. */
#define DBE_ARCHIVE  (1<<1)
#define DBE_LOG      DBE_ARCHIVE
/** Trigger an event when the alarm state changes */
#define DBE_ALARM    (1<<2)
/** Trigger an event when a property change (control limit, graphical
 * limit, status string, enum string ...) occurs. */
#define DBE_PROPERTY (1<<3)

#endif
