/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCa.h.c */

/****************************************************************
*
*	Current Author:		Bob Dalesio
*	Contributing Author:	Marty Kraimer
*	Date:			08APR96
*
****************************************************************/

/* link_action mask */
#define	CA_DELETE			0x1
#define	CA_CONNECT			0x2
#define	CA_WRITE_NATIVE			0x4
#define	CA_WRITE_STRING			0x8
#define	CA_MONITOR_NATIVE		0x10
#define	CA_MONITOR_STRING		0x20
#define	CA_GET_ATTRIBUTES		0x40

typedef struct caAttributes
{
    void (*callback)(void *usrPvt);
 struct dbr_ctrl_double	data;
	void		*usrPvt;
	int		gotData;
}caAttributes;

typedef struct caLink
{
	ELLNODE		node;
	struct link	*plink;
	chid 		chid;
	void 		*pgetNative;
	void		*pputNative;
	char		*pgetString;
	char		*pputString;
	caAttributes	*pcaAttributes;
	long		nelements;
	SEM_ID		lock;
	unsigned long	nDisconnect;
	unsigned long	nNoWrite;
	short		dbrType;
	short		link_action;
	unsigned short	sevr;
	TS_STAMP	timeStamp;
	char		gotInNative;
	char		gotOutNative;
	char		gotInString;
	char		gotOutString;
	char		newOutNative;
	char		newOutString;
	char		gotFirstConnection;
}caLink;
