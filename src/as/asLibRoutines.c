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
#ifdef vxWorks
#include <taskLib.h>
#endif

/*Declare storage for Global Variables */
ASBASE		*pasbase=NULL;
int	asActive = FALSE;

/*Forward declarations for Non ANSI routines*/
long calcPerform(double *parg,double *presult,char   *post);
long postfix(char *pinfix, char *ppostfix,short *perror);

#define RPCL_LEN 184
#define DEFAULT "DEFAULT"


/*private routines */
static void asFreeAll(ASBASE *pasbase);
static UAG *asUagAdd(char *uagName);
static long asUagAddUser(UAG *puag,char *user);
static LAG *asLagAdd(char *lagName);
static long asLagAddLocation(LAG *plag,char *location);
static ASG *asAsgAdd(char *asgName);
static long asAsgAddInp(ASG *pasg,char *inp,int inpIndex);
static ASGLEVEL *asAsgAddLevel(ASG *pasg,asAccessRights access,int level);
static long asAsgLevelUagAdd(ASGLEVEL *pasglevel,char *name);
static long asAsgLevelLagAdd(ASGLEVEL *pasglevel,char *name);
static long asAsgLevelCalc(ASGLEVEL *pasglevel,char *calc);

long asInitialize(ASINPUTFUNCPTR inputfunction)
{
    ASG		*pasg;
    long	status;
    ASBASE	*pasbaseold;

    pasbaseold = pasbase;
    pasbase = asCalloc(1,sizeof(ASBASE));
    ellInit(&pasbase->uagList);
    ellInit(&pasbase->lagList);
    ellInit(&pasbase->asgList);
    asAsgAdd(DEFAULT);
    status = myParse(inputfunction);
    if(status) {
	status = S_asLib_badConfig;
	/*Not safe to call asFreeAll */
	pasbase = pasbaseold;
	return(status);
    }
    asActive = TRUE;
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasg->pavalue = asCalloc(ASMAXINP,sizeof(double));
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
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
    long	status;

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
    ASGCLIENT	*pclient;
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
    pclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
    while(pclient) {
	    status = asCompute(pclient);
	    if(status) {
		errMessage(status,"asAddMember");
	    }
	    pclient = (ASGCLIENT *)ellNext((ELLNODE *)pclient);
    }
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

void *asGetMemberPvt(ASMEMBERPVT asMemberPvt)
{
    ASGMEMBER	*pasmember = asMemberPvt;

    if(!asActive) return(NULL);
    if(!pasmember) return(NULL);
    return(pasmember->userPvt);
}

void asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt)
{
    ASGMEMBER	*pasmember = asMemberPvt;
    if(!asActive) return;
    if(!pasmember) return;
    pasmember->userPvt = userPvt;
}

