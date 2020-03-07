/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    10-15-93 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <epicsTypes.h>
#include <osdSock.h>

#define epicsExportSharedSymbols
#include "osiSock.h"
#include "epicsTypes.h"
#include "epicsStdio.h"
#include "dbDefs.h"
#include "epicsThread.h"
#include "cantProceed.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "gpHash.h"
#include "freeList.h"
#include "macLib.h"
#include "postfix.h"
#include "asLib.h"

int asCheckClientIP;

static epicsMutexId asLock;
#define LOCK epicsMutexMustLock(asLock)
#define UNLOCK epicsMutexUnlock(asLock)

/*following must be global because asCa nneeds it*/
epicsShareDef ASBASE volatile *pasbase=NULL;
static ASBASE *pasbasenew=NULL;
epicsShareDef int   asActive = FALSE;

static void         *freeListPvt = NULL;


#define DEFAULT "DEFAULT"

/* Defined in asLib.y */
static int myParse(ASINPUTFUNCPTR inputfunction);

/*private routines */


static void asInitAddrList(ADDRLIST *st, int n_allocate);
#if 0
static void asDelAddrList(ADDRLIST *st);
static void asDumpAddrList(ADDRLIST *st);
#endif
static int asAddrListNo(ADDRLIST *st);
static void asPrintAddrList(FILE *stream, ADDRLIST *st);
static void asAddToAddrList(ADDRLIST *st, epicsUInt32 addr);
static int asIPCmp(const void *a_p, const void *b_p);
static epicsUInt32 *asFindInAddrList(ADDRLIST *st, epicsUInt32 addr);

static long asAddMemberPvt(ASMEMBERPVT *pasMemberPvt,const char *asgName);
static long asComputeAllAsgPvt(void);
static long asComputeAsgPvt(ASG *pasg);
static long asComputePvt(ASCLIENTPVT asClientPvt);
static UAG *asUagAdd(const char *uagName);
static long asUagAddUser(UAG *puag,const char *user);
static HAG *asHagAdd(const char *hagName);
static IPAG *asIPagAdd(const char *ipagName);
static long asHagAddHost(HAG *phag,const char *host);
static char *asIpToStr(epicsUInt32 addr);
static long asIPagAddIP(IPAG *pipag, char *dotted_ip);
static ASG *asAsgAdd(const char *asgName);
static long asAsgAddInp(ASG *pasg,const char *inp,int inpIndex);
static ASGRULE *asAsgAddRule(ASG *pasg,asAccessRights access,int level);
static long asAsgAddRuleOptions(ASGRULE *pasgrule,int trapMask);
static long asAsgRuleUagAdd(ASGRULE *pasgrule,const char *name);
static long asAsgRuleHagAdd(ASGRULE *pasgrule,const char *name);
static long asAsgRuleIPagAdd(ASGRULE *pasgrule, const char *name);
static long asAsgRuleCalc(ASGRULE *pasgrule,const char *calc);

/*
  asInitialize can be called while access security is already active.
  This is accomplished by doing the following:

  The version pointed to by pasbase is kept as is but locked against changes
  A new version is created and pointed to by pasbasenew
  If anything goes wrong. The original version is kept. This results is some
  wasted space but at least things still work.
  If the new access security configuration is successfully read then:
     the old memberList is moved from old to new.
     the old structures are freed.
*/
static void asInitializeOnce(void *arg)
{
    osiSockAttach();
    asLock  = epicsMutexMustCreate();
}
long epicsShareAPI asInitialize(ASINPUTFUNCPTR inputfunction)
{
    ASG		*pasg;
    long	status;
    ASBASE 	*pasbaseold;
    GPHENTRY	*pgphentry;
    UAG		*puag;
    UAGNAME	*puagname;
    HAG		*phag;
    HAGNAME	*phagname;
    static epicsThreadOnceId asInitializeOnceFlag = EPICS_THREAD_ONCE_INIT;

    epicsThreadOnce(&asInitializeOnceFlag,asInitializeOnce,(void *)0);
    LOCK;
    pasbasenew = asCalloc(1,sizeof(ASBASE));
    if(!freeListPvt) freeListInitPvt(&freeListPvt,sizeof(ASGCLIENT),20);
    ellInit(&pasbasenew->uagList);
    ellInit(&pasbasenew->hagList);
    ellInit(&pasbasenew->ipagList);
    ellInit(&pasbasenew->asgList);
    asAsgAdd(DEFAULT);
    status = myParse(inputfunction);
    if(status) {
	status = S_asLib_badConfig;
	/*Not safe to call asFreeAll */
	UNLOCK;
	return(status);
    }
    pasg = (ASG *)ellFirst(&pasbasenew->asgList);
    while(pasg) {
	pasg->pavalue = asCalloc(CALCPERFORM_NARGS, sizeof(double));
	pasg = (ASG *)ellNext(&pasg->node);
    }
    gphInitPvt(&pasbasenew->phash, 256);
    /*Hash each uagname and each hagname*/
    puag = (UAG *)ellFirst(&pasbasenew->uagList);
    while(puag) {
	puagname = (UAGNAME *)ellFirst(&puag->list);
	while(puagname) {
	    pgphentry = gphAdd(pasbasenew->phash,puagname->user,puag);
	    if(!pgphentry) {
		errlogPrintf("Duplicated user '%s' in UAG '%s'\n",
		    puagname->user, puag->name);
	    }
	    puagname = (UAGNAME *)ellNext(&puagname->node);
	}
	puag = (UAG *)ellNext(&puag->node);
    }

    phag = (HAG *)ellFirst(&pasbasenew->hagList);
    while(phag) {
	phagname = (HAGNAME *)ellFirst(&phag->list);
	while(phagname) {
	    pgphentry = gphAdd(pasbasenew->phash,phagname->host,phag);
	    if(!pgphentry) {
		errlogPrintf("Duplicated host '%s' in HAG '%s'\n",
		    phagname->host, phag->name);
	    }
	    phagname = (HAGNAME *)ellNext(&phagname->node);
	}
	phag = (HAG *)ellNext(&phag->node);
    }

    pasbaseold = (ASBASE *)pasbase;
    pasbase = (ASBASE volatile *)pasbasenew;
    if(pasbaseold) {
	ASG		*poldasg;
	ASGMEMBER	*poldmem;
	ASGMEMBER	*pnextoldmem;

	poldasg = (ASG *)ellFirst(&pasbaseold->asgList);
	while(poldasg) {
	    poldmem = (ASGMEMBER *)ellFirst(&poldasg->memberList);
	    while(poldmem) {
		pnextoldmem = (ASGMEMBER *)ellNext(&poldmem->node);
		ellDelete(&poldasg->memberList,&poldmem->node);
		status = asAddMemberPvt(&poldmem,poldmem->asgName);
		poldmem = pnextoldmem;
	    }
	    poldasg = (ASG *)ellNext(&poldasg->node);
	}
	asFreeAll(pasbaseold);
    }
    asActive = TRUE;
    UNLOCK;
    return(0);
}

