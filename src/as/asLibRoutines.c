/* share/src/as/asLibRoutines.c	*/
/* share/src/as $Id$ */
/*
 *      Author:  Marty Kraimer
 *      Date:    10-15-93
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
 * .01  10-15-93	mrk	Initial Implementation
 */
#include <dbDefs.h>
#include <asLib.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#define RPCL_LEN 184

static ASBASE *pasbase=NULL;

/*private routines */
void * asCalloc(size_t nobj,size_t size);
static void asFreeAll(ASBASE *pasbase);
static UAG *asUagAdd(char *uagName);
static long asUagAddUser(UAG *puag,char *user);
static LAG *asLagAdd(char *lagName);
static long asLagAddLocation(LAG *plag,char *location);
static ASG *asAsgAdd(char *asgName);
static long asAsgAddInp(ASG *pasg,char *inp,int inpIndex);
static ASGLEVEL *asAsgAddLevel(ASG *pasg,asAccessRights access,
	int levlow,int levHigh);
static long asAsgLevelSetUagAll(ASGLEVEL *pasglevel);
static long asAsgLevelUagAdd(ASGLEVEL *pasglevel,char *name);
static long asAsgLevelSetLagAll(ASGLEVEL *pasglevel);
static long asAsgLevelLagAdd(ASGLEVEL *pasglevel,char *name);
static long asAsgLevelCalc(ASGLEVEL *pasglevel,char *calc);
static void asCaStart(void);
static void asCaStop(void);
static int myParse(ASINPUTFUNCPTR inputfunction);

long asInitialize(ASINPUTFUNCPTR inputfunction)
{
    ASG		*pasg;
    ASGMEMBER	*porphan;
    ASGMEMBER	*pnextorphan;
    long	status;
    ASBASE	*pasbaseold = pasbase;

    if(pasbaseold) {
	asCaStop();
    }
    pasbase = asCalloc(1,sizeof(ASBASE));
    ellInit(&pasbase->uagList);
    ellInit(&pasbase->lagList);
    ellInit(&pasbase->asgList);
    ellInit(&pasbase->orphanList);
    status = myParse(inputfunction);
    if(status) {
	status = S_asLib_badConfig;
	asFreeAll(pasbase);
	pasbase = pasbaseold;
	return(status);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	pasg->pavalue = asCalloc(ellCount(&pasg->inpList),sizeof(double));
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
    }
    if(pasbaseold) {
	porphan = (ASGMEMBER *)ellFirst(&pasbase->asgList);
	while(porphan) {
	    pnextorphan = (ASGMEMBER *)ellNext((ELLNODE *)porphan);
	    ellDelete(&pasbase->orphanList,(ELLNODE *)porphan);
	    status = asAddMember(&porphan,porphan->asgName);
	    porphan = pnextorphan;
	}
	porphan = (ASGMEMBER *)ellFirst(&pasbase->orphanList);
	while(porphan) {
	    pnextorphan = (ASGMEMBER *)ellNext((ELLNODE *)porphan);
	    ellDelete(&pasbase->orphanList,(ELLNODE *)porphan);
	    status = asAddMember(&porphan,porphan->asgName);
	    porphan = pnextorphan;
	}
	asFreeAll(pasbaseold);
    }
    asCaStart();
    return(0);
}

long asAddMember(ASMEMBERPVT *asMemberPvt,char *asgName)
{
    ASGMEMBER	*pasgmember;
    ASG		*pgroup;
    ASGCLIENT	*pclient;
    long	status;

    if(*asMemberPvt) {
	pasgmember = *asMemberPvt;
    } else {
	pasgmember = asCalloc(1,sizeof(ASGMEMBER));
	ellInit(&pasgmember->clientList);
	*asMemberPvt = pasgmember;
    }
    pasgmember->asgName = asgName;
    pgroup = (ASG *)ellFirst(&pasbase->asgList);
    while(pgroup) {
	if(strcmp(pgroup->name,pasgmember->asgName)==0) {
	    pasgmember->pasg = pgroup;
	    ellAdd(&pgroup->memberList,(ELLNODE *)pasgmember);
	    pclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    while(pclient) {
		    status = asCompute(pclient);
		    if(status) {
			errMessage(status,"asAddMember");
		    }
		    pclient = (ASGCLIENT *)ellNext((ELLNODE *)pclient);
	    }
	    break;
	}
	pgroup = (ASG *)ellNext((ELLNODE *)pgroup);
    }
    if(!pgroup) {
	pasgmember->pasg = NULL;
	ellAdd(&pasbase->orphanList,(ELLNODE *)pasgmember);
    }
    return(0);
}

