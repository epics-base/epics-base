/* share/src/as/asLibRoutines.c	*/
/* share/src/as $Id$ */
/* Author:  Marty Kraimer Date:    10-15-93 */
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
 * .01  10-15-93	mrk	Initial Implementation
 */

#include <dbDefs.h>
#include <asLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <gpHash.h>
#include <freeList.h>
#ifdef vxWorks
#include <taskLib.h>
#endif

/*Declare storage for Global Variables */
ASBASE		*pasbase=NULL;
int	asActive = FALSE;

/*storage for freelist */
static void *freeListPvt = NULL;

/*Forward declarations for Non ANSI routines*/
long calcPerform(double *parg,double *presult,char   *post);
long postfix(char *pinfix, char *ppostfix,short *perror);

#define RPCL_LEN 184
#define DEFAULT "DEFAULT"


/*private routines */
static void asFreeAll(ASBASE *pasbase);
static UAG *asUagAdd(char *uagName);
static long asUagAddUser(UAG *puag,char *user);
static HAG *asHagAdd(char *hagName);
static long asHagAddHost(HAG *phag,char *host);
static ASG *asAsgAdd(char *asgName);
static long asAsgAddInp(ASG *pasg,char *inp,int inpIndex);
static ASGRULE *asAsgAddRule(ASG *pasg,asAccessRights access,int level);
static long asAsgRuleUagAdd(ASGRULE *pasgrule,char *name);
static long asAsgRuleHagAdd(ASGRULE *pasgrule,char *name);
static long asAsgRuleCalc(ASGRULE *pasgrule,char *calc);

long asInitialize(ASINPUTFUNCPTR inputfunction)
{
    ASG		*pasg;
    long	status;
    ASBASE	*pasbaseold;
    GPHENTRY	*pgphentry;
    UAG		*puag;
    UAGNAME	*puagname;
    HAG		*phag;
    HAGNAME	*phagname;

    pasbaseold = pasbase;
    pasbase = asCalloc(1,sizeof(ASBASE));
    if(!freeListPvt) freeListInitPvt(&freeListPvt,sizeof(ASGCLIENT),20);
    ellInit(&pasbase->uagList);
    ellInit(&pasbase->hagList);
    ellInit(&pasbase->asgList);
    asAsgAdd(DEFAULT);
    status = myParse(inputfunction);
    if(status) {
	status = S_asLib_badConfig;
	/*Not safe to call asFreeAll */
	pasbase = pasbaseold;
	return(status);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasg->pavalue = asCalloc(ASMAXINP,sizeof(double));
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    gphInitPvt(&pasbase->phash);
    /*Hash each uagname and each hagname*/
    puag = (UAG *)ellFirst(&pasbase->uagList);
    while(puag) {
	puagname = (UAGNAME *)ellFirst(&puag->list);
	while(puagname) {
	    pgphentry = gphAdd(pasbase->phash,puagname->user,puag);
	    if(!pgphentry) errMessage(-1,"asInitialize: gphAdd logic error");
	    puagname = (UAGNAME *)ellNext((ELLNODE *)puagname);
	}
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    phag = (HAG *)ellFirst(&pasbase->hagList);
    while(phag) {
	phagname = (HAGNAME *)ellFirst(&phag->list);
	while(phagname) {
	    pgphentry = gphAdd(pasbase->phash,phagname->host,phag);
	    if(!pgphentry) errMessage(-1,"asInitialize: gphAdd logic error");
	    phagname = (HAGNAME *)ellNext((ELLNODE *)phagname);
	}
	phag = (HAG *)ellNext((ELLNODE *)phag);
    }
    asActive = TRUE;
    if(pasbaseold) {
	ASG		*poldasg;
	ASGMEMBER	*poldmem;
	ASGMEMBER	*pnextoldmem;

	poldasg = (ASG *)ellFirst(&pasbaseold->asgList);
	while(poldasg) {
	    poldmem = (ASGMEMBER *)ellFirst(&poldasg->memberList);
	    while(poldmem) {
		pnextoldmem = (ASGMEMBER *)ellNext((ELLNODE *)poldmem);
		ellDelete(&poldasg->memberList,(ELLNODE *)poldmem);
		status = asAddMember(&poldmem,poldmem->asgName);
		poldmem = pnextoldmem;
	    }
	    poldasg = (ASG *)ellNext((ELLNODE *)poldasg);
	}
	asFreeAll(pasbaseold);
    }
    return(0);
}

long asAddMember(ASMEMBERPVT *pasMemberPvt,char *asgName)
{
    ASGMEMBER	*pasgmember;
    ASG		*pgroup;

    if(!asActive) return(0);
    if(*pasMemberPvt) {
	pasgmember = *pasMemberPvt;
    } else {
	pasgmember = asCalloc(1,sizeof(ASGMEMBER));
	ellInit(&pasgmember->clientList);
	*pasMemberPvt = pasgmember;
    }
    pasgmember->asgName = asgName;
    pgroup = (ASG *)ellFirst(&pasbase->asgList);
    while(pgroup) {
	if(strcmp(pgroup->name,pasgmember->asgName)==0) goto got_it;
	pgroup = (ASG *)ellNext((ELLNODE *)pgroup);
    }
    /* Put it in DEFAULT*/
    pgroup = (ASG *)ellFirst(&pasbase->asgList);
    while(pgroup) {
	if(strcmp(pgroup->name,DEFAULT)==0) goto got_it;
	pgroup = (ASG *)ellNext((ELLNODE *)pgroup);
    }
    errMessage(-1,"Logic Error in asAddMember");
    exit(1);
got_it:
    pasgmember->pasg = pgroup;
    ellAdd(&pgroup->memberList,(ELLNODE *)pasgmember);
    return(0);
}

long asRemoveMember(ASMEMBERPVT *asMemberPvt)
{
    ASGMEMBER	*pasgmember;

    if(!asActive) return(0);
    pasgmember = *asMemberPvt;
    if(ellCount(&pasgmember->clientList)>0) return(S_asLib_clientsExist);
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,(ELLNODE *)pasgmember);
    } else {
	errMessage(-1,"Logic error in asRemoveMember");
	exit(-1);
    }
    free((void *)pasgmember);
    *asMemberPvt = NULL;
    return(0);
}