long epicsShareAPI asInitFile(const char *filename,const char *substitutions)
{
    FILE *fp;
    long status;

    fp = fopen(filename,"r");
    if(!fp) {
	errlogPrintf("asInitFile: Can't open file '%s'\n", filename);
	return(S_asLib_badConfig);
    }
    status = asInitFP(fp,substitutions);
    if(fclose(fp)==EOF) {
	errMessage(0,"asInitFile: fclose failed!");
	if(!status) status = S_asLib_badConfig;
    }
    return(status);
}

#define BUF_SIZE 200
static char *my_buffer;
static char *my_buffer_ptr;
static FILE *stream;
static char *mac_input_buffer=NULL;
static MAC_HANDLE *macHandle = NULL;

static int myInputFunction(char *buf, int max_size)
{
    int	l,n;
    char *fgetsRtn;
    
    if(*my_buffer_ptr==0) {
	if(macHandle) {
	    fgetsRtn = fgets(mac_input_buffer,BUF_SIZE,stream);
	    if(fgetsRtn) {
		n = macExpandString(macHandle,mac_input_buffer,
		    my_buffer,BUF_SIZE);
		if(n<0) {
		    errlogPrintf("access security: macExpandString failed\n"
			"input line: %s\n",mac_input_buffer);
		    return(0);
		}
	    }
	} else {
	    fgetsRtn = fgets(my_buffer,BUF_SIZE,stream);
	}
	if(fgetsRtn==NULL) return(0);
	my_buffer_ptr = my_buffer;
    }
    l = strlen(my_buffer_ptr);
    n = (l<=max_size ? l : max_size);
    memcpy(buf,my_buffer_ptr,n);
    my_buffer_ptr += n;
    return(n);
}

long epicsShareAPI asInitFP(FILE *fp,const char *substitutions)
{
    char	buffer[BUF_SIZE];
    char	mac_buffer[BUF_SIZE];
    long	status;
    char	**macPairs;

    buffer[0] = 0;
    my_buffer = buffer;
    my_buffer_ptr = my_buffer;
    stream = fp;
    if(substitutions) {
	if((status = macCreateHandle(&macHandle,NULL))) {
	    errMessage(status,"asInitFP: macCreateHandle error");
	    return(status);
	}
	macParseDefns(macHandle,substitutions,&macPairs);
	if(macPairs ==NULL) {
	    macDeleteHandle(macHandle);
	    macHandle = NULL;
	} else {
	    macInstallMacros(macHandle,macPairs);
	    free(macPairs);
	    mac_input_buffer = mac_buffer;
	}
    }
    status = asInitialize(myInputFunction);	
    if(macHandle) {
	macDeleteHandle(macHandle);
	macHandle = NULL;
    }
    return(status);
}

static const char* membuf;

static int memInputFunction(char *buf, int max_size)
{
    int ret = 0;
    if(!membuf) return ret;

    while(max_size && *membuf) {
        *buf++ = *membuf++;
        max_size--;
        ret++;
    }

    return ret;
}

long epicsShareAPI asInitMem(const char *acf, const char *substitutions)
{
    long ret = S_asLib_InitFailed;
    if(!acf) return ret;

    membuf = acf;
    ret = asInitialize(&memInputFunction);
    membuf = NULL;

    return ret;
}

long epicsShareAPI asAddMember(ASMEMBERPVT *pasMemberPvt,const char *asgName)
{
    long	status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asAddMemberPvt(pasMemberPvt,asgName);
    UNLOCK;
    return(status);
}

long epicsShareAPI asRemoveMember(ASMEMBERPVT *asMemberPvt)
{
    ASGMEMBER	*pasgmember;

    if(!asActive) return(S_asLib_asNotActive);
    pasgmember = *asMemberPvt;
    if(!pasgmember) return(S_asLib_badMember);
    LOCK;
    if (ellCount(&pasgmember->clientList) > 0) {
        UNLOCK;
        return(S_asLib_clientsExist);
    }
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,&pasgmember->node);
    } else {
	errMessage(-1,"Logic error in asRemoveMember");
	UNLOCK;
	return(-1);
    }
    free(pasgmember);
    *asMemberPvt = NULL;
    UNLOCK;
    return(0);
}

long epicsShareAPI asChangeGroup(ASMEMBERPVT *asMemberPvt,const char *newAsgName)
{
    ASGMEMBER	*pasgmember;
    long	status;

    if(!asActive) return(S_asLib_asNotActive);
    pasgmember = *asMemberPvt;
    if(!pasgmember) return(S_asLib_badMember);
    LOCK;
    if(pasgmember->pasg) {
	ellDelete(&pasgmember->pasg->memberList,&pasgmember->node);
    } else {
	errMessage(-1,"Logic error in asChangeGroup");
	UNLOCK;
	return(-1);
    }
    status = asAddMemberPvt(asMemberPvt,newAsgName);
    UNLOCK;
    return(status);
}

void * epicsShareAPI asGetMemberPvt(ASMEMBERPVT asMemberPvt)
{
    ASGMEMBER	*pasgmember = asMemberPvt;

    if(!asActive) return(NULL);
    if(!pasgmember) return(NULL);
    return(pasgmember->userPvt);
}

void epicsShareAPI asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt)
{
    ASGMEMBER	*pasgmember = asMemberPvt;

    if(!asActive) return;
    if(!pasgmember) return;
    pasgmember->userPvt = userPvt;
    return;
}

long epicsShareAPI asAddClient(ASCLIENTPVT *pasClientPvt,ASMEMBERPVT asMemberPvt,
	int asl,const char *user,char *host, epicsUInt32 ip_addr)
{
    ASGMEMBER	*pasgmember = asMemberPvt;
    ASGCLIENT	*pasgclient;
    int		len, i;

    long	status;
    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgmember) return(S_asLib_badMember);
    pasgclient = freeListCalloc(freeListPvt);
    if(!pasgclient) return(S_asLib_noMemory);
    len = strlen(host);
    for (i = 0; i < len; i++) {
        host[i] = (char)tolower((int)host[i]);
    }
    *pasClientPvt = pasgclient;
    pasgclient->pasgMember = asMemberPvt;
    pasgclient->level = asl;
    pasgclient->user = user;
    pasgclient->host = host;
    pasgclient->ip_addr= ip_addr;
    LOCK;
    ellAdd(&pasgmember->clientList,&pasgclient->node);
    status = asComputePvt(pasgclient);
    UNLOCK;
    return(status);
}