long asRemoveMember(ASMEMBERPVT *asMemberPvt)
{
    ASGMEMBER	*pasgmember;

    pasgmember = *asMemberPvt;
    if(ellCount(&pasgmember->clientList)>0) return(S_asLib_clientsExist);
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,(ELLNODE *)pasgmember);
    } else {
	ellDelete(&pasbase->orphanList,(ELLNODE *)pasgmember);
    }
    free((void *)pasgmember);
    *asMemberPvt = NULL;
    return(0);
}

long asChangeGroup(ASMEMBERPVT *asMemberPvt,char *newAsgName)
{
    ASGMEMBER	*pasgmember;
    long	status;

    pasgmember = *asMemberPvt;
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,(ELLNODE *)pasgmember);
    } else {
	ellDelete(&pasbase->orphanList,(ELLNODE *)pasgmember);
    }
    status = asAddMember(asMemberPvt,newAsgName);
    return(status);
}

long asAddClient(ASCLIENTPVT *asClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,char *user,char *location)
{
    ASGCLIENT	*pasclient;
    long	status;

    pasclient = asCalloc(1,sizeof(ASCLIENTPVT));
    *asClientPvt = pasclient;
    pasclient->pasgMember = asMemberPvt;
    pasclient->level = asl;
    pasclient->user = user;
    pasclient->location = location;
    status = asCompute(pasclient);
    if(!asCheckGet((*asClientPvt))) {
	asRemoveClient(asClientPvt);
	return(S_asLib_noAccess);
    }
    return(status);
}

long asChangeClient(ASCLIENTPVT asClientPvt,int asl,char *user,char *location)
{
    ASGCLIENT	*pasclient = asClientPvt;
    long	status;

    pasclient->level = asl;
    pasclient->user = user;
    pasclient->location = location;
    status = asCompute(pasclient);
    return(status);
}

long asRemoveClient(ASCLIENTPVT *asClientPvt)
{
    ASGCLIENT	*pasgClient = *asClientPvt;
    ASGMEMBER	*pasgMember;

    if(!pasgClient) return(0);
    pasgMember = pasgClient->pasgMember;
    if(!pasgMember) {
	errMessage(-1,"asRemoveClient: No ASGMEMBER");
	return(-1);
    }
    ellDelete(&pasgMember->clientList,(ELLNODE *)pasgClient);
    free((void *)pasgClient);
    *asClientPvt = NULL;
    return(0);
}