long asAddClient(ASCLIENTPVT *pasClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,char *user,char *location)
{
    ASGMEMBER	*pasmember = asMemberPvt;
    ASGCLIENT	*pasclient;
    long	status;
    if(!asActive) return(0);
    pasclient = asCalloc(1,sizeof(ASGCLIENT));
    *pasClientPvt = pasclient;
    pasclient->pasgMember = asMemberPvt;
    pasclient->level = asl;
    pasclient->user = user;
    pasclient->location = location;
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    ellAdd(&pasmember->clientList,(ELLNODE *)pasclient);
    status = asCompute(pasclient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

long asChangeClient(ASCLIENTPVT asClientPvt,int asl,char *user,char *location)
{
    ASGCLIENT	*pasclient = asClientPvt;
    long	status;

    if(!asActive) return(0);
    pasclient->level = asl;
    pasclient->user = user;
    pasclient->location = location;
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    status = asCompute(pasclient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    return(status);
}

long asRemoveClient(ASCLIENTPVT *asClientPvt)
{
    ASGCLIENT	*pasgClient = *asClientPvt;
    ASGMEMBER	*pasgMember;

    if(!asActive) return(0);
    if(!pasgClient) return(0);
#ifdef vxWorks
    FASTLOCK(&asLock);
#endif
    pasgMember = pasgClient->pasgMember;
    if(!pasgMember) {
	errMessage(-1,"asRemoveClient: No ASGMEMBER");
#ifdef vxWorks
	FASTUNLOCK(&asLock);
#endif
	return(-1);
    }
    ellDelete(&pasgMember->clientList,(ELLNODE *)pasgClient);
#ifdef vxWorks
    FASTUNLOCK(&asLock);
#endif
    free((void *)pasgClient);
    *asClientPvt = NULL;
    return(0);
}

void *asGetClientPvt(ASCLIENTPVT asClientPvt)
{
    ASGCLIENT	*pasclient = asClientPvt;

    if(!asActive) return(NULL);
    if(!pasclient) return(NULL);
    return(pasclient->userPvt);
}

void asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt)
{
    ASGCLIENT	*pasclient = asClientPvt;
    if(!asActive) return;
    if(!pasclient) return;
    pasclient->userPvt = userPvt;
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
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
    while(pasgmember) {
	pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	while(pasgclient) {
	    asCompute((ASCLIENTPVT)pasgclient);
	    pasgclient = (ASGCLIENT *)ellNext((ELLNODE *)pasgclient);
	}
	pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
    }
}

long asCompute(ASCLIENTPVT asClientPvt)
{
    asAccessRights	access=asNOACCESS;
    ASGCLIENT		*pasgClient = asClientPvt;
    ASGMEMBER		*pasgMember = pasgClient->pasgMember;
    ASG			*pasg = pasgMember->pasg;
    ASGLEVEL		*pasglevel;
    double		result;
    long		status;

    if(!pasg) return(0);
    while(TRUE) {
	pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
	while(pasglevel) {
	    if(access == asWRITE) break;
	    if(access>=pasglevel->access) goto next_level;
	    if(pasgClient->level > pasglevel->level) goto next_level;
	    /*if uagList is empty then no need to check uag*/
	    if(ellCount(&pasglevel->uagList)>0){
		ASGUAG	*pasguag;
		UAG		*puag;
		UAGNAME	*puagname;

		pasguag = (ASGUAG *)ellFirst(&pasglevel->uagList);
		while(pasguag) {
		    if(puag = pasguag->puag) {
			puagname = (UAGNAME *)ellFirst(&puag->list);
			while(puagname) {
			    if(strcmp(pasgClient->user,puagname->user)==0) 
				goto check_lag;
			    puagname = (UAGNAME *)ellNext((ELLNODE *)puagname);
			}
		    }
		    pasguag = (ASGUAG *)ellNext((ELLNODE *)pasguag);
		}
		goto next_level;
	    }
check_lag:
	    /*if lagList is empty then no need to check lag*/
	    if(ellCount(&pasglevel->lagList)>0) {
		ASGLAG	*pasglag;
		LAG		*plag;
		LAGNAME	*plagname;

		pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
		while(pasglag) {
		    if(plag = pasglag->plag) {
			plagname = (LAGNAME *)ellFirst(&plag->list);
			while(plagname) {
			    if(strcmp(pasgClient->location,plagname->location)==0) 
				goto check_calc;
			    plagname = (LAGNAME *)ellNext((ELLNODE *)plagname);
			}
		    }
		    pasglag = (ASGLAG *)ellNext((ELLNODE *)pasglag);
		}
		goto next_level;
	    }
check_calc:
	    if(!pasglevel->calc) {
		access = pasglevel->access;
		goto next_level;
	    } else {/*If any INP used in calc is bad NO_ACCESS*/
		if(pasg->inpBad & pasglevel->inpUsed) goto next_level;
	    }
	    status = calcPerform(pasg->pavalue,&result,pasglevel->rpcl);
	    if(!status && result==1e0) access = pasglevel->access;
next_level:
	    pasglevel = (ASGLEVEL *)ellNext((ELLNODE *)pasglevel);
	}
	/*Also check DEFAULT*/
	if(access == asWRITE) break;
	if(strcmp(pasg->name,DEFAULT)==0) break;
	pasg = (ASG *)ellFirst(&pasbase->asgList);
	while(pasg) {
	    if(strcmp(pasg->name,DEFAULT)==0) goto again;
	    pasg = (ASG *)ellNext((ELLNODE *)pasg);
	}
	errMessage(-1,"Logic Error in asCompute");
	break;
again:
	continue;
    }
    pasgClient->access = access;
    return(0);
}

static char *asAccessName[] = {"NONE","READ","WRITE"};
int asDump(
	void (*memcallback)(struct asgMember *),
	void (*clientcallback)(struct asgClient *),
	int verbose)
{
    UAG		*puag;
    UAGNAME	*puagname;
    LAG		*plag;
    LAGNAME	*plagname;
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGLEVEL	*pasglevel;
    ASGLAG	*pasglag;
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
    plag = (LAG *)ellFirst(&pasbase->lagList);
    if(!plag) printf("No LAGs\n");
    while(plag) {
	printf("LAG(%s)",plag->name);
	plagname = (LAGNAME *)ellFirst(&plag->list);
	if(plagname) printf(" {"); else printf("\n");
	while(plagname) {
	    printf("%s",plagname->location);
	    plagname = (LAGNAME *)ellNext((ELLNODE *)plagname);
	    if(plagname) printf(","); else printf("}\n");
	}
	plag = (LAG *)ellNext((ELLNODE *)plag);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) printf("No ASGs\n");
    while(pasg) {
	int print_end_brace;

	printf("ASG(%s)",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
	if(pasginp || pasglevel) {
	    printf(" {\n");
	    print_end_brace = TRUE;
	} else {
	    printf("\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {
	    int		bad;
	    double	value;

	    printf("\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    if(verbose) {
		value = pasg->pavalue[pasginp->inpIndex];
		bad = (pasg->inpBad & (1<<pasginp->inpIndex)) ? 1 : 0;
		printf(" Bad=%d value=%f",bad,value);
	    }
	    printf("\n");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	while(pasglevel) {
	    int	print_end_brace;

	    printf("\tLEVEL(%d,%s)",
		pasglevel->level,asAccessName[pasglevel->access]);
	    pasguag = (ASGUAG *)ellFirst(&pasglevel->uagList);
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    if(pasguag || pasglag || pasglevel->calc) {
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
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    if(pasglag) printf("\t\tLAG(");
	    while(pasglag) {
		printf("%s",pasglag->plag->name);
		pasglag = (ASGLAG *)ellNext((ELLNODE *)pasglag);
		if(pasglag) printf(","); else printf(")\n");
	    }
	    if(pasglevel->calc) printf("\t\tCALC(\"%s\")\n",pasglevel->calc);
	    if(print_end_brace) printf("\t}\n");
	    pasglevel = (ASGLEVEL *)ellNext((ELLNODE *)pasglevel);
	}
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
	    while(pasgclient) {
		printf("\t\t\t %s %s %d %d",
		    pasgclient->user,pasgclient->location,
		    pasgclient->level,pasgclient->access);
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

int asDumpLag(char *lagname)
{
    LAG		*plag;
    LAGNAME	*plagname;

    if(!asActive) return(0);
    plag = (LAG *)ellFirst(&pasbase->lagList);
    if(!plag) printf("No LAGs\n");
    while(plag) {
	if(lagname && strcmp(lagname,plag->name)!=0) {
	    plag = (LAG *)ellNext((ELLNODE *)plag);
	    continue;
	}
	printf("LAG(%s)",plag->name);
	plagname = (LAGNAME *)ellFirst(&plag->list);
	if(plagname) printf(" {"); else printf("\n");
	while(plagname) {
	    printf("%s",plagname->location);
	    plagname = (LAGNAME *)ellNext((ELLNODE *)plagname);
	    if(plagname) printf(","); else printf("}\n");
	}
	plag = (LAG *)ellNext((ELLNODE *)plag);
    }
    return(0);
}

int asDumpLev(char *asgname)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGLEVEL	*pasglevel;
    ASGLAG	*pasglag;
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
	pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
	if(pasginp || pasglevel) {
	    printf(" {\n");
	    print_end_brace = TRUE;
	} else {
	    printf("\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {
	    int		bad;
	    double	value;

	    printf("\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    value = pasg->pavalue[pasginp->inpIndex];
	    bad = (pasg->inpBad & (1<<pasginp->inpIndex)) ? 1 : 0;
	    printf(" Bad=%d value=%f",bad,value);
	    printf("\n");
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	while(pasglevel) {
	    int	print_end_brace;

	    printf("\tLEVEL(%d,%s)",
		pasglevel->level,asAccessName[pasglevel->access]);
	    pasguag = (ASGUAG *)ellFirst(&pasglevel->uagList);
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    if(pasguag || pasglag || pasglevel->calc) {
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
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    if(pasglag) printf("\t\tLAG(");
	    while(pasglag) {
		printf("%s",pasglag->plag->name);
		pasglag = (ASGLAG *)ellNext((ELLNODE *)pasglag);
		if(pasglag) printf(","); else printf(")\n");
	    }
	    if(pasglevel->calc) printf("\t\tCALC(\"%s\")\n",pasglevel->calc);
	    if(print_end_brace) printf("\t}\n");
	    pasglevel = (ASGLEVEL *)ellNext((ELLNODE *)pasglevel);
	}
	if(print_end_brace) printf("}\n");
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
}

int asDumpMem(char *asgname,void (*memcallback)(ASMEMBERPVT),int clients)
{
    ASG		*pasg;
    ASGINP	*pasginp;
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
		printf("\t\t\t %s %s %d %d",
		    pasgclient->user,pasgclient->location,
		    pasgclient->level,pasgclient->access);
		printf("\n");
		pasgclient = (ASGCLIENT *)ellNext((ELLNODE *)pasgclient);
	    }
	    pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    return(0);
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
    LAG		*plag;
    LAGNAME	*plagname;
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGLEVEL	*pasglevel;
    ASGLAG	*pasglag;
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
    plag = (LAG *)ellFirst(&pasbase->lagList);
    while(plag) {
	plagname = (LAGNAME *)ellFirst(&plag->list);
	while(plagname) {
	    pnext = ellNext((ELLNODE *)plagname);
	    ellDelete(&plag->list,(ELLNODE *)plagname);
	    free((void *)plagname);
	    plagname = pnext;
	}
	pnext = ellNext((ELLNODE *)plag);
	ellDelete(&pasbase->lagList,(ELLNODE *)plag);
	free((void *)plag);
	plag = pnext;
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
	pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
	while(pasglevel) {
	    free((void *)pasglevel->calc);
	    free((void *)pasglevel->rpcl);
	    pasguag = (ASGUAG *)ellFirst(&pasglevel->uagList);
	    while(pasguag) {
		pnext = ellNext((ELLNODE *)pasguag);
		ellDelete(&pasglevel->uagList,(ELLNODE *)pasguag);
		free((void *)pasguag);
		pasguag = pnext;
	    }
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    while(pasglag) {
		pnext = ellNext((ELLNODE *)pasglag);
		ellDelete(&pasglevel->lagList,(ELLNODE *)pasglag);
		free((void *)pasglag);
		pasglag = pnext;
	    }
	    pnext = ellNext((ELLNODE *)pasglevel);
	    ellDelete(&pasg->levelList,(ELLNODE *)pasglevel);
	    free((void *)pasglevel);
	    pasglevel = pnext;
	}
	pnext = ellNext((ELLNODE *)pasg);
	ellDelete(&pasbase->asgList,(ELLNODE *)pasg);
	free((void *)pasg);
	pasg = pnext;
    }
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

static LAG *asLagAdd(char *lagName)
{
    LAG		*pprev;
    LAG		*pnext;
    LAG		*plag;
    int		cmpvalue;
    ASBASE	*pbase = (ASBASE *)pasbase;

    /*Insert in alphabetic order*/
    pnext = (LAG *)ellFirst(&pasbase->lagList);
    while(pnext) {
	cmpvalue = strcmp(lagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errMessage(-1,"Duplicate Location Access Group");
	    return(NULL);
	}
	pnext = (LAG *)ellNext((ELLNODE *)pnext);
    }
    plag = asCalloc(1,sizeof(LAG)+strlen(lagName)+1);
    ellInit(&plag->list);
    plag->name = (char *)(plag+1);
    strcpy(plag->name,lagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pbase->lagList,(ELLNODE *)plag);
    } else {
	pprev = (LAG *)ellPrevious((ELLNODE *)pnext);
	ellInsert(&pbase->lagList,(ELLNODE *)pprev,(ELLNODE *)plag);
    }
    return(plag);
}

static long asLagAddLocation(LAG *plag,char *location)
{
    LAGNAME	*plagname;

    if(!plag) return(0);
    plagname = asCalloc(1,sizeof(LAG)+strlen(location)+1);
    plagname->location = (char *)(plagname+1);
    strcpy(plagname->location,location);
    ellAdd(&plag->list,(ELLNODE *)plagname);
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
		&& ellCount(&pnext->levelList)==0)
			return(pnext);
	    }
	    errMessage(S_asLib_dupAsg,NULL);
	    return(NULL);
	}
	pnext = (ASG *)ellNext((ELLNODE *)pnext);
    }
    pasg = asCalloc(1,sizeof(ASG)+strlen(asgName)+1);
    ellInit(&pasg->inpList);
    ellInit(&pasg->levelList);
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

static ASGLEVEL *asAsgAddLevel(ASG *pasg,asAccessRights access,int level)
{
    ASGLEVEL	*pasglevel;
    int		bit;

    if(!pasg) return(0);
    pasglevel = asCalloc(1,sizeof(ASGLEVEL));
    pasglevel->access = access;
    pasglevel->level = level;
    ellInit(&pasglevel->uagList);
    ellInit(&pasglevel->lagList);
    ellAdd(&pasg->levelList,(ELLNODE *)pasglevel);
    return(pasglevel);
}

static long asAsgLevelUagAdd(ASGLEVEL *pasglevel,char *name)
{
    ASGUAG	*pasguag;
    UAG		*puag;

    if(!pasglevel) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    while(puag) {
	if(strcmp(puag->name,name)==0) break;
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    if(!puag) return(S_asLib_noUag);
    pasguag = asCalloc(1,sizeof(ASGUAG));
    pasguag->puag = puag;
    ellAdd(&pasglevel->uagList,(ELLNODE *)pasguag);
    return(0);
}

static long asAsgLevelLagAdd(ASGLEVEL *pasglevel,char *name)
{
    ASGLAG	*pasglag;
    LAG		*plag;

    if(!pasglevel) return(0);
    plag = (LAG *)ellFirst(&pasbase->lagList);
    while(plag) {
	if(strcmp(plag->name,name)==0) break;
	plag = (LAG *)ellNext((ELLNODE *)plag);
    }
    if(!plag) return(S_asLib_noLag);
    pasglag = asCalloc(1,sizeof(ASGLAG));
    pasglag->plag = plag;
    ellAdd(&pasglevel->lagList,(ELLNODE *)pasglag);
    return(0);
}

static long asAsgLevelCalc(ASGLEVEL *pasglevel,char *calc)
{
    short	error_number;
    long	status;

    if(!pasglevel) return(0);
    pasglevel->calc = asCalloc(1,strlen(calc)+1);
    strcpy(pasglevel->calc,calc);
    pasglevel->rpcl = asCalloc(1,RPCL_LEN);
    status=postfix(pasglevel->calc,pasglevel->rpcl,&error_number);
    if(status) {
	free((void *)pasglevel->calc);
	free((void *)pasglevel->rpcl);
	pasglevel->calc = NULL;
	pasglevel->rpcl = NULL;
	status = S_asLib_badCalc;
    } else {
	int i;

	for(i=0; i<ASMAXINP; i++) {
	    if(strchr(calc,'A'+i)) pasglevel->inpUsed |= (1<<i);
	}
    }
    return(status);
}