long epicsShareAPI asChangeClient(
    ASCLIENTPVT asClientPvt,int asl,const char *user,char *host, epicsUInt32 ip_addr)
{
    ASGCLIENT	*pasgclient = asClientPvt;
    long	status;
    int		len, i;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    len = strlen(host);
    for (i = 0; i < len; i++) {
        host[i] = (char)tolower((int)host[i]);
    }
    LOCK;
    pasgclient->level = asl;
    pasgclient->user = user;
    pasgclient->host = host;
    pasgclient->ip_addr= ip_addr;
    status = asComputePvt(pasgclient);
    UNLOCK;
    return(status);
}

long epicsShareAPI asRemoveClient(ASCLIENTPVT *asClientPvt)
{
    ASGCLIENT	*pasgclient = *asClientPvt;
    ASGMEMBER	*pasgMember;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    LOCK;
    pasgMember = pasgclient->pasgMember;
    if(!pasgMember) {
	errMessage(-1,"asRemoveClient: No ASGMEMBER");
	UNLOCK;
	return(-1);
    }
    ellDelete(&pasgMember->clientList,&pasgclient->node);
    UNLOCK;
    freeListFree(freeListPvt,pasgclient);
    *asClientPvt = NULL;
    return(0);
}

long epicsShareAPI asRegisterClientCallback(ASCLIENTPVT asClientPvt,
	ASCLIENTCALLBACK pcallback)
{
    ASGCLIENT	*pasgclient = asClientPvt;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    LOCK;
    pasgclient->pcallback = pcallback;
    (*pasgclient->pcallback)(pasgclient,asClientCOAR);
    UNLOCK;
    return(0);
}

void * epicsShareAPI asGetClientPvt(ASCLIENTPVT asClientPvt)
{
    ASGCLIENT	*pasgclient = asClientPvt;

    if(!asActive) return(NULL);
    if(!pasgclient) return(NULL);
    return(pasgclient->userPvt);
}

void epicsShareAPI asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt)
{
    ASGCLIENT	*pasgclient = asClientPvt;
    if(!asActive) return;
    if(!pasgclient) return;
    LOCK;
    pasgclient->userPvt = userPvt;
    UNLOCK;
}

long epicsShareAPI asComputeAllAsg(void)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputeAllAsgPvt();
    UNLOCK;
    return(status);
}

long epicsShareAPI asComputeAsg(ASG *pasg)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputeAsgPvt(pasg);
    UNLOCK;
    return(status);
}

long epicsShareAPI asCompute(ASCLIENTPVT asClientPvt)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputePvt(asClientPvt);
    UNLOCK;
    return(status);
}

/*The dump routines do not lock. Thus they may get inconsistant data.*/
/*HOWEVER if they did lock and a user interrupts one of then then BAD BAD*/
static const char *asAccessName[] = {"NONE","READ","WRITE"};
static const char *asTrapOption[] = {"NOTRAPWRITE","TRAPWRITE"};
static const char *asLevelName[] = {"ASL0","ASL1"};
int epicsShareAPI asDump(
	void (*memcallback)(struct asgMember *,FILE *),
	void (*clientcallback)(struct asgClient *,FILE *),
	int verbose)
{
    return asDumpFP(stdout,memcallback,clientcallback,verbose);
}

