/* dbCa.h.c */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1991 Regents of the University of California,
and the University of Chicago Board of Governors.
 
This software was developed under a United States Government license
described on the COPYRIGHT_Combined file included as part
of this distribution.
**********************************************************************/

/****************************************************************
*
*	Current Author:		Bob Dalesio
*	Contributing Author:	Marty Kraimer
*	Date:			08APR96
*
*
* Modification Log:
* -----------------
* .01  08APR96	mrk	Made separate module for dbcar
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
