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
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeffrey O. Hill
 *
 */ 


#ifndef INCLcaerrh
#define INCLcaerrh

#ifdef epicsExportSharedSymbols
#   define INCLcaerrh_accessh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#   include "epicsTypes.h"

#ifdef INCLcaerrh_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#endif

#include "shareLib.h"

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
#define ECA_NORMAL          DEFMSG(CA_K_SUCCESS,    0)
#define ECA_MAXIOC          DEFMSG(CA_K_ERROR,      1) /* defunct */
#define ECA_UKNHOST         DEFMSG(CA_K_ERROR,      2) /* defunct */
#define ECA_UKNSERV         DEFMSG(CA_K_ERROR,      3) /* defunct */
#define ECA_SOCK            DEFMSG(CA_K_ERROR,      4) /* defunct */
#define ECA_CONN            DEFMSG(CA_K_WARNING,    5) /* defunct */
#define ECA_ALLOCMEM        DEFMSG(CA_K_WARNING,    6)
#define ECA_UKNCHAN         DEFMSG(CA_K_WARNING,    7) /* defunct */
#define ECA_UKNFIELD        DEFMSG(CA_K_WARNING,    8) /* defunct */
#define ECA_TOLARGE         DEFMSG(CA_K_WARNING,    9)
#define ECA_TIMEOUT         DEFMSG(CA_K_WARNING,   10)
#define ECA_NOSUPPORT       DEFMSG(CA_K_WARNING,   11) /* defunct */
#define ECA_STRTOBIG        DEFMSG(CA_K_WARNING,   12) /* defunct */
#define ECA_DISCONNCHID     DEFMSG(CA_K_ERROR,     13) /* defunct */
#define ECA_BADTYPE         DEFMSG(CA_K_ERROR,     14)
#define ECA_CHIDNOTFND      DEFMSG(CA_K_INFO,      15) /* defunct */
#define ECA_CHIDRETRY       DEFMSG(CA_K_INFO,      16) /* defunct */
#define ECA_INTERNAL        DEFMSG(CA_K_FATAL,     17)
#define ECA_DBLCLFAIL       DEFMSG(CA_K_WARNING,   18) /* defunct */
#define ECA_GETFAIL         DEFMSG(CA_K_WARNING,   19)
#define ECA_PUTFAIL         DEFMSG(CA_K_WARNING,   20)
#define ECA_ADDFAIL         DEFMSG(CA_K_WARNING,   21) /* defunct */
#define ECA_BADCOUNT        DEFMSG(CA_K_WARNING,   22)
#define ECA_BADSTR          DEFMSG(CA_K_ERROR,     23)
#define ECA_DISCONN         DEFMSG(CA_K_WARNING,   24)
#define ECA_DBLCHNL         DEFMSG(CA_K_WARNING,   25)
#define ECA_EVDISALLOW      DEFMSG(CA_K_ERROR,     26)
#define ECA_BUILDGET        DEFMSG(CA_K_WARNING,   27) /* defunct */
#define ECA_NEEDSFP         DEFMSG(CA_K_WARNING,   28) /* defunct */
#define ECA_OVEVFAIL        DEFMSG(CA_K_WARNING,   29) /* defunct */
#define ECA_BADMONID        DEFMSG(CA_K_ERROR,     30)
#define ECA_NEWADDR         DEFMSG(CA_K_WARNING,   31) /* defunct */
#define ECA_NEWCONN         DEFMSG(CA_K_INFO,      32) /* defunct */
#define ECA_NOCACTX         DEFMSG(CA_K_WARNING,   33) /* defunct */
#define ECA_DEFUNCT         DEFMSG(CA_K_FATAL,     34) /* defunct */
#define ECA_EMPTYSTR        DEFMSG(CA_K_WARNING,   35) /* defunct */
#define ECA_NOREPEATER      DEFMSG(CA_K_WARNING,   36) /* defunct */
#define ECA_NOCHANMSG       DEFMSG(CA_K_WARNING,   37) /* defunct */
#define ECA_DLCKREST        DEFMSG(CA_K_WARNING,   38) /* defunct */
#define ECA_SERVBEHIND      DEFMSG(CA_K_WARNING,   39) /* defunct */
#define ECA_NOCAST          DEFMSG(CA_K_WARNING,   40) /* defunct */
#define ECA_BADMASK         DEFMSG(CA_K_ERROR,     41)
#define ECA_IODONE          DEFMSG(CA_K_INFO,      42)
#define ECA_IOINPROGRESS    DEFMSG(CA_K_INFO,      43)
#define ECA_BADSYNCGRP      DEFMSG(CA_K_ERROR,     44)
#define ECA_PUTCBINPROG     DEFMSG(CA_K_ERROR,     45)
#define ECA_NORDACCESS      DEFMSG(CA_K_WARNING,   46)
#define ECA_NOWTACCESS      DEFMSG(CA_K_WARNING,   47)
#define ECA_ANACHRONISM     DEFMSG(CA_K_ERROR,     48)
#define ECA_NOSEARCHADDR    DEFMSG(CA_K_WARNING,   49)
#define ECA_NOCONVERT       DEFMSG(CA_K_WARNING,   50)
#define ECA_BADCHID         DEFMSG(CA_K_ERROR,     51)
#define ECA_BADFUNCPTR      DEFMSG(CA_K_ERROR,     52)
#define ECA_ISATTACHED      DEFMSG(CA_K_WARNING,   53)
#define ECA_NOTINSERVICE    DEFMSG(CA_K_WARNING,   54)
#define ECA_CHANDESTROY     DEFMSG(CA_K_WARNING,   55)
#define ECA_BADPRIORITY     DEFMSG(CA_K_ERROR,     56)
#define ECA_NOTTHREADED     DEFMSG(CA_K_ERROR,     57)