int epicsShareAPI asDumpFP(
	FILE *fp,
	void (*memcallback)(struct asgMember *,FILE *),
	void (*clientcallback)(struct asgClient *,FILE *),
	int verbose)
{
    UAG		*puag;
    UAGNAME	*puagname;
    HAG		*phag;
    HAGNAME	*phagname;
    IPAG	*pipag;
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGRULE	*pasgrule;
    ASGHAG	*pasghag;
    ASGIPAG	*pasgipag;
    ASGUAG	*pasguag;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    if(!puag) fprintf(fp,"No UAGs\n");
    while(puag) {
	fprintf(fp,"UAG(%s)",puag->name);
	puagname = (UAGNAME *)ellFirst(&puag->list);
	if(puagname) fprintf(fp," {"); else fprintf(fp,"\n");
	while(puagname) {
	    fprintf(fp,"%s",puagname->user);
	    puagname = (UAGNAME *)ellNext(&puagname->node);
	    if(puagname) fprintf(fp,","); else fprintf(fp,"}\n");
	}
	puag = (UAG *)ellNext(&puag->node);
    }

    phag = (HAG *)ellFirst(&pasbase->hagList);
    if(!phag) fprintf(fp,"No HAGs\n");
    while(phag) {
	fprintf(fp,"HAG(%s)",phag->name);
	phagname = (HAGNAME *)ellFirst(&phag->list);
	if(phagname) fprintf(fp," {"); else fprintf(fp,"\n");
	while(phagname) {
	    fprintf(fp,"%s",phagname->host);
	    phagname = (HAGNAME *)ellNext(&phagname->node);
	    if(phagname) fprintf(fp,","); else fprintf(fp,"}\n");
	}
	phag = (HAG *)ellNext(&phag->node);
    }

    pipag = (IPAG *)ellFirst(&pasbase->ipagList);
    if(!pipag) fprintf(fp,"No IPAGs\n");
    while(pipag) {
	fprintf(fp,"IPAG(%s)",pipag->name);
        if (asAddrListNo(&(pipag->addrlist))) {
	    fprintf(fp," {"); 
            asPrintAddrList(fp, &(pipag->addrlist));
	    fprintf(fp,"}"); 
        }
	fprintf(fp,"\n"); 
	pipag = (IPAG *)ellNext(&pipag->node);
    }

    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) fprintf(fp,"No ASGs\n");
    while(pasg) {
	int print_end_brace;

	fprintf(fp,"ASG(%s)",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	if(pasginp || pasgrule) {
	    fprintf(fp," {\n");
	    print_end_brace = TRUE;
	} else {
	    fprintf(fp,"\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {

	    fprintf(fp,"\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    if(verbose) {
		if((pasg->inpBad & (1ul << pasginp->inpIndex)))
			fprintf(fp," INVALID");
		else
			fprintf(fp,"   VALID");
		fprintf(fp," value=%f",pasg->pavalue[pasginp->inpIndex]);
	    }
	    fprintf(fp,"\n");
	    pasginp = (ASGINP *)ellNext(&pasginp->node);
	}
	while(pasgrule) {
	    int	print_end_brace;

	    fprintf(fp,"\tRULE(%d,%s,%s)",
		pasgrule->level,asAccessName[pasgrule->access],
                asTrapOption[pasgrule->trapMask]);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasguag || pasghag || pasgrule->calc) {
		fprintf(fp," {\n");
		print_end_brace = TRUE;
	    } else {
		fprintf(fp,"\n");
		print_end_brace = FALSE;
	    }
	    if(pasguag) fprintf(fp,"\t\tUAG(");
	    while(pasguag) {
		fprintf(fp,"%s",pasguag->puag->name);
		pasguag = (ASGUAG *)ellNext(&pasguag->node);
		if(pasguag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasghag) fprintf(fp,"\t\tHAG(");
	    while(pasghag) {
		fprintf(fp,"%s",pasghag->phag->name);
		pasghag = (ASGHAG *)ellNext(&pasghag->node);
		if(pasghag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    pasgipag = (ASGIPAG *)ellFirst(&pasgrule->ipagList);
	    if(pasgipag) fprintf(fp,"\t\tIPAG(");
	    while(pasgipag) {
		fprintf(fp,"%s",pasgipag->pipag->name);
		pasgipag = (ASGIPAG *)ellNext(&pasgipag->node);
		if(pasgipag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    if(pasgrule->calc) {
		fprintf(fp,"\t\tCALC(\"%s\")",pasgrule->calc);
		if(verbose)
		    fprintf(fp," result=%s",(pasgrule->result==1 ? "TRUE" : "FALSE"));
		fprintf(fp,"\n");
	    }
	    if(print_end_brace) fprintf(fp,"\t}\n");
	    pasgrule = (ASGRULE *)ellNext(&pasgrule->node);
	}
	pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
	if(!verbose) pasgmember = NULL;
	if(pasgmember) fprintf(fp,"\tMEMBERLIST\n");
	while(pasgmember) {
	    if(strlen(pasgmember->asgName)==0) 
		fprintf(fp,"\t\t<null>");
	    else
		fprintf(fp,"\t\t%s",pasgmember->asgName);
	    if(memcallback) memcallback(pasgmember,fp);
	    fprintf(fp,"\n");
	    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    while(pasgclient) {
		fprintf(fp,"\t\t\t %s %s %s",
                        pasgclient->user,
                        pasgclient->host,
                        asIpToStr(pasgclient->ip_addr));
		if(pasgclient->level>=0 && pasgclient->level<=1)
			fprintf(fp," %s",asLevelName[pasgclient->level]);
		else
			fprintf(fp," Illegal Level %d",pasgclient->level);
		if(pasgclient->access<=2)
			fprintf(fp," %s %s",
                            asAccessName[pasgclient->access],
                            asTrapOption[pasgclient->trapMask]);
		else
			fprintf(fp," Illegal Access %d",pasgclient->access);
		if(clientcallback) clientcallback(pasgclient,fp);
		fprintf(fp,"\n");
		pasgclient = (ASGCLIENT *)ellNext(&pasgclient->node);
	    }
	    pasgmember = (ASGMEMBER *)ellNext(&pasgmember->node);
	}
	if(print_end_brace) fprintf(fp,"}\n");
	pasg = (ASG *)ellNext(&pasg->node);
    }
    return(0);
}

int epicsShareAPI asDumpUag(const char *uagname)
{
    return asDumpUagFP(stdout,uagname);
}

int epicsShareAPI asDumpUagFP(FILE *fp,const char *uagname)
{
    UAG		*puag;
    UAGNAME	*puagname;

    if(!asActive) return(0);
    puag = (UAG *)ellFirst(&pasbase->uagList);
    if(!puag) fprintf(fp,"No UAGs\n");
    while(puag) {
	if(uagname && strcmp(uagname,puag->name)!=0) {
	    puag = (UAG *)ellNext(&puag->node);
	    continue;
	}
	fprintf(fp,"UAG(%s)",puag->name);
	puagname = (UAGNAME *)ellFirst(&puag->list);
	if(puagname) fprintf(fp," {"); else fprintf(fp,"\n");
	while(puagname) {
	    fprintf(fp,"%s",puagname->user);
	    puagname = (UAGNAME *)ellNext(&puagname->node);
	    if(puagname) fprintf(fp,","); else fprintf(fp,"}\n");
	}
	puag = (UAG *)ellNext(&puag->node);
    }
    return(0);
}

int epicsShareAPI asDumpHag(const char *hagname)
{
    return asDumpHagFP(stdout,hagname);
}

int epicsShareAPI asDumpHagFP(FILE *fp,const char *hagname)
{
    HAG		*phag;
    HAGNAME	*phagname;

    if(!asActive) return(0);
    phag = (HAG *)ellFirst(&pasbase->hagList);
    if(!phag) fprintf(fp,"No HAGs\n");
    while(phag) {
	if(hagname && strcmp(hagname,phag->name)!=0) {
	    phag = (HAG *)ellNext(&phag->node);
	    continue;
	}
	fprintf(fp,"HAG(%s)",phag->name);
	phagname = (HAGNAME *)ellFirst(&phag->list);
	if(phagname) fprintf(fp," {"); else fprintf(fp,"\n");
	while(phagname) {
	    fprintf(fp,"%s",phagname->host);
	    phagname = (HAGNAME *)ellNext(&phagname->node);
	    if(phagname) fprintf(fp,","); else fprintf(fp,"}\n");
	}
	phag = (HAG *)ellNext(&phag->node);
    }
    return(0);
}

int epicsShareAPI asDumpIPag(const char *ipagname)
{
    return asDumpIPagFP(stdout,ipagname);
}

int epicsShareAPI asDumpIPagFP(FILE *fp,const char *ipagname)
{
    IPAG		*pipag;

    if(!asActive) return(0);
    pipag = (IPAG *)ellFirst(&pasbase->ipagList);
    if(!pipag) fprintf(fp,"No IPAGs\n");
    while(pipag) {
	if(ipagname && strcmp(ipagname,pipag->name)!=0) {
	    pipag = (IPAG *)ellNext(&pipag->node);
	    continue;
	}
	fprintf(fp,"IPAG(%s)",pipag->name);
        if (asAddrListNo(&(pipag->addrlist))) {
	    fprintf(fp," {"); 
            asPrintAddrList(fp, &(pipag->addrlist));
	    fprintf(fp,"}"); 
        }
	fprintf(fp,"\n"); 
	pipag = (IPAG *)ellNext(&pipag->node);
    }
    return(0);
}

int epicsShareAPI asDumpRules(const char *asgname)
{
    return asDumpRulesFP(stdout,asgname);
}

int epicsShareAPI asDumpRulesFP(FILE *fp,const char *asgname)
{
    ASG		*pasg;
    ASGINP	*pasginp;
    ASGRULE	*pasgrule;
    ASGHAG	*pasghag;
    ASGIPAG	*pasgipag;
    ASGUAG	*pasguag;

    if(!asActive) return(0);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) fprintf(fp,"No ASGs\n");
    while(pasg) {
	int print_end_brace;

	if(asgname && strcmp(asgname,pasg->name)!=0) {
	    pasg = (ASG *)ellNext(&pasg->node);
	    continue;
	}
	fprintf(fp,"ASG(%s)",pasg->name);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	if(pasginp || pasgrule) {
	    fprintf(fp," {\n");
	    print_end_brace = TRUE;
	} else {
	    fprintf(fp,"\n");
	    print_end_brace = FALSE;
	}
	while(pasginp) {

	    fprintf(fp,"\tINP%c(%s)",(pasginp->inpIndex + 'A'),pasginp->inp);
	    if ((pasg->inpBad & (1ul << pasginp->inpIndex)))
		fprintf(fp," INVALID");
	    fprintf(fp," value=%f",pasg->pavalue[pasginp->inpIndex]);
	    fprintf(fp,"\n");
	    pasginp = (ASGINP *)ellNext(&pasginp->node);
	}
	while(pasgrule) {
	    int	print_end_brace;

	    fprintf(fp,"\tRULE(%d,%s,%s)",
		pasgrule->level,asAccessName[pasgrule->access],
                asTrapOption[pasgrule->trapMask]);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    pasgipag = (ASGIPAG *)ellFirst(&pasgrule->ipagList);
	    if(pasguag || pasghag || pasgipag || pasgrule->calc) {
		fprintf(fp," {\n");
		print_end_brace = TRUE;
	    } else {
		fprintf(fp,"\n");
		print_end_brace = FALSE;
	    }
	    if(pasguag) fprintf(fp,"\t\tUAG(");
	    while(pasguag) {
		fprintf(fp,"%s",pasguag->puag->name);
		pasguag = (ASGUAG *)ellNext(&pasguag->node);
		if(pasguag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    if(pasghag) fprintf(fp,"\t\tHAG(");
	    while(pasghag) {
		fprintf(fp,"%s",pasghag->phag->name);
		pasghag = (ASGHAG *)ellNext(&pasghag->node);
		if(pasghag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    pasgipag = (ASGIPAG *)ellFirst(&pasgrule->ipagList);
	    if(pasgipag) fprintf(fp,"\t\tIPAG(");
	    while(pasgipag) {
		fprintf(fp,"%s",pasgipag->pipag->name);
		pasgipag = (ASGIPAG *)ellNext(&pasgipag->node);
		if(pasgipag) fprintf(fp,","); else fprintf(fp,")\n");
	    }

	    if(pasgrule->calc) {
		fprintf(fp,"\t\tCALC(\"%s\")",pasgrule->calc);
		fprintf(fp," result=%s",(pasgrule->result==1 ? "TRUE" : "FALSE"));
		fprintf(fp,"\n");
	    }
	    if(print_end_brace) fprintf(fp,"\t}\n");
	    pasgrule = (ASGRULE *)ellNext(&pasgrule->node);
	}
	if(print_end_brace) fprintf(fp,"}\n");
	pasg = (ASG *)ellNext(&pasg->node);
    }
    return(0);
}

int epicsShareAPI asDumpMem(const char *asgname,void (*memcallback)(ASMEMBERPVT,FILE *),
  int clients)
{
    return asDumpMemFP(stdout,asgname,memcallback,clients);
}

int epicsShareAPI asDumpMemFP(FILE *fp,const char *asgname,
  void (*memcallback)(ASMEMBERPVT,FILE *),int clients)
{
    ASG		*pasg;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(0);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) fprintf(fp,"No ASGs\n");
    while(pasg) {

	if(asgname && strcmp(asgname,pasg->name)!=0) {
	    pasg = (ASG *)ellNext(&pasg->node);
	    continue;
	}
	fprintf(fp,"ASG(%s)\n",pasg->name);
	pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
	if(pasgmember) fprintf(fp,"\tMEMBERLIST\n");
	while(pasgmember) {
	    if(strlen(pasgmember->asgName)==0) 
		fprintf(fp,"\t\t<null>");
	    else
		fprintf(fp,"\t\t%s",pasgmember->asgName);
	    if(memcallback) memcallback(pasgmember,fp);
	    fprintf(fp,"\n");
	    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	    if(!clients) pasgclient = NULL;
	    while(pasgclient) {
		fprintf(fp,"\t\t\t %s %s",
		    pasgclient->user,pasgclient->host);
		if(pasgclient->level>=0 && pasgclient->level<=1)
		    fprintf(fp," %s",asLevelName[pasgclient->level]);
		else
		    fprintf(fp," Illegal Level %d",pasgclient->level);
		if(pasgclient->access<=2)
		    fprintf(fp," %s %s",
                        asAccessName[pasgclient->access],
                        asTrapOption[pasgclient->trapMask]);
		else
		    fprintf(fp," Illegal Access %d",pasgclient->access);
		fprintf(fp,"\n");
		pasgclient = (ASGCLIENT *)ellNext(&pasgclient->node);
	    }
	    pasgmember = (ASGMEMBER *)ellNext(&pasgmember->node);
	}
	pasg = (ASG *)ellNext(&pasg->node);
    }
    return(0);
}

epicsShareFunc int epicsShareAPI asDumpHash(void)
{
    return asDumpHashFP(stdout);
}

epicsShareFunc int epicsShareAPI asDumpHashFP(FILE *fp)
{
    if(!asActive) return(0);
    gphDumpFP(fp,pasbase->phash);
    return(0);
}

/*Start of private routines*/
/* asCalloc is "friend" function */
epicsShareFunc void * epicsShareAPI asCalloc(size_t nobj,size_t size)
{
    void *p;

    p=callocMustSucceed(nobj,size,"asCalloc");
    return(p);
}
epicsShareFunc char * epicsShareAPI asStrdup(unsigned char *str)
{
	size_t len = strlen((char *) str);
	char *buf = asCalloc(1, len + 1);
	strcpy(buf, (char *) str);
	return buf;
}

static void asInitAddrList(ADDRLIST *st, int n_allocate)
  /* Init a new ADDRLIST structure.
   * allocate memory for <n_allocate> entries. */
  {
    st->addrs= calloc(n_allocate, sizeof(epicsUInt32));
    st->n_addrs= 0;
    st->n_allocated= n_allocate;
  }

#if 0
static void asDelAddrList(ADDRLIST *st)
  /* Delete the ADDRLIST structure. */
  {
    free(st->addrs);
    free(st);
  }

static void asDumpAddrList(ADDRLIST *st)
  /* Print the contents of the ADDRLIST structure to the console. */
  {
    int i;
    printf("allocated elements: %d\n", st->n_allocated);
    printf("elements:           %d\n", st->n_addrs);
    for(i=0; i<st->n_addrs; i++)
      {
        printf("%5d", (st->addrs)[i]);
      }
    printf("\n");
  }
#endif

static int asAddrListNo(ADDRLIST *st)
  {
    return st->n_addrs;
  }

static void asPrintAddrList(FILE *stream, ADDRLIST *st)
  /* Print all ip addresses to a file */
  {
    int i;
    for(i=0; i<st->n_addrs; i++)
      {
        if (i==0)
            fprintf(stream, "%s", asIpToStr((st->addrs)[i]));
        else
            fprintf(stream, ",%s", asIpToStr((st->addrs)[i]));
      }
  }

static void asAddToAddrList(ADDRLIST *st, epicsUInt32 addr)
  /* Add a new address to the ADDRLIST structure. */
  {
    epicsUInt32 *new, *dest, *src;
    int n;
    if (st->n_addrs>=st->n_allocated)
      {
        st->n_allocated+= 10; /* allocate space for 10 more IP addresses */
        new= calloc(st->n_allocated, sizeof(epicsUInt32));
        src= st->addrs;
        dest= new;
        n= st->n_addrs;
        while((addr>*src)&&(n>0))
          {
            *(dest++)= *(src++);
            n--;
          }
        if (addr!=*src)
          {
            *(dest++)= addr;
            st->n_addrs+=1;
          }
        for(;n>0;n--)
          *(dest++)= *(src++);
        free(st->addrs);
        st->addrs= new;
      }
    else
      {
        src= st->addrs;
        n= st->n_addrs;
        while((addr>*src)&&(n>0))
          {
            src++;
            n--;
          }
        if (addr==*src)
          return; /* do nothing, already in list */
        memmove(src+1, src, sizeof(epicsUInt32)*n);
        *src= addr;
        st->n_addrs+=1;
      }
  }

static int asIPCmp(const void *a_p, const void *b_p)
  /* Internal compare function for ADDRLIST. */
  {
    epicsUInt32 a= *((epicsUInt32 *)a_p);
    epicsUInt32 b= *((epicsUInt32 *)b_p);
    if (a < b)
      return -1;
    if (a > b)
      return 1;
    return 0;
  }

static epicsUInt32 *asFindInAddrList(ADDRLIST *st, epicsUInt32 addr)
  /* Find an address in the ADDRLIST structure.
   * Returns a pointer to the found element or NULL if it was not found. */
  {
    return (epicsUInt32 *)bsearch(&addr, st->addrs, st->n_addrs, 
                               sizeof(epicsUInt32), asIPCmp);
  }


static long asAddMemberPvt(ASMEMBERPVT *pasMemberPvt,const char *asgName)
{
    ASGMEMBER	*pasgmember;
    ASG		*pgroup;
    ASGCLIENT	*pasgclient;

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
	pgroup = (ASG *)ellNext(&pgroup->node);
    }
    /* Put it in DEFAULT*/
    pgroup = (ASG *)ellFirst(&pasbase->asgList);
    while(pgroup) {
	if(strcmp(pgroup->name,DEFAULT)==0) goto got_it;
	pgroup = (ASG *)ellNext(&pgroup->node);
    }
    errMessage(-1,"Logic Error in asAddMember");
    return(-1);
got_it:
    pasgmember->pasg = pgroup;
    ellAdd(&pgroup->memberList,&pasgmember->node);
    pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
    while(pasgclient) {
	asComputePvt((ASCLIENTPVT)pasgclient);
	pasgclient = (ASGCLIENT *)ellNext(&pasgclient->node);
    }
    return(0);
}

static long asComputeAllAsgPvt(void)
{
    ASG         *pasg;

    if(!asActive) return(S_asLib_asNotActive);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	asComputeAsgPvt(pasg);
	pasg = (ASG *)ellNext(&pasg->node);
    }
    return(0);
}

static long asComputeAsgPvt(ASG *pasg)
{
    ASGRULE	*pasgrule;
    ASGMEMBER	*pasgmember;
    ASGCLIENT	*pasgclient;

    if(!asActive) return(S_asLib_asNotActive);
    pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
    while(pasgrule) {
	double	result = pasgrule->result;  /* set for VAL */
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
	pasgrule = (ASGRULE *)ellNext(&pasgrule->node);
    }
    pasg->inpChanged = FALSE;
    pasgmember = (ASGMEMBER *)ellFirst(&pasg->memberList);
    while(pasgmember) {
	pasgclient = (ASGCLIENT *)ellFirst(&pasgmember->clientList);
	while(pasgclient) {
	    asComputePvt((ASCLIENTPVT)pasgclient);
	    pasgclient = (ASGCLIENT *)ellNext(&pasgclient->node);
	}
	pasgmember = (ASGMEMBER *)ellNext(&pasgmember->node);
    }
    return(0);
}

static long asComputePvt(ASCLIENTPVT asClientPvt)
{
    asAccessRights	access=asNOACCESS;
    int			trapMask=0;
    ASGCLIENT		*pasgclient = asClientPvt;
    ASGMEMBER		*pasgMember;
    ASG			*pasg;
    ASGRULE		*pasgrule;
    asAccessRights	oldaccess;
    GPHENTRY		*pgphentry;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    pasgMember = pasgclient->pasgMember;
    if(!pasgMember) return(S_asLib_badMember);
    pasg = pasgMember->pasg;
    if(!pasg) return(S_asLib_badAsg);
    oldaccess=pasgclient->access;
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
		if((puag = pasguag->puag)) {
		    pgphentry = gphFind(pasbase->phash,pasgclient->user,puag);
		    if(pgphentry) goto check_hag;
		}
		pasguag = (ASGUAG *)ellNext(&pasguag->node);
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
		if((phag = pasghag->phag)) {
		    pgphentry=gphFind(pasbase->phash,pasgclient->host,phag);
		    if(pgphentry) goto check_ipag;
		}
		pasghag = (ASGHAG *)ellNext(&pasghag->node);
	    }
	    goto next_rule;
	}
check_ipag: 
	/*if hagList is empty then no need to check hag*/
	if(ellCount(&pasgrule->ipagList)>0) {
	    ASGIPAG	*pasgipag;
	    IPAG        *pipag;

	    pasgipag = (ASGIPAG *)ellFirst(&pasgrule->ipagList);
	    while(pasgipag) {
		if((pipag = pasgipag->pipag)) {
                    if (asFindInAddrList(&(pipag->addrlist), pasgclient->ip_addr))
                        goto check_calc;
		}
		pasgipag = (ASGIPAG *)ellNext(&pasgipag->node);
	    }
	    goto next_rule;
	}
check_calc:
	if(!pasgrule->calc
	|| (!(pasg->inpBad & pasgrule->inpUsed) && (pasgrule->result==1))) {
	    access = pasgrule->access;
            trapMask = pasgrule->trapMask;
        }
next_rule:
	pasgrule = (ASGRULE *)ellNext(&pasgrule->node);
    }
    pasgclient->access = access;
    pasgclient->trapMask = trapMask;
    if(pasgclient->pcallback && oldaccess!=access) {
	(*pasgclient->pcallback)(pasgclient,asClientCOAR);
    }
    return(0);
}

void asFreeAll(ASBASE *pasbase)
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
	    pnext = ellNext(&puagname->node);
	    ellDelete(&puag->list,&puagname->node);
	    free(puagname);
	    puagname = pnext;
	}
	pnext = ellNext(&puag->node);
	ellDelete(&pasbase->uagList,&puag->node);
	free(puag);
	puag = pnext;
    }
    phag = (HAG *)ellFirst(&pasbase->hagList);
    while(phag) {
	phagname = (HAGNAME *)ellFirst(&phag->list);
	while(phagname) {
	    pnext = ellNext(&phagname->node);
	    ellDelete(&phag->list,&phagname->node);
	    free(phagname);
	    phagname = pnext;
	}
	pnext = ellNext(&phag->node);
	ellDelete(&pasbase->hagList,&phag->node);
	free(phag);
	phag = pnext;
    }
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    while(pasg) {
	free(pasg->pavalue);
	pasginp = (ASGINP *)ellFirst(&pasg->inpList);
	while(pasginp) {
	    pnext = ellNext(&pasginp->node);
	    ellDelete(&pasg->inpList,&pasginp->node);
	    free(pasginp);
	    pasginp = pnext;
	}
	pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
	while(pasgrule) {
	    free(pasgrule->calc);
	    free(pasgrule->rpcl);
	    pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
	    while(pasguag) {
		pnext = ellNext(&pasguag->node);
		ellDelete(&pasgrule->uagList,&pasguag->node);
		free(pasguag);
		pasguag = pnext;
	    }
	    pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
	    while(pasghag) {
		pnext = ellNext(&pasghag->node);
		ellDelete(&pasgrule->hagList,&pasghag->node);
		free(pasghag);
		pasghag = pnext;
	    }
	    pnext = ellNext(&pasgrule->node);
	    ellDelete(&pasg->ruleList,&pasgrule->node);
	    free(pasgrule);
	    pasgrule = pnext;
	}
	pnext = ellNext(&pasg->node);
	ellDelete(&pasbase->asgList,&pasg->node);
	free(pasg);
	pasg = pnext;
    }
    gphFreeMem(pasbase->phash);
    free(pasbase);
}

