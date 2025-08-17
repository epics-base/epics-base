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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeffrey O. Hill
 *
 */

/** \file caerr.h
 *
 * \brief Channel access error definitions.
 */

#ifndef INC_caerr_H
#define INC_caerr_H

#include "epicsTypes.h"

#include "libCaAPI.h"

/*  CA Status Code Definitions   */

#define CA_K_INFO       3   /* successful */
#define CA_K_ERROR      2   /* failed- continue */
#define CA_K_SUCCESS    1   /* successful */
#define CA_K_WARNING    0   /* unsuccessful */
#define CA_K_SEVERE     4   /* failed- quit */
#define CA_K_FATAL  CA_K_ERROR | CA_K_SEVERE

#define CA_M_MSG_NO     0x0000FFF8
#define CA_M_SEVERITY   0x00000007
#define CA_M_LEVEL      0x00000003
#define CA_M_SUCCESS    0x00000001
#define CA_M_ERROR      0x00000002
#define CA_M_SEVERE     0x00000004

#define CA_S_MSG_NO     0x0D
#define CA_S_SEVERITY   0x03

#define CA_V_MSG_NO     0x03
#define CA_V_SEVERITY   0x00
#define CA_V_SUCCESS    0x00

/* Define MACROS to extract/insert individual fields from a status value */

#define CA_EXTRACT_MSG_NO(code)\
( ( (code) & CA_M_MSG_NO )  >> CA_V_MSG_NO )
#define CA_EXTRACT_SEVERITY(code)\
( ( (code) & CA_M_SEVERITY )    >> CA_V_SEVERITY )
#define CA_EXTRACT_SUCCESS(code)\
( ( (code) & CA_M_SUCCESS )     >> CA_V_SUCCESS )

#define CA_INSERT_MSG_NO(code)\
(   ((code)<< CA_V_MSG_NO)  & CA_M_MSG_NO   )
#define CA_INSERT_SEVERITY(code)\
(   ((code)<< CA_V_SEVERITY)& CA_M_SEVERITY )
#define CA_INSERT_SUCCESS(code)\
(   ((code)<< CA_V_SUCCESS) & CA_M_SUCCESS  )

#define DEFMSG(SEVERITY,NUMBER)\
(CA_INSERT_MSG_NO(NUMBER) | CA_INSERT_SEVERITY(SEVERITY))

/*
 * In the lines below "defunct" indicates that current release
 * servers and client library will not return this error code, but
 * servers on earlier releases that communicate with current clients
 * might still generate exceptions with these error constants
 */

