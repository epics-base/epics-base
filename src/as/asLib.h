/* $Id$ */
/* Author:  Marty Kraimer Date:    09-27-93*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01  09-27-93	mrk	Initial Implementation
 */
#ifndef INCasLibh
#define INCasLibh

#include "shareLib.h"

typedef struct asgMember *ASMEMBERPVT;
typedef struct asgClient *ASCLIENTPVT;
typedef int (*ASINPUTFUNCPTR)(char *buf,int max_size);
typedef enum{
    asClientCOAR	/*Change of access rights*/
    /*For now this is all*/
} asClientStatus;

typedef void (*ASCLIENTCALLBACK) (ASCLIENTPVT,asClientStatus);
/* The following  routines are macros with the following syntax
long asCheckGet(ASCLIENTPVT asClientPvt);
long asCheckPut(ASCLIENTPVT asClientPvt);
*/
#define asCheckGet(asClientPvt)\
	(asActive ?\
		((asClientPvt)->access>=asREAD ? TRUE : FALSE)\
	: TRUE)
#define asCheckPut(asClientPvt)\
	(asActive ?\
		((asClientPvt)->access>=asWRITE ? TRUE : FALSE)\
	: TRUE)

epicsShareFunc int epicsShareAPI asInit(void);
epicsShareFunc long epicsShareAPI asInitialize(ASINPUTFUNCPTR inputfunction);
epicsShareFunc long epicsShareAPI asInitFile(const char *filename,const char *substitutions);
epicsShareFunc long epicsShareAPI asInitFP(FILE *fp,const char *substitutions);
/*caller must provide permanent storage for asgName*/
epicsShareFunc long epicsShareAPI asAddMember(ASMEMBERPVT *asMemberPvt,char *asgName);
epicsShareFunc long epicsShareAPI asRemoveMember(ASMEMBERPVT *asMemberPvt);
/*caller must provide permanent storage for newAsgName*/
epicsShareFunc long epicsShareAPI asChangeGroup(ASMEMBERPVT *asMemberPvt,char *newAsgName);
epicsShareFunc void * epicsShareAPI asGetMemberPvt(ASMEMBERPVT asMemberPvt);
epicsShareFunc void epicsShareAPI asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt);
/*client must provide permanent storage for user and host*/
epicsShareFunc long epicsShareAPI asAddClient(ASCLIENTPVT *asClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,char *user,char *host);
/*client must provide permanent storage for user and host*/
epicsShareFunc long epicsShareAPI asChangeClient(ASCLIENTPVT asClientPvt,int asl,char *user,char *host);
epicsShareFunc long epicsShareAPI asRemoveClient(ASCLIENTPVT *asClientPvt);
epicsShareFunc void * epicsShareAPI asGetClientPvt(ASCLIENTPVT asClientPvt);
epicsShareFunc void epicsShareAPI asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt);
epicsShareFunc long epicsShareAPI asRegisterClientCallback(ASCLIENTPVT asClientPvt,
	ASCLIENTCALLBACK pcallback);
epicsShareFunc long epicsShareAPI asComputeAllAsg(void);
/* following declared below after ASG is declared
epicsShareFunc long epicsShareAPI asComputeAsg(ASG *pasg);
*/
epicsShareFunc long epicsShareAPI asCompute(ASCLIENTPVT asClientPvt);
epicsShareFunc int epicsShareAPI asDump(void (*memcallback)(ASMEMBERPVT),
    void (*clientcallback)(ASCLIENTPVT),int verbose);
epicsShareFunc int epicsShareAPI asDumpUag(char *uagname);
epicsShareFunc int epicsShareAPI asDumpHag(char *hagname);
epicsShareFunc int epicsShareAPI asDumpRules(char *asgname);
epicsShareFunc int epicsShareAPI asDumpMem(char *asgname,void (*memcallback)(ASMEMBERPVT),int clients);
epicsShareFunc int epicsShareAPI asDumpHash(void);