/*Beginning of routines called by lex code*/
static UAG *asUagAdd(const char *uagName)
{
    UAG		*pprev;
    UAG		*pnext;
    UAG		*puag;
    int		cmpvalue;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    /*Insert in alphabetic order*/
    pnext = (UAG *)ellFirst(&pasbase->uagList);
    while(pnext) {
	cmpvalue = strcmp(uagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errlogPrintf("Duplicate User Access Group named '%s'\n", uagName);
	    return(NULL);
	}
	pnext = (UAG *)ellNext(&pnext->node);
    }
    puag = asCalloc(1,sizeof(UAG)+strlen(uagName)+1);
    ellInit(&puag->list);
    puag->name = (char *)(puag+1);
    strcpy(puag->name,uagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pasbase->uagList,&puag->node);
    } else {
	pprev = (UAG *)ellPrevious(&pnext->node);
        ellInsert(&pasbase->uagList,&pprev->node,&puag->node);
    }
    return(puag);
}

static long asUagAddUser(UAG *puag,const char *user)
{
    UAGNAME	*puagname;

    if(!puag) return(0);
    puagname = asCalloc(1,sizeof(UAGNAME)+strlen(user)+1);
    puagname->user = (char *)(puagname+1);
    strcpy(puagname->user,user);
    ellAdd(&puag->list,&puagname->node);
    return(0);
}

