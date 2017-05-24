/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* link.h */

/*
 * Original Authors: Bob Dalesio, Marty Kraimer
 */

#ifndef INC_link_H
#define INC_link_H

#include "dbDefs.h"
#include "ellLib.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* link types */
#define CONSTANT	0
#define PV_LINK		1
#define VME_IO		2
#define CAMAC_IO	3
#define	AB_IO		4
#define GPIB_IO		5
#define BITBUS_IO	6
#define MACRO_LINK	7
#define JSON_LINK       8
#define PN_LINK		9
#define DB_LINK		10
#define CA_LINK		11
#define INST_IO		12		/* instrument */
#define	BBGPIB_IO	13		/* bitbus -> gpib */
#define RF_IO		14
#define VXI_IO		15
#define LINK_NTYPES 16
typedef struct maplinkType {
    char *strvalue;
    int  value;
} maplinkType;

epicsShareExtern maplinkType pamaplinkType[];

#define VXIDYNAMIC	0
#define VXISTATIC	1

/* structure of a PV_LINK DB_LINK and a CA_LINK	*/
/*Options defined by pvlMask			*/
#define pvlOptMsMode       0x3	/*Maximize Severity mode selection*/
#define pvlOptNMS            0	/*Don't Maximize Severity*/
#define pvlOptMS             1	/*Maximize Severity always*/
#define pvlOptMSI            2	/*Maximize Severity if INVALID*/
#define pvlOptMSS            3	/*Maximize Severity and copy Status*/
#define pvlOptPP           0x4	/*Process Passive*/
#define pvlOptCA           0x8	/*Always make it a CA link*/
#define pvlOptCP          0x10	/*CA + process on monitor*/
#define pvlOptCPP         0x20	/*CA + process passive record on monitor*/
#define pvlOptFWD         0x40	/*Generate ca_put for forward link*/
#define pvlOptInpNative   0x80	/*Input native*/
#define pvlOptInpString  0x100	/*Input as string*/
#define pvlOptOutNative  0x200	/*Output native*/
#define pvlOptOutString  0x400	/*Output as string*/
#define pvlOptTSELisTime 0x800	/*Field TSEL is getting timeStamp*/

/* DBLINK Flag bits */
#define DBLINK_FLAG_INITIALIZED    1 /* dbInitLink() called */

struct macro_link {
    char *macroStr;
};

struct dbCommon;
typedef long (*LINKCVT)();

struct pv_link {
    ELLNODE	backlinknode;
    char	*pvname;	/* pvname link points to */
    void	*pvt;		/* CA or DB private */
    LINKCVT	getCvt;		/* input conversion function */
    short	pvlMask;	/* Options mask */
    short	lastGetdbrType;	/* last dbrType for DB or CA get */
};

struct jlink;
struct json_link {
    char *string;
    struct jlink *jlink;
};

/* structure of a VME io channel */
struct vmeio {
    short	card;
    short	signal;
    char	*parm;
};

/* structure of a CAMAC io channel */
struct camacio {
    short	b;
    short	c;
    short	n;
    short	a;
    short	f;
    char	*parm;
};

/* structure of a RF io channel */
struct rfio {
    short	branch;
    short	cryo;
    short	micro;
    short	dataset;
    short	element;
    long	ext;
};

/* structure of a Allen-Bradley io channel */
struct abio {
    short	link;
    short	adapter;
    short	card;
    short	signal;
    char	*parm;
};

/* structure of a gpib io channel */
struct gpibio {
    short	link;
    short	addr;		/* device address */
    char	*parm;
};

/* structure of a bitbus io channel */
struct bitbusio {
    unsigned char	link;
    unsigned char	node;
    unsigned char	port;
    unsigned char	signal;
    char		*parm;
};

/* structure of a bitbus to gpib io channel */
struct bbgpibio {
    unsigned char	link;
    unsigned char	bbaddr;
    unsigned char	gpibaddr;
    unsigned char	pad;
    char		*parm;
};

/* structure of an instrument io link */
struct instio {
    char *string;
};

/* structure of a vxi link */
struct vxiio {
    short	flag;			/* 0 = frame/slot, 1 = SA */
    short	frame;
    short	slot;
    short	la;			/* logical address if flag =1 */
    short	signal;
    char	*parm;
};

/* union of possible address structures */
union value {
    char		*constantStr;	/*constant string*/
    struct macro_link	macro_link;	/* link containing macro substitution*/
    struct json_link    json;           /* JSON-encoded link */
    struct pv_link	pv_link;	/* link to process variable*/
    struct vmeio	vmeio;		/* vme io point */
    struct camacio	camacio;	/* camac io point */
    struct rfio		rfio;		/* CEBAF RF buffer interface */
    struct abio		abio;		/* allen-bradley io point */
    struct gpibio	gpibio;
    struct bitbusio	bitbusio;
    struct instio	instio;		/* instrument io link */
    struct bbgpibio	bbgpibio;	/* bitbus to gpib io link */
    struct vxiio	vxiio;		/* vxi io */
};

struct lset;

struct link {
    struct dbCommon *precord;	/* Pointer to record owning link */
    short type;
    short flags;
    struct lset *lset;
    char *text;             /* Raw link text */
    union value value;
};

typedef struct link DBLINK;

#ifdef __cplusplus
}
#endif
#endif /* INC_link_H */