long asCompute(ASCLIENTPVT asClientPvt)
{
    asAccessRights	access=0;
    ASGCLIENT		*pasgClient = asClientPvt;
    ASGMEMBER		*pasgMember = pasgClient->pasgMember;
    ASG			*pasg = pasgMember->pasg;
    ASGLEVEL		*pasglevel;
    double		result;
    long		status;

    if(!pasg) return(0);
    pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
    while(pasglevel) {
	if(access>=pasglevel->access) goto next_level;
	if(!((1<<pasgClient->level)&pasglevel->level_mask)) goto next_level;
	if(!pasglevel->all_uag) {/*if all_uag then no need to check uag*/
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
	if(!pasglevel->all_lag) {/*if all_lag then no need to check lag*/
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
	}
	status = calcPerform(pasg->pavalue,&result,pasglevel->rpcl);
	if(!status && result==1e0) access = pasglevel->access;
next_level:
	pasglevel = (ASGLEVEL *)ellNext((ELLNODE *)pasglevel);
    }
    pasgClient->access = access;
    return(0);
}

void asDump(void)
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

    puag = (UAG *)ellFirst(&pasbase->uagList);
    if(!puag) printf("No UAGs\n");
    while(puag) {
	printf("UAG(%s) {",puag->name);
	puagname = (UAGNAME *)ellFirst(&puag->list);
	while(puagname) {
	    printf("%s",puagname->user);
	    puagname = (UAGNAME *)ellNext((ELLNODE *)puagname);
	    if(puagname) printf(",");
	}
	printf("}\n");
	puag = (UAG *)ellNext((ELLNODE *)puag);
    }
    plag = (LAG *)ellFirst(&pasbase->lagList);
    if(!plag) printf("No LAGs\n");
    while(plag) {
	printf("LAG(%s) {",plag->name);
	plagname = (LAGNAME *)ellFirst(&plag->list);
	while(plagname) {
	    printf("%s",plagname->location);
	    plagname = (LAGNAME *)ellNext((ELLNODE *)plagname);
	    if(plagname) printf(",");
	}
	printf("}\n");
	plag = (LAG *)ellNext((ELLNODE *)plag);
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) printf("No ASGs\n");
    while(pasg) {
	printf("ASG(%s) {\n",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    printf("\tINP%c(%s)\n",(pasginp->inpIndex + 'A'),pasginp->inp);
	    pasginp = (ASGINP *)ellNext((ELLNODE *)pasginp);
	}
	pasglevel = (ASGLEVEL *)ellFirst(&pasg->levelList);
	while(pasglevel) {
	    printf("\tLEVEL(0x%0.2x,%d) {\n",
		pasglevel->level_mask,pasglevel->access);
	    pasguag = (ASGUAG *)ellFirst(&pasglevel->uagList);
	    if(pasguag) printf("\t\tUAG(");
	    while(pasguag) {
		printf("%s",pasguag->puag->name);
		pasguag = (ASGUAG *)ellNext((ELLNODE *)pasguag);
		if(pasguag) printf(","); else printf(")\n");
	    }
	    if(pasglevel->all_uag) printf("\t\tUAG(*)\n");
	    pasglag = (ASGLAG *)ellFirst(&pasglevel->lagList);
	    if(pasglag) printf("\t\tLAG(");
	    while(pasglag) {
		printf("%s",pasglag->plag->name);
		pasglag = (ASGLAG *)ellNext((ELLNODE *)pasglag);
		if(pasglag) printf(","); else printf(")\n");
	    }
	    if(pasglevel->all_lag) printf("\t\tLAG(*)\n");
	    if(pasglevel->calc) printf("\t\tCALC(\"%s\")\n",pasglevel->calc);
	    pasglevel = (ASGLEVEL *)ellNext((ELLNODE *)pasglevel);
	    if(!pasglevel) printf("\t}\n");
	}
	pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
	if(pasgmember) printf("\tMEMBERLIST\n");
	while(pasgmember) {
	    printf("\t\t%s\n",pasgmember->asgName);
	    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    while(pasgclient) {
		printf("\t\t\t %s %s %d %d\n",
		    pasgclient->user,pasgclient->location,
		    pasgclient->level,pasgclient->access);
		pasgclient = (ASGCLIENT *)((ELLNODE *)pasgclient);
	    }
	    pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
	}
	pasg = (ASG *)ellNext((ELLNODE *)pasg);
	if(!pasg) printf("}\n");
    }
    pasgmember = (ASGMEMBER *)ellFirst(&pasbase->orphanList);
    if(pasgmember) printf("ORPHANLIST\n");
    while(pasgmember) {
	printf("\t%s\n",pasgmember->asgName);
	pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	while(pasgclient) {
	    printf("\t\t %s %s %d %d\n",
		pasgclient->user,pasgclient->location,
		pasgclient->level,pasgclient->access);
	    pasgclient = (ASGCLIENT *)((ELLNODE *)pasgclient);
	}
	pasgmember = (ASGMEMBER *)ellNext((ELLNODE *)pasgmember);
    }
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
}

