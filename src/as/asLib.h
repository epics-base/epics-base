/* $Id$ */
/* Author:  Marty Kraimer Date:    09-27-93*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

THE FOLLOWING IS A NOTICE OF COPYRIGHT, AVAILABILITY OF THE CODE,
AND DISCLAIMER WHICH MUST BE INCLUDED IN THE PROLOGUE OF THE CODE
AND IN ALL SOURCE LISTINGS OF THE CODE.
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
Argonne National Laboratory (ANL), with facilities in the States of 
Illinois and Idaho, is owned by the United States Government, and
operated by the University of Chicago under provision of a contract
with the Department of Energy.

Portions of this material resulted from work developed under a U.S.
Government contract and are subject to the following license:  For
a period of five years from March 30, 1993, the Government is
granted for itself and others acting on its behalf a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, and perform
publicly and display publicly.  With the approval of DOE, this
period may be renewed for two additional five year periods. 
Following the expiration of this period or periods, the Government
is granted for itself and others acting on its behalf, a paid-up,
nonexclusive, irrevocable worldwide license in this computer
software to reproduce, prepare derivative works, distribute copies
to the public, perform publicly and display publicly, and to permit
others to do so.

*****************************************************************
                                DISCLAIMER
*****************************************************************

NEITHER THE UNITED STATES GOVERNMENT NOR ANY AGENCY THEREOF, NOR
THE UNIVERSITY OF CHICAGO, NOR ANY OF THEIR EMPLOYEES OR OFFICERS,
MAKES ANY WARRANTY, EXPRESS OR IMPLIED, OR ASSUMES ANY LEGAL
LIABILITY OR RESPONSIBILITY FOR THE ACCURACY, COMPLETENESS, OR
USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY
OWNED RIGHTS.  

*****************************************************************
LICENSING INQUIRIES MAY BE DIRECTED TO THE INDUSTRIAL TECHNOLOGY
DEVELOPMENT CENTER AT ARGONNE NATIONAL LABORATORY (708-252-2000).
 *
 * Modification Log:
 * -----------------
 * .01  09-27-93	mrk	Initial Implementation
 */
#ifndef INCasLibh
#define INCasLibh

#include <stddef.h>
#include <stdlib.h>
#include <errMdef.h>
#include <ellLib.h>

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
end of macros
*/
int asInit(void);
long asInitialize(ASINPUTFUNCPTR inputfunction);
/*caller must provide permanent storage for asgName*/
long asAddMember(ASMEMBERPVT *asMemberPvt,char *asgName);
long asRemoveMember(ASMEMBERPVT *asMemberPvt);
/*caller must provide permanent storage for newAsgName*/
long asChangeGroup(ASMEMBERPVT *asMemberPvt,char *newAsgName);
void *asGetMemberPvt(ASMEMBERPVT asMemberPvt);
void asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt);
/*client must provide permanent storage for user and host*/
long asAddClient(ASCLIENTPVT *asClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,char *user,char *host);
/*client must provide permanent storage for user and host*/
long asChangeClient(ASCLIENTPVT asClientPvt,int asl,char *user,char *host);
long asRemoveClient(ASCLIENTPVT *asClientPvt);
void *asGetClientPvt(ASCLIENTPVT asClientPvt);
void asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt);
long asRegisterClientCallback(ASCLIENTPVT asClientPvt,
	ASCLIENTCALLBACK pcallback);
long asComputeAllAsg(void);
/*asComputeAsg is defined after ASG is defined*/
long asCompute(ASCLIENTPVT asClientPvt);
int asDump(void (*memcallback)(ASMEMBERPVT),void (*clientcallback)(ASCLIENTPVT),int verbose);
int asDumpUag(char *uagname);
int asDumpHag(char *hagname);
int asDumpRules(char *asgname);
int asDumpMem(char *asgname,void (*memcallback)(ASMEMBERPVT),int clients);
int asDumpHash(void);

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
#ifdef vxWorks
#include <fast_lock.h>
extern FAST_LOCK asLock;
extern int asLockInit;
#endif	
extern int asActive;
/* definition of access rights*/
typedef enum{asNOACCESS,asREAD,asWRITE} asAccessRights;

/*Base pointers for access security*/
typedef struct asBase{
	ELLLIST	uagList;
	ELLLIST	hagList;
	ELLLIST	asgList;
	void	*phash;
} ASBASE;
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
/* function prototypes*/
void *asCalloc(size_t nobj,size_t size);
long asComputeAsg(ASG *pasg);
/*macros*/
#define asCheckGet(asClientPvt)\
	(asActive ?\
		((asClientPvt)->access>=asREAD ? TRUE : FALSE)\
	: TRUE)
#define asCheckPut(asClientPvt)\
	(asActive ?\
		((asClientPvt)->access>=asWRITE ? TRUE : FALSE)\
	: TRUE)

#endif /*INCasLibh*/
