/* link.h */
/* base/include $Id$ */

/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  12-18-91        jba     Changed caddr_t to void *
 * .02	3-16-92		frl	changed  link.value.value to double
 * .03  9-9-92		frl	added vxi link type
 * .04  12-13-93	mcn	added conversion pointer for fast links
 * .05  1-10-95		jt	added rf_io structure
 * .06	01-10-96	lrd	added ca_link structure fields
 */

#include <dbDefs.h>

#ifndef INClinkh
#define INClinkh 1

/* link types */
#define CONSTANT	0
#define PV_LINK		1
#define VME_IO		2
#define CAMAC_IO	3
#define	AB_IO		4
#define GPIB_IO		5
#define BITBUS_IO	6
#define MACRO_LINK	7
#define DB_LINK		10
#define CA_LINK		11
#define INST_IO		12		/* instrument */
#define	BBGPIB_IO	13		/* bitbus -> gpib */
#define RF_IO		14
#define VXI_IO		15
#define LINK_NTYPES 14
typedef struct maplinkType{
	char	*strvalue;
	int	value;
}maplinkType;

#ifndef LINK_GBLSOURCE
extern  maplinkType pamaplinkType[];
#else
maplinkType pamaplinkType[LINK_NTYPES] = {
	{"CONSTANT",CONSTANT},
	{"PV_LINK",PV_LINK},
	{"VME_IO",VME_IO},
	{"CAMAC_IO",CAMAC_IO},
	{"AB_IO",AB_IO},
	{"GPIB_IO",GPIB_IO},
	{"BITBUS_IO",BITBUS_IO},
	{"MACRO_LINK",MACRO_LINK},
	{"DB_LINK",DB_LINK},
	{"CA_LINK",CA_LINK},
	{"INST_IO",INST_IO},
	{"BBGPIB_IO",BBGPIB_IO},
	{"RF_IO",RF_IO},
	{"VXI_IO",VXI_IO}
};
#endif /*LINK_GBLSOURCE*/

#define VXIDYNAMIC	0
#define VXISTATIC	1

/* structure of a PV_LINK DB_LINK and a CA_LINK	*/
/*Options defined by pvlMask			*/
#define pvlOptMS	0x1	/*Maximize Severity*/
#define pvlOptPP	0x2	/*Process Passive*/
#define pvlOptCA	0x4	/*Always make it a CA link*/
#define pvlOptCP	0x8	/*CA + process on monitor*/
#define pvlOptCPP	0x10	/*CA + process passive record on monitor*/
#define pvlOptFWD	0x20	/*Generate ca_put for forward link*/
#define pvlOptInpNative	0x40	/*Input native*/
#define pvlOptInpString	0x80	/*Input as string*/
#define pvlOptOutNative	0x100	/*Output native*/
#define pvlOptOutString	0x200	/*Output as string*/

typedef long (*LINKCVT)();

struct macro_link {
	char	*macroStr;
};

struct pv_link {
	char	*pvname;	/*pvname to link to*/
	void	*precord;	/*Address of record containing link*/
	void	*pvt;		/*CA or DB private*/
	LINKCVT	getCvt;		/*input conversion function*/
	short	pvlMask;	/*Options mask*/
	short	lastGetdbrType;	/*last dbrType for DB or CA get*/
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
struct	bitbusio {
	unsigned char	link;
	unsigned char	node;
	unsigned char	port;
	unsigned char	signal;
	char	*parm;
};

/* structure of a bitbus to gpib io channel */
struct	bbgpibio {
	unsigned char	link;
	unsigned char	bbaddr;
	unsigned char	gpibaddr;
	unsigned char	pad;
	char	*parm;
};

/* structure of an instrument io link */
struct	instio {
	char	*string;		/* the cat of location.
							signal.parameter  */
};

/* structure of a vxi link */
struct	vxiio{
	short	flag;				/* 0 = frame/slot, 1 = SA */
	short	frame;
	short	slot;
	short	la;				/* logical address if flag =1 */
	short	signal;
	char	*parm;
};

/* union of possible address structures */
union value{
	char		*constantStr;	/*constant string*/
	struct macro_link macro_link;	/* link containing macro substitution*/
	struct pv_link	pv_link;	/* link to process variable*/
	struct vmeio	vmeio;		/* vme io point */
	struct camacio	camacio;	/* camac io point */
	struct rfio	rfio;		/* CEBAF RF buffer interface */
	struct abio	abio;		/* allen-bradley io point */
	struct gpibio	gpibio;
	struct bitbusio	bitbusio;
	struct instio	instio;		/* instrument io link */
	struct bbgpibio bbgpibio;	/* bitbus to gpib io link */
	struct	vxiio	vxiio;		/* vxi io */
};

struct link{
	union value	value;
	short		type;
};

typedef struct link DBLINK;
#endif /*INClinkh*/
