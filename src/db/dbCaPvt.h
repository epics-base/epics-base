/* dbCaPvt.h */
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

#ifndef INCdbCaPvth
#define INCdbCaPvth 1

/* link_action mask */
#define	CA_CLEAR_CHANNEL		0x1
#define	CA_CONNECT			0x2
#define	CA_WRITE_NATIVE			0x4
#define	CA_WRITE_STRING			0x8
#define	CA_MONITOR_NATIVE		0x10
#define	CA_MONITOR_STRING		0x20
#define	CA_GET_ATTRIBUTES		0x40

typedef struct caLink
{
	ELLNODE		node;
	epicsMutexId	lock;
	struct link	*plink;
        char		*pvname;
	chid 		chid;
	short		link_action;
        /* The following have new values after each data event*/
	unsigned short	sevr;
	epicsTimeStamp	timeStamp;
        /* The following have values after connection*/
	short		dbrType;
	long		nelements;
        char		hasReadAccess;
        char		hasWriteAccess;
        char            isConnected;
	char		gotFirstConnection;
        /* The following are for access to additional attributes*/
        char            gotAttributes;
        void            (*callback)(void *usrPvt);
        void            *userPvt;
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
	char		gotInNative;
	char		gotInString;
	char		gotOutNative;
	char		gotOutString;
	char		newOutNative;
	char		newOutString;
        /* The following are for dbcar*/
	unsigned long	nDisconnect;
	unsigned long	nNoWrite; /*only modified by dbCaPutLink*/
}caLink;

#endif /*INCdbCaPvth*/