#ifndef CA_ERROR_GLBLSOURCE
epicsShareExtern READONLY char  *ca_message_text[];
#else
READONLY char   *ca_message_text[]
=
{
"Normal successful completion",
"Maximum simultaneous IOC connections exceeded",
"Unknown internet host",
"Unknown internet service",
"Unable to allocate a new socket",

"Unable to connect to internet host or service",
"Unable to allocate additional dynamic memory",
"Unknown IO channel",
"Record field specified inappropriate for channel specified",
"The requested transfer is greater than available memory or EPICS_CA_MAX_ARRAY_BYTES",

"User specified timeout on IO operation expired",
"Sorry, that feature is planned but not supported at this time",
"The supplied string is unusually large",
"The request was ignored because the specified channel is disconnected",
"The data type specifed is invalid",

"Remote Channel not found",
"Unable to locate all user specified channels",
"Channel Access Internal Failure",
"The requested local DB operation failed",
"Could not perform a database value get for that channel",

"Could not perform a database value put for that channel",
"Could not perform a database event add for that channel",
"Count requested inappropriate for that channel",
"The supplied string has improper format",
"Virtual circuit disconnect",

"Identical process variable name on multiple servers",
"The CA routine called is inappropriate for use within an event handler",
"Database value get for that channel failed during channel search",
"Unable to initialize without the vxWorks VX_FP_TASK task option set",
"Event queue overflow has prevented first pass event after event add",

"bad event subscription identifier",
"Remote channel has new network address",
"New or resumed network connection",
"Specified task isnt a member of a CA context",
"Attempt to use defunct CA feature failed",

"The supplied string is empty",
"Unable to spawn the CA repeater thread- auto reconnect will fail",
"No channel id match for search reply- search reply ignored",
"Reseting dead connection- will try to reconnect",
"Server (IOC) has fallen behind or is not responding- still waiting",

"No internet interface with broadcast available",
"The event selection mask supplied is empty or inappropriate",
"IO operations have completed",
"IO operations are in progress",
"Invalid synchronous group identifier",

"Put callback timed out",
"Read access denied",
"Write access denied",
"Sorry, that anachronistic feature of CA is no longer supported",
"The search/beacon request address list was empty after initialization",

"Data conversion between client's type and the server's type failed",
"Invalid channel identifier",
"Invalid function pointer",
"Thread is already attached to a client context",
"No support in service",

"User destroyed channel"
"Priority out of range"
"Preemptive callback not enabled - additional threads may not join"

};
#endif

#ifdef __STDC__ 
#define CAERR_USE_FUNC_PROTO
#endif

#ifdef __cplusplus
#ifndef CAERR_USE_FUNC_PROTO
#define CAERR_USE_FUNC_PROTO
#endif
extern "C" {
#endif

#ifdef CAERR_USE_FUNC_PROTO
epicsShareFunc READONLY char * epicsShareAPI ca_message(long ca_status);
#else /* CAERR_USE_FUNC_PROTO */
READONLY char *ca_message();
#endif

#ifdef __cplusplus
}
#endif

#endif