static UAG *asUagAdd(char *uagName)
{
    UAG	*puag;

    puag = asCalloc(1,sizeof(UAG)+strlen(uagName)+1);
    ellInit(&puag->list);
    puag->name = (char *)(puag+1);
    strcpy(puag->name,uagName);
    ellAdd(&pasbase->uagList,(ELLNODE *)puag);
    return(puag);
}

static long asUagAddUser(UAG *puag,char *user)
{
    UAGNAME	*puagname;

    puagname = asCalloc(1,sizeof(UAG)+strlen(user)+1);
    puagname->user = (char *)(puagname+1);
    strcpy(puagname->user,user);
    ellAdd(&puag->list,(ELLNODE *)puagname);
    return(0);
}

static LAG *asLagAdd(char *lagName)
{
    LAG	*plag;

    plag = asCalloc(1,sizeof(LAG)+strlen(lagName)+1);
    ellInit(&plag->list);
    plag->name = (char *)(plag+1);
    strcpy(plag->name,lagName);
    ellAdd(&pasbase->lagList,(ELLNODE *)plag);
    return(plag);
}

static long asLagAddLocation(LAG *plag,char *location)
{
    LAGNAME	*plagname;

    plagname = asCalloc(1,sizeof(LAG)+strlen(location)+1);
    plagname->location = (char *)(plagname+1);
    strcpy(plagname->location,location);
    ellAdd(&plag->list,(ELLNODE *)plagname);
    return(0);
}

static ASG *asAsgAdd(char *asgName)
{
    ASG	*pasg;

    pasg = asCalloc(1,sizeof(ASG)+strlen(asgName)+1);
    ellInit(&pasg->inpList);
    ellInit(&pasg->levelList);
    ellInit(&pasg->memberList);
    pasg->name = (char *)(pasg+1);
    strcpy(pasg->name,asgName);
    ellAdd(&pasbase->asgList,(ELLNODE *)pasg);
    return(pasg);
}

static long asAsgAddInp(ASG *pasg,char *inp,int inpIndex)
{
    ASGINP	*pasginp;

    pasginp = asCalloc(1,sizeof(ASGINP)+strlen(inp)+1);
    pasginp->inp = (char *)(pasginp+1);
    strcpy(pasginp->inp,inp);
    pasginp->pasg = pasg;
    pasginp->inpIndex = inpIndex;
    ellAdd(&pasg->inpList,(ELLNODE *)pasginp);
    return(0);
}

static ASGLEVEL *asAsgAddLevel(ASG *pasg,asAccessRights access,
	int levlow,int levHigh)
{
    ASGLEVEL	*pasglevel;
    int		bit;

    pasglevel = asCalloc(1,sizeof(ASGLEVEL));
    pasglevel->access = access;
    for(bit=levlow; bit<=levHigh; bit++) {
	pasglevel->level_mask |= (1<<bit);
    }
    ellInit(&pasglevel->uagList);
    ellInit(&pasglevel->lagList);
    ellAdd(&pasg->levelList,(ELLNODE *)pasglevel);
    return(pasglevel);
}

static long asAsgLevelSetUagAll(ASGLEVEL *pasglevel)
{
    pasglevel->all_uag = TRUE;
    return(0);
}

static long asAsgLevelUagAdd(ASGLEVEL *pasglevel,char *name)
{
    ASGUAG	*pasguag;
    UAG		*puag;

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

static long asAsgLevelSetLagAll(ASGLEVEL *pasglevel)
{
    pasglevel->all_lag = TRUE;
    return(0);
}

static long asAsgLevelLagAdd(ASGLEVEL *pasglevel,char *name)
{
    ASGLAG	*pasglag;
    LAG		*plag;

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
    }
    return(status);
}

static void asCaStart(void)
{
    /*MARTY dont forget to allocate pavalue*/
}
static void asCaStop(void)
{
}
