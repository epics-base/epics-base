
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
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 */

#ifndef caHdrLargeArrayh
#define caHdrLargeArrayh

#ifdef epicsExportSharedSymbols
#   define epicsExportSharedSymbols_caHdrLargeArrayh
#   undef epicsExportSharedSymbols
#endif

// external headers included here
#include "caProto.h"

#ifdef epicsExportSharedSymbols_caHdrLargeArrayh
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

static const unsigned char CA_MINOR_PROTOCOL_REVISION = 13;

typedef ca_uint32_t caResId;

/* a modified ca header with capacity for large arrays */
struct caHdrLargeArray {
    ca_uint32_t m_postsize;     /* size of message extension */
    ca_uint32_t m_count;        /* operation data count      */
    ca_uint32_t m_cid;          /* channel identifier        */
    ca_uint32_t m_available;    /* protocol stub dependent   */
    ca_uint16_t m_dataType;     /* operation data type       */
    ca_uint16_t m_cmmd;         /* operation to be performed */
};

#endif // caHdrLargeArrayh