static HAG *asHagAdd(const char *hagName)
{
    HAG		*pprev;
    HAG		*pnext;
    HAG		*phag;
    int		cmpvalue;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    /*Insert in alphabetic order*/
    pnext = (HAG *)ellFirst(&pasbase->hagList);
    while(pnext) {
	cmpvalue = strcmp(hagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errlogPrintf("Duplicate Host Access Group named '%s'\n", hagName);
	    return(NULL);
	}
	pnext = (HAG *)ellNext(&pnext->node);
    }
    phag = asCalloc(1,sizeof(HAG)+strlen(hagName)+1);
    ellInit(&phag->list);
    phag->name = (char *)(phag+1);
    strcpy(phag->name,hagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pasbase->hagList,&phag->node);
    } else {
	pprev = (HAG *)ellPrevious(&pnext->node);
	ellInsert(&pasbase->hagList,&pprev->node,&phag->node);
    }
    return(phag);
}

static IPAG *asIPagAdd(const char *ipagName)
{
    IPAG		*pprev;
    IPAG		*pnext;
    IPAG		*pipag;
    int		cmpvalue;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    /*Insert in alphabetic order*/
    pnext = (IPAG *)ellFirst(&pasbase->ipagList);
    while(pnext) {
	cmpvalue = strcmp(ipagName,pnext->name);
	if(cmpvalue < 0) break;
	if(cmpvalue==0) {
	    errlogPrintf("Duplicate IP Access Group named '%s'\n", ipagName);
	    return(NULL);
	}
	pnext = (IPAG *)ellNext(&pnext->node);
    }
    pipag = asCalloc(1,sizeof(IPAG)+strlen(ipagName)+1);
    asInitAddrList(&(pipag->addrlist), 5); /* initially space for 5 IP adresses */
    pipag->name = (char *)(pipag+1);
    strcpy(pipag->name,ipagName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pasbase->ipagList,&pipag->node);
    } else {
	pprev = (IPAG *)ellPrevious(&pnext->node);
	ellInsert(&pasbase->ipagList,&pprev->node,&pipag->node);
    }
    return(pipag);
}

