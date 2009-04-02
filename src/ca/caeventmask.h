/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INCLcaeventmaskh
#define INCLcaeventmaskh

/*
    event selections
    (If any more than 8 of these are needed then update the 
    select field in the event_block struct in db_event.c from 
    unsigned char to unsigned short)


    DBE_VALUE 
    Trigger an event when a significant change in the channel's value
    occurs. Relies on the monitor deadband field under DCT.

    DBE_ARCHIVE (DBE_LOG)
    Trigger an event when an archive significant change in the channel's
    valuue occurs. Relies on the archiver monitor deadband field under DCT.

    DBE_ALARM
    Trigger an event when the alarm state changes

    DBE_PROPERTY
    Trigger an event when a property change (control limit, graphical
    limit, status string, enum string ...) occurs.

*/

#define DBE_VALUE    (1<<0)
#define DBE_ARCHIVE  (1<<1)
#define DBE_LOG      DBE_ARCHIVE
#define DBE_ALARM    (1<<2)
#define DBE_PROPERTY (1<<3)

#endif