/// Normal successful completion
#define ECA_NORMAL          DEFMSG(CA_K_SUCCESS,    0) /* success */
/// Maximum simultaneous IOC connections exceeded
#define ECA_MAXIOC          DEFMSG(CA_K_ERROR,      1) /* defunct */
/// Unknown internet host
#define ECA_UKNHOST         DEFMSG(CA_K_ERROR,      2) /* defunct */
/// Unknown internet service
#define ECA_UKNSERV         DEFMSG(CA_K_ERROR,      3) /* defunct */
/// Unable to allocate a new socket
#define ECA_SOCK            DEFMSG(CA_K_ERROR,      4) /* defunct */
/// Unable to connect to internet host or service
#define ECA_CONN            DEFMSG(CA_K_WARNING,    5) /* defunct */
/// Unable to allocate additional dynamic memory
#define ECA_ALLOCMEM        DEFMSG(CA_K_WARNING,    6)
/// Unknown IO channel
#define ECA_UKNCHAN         DEFMSG(CA_K_WARNING,    7) /* defunct */
/// ECA_UKNFIELD - Record field specified inappropriate for channel specified
#define ECA_UKNFIELD        DEFMSG(CA_K_WARNING,    8) /* defunct */
/// The requested data transfer is greater than available memory or EPICS_CA_MAX_ARRAY_BYTES
#define ECA_TOLARGE         DEFMSG(CA_K_WARNING,    9)
/// User specified timeout on IO operation expired
#define ECA_TIMEOUT         DEFMSG(CA_K_WARNING,   10)
/// Sorry, that feature is planned but not supported at this time
#define ECA_NOSUPPORT       DEFMSG(CA_K_WARNING,   11) /* defunct */
/// The supplied string is unusually large
#define ECA_STRTOBIG        DEFMSG(CA_K_WARNING,   12) /* defunct */
/// The request was ignored because the specified channel is disconnected
#define ECA_DISCONNCHID     DEFMSG(CA_K_ERROR,     13) /* defunct */
/// The data type specifed is invalid
#define ECA_BADTYPE         DEFMSG(CA_K_ERROR,     14)
/// Remote Channel not found
#define ECA_CHIDNOTFND      DEFMSG(CA_K_INFO,      15) /* defunct */
/// Unable to locate all user specified channels
#define ECA_CHIDRETRY       DEFMSG(CA_K_INFO,      16) /* defunct */
/// Channel Access Internal Failure
#define ECA_INTERNAL        DEFMSG(CA_K_FATAL,     17)
/// The requested local DB operation failed
#define ECA_DBLCLFAIL       DEFMSG(CA_K_WARNING,   18) /* defunct */
/// Channel read request failed
#define ECA_GETFAIL         DEFMSG(CA_K_WARNING,   19)
/// Channel write request failed
#define ECA_PUTFAIL         DEFMSG(CA_K_WARNING,   20)
/// Channel subscription request failed
#define ECA_ADDFAIL         DEFMSG(CA_K_WARNING,   21) /* defunct */
/// Invalid element count requested
#define ECA_BADCOUNT        DEFMSG(CA_K_WARNING,   22)
/// Invalid string
#define ECA_BADSTR          DEFMSG(CA_K_ERROR,     23)
/// Virtual circuit disconnect
#define ECA_DISCONN         DEFMSG(CA_K_WARNING,   24)
/// Identical process variable names on multiple servers
#define ECA_DBLCHNL         DEFMSG(CA_K_WARNING,   25)
/// Request inappropriate within subscription (monitor) update callback
#define ECA_EVDISALLOW      DEFMSG(CA_K_ERROR,     26)
/// Database value get for that channel failed during channel search
#define ECA_BUILDGET        DEFMSG(CA_K_WARNING,   27) /* defunct */
/// Unable to initialize without the vxWorks VX_FP_TASK task option set
#define ECA_NEEDSFP         DEFMSG(CA_K_WARNING,   28) /* defunct */
/// Event queue overflow has prevented first pass event after event add
#define ECA_OVEVFAIL        DEFMSG(CA_K_WARNING,   29) /* defunct */
/// Bad event subscription (monitor) identifier
#define ECA_BADMONID        DEFMSG(CA_K_ERROR,     30)
/// Remote channel has new network address
#define ECA_NEWADDR         DEFMSG(CA_K_WARNING,   31) /* defunct */
/// New or resumed network connection
#define ECA_NEWCONN         DEFMSG(CA_K_INFO,      32) /* defunct */
/// Specified task isnt a member of a CA context
#define ECA_NOCACTX         DEFMSG(CA_K_WARNING,   33) /* defunct */
/// Attempt to use defunct CA feature failed
#define ECA_DEFUNCT         DEFMSG(CA_K_FATAL,     34) /* defunct */
/// The supplied string is empty
#define ECA_EMPTYSTR        DEFMSG(CA_K_WARNING,   35) /* defunct */
/// Unable to spawn the CA repeater thread- auto reconnect will fail
#define ECA_NOREPEATER      DEFMSG(CA_K_WARNING,   36) /* defunct */
/// No channel id match for search reply- search reply ignored
#define ECA_NOCHANMSG       DEFMSG(CA_K_WARNING,   37) /* defunct */
/// Reseting dead connection- will try to reconnect
#define ECA_DLCKREST        DEFMSG(CA_K_WARNING,   38) /* defunct */
/// Server (IOC) has fallen behind or is not responding- still waiting
#define ECA_SERVBEHIND      DEFMSG(CA_K_WARNING,   39) /* defunct */
/// No internet interface with broadcast available
#define ECA_NOCAST          DEFMSG(CA_K_WARNING,   40) /* defunct */
/// Invalid event selection mask
#define ECA_BADMASK         DEFMSG(CA_K_ERROR,     41)
/// IO operations have completed
#define ECA_IODONE          DEFMSG(CA_K_INFO,      42)
/// IO operations are in progress
#define ECA_IOINPROGRESS    DEFMSG(CA_K_INFO,      43)
/// Invalid synchronous group identifier
#define ECA_BADSYNCGRP      DEFMSG(CA_K_ERROR,     44)
/// Put callback timed out
#define ECA_PUTCBINPROG     DEFMSG(CA_K_ERROR,     45)
/// Read access denied
#define ECA_NORDACCESS      DEFMSG(CA_K_WARNING,   46)
/// Write access denied
#define ECA_NOWTACCESS      DEFMSG(CA_K_WARNING,   47)
/// Requested feature is no longer supported
#define ECA_ANACHRONISM     DEFMSG(CA_K_ERROR,     48)
/// Empty PV search address list
#define ECA_NOSEARCHADDR    DEFMSG(CA_K_WARNING,   49)
/// No reasonable data conversion between client and server types
#define ECA_NOCONVERT       DEFMSG(CA_K_WARNING,   50)
/// Invalid channel identifier
#define ECA_BADCHID         DEFMSG(CA_K_ERROR,     51)
/// Invalid function pointer
#define ECA_BADFUNCPTR      DEFMSG(CA_K_ERROR,     52)
/// Thread is already attached to a client context
#define ECA_ISATTACHED      DEFMSG(CA_K_WARNING,   53)
/// Not supported by attached service
#define ECA_UNAVAILINSERV   DEFMSG(CA_K_WARNING,   54)
/// User destroyed channel
#define ECA_CHANDESTROY     DEFMSG(CA_K_WARNING,   55)
/// Invalid channel priority
#define ECA_BADPRIORITY     DEFMSG(CA_K_ERROR,     56)
/// Preemptive callback not enabled - additional threads may not join context
#define ECA_NOTTHREADED     DEFMSG(CA_K_ERROR,     57)
/// Client's protocol revision does not support transfers exceeding 16k bytes
#define ECA_16KARRAYCLIENT  DEFMSG(CA_K_WARNING,   58)
/// Virtual circuit connection sequence aborted
#define ECA_CONNSEQTMO      DEFMSG(CA_K_WARNING,   59)
/// Virtual circuit unresponsive
#define ECA_UNRESPTMO       DEFMSG(CA_K_WARNING,   60)

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Return a message character string corresponding to a user specified CA status code.
 *
 * \param[in] ca_status A CA status code.
 * \returns The corresponding error message string.
 */
LIBCA_API const char * epicsStdCall ca_message(long ca_status);

LIBCA_API extern const char * ca_message_text [];

#ifdef __cplusplus
}
#endif

#endif