#define S_asLib_clientsExist 	(M_asLib| 1) /*Client Exists*/
#define S_asLib_noUag 		(M_asLib| 2) /*User Access Group does not exist*/
#define S_asLib_noHag 		(M_asLib| 3) /*Host Access Group does not exist*/
#define S_asLib_noAccess	(M_asLib| 4) /*access security: no access allowed*/
#define S_asLib_noModify	(M_asLib| 5) /*access security: no modification allowed*/
#define S_asLib_badConfig	(M_asLib| 6) /*access security: bad configuration file*/
#define S_asLib_badCalc		(M_asLib| 7) /*access security: bad calculation espression*/
#define S_asLib_dupAsg 		(M_asLib| 8) /*Duplicate Access Security Group */
#define S_asLib_InitFailed 	(M_asLib| 9) /*access security: Init failed*/
#define S_asLib_asNotActive 	(M_asLib|10) /*access security is not active*/
#define S_asLib_badMember 	(M_asLib|11) /*access security: bad ASMEMBERPVT*/
#define S_asLib_badClient 	(M_asLib|12) /*access security: bad ASCLIENTPVT*/
#define S_asLib_badAsg 		(M_asLib|13) /*access security: bad ASG*/
#define S_asLib_noMemory	(M_asLib|14) /*access security: no Memory */

/*Private declarations */
#define ASMAXINP 12
epicsShareExtern int asActive;

/* definition of access rights*/
typedef enum{asNOACCESS,asREAD,asWRITE} asAccessRights;

/*Base pointers for access security*/
typedef struct asBase{
	ELLLIST	uagList;
	ELLLIST	hagList;
	ELLLIST	asgList;
	void	*phash;
} ASBASE;

epicsShareExtern volatile ASBASE *pasbase;

/*Defs for User Access Groups*/
typedef struct{
	ELLNODE	node;
	char	*user;
} UAGNAME;
typedef struct uag{
	ELLNODE	node;
	char	*name;
	ELLLIST	list;	/*list of UAGNAME*/
} UAG;
/*Defs for Host Access Groups*/
typedef struct{
	ELLNODE	node;
	char	*host;
} HAGNAME;
typedef struct hag{
	ELLNODE	node;
	char	*name;
	ELLLIST	list;	/*list of HAGNAME*/
} HAG;
/*Defs for Access SecurityGroups*/
typedef struct {
	ELLNODE	node;
	UAG	*puag;
}ASGUAG;
typedef struct {
	ELLNODE	node;
	HAG	*phag;
}ASGHAG;
typedef struct{
	ELLNODE		node;
	asAccessRights	access;
	int		level;
	int		inpUsed; /*mask for which inputs are used*/
	int		result;  /*Result of calc converted to TRUE/FALSE*/
	char		*calc;
	void		*rpcl;
	ELLLIST		uagList; /*List of ASGUAG*/
	ELLLIST		hagList; /*List of ASGHAG*/
} ASGRULE;
typedef struct{
	ELLNODE		node;
	char		*inp;
	void		*capvt;
	struct asg	*pasg;
	int		inpIndex;
}ASGINP;

typedef struct asg{
	ELLNODE	node;
	char	*name;
	ELLLIST	inpList;
	ELLLIST	ruleList;
	ELLLIST	memberList;
	double	*pavalue;	/*pointer to array of input values*/
	int	inpBad;		/*mask for which inputs are bad*/
	int	inpChanged;	/*mask showing inputs that have changed*/
} ASG;
typedef struct asgMember {
	ELLNODE		node;
	ASG		*pasg;
	ELLLIST		clientList;
	char		*asgName;
	void		*userPvt;
} ASGMEMBER;
typedef struct asgClient {
	ELLNODE		node;	
	ASGMEMBER	*pasgMember;
	char		*user;
	char		*host;
	void		*userPvt;
	ASCLIENTCALLBACK pcallback;
	int		level;
	asAccessRights	access;
} ASGCLIENT;

epicsShareFunc long epicsShareAPI asComputeAsg(ASG *pasg);
/*following is "friend" function*/
epicsShareFunc void * epicsShareAPI asCalloc(size_t nobj,size_t size);

#endif /*INCasLibh*/