long asChangeGroup(ASMEMBERPVT *asMemberPvt,char *newAsgName)
{
    ASGMEMBER	*pasgmember;
    long	status;

    if(!asActive) return(0);
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    pasgmember = *asMemberPvt;
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,(ELLNODE *)pasgmember);
    } else {
	errMessage(-1,"Logic error in asChangeGroup");
#ifdef vxWorks
	FASTUNLOCK(&asLock);
#endif
	exit(-1);
    }
    status = asAddMember(asMemberPvt,newAsgName);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

void *asGetMemberPvt(ASMEMBERPVT asMemberPvt)
{
    ASGMEMBER	*pasgmember = asMemberPvt;

    if(!asActive) return(NULL);
    if(!pasgmember) return(NULL);
    return(pasgmember->userPvt);
}

void asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt)
{
    ASGMEMBER	*pasgmember = asMemberPvt;
    if(!asActive) return;
    if(!pasgmember) return;
    pasgmember->userPvt = userPvt;
}

long asAddClient(ASCLIENTPVT *pasClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,char *user,char *host)
{
    ASGMEMBER	*pasgmember = asMemberPvt;
    ASGCLIENT	*pasgclient;
    long	status;
    if(!asActive) return(0);
    pasgclient = freeListCalloc(freeListPvt);
    *pasClientPvt = pasgclient;
    pasgclient->pasgMember = asMemberPvt;
    pasgclient->level = asl;
    pasgclient->user = user;
    pasgclient->host = host;
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    ellAdd(&pasgmember->clientList,(ELLNODE *)pasgclient);
    status = asCompute(pasgclient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

long asChangeClient(ASCLIENTPVT asClientPvt,int asl,char *user,char *host)
{
    ASGCLIENT	*pasgclient = asClientPvt;
    long	status;

    if(!asActive) return(0);
    pasgclient->level = asl;
    pasgclient->user = user;
    pasgclient->host = host;
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    status = asCompute(pasgclient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

long asRemoveClient(ASCLIENTPVT *asClientPvt)
{
    ASGCLIENT	*pasgclient = *asClientPvt;
    ASGMEMBER	*pasgMember;

    if(!asActive) return(0);
    if(!pasgclient) return(0);
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    pasgMember = pasgclient->pasgMember;
    if(!pasgMember) {
	errMessage(-1,"asRemoveClient: No ASGMEMBER");
#ifdef vxWorks
	FASTUNLOCK(&asLock);
#endif
	return(-1);
    }
    ellDelete(&pasgMember->clientList,(ELLNODE *)pasgclient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    freeListFree(freeListPvt,pasgclient);
    *asClientPvt = NULL;
    return(0);
}

void asRegisterClientCallback(ASCLIENTPVT asClientPvt,
	ASCLIENTCALLBACK pcallback)
{
    ASGCLIENT	*pasgclient = asClientPvt;

#ifdef vxWorks
    FASTLOCK(&asLock);
    pasgclient->pcallback = pcallback;
    (*pasgclient->pcallback)(pasgclient,asClientCOAR);
    FASTUNLOCK(&asLock);
#endif
}

void *asGetClientPvt(ASCLIENTPVT asClientPvt)
{
    ASGCLIENT	*pasgclient = asClientPvt;

    if(!asActive) return(NULL);
    if(!pasgclient) return(NULL);
    return(pasgclient->userPvt);
}

void asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt)
{
    ASGCLIENT	*pasgclient = asClientPvt;
    if(!asActive) return;
    if(!pasgclient) return;
    pasgclient->userPvt = userPvt;
}

long asComputeAllAsg(void)
{
    ASG         *pasg;

    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	asComputeAsg(pasg);
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
}

long asComputeAsg(ASG *pasg)
{
    ASGRULE	*pasgrule;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
    while(pasgrule) {
	double	result;
	long	status;

	if(pasgrule->calc && (pasg->inpChanged & pasgrule->inpUsed)) {
	    status = calcPerform(pasg->pavalue,&result,pasgrule->rpcl);
	    if(status) {
		pasgrule->result = 0;
		errMessage(status,"asComputeAsg");
	    } else {
		pasgrule->result = ((result>.99) && (result<1.01)) ? 1 : 0;
	    }
	}
	pasgrule = (ASGRULE *)ellNext((ELLNODE *)pasgrule);
    }
    pasg->inpChanged = FALSE;
    pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
    while(pasgmember) {
	pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	while(pasgclient) {
	    asCompute((ASCLIENTPVT)pasgclient);
	    pasgclient = (ASGCLIENT *)ellNext((ELLNODE *)pasgclient);
	}
	pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
    }
    return(0);
}

long asCompute(ASCLIENTPVT asClientPvt)
{
    asAccessRights	access=asNOACCESS;
    ASGCLIENT		*pasgclient = asClientPvt;
    ASGMEMBER		*pasgMember = pasgclient->pasgMember;
    ASG			*pasg = pasgMember->pasg;
    ASGRULE		*pasgrule;
    asAccessRights	oldaccess=pasgclient->access;
    GPHENTRY		*pgphentry;

    if(!pasg) return(0);
    pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
    while(pasgrule) {
	if(access == asWRITE) break;
	if(access>=pasgrule->access) goto next_rule;
	if(pasgclient->level > pasgrule->level) goto next_rule;
	/*if uagList is empty then no need to check uag*/
	if(ellCount(&pasgrule->uagList)>0){
	    ASGUAG	*pasguag;
	    UAG		*puag;

	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    while(pasguag) {
		if(puag = pasguag->puag) {
		    pgphentry = gphFind(pasbase->phash,pasgclient->user,puag);
		    if(pgphentry) goto check_hag;
		}
		pasguag = (ASGUAG *)ellNext((ELLNODE *)pasguag);
	    }
	    goto next_rule;
	}
check_hag:
	/*if hagList is empty then no need to check hag*/
	if(ellCount(&pasgrule->hagList)>0) {
	    ASGHAG	*pasghag;
	    HAG		*phag;

	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    while(pasghag) {
		if(phag = pasghag->phag) {
		    pgphentry=gphFind(pasbase->phash,pasgclient->host,phag);
		    if(pgphentry) goto check_calc;
		}
		pasghag = (ASGHAG *)ellNext((ELLNODE *)pasghag);
	    }
	    goto next_rule;
	}
check_calc:
	if(!pasgrule->calc
	|| (!(pasg->inpBad & pasgrule->inpUsed) && (pasgrule->result==1)))
	    access = pasgrule->access;
next_rule:
	pasgrule = (ASGRULE *)ellNext((ELLNODE *)pasgrule);
    }
    pasgclient->access = access;
    if(pasgclient->pcallback && oldaccess!=access) {
	(*pasgclient->pcallback)(pasgclient,asClientCOAR);
    }
    return(0);
}

static char *asAccessName[] = {"NONE","READ","WRITE"};
static char *asLevelName[] = {"ASL0","ASL1"};
int asDump(
	void (*memcallback)(struct asgMember *),
	void (*clientcallback)(struct asgClient *),
	int verbose)
{
    UAG		*puag;
    UAGNAME	*puagname;
    HAG		*phag;
    HAGNAME	*phagname;
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGRULE	*pasgrule;
    ASGHAG	*pasghag;
    ASGUAG	*pasguag;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    if(!puag) printf("No UAGs\n");
    while(puag) {
	printf("UAG(%s)",puag->name);
	puagname = (UAGNAME *)ellFirst(&puag->list);
	if(puagname) printf(" {"); else printf("\n");
	while(puagname) {
	    printf("%s",puagname->user);
	    puagname = (UAGNAME *)ellNext((ELLNODE *)puagname);
	    if(puagname) printf(","); else printf("}\n");
	}
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    phag = (HAG *)ellFirst(&pasbase->hagList);
    if(!phag) printf("No HAGs\n");
    while(phag) {
	printf("HAG(%s)",phag->name);
	phagname = (HAGNAME *)ellFirst(&phag->list);
	if(phagname) printf(" {"); else printf("\n");
	while(phagname) {
	    printf("%s",phagname->host);
	    phagname = (HAGNAME *)ellNext((ELLNODE *)phagname);
	    if(phagname) printf(","); else printf("}\n");
	}
	phag = (HAG *)ellNext((ELLNODE *)phag);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) printf("No ASGs\n");
    while(pasg) {
	int print_end_brace;

	printf("ASG(%s)",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	if(pasginp || pasgrule) {
	    printf(" {\n");
	    print_end_brace = TRUE;
	} else {
	    printf("\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {

	    printf("\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    if(verbose) {
		if((pasg->inpBad & (1<<pasginp->inpIndex)))
			printf(" INVALID");
		else
			printf("   VALID");
		printf(" value=%f",pasg->pavalue[pasginp->inpIndex]);
	    }
	    printf("\n");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	while(pasgrule) {
	    int	print_end_brace;

	    printf("\tRULE(%d,%s)",
		pasgrule->level,asAccessName[pasgrule->access]);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasguag || pasghag || pasgrule->calc) {
		printf(" {\n");
		print_end_brace = TRUE;
	    } else {
		printf("\n");
		print_end_brace = FALSE;
	    }
	    if(pasguag) printf("\t\tUAG(");
	    while(pasguag) {
		printf("%s",pasguag->puag->name);
		pasguag = (ASGUAG *)ellNext((ELLNODE *)pasguag);
		if(pasguag) printf(","); else printf(")\n");
	    }
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasghag) printf("\t\tHAG(");
	    while(pasghag) {
		printf("%s",pasghag->phag->name);
		pasghag = (ASGHAG *)ellNext((ELLNODE *)pasghag);
		if(pasghag) printf(","); else printf(")\n");
	    }
	    if(pasgrule->calc) {
		printf("\t\tCALC(\"%s\")",pasgrule->calc);
		if(verbose)
		    printf(" result=%s",(pasgrule->result ? "TRUE" : "FALSE"));
		printf("\n");
	    }
	    if(print_end_brace) printf("\t}\n");
	    pasgrule = (ASGRULE *)ellNext((ELLNODE *)pasgrule);
	}
	pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
	if(!verbose) pasgmember = NULL;
	if(pasgmember) printf("\tMEMBERLIST\n");
	while(pasgmember) {
	    if(strlen(pasgmember->asgName)==0) 
		printf("\t\t<null>");
	    else
		printf("\t\t%s",pasgmember->asgName);
	    if(memcallback) memcallback(pasgmember);
	    printf("\n");
	    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    while(pasgclient) {
		printf("\t\t\t %s %s",pasgclient->user,pasgclient->host);
		if(pasgclient->level>=0 && pasgclient->level<=1)
			printf(" %s",asLevelName[pasgclient->level]);
		else
			printf(" Illegal Level %d",pasgclient->level);
		if(pasgclient->access>=0 && pasgclient->access<=2)
			printf(" %s",asAccessName[pasgclient->access]);
		else
			printf(" Illegal Access %d",pasgclient->access);
		if(clientcallback) clientcallback(pasgclient);
		printf("\n");
		pasgclient = (ASGCLIENT *)ellNext((ELLNODE *)pasgclient);
	    }
	    pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
	}
	if(print_end_brace) printf("}\n");
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
}

int asDumpUag(char *uagname)
{
    UAG		*puag;
    UAGNAME	*puagname;

    if(!asActive) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    if(!puag) printf("No UAGs\n");
    while(puag) {
	if(uagname && strcmp(uagname,puag->name)!=0) {
	    puag = (UAG *)ellNext((ELLNODE *)puag);
	    continue;
	}
	printf("UAG(%s)",puag->name);
	puagname = (UAGNAME *)ellFirst(&puag->list);
	if(puagname) printf(" {"); else printf("\n");
	while(puagname) {
	    printf("%s",puagname->user);
	    puagname = (UAGNAME *)ellNext((ELLNODE *)puagname);
	    if(puagname) printf(","); else printf("}\n");
	}
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    return(0);
}

int asDumpHag(char *hagname)
{
    HAG		*phag;
    HAGNAME	*phagname;

    if(!asActive) return(0);
    phag = (HAG *)ellFirst(&pasbase->hagList);
    if(!phag) printf("No HAGs\n");
    while(phag) {
	if(hagname && strcmp(hagname,phag->name)!=0) {
	    phag = (HAG *)ellNext((ELLNODE *)phag);
	    continue;
	}
	printf("HAG(%s)",phag->name);
	phagname = (HAGNAME *)ellFirst(&phag->list);
	if(phagname) printf(" {"); else printf("\n");
	while(phagname) {
	    printf("%s",phagname->host);
	    phagname = (HAGNAME *)ellNext((ELLNODE *)phagname);
	    if(phagname) printf(","); else printf("}\n");
	}
	phag = (HAG *)ellNext((ELLNODE *)phag);
    }
    return(0);
}

int asDumpRules(char *asgname)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGRULE	*pasgrule;
    ASGHAG	*pasghag;
    ASGUAG	*pasguag;

    if(!asActive) return(0);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) printf("No ASGs\n");
    while(pasg) {
	int print_end_brace;

	if(asgname && strcmp(asgname,pasg->name)!=0) {
	    pasg = (ASG *)ellNext((ELLNODE *)pasg);
	    continue;
	}
	printf("ASG(%s)",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	if(pasginp || pasgrule) {
	    printf(" {\n");
	    print_end_brace = TRUE;
	} else {
	    printf("\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {

	    printf("\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    if((pasg->inpBad & (1<<pasginp->inpIndex))) printf(" INVALID");
	    printf(" value=%f",pasg->pavalue[pasginp->inpIndex]);
	    printf("\n");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	while(pasgrule) {
	    int	print_end_brace;

	    printf("\tRULE(%d,%s)",
		pasgrule->level,asAccessName[pasgrule->access]);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasguag || pasghag || pasgrule->calc) {
		printf(" {\n");
		print_end_brace = TRUE;
	    } else {
		printf("\n");
		print_end_brace = FALSE;
	    }
	    if(pasguag) printf("\t\tUAG(");
	    while(pasguag) {
		printf("%s",pasguag->puag->name);
		pasguag = (ASGUAG *)ellNext((ELLNODE *)pasguag);
		if(pasguag) printf(","); else printf(")\n");
	    }
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasghag) printf("\t\tHAG(");
	    while(pasghag) {
		printf("%s",pasghag->phag->name);
		pasghag = (ASGHAG *)ellNext((ELLNODE *)pasghag);
		if(pasghag) printf(","); else printf(")\n");
	    }
	    if(pasgrule->calc) {
		printf("\t\tCALC(\"%s\")",pasgrule->calc);
		printf(" result=%s",(pasgrule->result ? "TRUE" : "FALSE"));
		printf("\n");
	    }
	    if(print_end_brace) printf("\t}\n");
	    pasgrule = (ASGRULE *)ellNext((ELLNODE *)pasgrule);
	}
	if(print_end_brace) printf("}\n");
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
}

int asDumpMem(char *asgname,void (*memcallback)(ASMEMBERPVT),int clients)
{
    ASG		*pasg;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) printf("No ASGs\n");
    while(pasg) {

	if(asgname && strcmp(asgname,pasg->name)!=0) {
	    pasg = (ASG *)ellNext((ELLNODE *)pasg);
	    continue;
	}
	printf("ASG(%s)\n",pasg->name);
	pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
	if(pasgmember) printf("\tMEMBERLIST\n");
	while(pasgmember) {
	    if(strlen(pasgmember->asgName)==0) 
		printf("\t\t<null>");
	    else
		printf("\t\t%s",pasgmember->asgName);
	    if(memcallback) memcallback(pasgmember);
	    printf("\n");
	    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    if(!clients) pasgclient = NULL;
	    while(pasgclient) {
		printf("\t\t\t %s %s",
		    pasgclient->user,pasgclient->host);
		if(pasgclient->level>=0 && pasgclient->level<=1)
		    printf(" %s",asLevelName[pasgclient->level]);
		else
		    printf(" Illegal Level %d",pasgclient->level);
		if(pasgclient->access>=0 && pasgclient->access<=2)
		    printf(" %s",asAccessName[pasgclient->access]);
		else
		    printf(" Illegal Access %d",pasgclient->access);
		printf("\n");
		pasgclient = (ASGCLIENT *)ellNext((ELLNODE *)pasgclient);
	    }
	    pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
}

int asDumpHash(void)
{
    gphDump(pasbase->phash);
}

/*Start of private routines*/
void * asCalloc(size_t nobj,size_t size)
{
    void *p;

    p=calloc(nobj,size);
    if(p) return(p);
#ifdef vxWorks
    taskSuspend(0);
#else
    abort();
#endif
    return(NULL);
}

static void asFreeAll(ASBASE *pasbase)
{
    UAG		*puag;
    UAGNAME	*puagname;
    HAG		*phag;
    HAGNAME	*phagname;
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGRULE	*pasgrule;
    ASGHAG	*pasghag;
    ASGUAG	*pasguag;
    void	*pnext;

    puag = (UAG *)ellFirst(&pasbase->uagList);
    while(puag) {
	puagname = (UAGNAME *)ellFirst(&puag->list);
	while(puagname) {
	    pnext = ellNext((ELLNODE *)puagname);
	    ellDelete(&puag->list,(ELLNODE *)puagname);
	    free((void *)puagname);
	    puagname = pnext;
	}
	pnext = ellNext((ELLNODE *)puag);
	ellDelete(&pasbase->uagList,(ELLNODE *)puag);
	free((void *)puag);
	puag = pnext;
    }
    phag = (HAG *)ellFirst(&pasbase->hagList);
    while(phag) {
	phagname = (HAGNAME *)ellFirst(&phag->list);
	while(phagname) {
	    pnext = ellNext((ELLNODE *)phagname);
	    ellDelete(&phag->list,(ELLNODE *)phagname);
	    free((void *)phagname);
	    phagname = pnext;
	}
	pnext = ellNext((ELLNODE *)phag);
	ellDelete(&pasbase->hagList,(ELLNODE *)phag);
	free((void *)phag);
	phag = pnext;
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	free((void *)pasg->pavalue);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    pnext = ellNext((ELLNODE *)pasginp);
	    ellDelete(&pasg->inpList,(ELLNODE *)pasginp);
	    free((void *)pasginp);
	    pasginp = pnext;
	}
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	while(pasgrule) {
	    free((void *)pasgrule->calc);
	    free((void *)pasgrule->rpcl);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    while(pasguag) {
		pnext = ellNext((ELLNODE *)pasguag);
		ellDelete(&pasgrule->uagList,(ELLNODE *)pasguag);
		free((void *)pasguag);
		pasguag = pnext;
	    }
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    while(pasghag) {
		pnext = ellNext((ELLNODE *)pasghag);
		ellDelete(&pasgrule->hagList,(ELLNODE *)pasghag);
		free((void *)pasghag);
		pasghag = pnext;
	    }
	    pnext = ellNext((ELLNODE *)pasgrule);
	    ellDelete(&pasg->ruleList,(ELLNODE *)pasgrule);
	    free((void *)pasgrule);
	    pasgrule = pnext;
	}
	pnext = ellNext((ELLNODE *)pasg);
	ellDelete(&pasbase->asgList,(ELLNODE *)pasg);
	free((void *)pasg);
	pasg = pnext;
    }
    gphFreeMem(pasbase->phash);
    free((void *)pasbase);
}

static UAG *asUagAdd(char *uagName)
{
    UAG		*pprev;
    UAG		*pnext;
    UAG		*puag;
    int		cmpvalue;
    ASBASE	*pbase = (ASBASE *)pasbase;

    /*Insert in alphabetic order*/
    pnext = (UAG *)ellFirst(&pasbase->uagList);
    while(pnext) {
	cmpvalue = strcmp(uagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errMessage(-1,"Duplicate User Access Group");
	    return(NULL);
	}
	pnext = (UAG *)ellNext((ELLNODE *)pnext);
    }
    puag = asCalloc(1,sizeof(UAG)+strlen(uagName)+1);
    ellInit(&puag->list);
    puag->name = (char *)(puag+1);
    strcpy(puag->name,uagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pbase->uagList,(ELLNODE *)puag);
    } else {
	pprev = (UAG *)ellPrevious((ELLNODE *)pnext);
        ellInsert(&pbase->uagList,(ELLNODE *)pprev,(ELLNODE *)puag);
    }
    return(puag);
}

static long asUagAddUser(UAG *puag,char *user)
{
    UAGNAME	*puagname;

    if(!puag) return(0);
    puagname = asCalloc(1,sizeof(UAG)+strlen(user)+1);
    puagname->user = (char *)(puagname+1);
    strcpy(puagname->user,user);
    ellAdd(&puag->list,(ELLNODE *)puagname);
    return(0);
}

static HAG *asHagAdd(char *hagName)
{
    HAG		*pprev;
    HAG		*pnext;
    HAG		*phag;
    int		cmpvalue;
    ASBASE	*pbase = (ASBASE *)pasbase;

    /*Insert in alphabetic order*/
    pnext = (HAG *)ellFirst(&pasbase->hagList);
    while(pnext) {
	cmpvalue = strcmp(hagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errMessage(-1,"Duplicate Host Access Group");
	    return(NULL);
	}
	pnext = (HAG *)ellNext((ELLNODE *)pnext);
    }
    phag = asCalloc(1,sizeof(HAG)+strlen(hagName)+1);
    ellInit(&phag->list);
    phag->name = (char *)(phag+1);
    strcpy(phag->name,hagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pbase->hagList,(ELLNODE *)phag);
    } else {
	pprev = (HAG *)ellPrevious((ELLNODE *)pnext);
	ellInsert(&pbase->hagList,(ELLNODE *)pprev,(ELLNODE *)phag);
    }
    return(phag);
}

static long asHagAddHost(HAG *phag,char *host)
{
    HAGNAME	*phagname;

    if(!phag) return(0);
    phagname = asCalloc(1,sizeof(HAG)+strlen(host)+1);
    phagname->host = (char *)(phagname+1);
    strcpy(phagname->host,host);
    ellAdd(&phag->list,(ELLNODE *)phagname);
    return(0);
}

static ASG *asAsgAdd(char *asgName)
{
    ASG		*pprev;
    ASG		*pnext;
    ASG		*pasg;
    int		cmpvalue;
    ASBASE	*pbase = (ASBASE *)pasbase;

    /*Insert in alphabetic order*/
    pnext = (ASG *)ellFirst(&pasbase->asgList);
    while(pnext) {
	cmpvalue = strcmp(asgName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    if(strcmp(DEFAULT,pnext->name)==0) {
		if(ellCount(&pnext->inpList)==0
		&& ellCount(&pnext->ruleList)==0)
			return(pnext);
	    }
	    errMessage(S_asLib_dupAsg,NULL);
	    return(NULL);
	}
	pnext = (ASG *)ellNext((ELLNODE *)pnext);
    }
    pasg = asCalloc(1,sizeof(ASG)+strlen(asgName)+1);
    ellInit(&pasg->inpList);
    ellInit(&pasg->ruleList);
    ellInit(&pasg->memberList);
    pasg->name = (char *)(pasg+1);
    strcpy(pasg->name,asgName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pbase->asgList,(ELLNODE *)pasg);
    } else {
	pprev = (ASG *)ellPrevious((ELLNODE *)pnext);
	ellInsert(&pbase->asgList,(ELLNODE *)pprev,(ELLNODE *)pasg);
    }
    return(pasg);
}

static long asAsgAddInp(ASG *pasg,char *inp,int inpIndex)
{
    ASGINP	*pasginp;

    if(!pasg) return(0);
    pasginp = asCalloc(1,sizeof(ASGINP)+strlen(inp)+1);
    pasginp->inp = (char *)(pasginp+1);
    strcpy(pasginp->inp,inp);
    pasginp->pasg = pasg;
    pasginp->inpIndex = inpIndex;
    ellAdd(&pasg->inpList,(ELLNODE *)pasginp);
    return(0);
}

static ASGRULE *asAsgAddRule(ASG *pasg,asAccessRights access,int level)
{
    ASGRULE	*pasgrule;

    if(!pasg) return(0);
    pasgrule = asCalloc(1,sizeof(ASGRULE));
    pasgrule->access = access;
    pasgrule->level = level;
    ellInit(&pasgrule->uagList);
    ellInit(&pasgrule->hagList);
    ellAdd(&pasg->ruleList,(ELLNODE *)pasgrule);
    return(pasgrule);
}

static long asAsgRuleUagAdd(ASGRULE *pasgrule,char *name)
{
    ASGUAG	*pasguag;
    UAG		*puag;

    if(!pasgrule) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    while(puag) {
	if(strcmp(puag->name,name)==0) break;
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    if(!puag) return(S_asLib_noUag);
    pasguag = asCalloc(1,sizeof(ASGUAG));
    pasguag->puag = puag;
    ellAdd(&pasgrule->uagList,(ELLNODE *)pasguag);
    return(0);
}

static long asAsgRuleHagAdd(ASGRULE *pasgrule,char *name)
{
    ASGHAG	*pasghag;
    HAG		*phag;

    if(!pasgrule) return(0);
    phag = (HAG *)ellFirst(&pasbase->hagList);
    while(phag) {
	if(strcmp(phag->name,name)==0) break;
	phag = (HAG *)ellNext((ELLNODE *)phag);
    }
    if(!phag) return(S_asLib_noHag);
    pasghag = asCalloc(1,sizeof(ASGHAG));
    pasghag->phag = phag;
    ellAdd(&pasgrule->hagList,(ELLNODE *)pasghag);
    return(0);
}

static long asAsgRuleCalc(ASGRULE *pasgrule,char *calc)
{
    short	error_number;
    long	status;

    if(!pasgrule) return(0);
    pasgrule->calc = asCalloc(1,strlen(calc)+1);
    strcpy(pasgrule->calc,calc);
    pasgrule->rpcl = asCalloc(1,RPCL_LEN);
    status=postfix(pasgrule->calc,pasgrule->rpcl,&error_number);
    if(status) {
	free((void *)pasgrule->calc);
	free((void *)pasgrule->rpcl);
	pasgrule->calc = NULL;
	pasgrule->rpcl = NULL;
	status = S_asLib_badCalc;
    } else {
	int i;

	for(i=0; i<ASMAXINP; i++) {
	    if(strchr(calc,'A'+i)) pasgrule->inpUsed |= (1<<i);
	    if(strchr(calc,'a'+i)) pasgrule->inpUsed |= (1<<i);
	}
    }
    return(status);
}
