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
* .01  08APR96	mrk	Made separate module for dbcal
****************************************************************/

/* for input links */
#define	CA_CONNECT		1
#define	CA_DELETE		2
#define	CA_WRITE_NATIVE		3
#define	CA_WRITE_STRING		4
#define	CA_MONITOR_STRING	5
typedef struct caLink
{
	ELLNODE		node;
	struct link	*plink;
	chid 		chid;
	void 		*pgetNative;
	void		*pputNative;
	char		*pgetString;
	char		*pputString;
	long		nelements;
	SEM_ID		lock;
	unsigned long	nDisconnect;
	unsigned long	nNoWrite;
	short		dbrType;
	short		link_action;
	unsigned short	sevr;
	char		gotInNative;
	char		gotOutNative;
	char		gotInString;
	char		gotOutString;
	char		newWrite;
}caLink;
