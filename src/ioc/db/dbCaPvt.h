/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbCaPvt.h
 *
 * Original Authors: Bob Dalesio, Marty Kraimer
 *
 */

#ifndef INC_dbCaPvt_H
#define INC_dbCaPvt_H

#include "dbCa.h"
#include "ellLib.h"
#include "epicsMutex.h"
#include "epicsTypes.h"
#include "link.h"

/* link_action mask */
#define	CA_CLEAR_CHANNEL		0x1
#define	CA_CONNECT			0x2
#define	CA_WRITE_NATIVE			0x4
#define	CA_WRITE_STRING			0x8
#define	CA_MONITOR_NATIVE		0x10
#define	CA_MONITOR_STRING		0x20
#define	CA_GET_ATTRIBUTES		0x40
#define	CA_SYNC                 0x1000
/* write type */
#define CA_PUT          0x1
#define CA_PUT_CALLBACK 0x2

typedef struct caLink
{
    ELLNODE		node;
    int         refcount;
    epicsMutexId	lock;
    struct link	*plink;
    char		*pvname;
    chid 		chid;
    short		link_action;
    /* The following have new values after each data event*/
    epicsEnum16	sevr;
    epicsEnum16	stat;
    epicsTimeStamp	timeStamp;
    /* The following have values after connection*/
    short		dbrType;
    size_t elementSize; /* size of one element in pgetNative */
    unsigned long nelements; /* PVs max array size */
    unsigned long usedelements; /* currently used in pgetNative */
    unsigned long putnelements; /* currently used in pputNative */
    char		hasReadAccess;
    char		hasWriteAccess;
    char            isConnected;
    char		gotFirstConnection;
    /* The following are for dbCaAddLinkCallback */
    dbCaCallback    connect;
    dbCaCallback    monitor;
    void            *userPvt;
    /* The following are for write request */
    short           putType;
    dbCaCallback    putCallback;
    void            *putUserPvt;
    /* The following are for access to additional attributes*/
    char            gotAttributes;
    dbCaCallback	getAttributes;
    void            *getAttributesPvt;
    /* The following have values after getAttribEventCallback*/
    double          controlLimits[2];
    double          displayLimits[2];
    double          alarmLimits[4];
    short           precision;
    char            units[MAX_UNITS_SIZE];  /* units of value */
    /* The following are for handling data*/
    void 		*pgetNative;
    char		*pgetString;
    void		*pputNative;
    char		*pputString;
    evid		evidNative;
    evid		evidString;
    char		gotInNative;
    char		gotInString;
    char		gotOutNative;
    char		gotOutString;
    char		newOutNative;
    char		newOutString;
    unsigned char scanningOnce;
    /* The following are for dbcar*/
    unsigned long	nDisconnect;
    unsigned long	nNoWrite; /*only modified by dbCaPutLink*/
    unsigned long   nUpdate;
}caLink;

#endif /* INC_dbCaPvt_H */