static long asHagAddHost(HAG *phag,const char *host)
{
    HAGNAME *phagname;

    if (!phag) return 0;
    if(!asCheckClientIP) {
        size_t i, len = strlen(host);
        phagname = asCalloc(1, sizeof(*phagname) + len);
        for (i = 0; i < len; i++) {
            phagname->host[i] = (char)tolower((int)host[i]);
        }

    } else {
        struct sockaddr_in addr;
        epicsUInt32 ip;

        if(aToIPAddr(host, 0, &addr)) {
            static const char unresolved[] = "unresolved:";

            errlogPrintf("ACF: Unable to resolve host '%s'\n", host);

            phagname = asCalloc(1, sizeof(*phagname) + sizeof(unresolved)-1+strlen(host));
            strcpy(phagname->host, unresolved);
            strcat(phagname->host, host);

        } else {
            ip = ntohl(addr.sin_addr.s_addr);
            phagname = asCalloc(1, sizeof(*phagname) + 24);
            epicsSnprintf(phagname->host, 24,
                          "%u.%u.%u.%u",
                          (ip>>24)&0xff,
                          (ip>>16)&0xff,
                          (ip>>8)&0xff,
                          (ip>>0)&0xff);
        }
    }
    ellAdd(&phag->list, &phagname->node);
    return 0;
}

epicsShareFunc int asScanIp(const char *dotted_ip, epicsUInt32 *addr)
{
    int a[4];
    int rc;
    int i;
    rc= sscanf(dotted_ip, "%d.%d.%d.%d", a, a+1, a+2, a+3);
    if (rc!=4)
        return 0;
    for(i=0; i<4; i++)
        if ((a[i]<0) || (a[i]>255))
            return 0;
    *addr= htonl((a[0]<<24) | (a[1]<<16) | (a[2]<<8) | (a[3]));
    return 1;
}

static char *asIpToStr(epicsUInt32 addr)
{
    static char buf[20];
    epicsUInt32 haddr;
    haddr= ntohl(addr);
    sprintf(buf, "%lu.%lu.%lu.%lu", 
            (unsigned long)( haddr >> 24        ), 
            (unsigned long)((haddr >> 16) & 0xFF), 
            (unsigned long)((haddr >>  8) & 0xFF), 
            (unsigned long)( haddr        & 0xFF)
           );
    return buf;
}

static long asIPagAddIP(IPAG *pipag, char *dotted_ip)
{
    epicsUInt32 addr;

    if (!pipag) return 0;
    if (!asScanIp(dotted_ip, &addr)) return 1;
    asAddToAddrList(&(pipag->addrlist), addr);
    return 0;
}

static ASG *asAsgAdd(const char *asgName)
{
    ASG		*pprev;
    ASG		*pnext;
    ASG		*pasg;
    int		cmpvalue;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

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
	    errlogPrintf("Duplicate Access Security Group named '%s'\n", asgName);
	    return(NULL);
	}
	pnext = (ASG *)ellNext(&pnext->node);
    }
    pasg = asCalloc(1,sizeof(ASG)+strlen(asgName)+1);
    ellInit(&pasg->inpList);
    ellInit(&pasg->ruleList);
    ellInit(&pasg->memberList);
    pasg->name = (char *)(pasg+1);
    strcpy(pasg->name,asgName);
    if(pnext==NULL) { /*Add to end of list*/
	ellAdd(&pasbase->asgList,&pasg->node);
    } else {
	pprev = (ASG *)ellPrevious(&pnext->node);
	ellInsert(&pasbase->asgList,&pprev->node,&pasg->node);
    }
    return(pasg);
}

static long asAsgAddInp(ASG *pasg,const char *inp,int inpIndex)
{
    ASGINP	*pasginp;

    if(!pasg) return(0);
    pasginp = asCalloc(1,sizeof(ASGINP)+strlen(inp)+1);
    pasginp->inp = (char *)(pasginp+1);
    strcpy(pasginp->inp,inp);
    pasginp->pasg = pasg;
    pasginp->inpIndex = inpIndex;
    ellAdd(&pasg->inpList,&pasginp->node);
    return(0);
}

static ASGRULE *asAsgAddRule(ASG *pasg,asAccessRights access,int level)
{
    ASGRULE	*pasgrule;

    if(!pasg) return(0);
    pasgrule = asCalloc(1,sizeof(ASGRULE));
    pasgrule->access = access;
    pasgrule->trapMask = 0;
    pasgrule->level = level;
    ellInit(&pasgrule->uagList);
    ellInit(&pasgrule->hagList);
    ellAdd(&pasg->ruleList,&pasgrule->node);
    return(pasgrule);
}

static long asAsgAddRuleOptions(ASGRULE *pasgrule,int trapMask)
{
    if(!pasgrule) return(0);
    pasgrule->trapMask = trapMask;
    return(0);
}

static long asAsgRuleUagAdd(ASGRULE *pasgrule, const char *name)
{
    ASGUAG	*pasguag;
    UAG		*puag;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    if (!pasgrule)
        return 0;

    puag = (UAG *)ellFirst(&pasbase->uagList);
    while (puag) {
        if (strcmp(puag->name, name)==0)
            break;
        puag = (UAG *)ellNext(&puag->node);
    }
    if (!puag){
        errlogPrintf("No User Access Group named '%s' defined\n", name);
        return S_asLib_noUag;
    }

    pasguag = asCalloc(1, sizeof(ASGUAG));
    pasguag->puag = puag;
    ellAdd(&pasgrule->uagList, &pasguag->node);
    return 0;
}

static long asAsgRuleHagAdd(ASGRULE *pasgrule, const char *name)
{
    ASGHAG	*pasghag;
    HAG		*phag;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    if (!pasgrule)
        return 0;

    phag = (HAG *)ellFirst(&pasbase->hagList);
    while (phag) {
        if (strcmp(phag->name, name)==0)
            break;
        phag = (HAG *)ellNext(&phag->node);
    }
    if (!phag){
        errlogPrintf("No Host Access Group named '%s' defined\n", name);
        return S_asLib_noHag;
    }

    pasghag = asCalloc(1, sizeof(ASGHAG));
    pasghag->phag = phag;
    ellAdd(&pasgrule->hagList, &pasghag->node);
    return 0;
}

static long asAsgRuleIPagAdd(ASGRULE *pasgrule, const char *name)
{
    ASGIPAG	*pasgipag;
    IPAG	*pipag;
    ASBASE	*pasbase = (ASBASE *)pasbasenew;

    if (!pasgrule)
        return 0;

    pipag = (IPAG *)ellFirst(&pasbase->ipagList);
    while (pipag) {
        if (strcmp(pipag->name, name)==0)
            break;
        pipag = (IPAG *)ellNext(&pipag->node);
    }
    if (!pipag){
        errlogPrintf("No Host Access Group named '%s' defined\n", name);
        return S_asLib_noIPag;
    }

    pasgipag = asCalloc(1, sizeof(ASGIPAG));
    pasgipag->pipag = pipag;
    ellAdd(&pasgrule->ipagList, &pasgipag->node);
    return 0;
}

static long asAsgRuleCalc(ASGRULE *pasgrule,const char *calc)
{
    short err;
    long status;
    size_t insize;
    unsigned long stores;

    if (!pasgrule) return 0;
    insize = strlen(calc) + 1;
    pasgrule->calc = asCalloc(1, insize);
    strcpy(pasgrule->calc, calc);
    pasgrule->rpcl = asCalloc(1, INFIX_TO_POSTFIX_SIZE(insize));
    status = postfix(pasgrule->calc, pasgrule->rpcl, &err);
    if(status) {
	free(pasgrule->calc);
	free(pasgrule->rpcl);
	pasgrule->calc = NULL;
	pasgrule->rpcl = NULL;
	status = S_asLib_badCalc;
	errlogPrintf("%s in CALC expression '%s'\n", calcErrorStr(err), calc);
	return status;
    }
    calcArgUsage(pasgrule->rpcl, &pasgrule->inpUsed, &stores);
    /* Until someone proves stores are not dangerous, don't allow them */
    if (stores) {
	free(pasgrule->calc);
	free(pasgrule->rpcl);
	pasgrule->calc = NULL;
	pasgrule->rpcl = NULL;
	status = S_asLib_badCalc;
	errlogPrintf("Assignment operator used in CALC expression '%s'\n",
		     calc);
    }
    return(status);
}
