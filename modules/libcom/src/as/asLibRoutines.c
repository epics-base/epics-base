/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    10-15-93 */

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "osiSock.h"
#include "epicsTypes.h"
#include "epicsStdio.h"
#include "dbDefs.h"
#include "epicsThread.h"
#include "epicsString.h"
#include "epicsTime.h"
#include "cantProceed.h"
#include "epicsMutex.h"
#include "errlog.h"
#include "gpHash.h"
#include "freeList.h"
#include "macLib.h"
#include "postfix.h"
#include "asLib.h"
#include "as/asLibPvt.h"

#undef ECHO /* from termios.h */

int asCheckClientIP;

static epicsMutexId asLock;
static epicsMutexId asHagRefreshLock;
#define LOCK epicsMutexMustLock(asLock)
#define UNLOCK epicsMutexUnlock(asLock)

/*following must be global because asCa nneeds it*/
ASBASE volatile *pasbase=NULL;
static ASBASE *pasbasenew=NULL;
int   asActive = FALSE;

static void         *freeListPvt = NULL;


#define DEFAULT "DEFAULT"
#define AS_HAG_SUCCESS_INTERVAL 300u
#define AS_HAG_FAILURE_INTERVAL 60u
#define AS_HAG_NSEC_PER_SEC 1000000000uLL
#define AS_HAG_IP_BUFSIZE 24u
#define AS_HAG_SOURCE_MAX 512u
#define AS_HAG_UNRESOLVED "unresolved:"

typedef struct asHagEntryState {
    ELLNODE node;
    HAG *group;
    HAGNAME *name;
    char *source;
    size_t capacity;
    epicsUInt64 due;
    unsigned index;
    unsigned resolved;
} ASHAGENTRYSTATE;

typedef struct asHagBaseState {
    ELLNODE node;
    ASBASE *base;
    ELLLIST entries;
    epicsUInt64 generation;
    epicsUInt64 nextDue;
} ASHAGBASESTATE;

typedef struct asHagUpdate {
    unsigned index;
    char *source;
    char address[AS_HAG_IP_BUFSIZE];
    unsigned resolved;
} ASHAGUPDATE;

typedef struct asHagMapEntry {
    HAG *group;
    char address[AS_HAG_IP_BUFSIZE];
} ASHAGMAPENTRY;

static ELLLIST asHagStates;
static epicsUInt64 asHagGeneration;
static asHagResolver asHagResolve = aToIPAddr;
static asHagClock asHagNow = epicsMonotonicGet;
static epicsThreadOnceId asInitializeOnceFlag = EPICS_THREAD_ONCE_INIT;

/* Defined in asLib.y */
static int myParse(ASINPUTFUNCPTR inputfunction);

/*private routines */
static long asAddMemberPvt(ASMEMBERPVT *pasMemberPvt,const char *asgName);
static long asComputeAllAsgPvt(void);
static long asComputeAsgPvt(ASG *pasg);
static long asComputePvt(ASCLIENTPVT asClientPvt);
static UAG *asUagAdd(const char *uagName);
static long asUagAddUser(UAG *puag,const char *user);
static HAG *asHagAdd(const char *hagName);
static long asHagAddHost(HAG *phag,const char *host);
static ASHAGBASESTATE *asHagStateFind(ASBASE *base);
static void asHagStateDestroy(ASBASE *base);
static void asHashRebuild(ASBASE *base);
static ASG *asAsgAdd(const char *asgName);
static long asAsgAddInp(ASG *pasg,const char *inp,int inpIndex);
static ASGRULE *asAsgAddRule(ASG *pasg,asAccessRights access,int level);
static long asAsgAddRuleOptions(ASGRULE *pasgrule,int trapMask);
static long asAsgRuleUagAdd(ASGRULE *pasgrule,const char *name);
static long asAsgRuleHagAdd(ASGRULE *pasgrule,const char *name);
static long asAsgRuleCalc(ASGRULE *pasgrule,const char *calc);
static long asAsgRuleDisable(ASGRULE *pasgrule);

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
    asHagRefreshLock = epicsMutexMustCreate();
    ellInit(&asHagStates);
}

static void asEnsureInitialized(void)
{
    epicsThreadOnce(&asInitializeOnceFlag,asInitializeOnce,NULL);
}

void asTestSetHagResolver(asHagResolver resolver)
{
    asEnsureInitialized();
    epicsMutexMustLock(asHagRefreshLock);
    asHagResolve = resolver ? resolver : aToIPAddr;
    epicsMutexUnlock(asHagRefreshLock);
}

void asTestSetHagClock(asHagClock clock)
{
    asEnsureInitialized();
    epicsMutexMustLock(asHagRefreshLock);
    asHagNow = clock ? clock : epicsMonotonicGet;
    epicsMutexUnlock(asHagRefreshLock);
}

void asTestResetHagHooks(void)
{
    asEnsureInitialized();
    epicsMutexMustLock(asHagRefreshLock);
    asHagResolve = aToIPAddr;
    asHagNow = epicsMonotonicGet;
    epicsMutexUnlock(asHagRefreshLock);
}

static ASHAGBASESTATE *asHagStateFind(ASBASE *base)
{
    ASHAGBASESTATE *state = (ASHAGBASESTATE *)ellFirst(&asHagStates);

    while(state) {
        if(state->base == base)
            return state;
        state = (ASHAGBASESTATE *)ellNext(&state->node);
    }
    return NULL;
}

static ASHAGBASESTATE *asHagStateCreate(ASBASE *base)
{
    ASHAGBASESTATE *state = asCalloc(1, sizeof(*state));

    state->base = base;
    ellInit(&state->entries);
    ellAdd(&asHagStates, &state->node);
    return state;
}

static void asHagStateDestroy(ASBASE *base)
{
    ASHAGBASESTATE *state = asHagStateFind(base);

    if(state) {
        ASHAGENTRYSTATE *entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);

        while(entry) {
            ASHAGENTRYSTATE *next = (ASHAGENTRYSTATE *)ellNext(&entry->node);

            ellDelete(&state->entries, &entry->node);
            free(entry->source);
            free(entry);
            entry = next;
        }
        ellDelete(&asHagStates, &state->node);
        free(state);
    }
}

static ASHAGENTRYSTATE *asHagEntryFind(ASHAGBASESTATE *state, HAGNAME *name)
{
    ASHAGENTRYSTATE *entry;

    if(!state)
        return NULL;
    entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);
    while(entry) {
        if(entry->name == name)
            return entry;
        entry = (ASHAGENTRYSTATE *)ellNext(&entry->node);
    }
    return NULL;
}

static epicsUInt64 asHagAfter(epicsUInt64 now, unsigned seconds)
{
    epicsUInt64 delta = (epicsUInt64)seconds * AS_HAG_NSEC_PER_SEC;

    return now > ~(epicsUInt64)0 - delta ? ~(epicsUInt64)0 : now + delta;
}

static void asHagScheduleNext(ASHAGBASESTATE *state)
{
    ASHAGENTRYSTATE *entry;
    epicsUInt64 next = 0;

    if(!state)
        return;
    entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);
    while(entry) {
        if(!next || entry->due < next)
            next = entry->due;
        entry = (ASHAGENTRYSTATE *)ellNext(&entry->node);
    }
    state->nextDue = next;
}

static int asHagSetIPAddr(epicsUInt32 rawAddr, struct sockaddr_in *address)
{
    static const struct sockaddr_in emptyAddress = {0};

    *address = emptyAddress;
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(rawAddr);
    return 0;
}

static int asHagParseNumeric(const char *host, struct sockaddr_in *address)
{
    unsigned octet[4];
    unsigned long raw;
    unsigned port;
    char extra[8];
    int status;

    status = sscanf(host, " %u . %u . %u . %u %7s ",
        octet, octet+1, octet+2, octet+3, extra);
    if(status == 4) {
        if(octet[0] <= 0xff && octet[1] <= 0xff &&
           octet[2] <= 0xff && octet[3] <= 0xff)
            return asHagSetIPAddr(((epicsUInt32)octet[0] << 24) |
                                  ((epicsUInt32)octet[1] << 16) |
                                  ((epicsUInt32)octet[2] << 8) |
                                  (epicsUInt32)octet[3], address);
        return -1;
    }
    status = sscanf(host, " %u . %u . %u . %u : %u %7s ",
        octet, octet+1, octet+2, octet+3, &port, extra);
    if(status == 5 && port <= 0xffff &&
       octet[0] <= 0xff && octet[1] <= 0xff &&
       octet[2] <= 0xff && octet[3] <= 0xff)
        return asHagSetIPAddr(((epicsUInt32)octet[0] << 24) |
                              ((epicsUInt32)octet[1] << 16) |
                              ((epicsUInt32)octet[2] << 8) |
                              (epicsUInt32)octet[3], address);

    status = sscanf(host, " %lu %7s ", &raw, extra);
    if(status == 1 && raw <= 0xfffffffful)
        return asHagSetIPAddr((epicsUInt32)raw, address);
    status = sscanf(host, " %lu : %u %7s ", &raw, &port, extra);
    if(status == 2 && raw <= 0xfffffffful && port <= 0xffff)
        return asHagSetIPAddr((epicsUInt32)raw, address);
    return -1;
}

static void asHagFormatAddress(const struct sockaddr_in *address,
    char *buffer, size_t size)
{
    epicsUInt32 ip = ntohl(address->sin_addr.s_addr);

    epicsSnprintf(buffer, size, "%u.%u.%u.%u",
                  (ip>>24)&0xff, (ip>>16)&0xff,
                  (ip>>8)&0xff, ip&0xff);
}

static void asHagSetEntry(ASHAGENTRYSTATE *entry, int resolved,
    const char *address)
{
    if(resolved) {
        epicsSnprintf(entry->name->host, entry->capacity, "%s", address);
    } else {
        epicsSnprintf(entry->name->host, entry->capacity, "%s%s",
            AS_HAG_UNRESOLVED, entry->source);
    }
    entry->resolved = !!resolved;
}

static int asHagMapContains(const ASHAGMAPENTRY *map, size_t count,
    HAG *group, const char *address)
{
    size_t i;

    for(i = 0; i < count; i++) {
        if(map[i].group == group && strcmp(map[i].address, address) == 0)
            return 1;
    }
    return 0;
}

static ASHAGMAPENTRY *asHagCollectMap(ASBASE *base, size_t *count)
{
    ASHAGBASESTATE *state = asHagStateFind(base);
    HAG *group;
    ASHAGMAPENTRY *map;
    size_t capacity = 0;

    group = (HAG *)ellFirst(&base->hagList);
    while(group) {
        capacity += (size_t)ellCount(&group->list);
        group = (HAG *)ellNext(&group->node);
    }
    map = capacity ? asCalloc(capacity, sizeof(*map)) : NULL;
    *count = 0;
    group = (HAG *)ellFirst(&base->hagList);
    while(group) {
        HAGNAME *name = (HAGNAME *)ellFirst(&group->list);

        while(name) {
            ASHAGENTRYSTATE *entry = asHagEntryFind(state, name);

            if((!entry || entry->resolved) &&
               !asHagMapContains(map, *count, group, name->host)) {
                map[*count].group = group;
                epicsSnprintf(map[*count].address,
                    sizeof(map[*count].address), "%s", name->host);
                (*count)++;
            }
            name = (HAGNAME *)ellNext(&name->node);
        }
        group = (HAG *)ellNext(&group->node);
    }
    return map;
}

static int asHagMapsEqual(const ASHAGMAPENTRY *left, size_t leftCount,
    const ASHAGMAPENTRY *right, size_t rightCount)
{
    size_t i;

    if(leftCount != rightCount)
        return 0;
    for(i = 0; i < leftCount; i++) {
        if(!asHagMapContains(right, rightCount,
                            left[i].group, left[i].address))
            return 0;
    }
    return 1;
}

static void asHashRebuild(ASBASE *base)
{
    struct gphPvt *oldhash = base->phash;
    ASHAGBASESTATE *state = asHagStateFind(base);
    UAG *puag;
    HAG *phag;

    gphInitPvt(&base->phash, 256);
    puag = (UAG *)ellFirst(&base->uagList);
    while(puag) {
        UAGNAME *name = (UAGNAME *)ellFirst(&puag->list);

        while(name) {
            if(!gphAdd(base->phash, name->user, puag))
                errlogPrintf("Duplicated user '%s' in UAG '%s'\n",
                    name->user, puag->name);
            name = (UAGNAME *)ellNext(&name->node);
        }
        puag = (UAG *)ellNext(&puag->node);
    }
    phag = (HAG *)ellFirst(&base->hagList);
    while(phag) {
        HAGNAME *name = (HAGNAME *)ellFirst(&phag->list);

        while(name) {
            ASHAGENTRYSTATE *entry = asHagEntryFind(state, name);

            if((!entry || entry->resolved) &&
               !gphAdd(base->phash, name->host, phag))
                errlogPrintf("Duplicated host '%s' in HAG '%s'\n",
                    name->host, phag->name);
            name = (HAGNAME *)ellNext(&name->node);
        }
        phag = (HAG *)ellNext(&phag->node);
    }
    if(oldhash)
        gphFreeMem(oldhash);
}

long epicsStdCall asInitialize(ASINPUTFUNCPTR inputfunction)
{
    ASG         *pasg;
    long        status;
    ASBASE      *pasbaseold;
    ASHAGBASESTATE *hagstate;

    asEnsureInitialized();
    LOCK;
    pasbasenew = asCalloc(1,sizeof(ASBASE));
    hagstate = asHagStateCreate(pasbasenew);
    if(!freeListPvt) freeListInitPvt(&freeListPvt,sizeof(ASGCLIENT),20);
    ellInit(&pasbasenew->uagList);
    ellInit(&pasbasenew->hagList);
    ellInit(&pasbasenew->asgList);
    asAsgAdd(DEFAULT);
    status = myParse(inputfunction);
    if(status) {
        status = S_asLib_badConfig;
        asHagStateDestroy(pasbasenew);
        /*Not safe to call asFreeAll */
        UNLOCK;
        return(status);
    }
    pasg = (ASG *)ellFirst(&pasbasenew->asgList);
    while(pasg) {
        pasg->pavalue = asCalloc(CALCPERFORM_NARGS, sizeof(double));
        pasg = (ASG *)ellNext(&pasg->node);
    }
    asHashRebuild(pasbasenew);
    hagstate->generation = ++asHagGeneration;
    pasbaseold = (ASBASE *)pasbase;
    pasbase = (ASBASE volatile *)pasbasenew;
    if(pasbaseold) {
        ASG             *poldasg;
        ASGMEMBER       *poldmem;
        ASGMEMBER       *pnextoldmem;

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

long epicsStdCall asInitFile(const char *filename,const char *substitutions)
{
    FILE *fp;
    long status;

    fp = fopen(filename,"r");
    if(!fp) {
        fprintf(stderr, ERL_ERROR " asInitFile: Can't open file '%s'\n", filename);
        return(S_asLib_badConfig);
    }
    status = asInitFP(fp,substitutions);
    if(fclose(fp)==EOF) {
        fprintf(stderr, ERL_ERROR " asInitFile: fclose failed!");
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
    int l,n;
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

long epicsStdCall asInitFP(FILE *fp,const char *substitutions)
{
    char        buffer[BUF_SIZE];
    char        mac_buffer[BUF_SIZE];
    long        status;
    char        **macPairs;

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

long epicsStdCall asInitMem(const char *acf, const char *substitutions)
{
    long ret = S_asLib_InitFailed;
    if(!acf) return ret;

    membuf = acf;
    ret = asInitialize(&memInputFunction);
    membuf = NULL;

    return ret;
}

long epicsStdCall asAddMember(ASMEMBERPVT *pasMemberPvt,const char *asgName)
{
    long        status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asAddMemberPvt(pasMemberPvt,asgName);
    UNLOCK;
    return(status);
}

long epicsStdCall asRemoveMember(ASMEMBERPVT *asMemberPvt)
{
    ASGMEMBER   *pasgmember;

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

long epicsStdCall asChangeGroup(ASMEMBERPVT *asMemberPvt,const char *newAsgName)
{
    ASGMEMBER   *pasgmember;
    long        status;

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

void * epicsStdCall asGetMemberPvt(ASMEMBERPVT asMemberPvt)
{
    ASGMEMBER   *pasgmember = asMemberPvt;

    if(!asActive) return(NULL);
    if(!pasgmember) return(NULL);
    return(pasgmember->userPvt);
}

void epicsStdCall asPutMemberPvt(ASMEMBERPVT asMemberPvt,void *userPvt)
{
    ASGMEMBER   *pasgmember = asMemberPvt;

    if(!asActive) return;
    if(!pasgmember) return;
    pasgmember->userPvt = userPvt;
}

long epicsStdCall asAddClient(ASCLIENTPVT *pasClientPvt,ASMEMBERPVT asMemberPvt,
        int asl,const char *user,char *host)
{
    ASGMEMBER   *pasgmember = asMemberPvt;
    ASGCLIENT   *pasgclient;
    size_t      len, i;

    long        status;
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
    LOCK;
    ellAdd(&pasgmember->clientList,&pasgclient->node);
    status = asComputePvt(pasgclient);
    UNLOCK;
    return(status);
}

long epicsStdCall asChangeClient(
    ASCLIENTPVT asClientPvt,int asl,const char *user,char *host)
{
    ASGCLIENT   *pasgclient = asClientPvt;
    long        status;
    size_t      len, i;

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
    status = asComputePvt(pasgclient);
    UNLOCK;
    return(status);
}

long epicsStdCall asRemoveClient(ASCLIENTPVT *asClientPvt)
{
    ASGCLIENT   *pasgclient = *asClientPvt;
    ASGMEMBER   *pasgMember;

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

long epicsStdCall asRegisterClientCallback(ASCLIENTPVT asClientPvt,
        ASCLIENTCALLBACK pcallback)
{
    ASGCLIENT   *pasgclient = asClientPvt;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    LOCK;
    pasgclient->pcallback = pcallback;
    (*pasgclient->pcallback)(pasgclient,asClientCOAR);
    UNLOCK;
    return(0);
}

void * epicsStdCall asGetClientPvt(ASCLIENTPVT asClientPvt)
{
    ASGCLIENT   *pasgclient = asClientPvt;

    if(!asActive) return(NULL);
    if(!pasgclient) return(NULL);
    return(pasgclient->userPvt);
}

void epicsStdCall asPutClientPvt(ASCLIENTPVT asClientPvt,void *userPvt)
{
    ASGCLIENT   *pasgclient = asClientPvt;
    if(!asActive) return;
    if(!pasgclient) return;
    LOCK;
    pasgclient->userPvt = userPvt;
    UNLOCK;
}

long epicsStdCall asComputeAllAsg(void)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputeAllAsgPvt();
    UNLOCK;
    return(status);
}

long epicsStdCall asComputeAsg(ASG *pasg)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputeAsgPvt(pasg);
    UNLOCK;
    return(status);
}

long epicsStdCall asCompute(ASCLIENTPVT asClientPvt)
{
    long status;

    if(!asActive) return(S_asLib_asNotActive);
    LOCK;
    status = asComputePvt(asClientPvt);
    UNLOCK;
    return(status);
}

long epicsStdCall asRefreshHag(unsigned *changed)
{
    ASHAGBASESTATE *state;
    ASHAGENTRYSTATE *entry;
    ASHAGUPDATE *updates = NULL;
    ASHAGMAPENTRY *before = NULL;
    ASHAGMAPENTRY *after = NULL;
    epicsUInt64 generation = 0;
    epicsUInt64 now;
    size_t updateCount = 0;
    size_t beforeCount = 0;
    size_t afterCount = 0;
    size_t i;
    int entryChanged = 0;
    int effectiveChanged = 0;
    long status = 0;

    if(changed)
        *changed = 0;
    asEnsureInitialized();
    if(!asActive)
        return S_asLib_asNotActive;

    epicsMutexMustLock(asHagRefreshLock);
    now = asHagNow();
    LOCK;
    if(!asActive) {
        status = S_asLib_asNotActive;
        UNLOCK;
        goto done;
    }
    state = asHagStateFind((ASBASE *)pasbase);
    if(!asCheckClientIP || !state || !state->nextDue || now < state->nextDue) {
        UNLOCK;
        goto done;
    }
    entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);
    while(entry) {
        if(now >= entry->due)
            updateCount++;
        entry = (ASHAGENTRYSTATE *)ellNext(&entry->node);
    }
    if(!updateCount) {
        asHagScheduleNext(state);
        UNLOCK;
        goto done;
    }
    generation = state->generation;
    updates = asCalloc(updateCount, sizeof(*updates));
    entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);
    i = 0;
    while(entry) {
        if(now >= entry->due) {
            updates[i].index = entry->index;
            updates[i].source = epicsStrDup(entry->source);
            i++;
        }
        entry = (ASHAGENTRYSTATE *)ellNext(&entry->node);
    }
    UNLOCK;

    for(i = 0; i < updateCount; i++) {
        struct sockaddr_in address;

        if(asHagResolve(updates[i].source, 0, &address) == 0) {
            asHagFormatAddress(&address, updates[i].address,
                               sizeof(updates[i].address));
            updates[i].resolved = 1;
        } else {
            errlogPrintf("ACF: Unable to refresh host '%s'\n",
                         updates[i].source);
        }
    }

    now = asHagNow();
    LOCK;
    state = asHagStateFind((ASBASE *)pasbase);
    if(!asActive || !state || state->generation != generation) {
        UNLOCK;
        goto done;
    }
    before = asHagCollectMap((ASBASE *)pasbase, &beforeCount);
    entry = (ASHAGENTRYSTATE *)ellFirst(&state->entries);
    while(entry) {
        for(i = 0; i < updateCount; i++) {
            int different;

            if(updates[i].index != entry->index)
                continue;
            different = entry->resolved != updates[i].resolved ||
                (entry->resolved &&
                 strcmp(entry->name->host, updates[i].address) != 0);
            if(different) {
                asHagSetEntry(entry, updates[i].resolved,
                              updates[i].address);
                entryChanged = 1;
            }
            entry->due = asHagAfter(now,
                updates[i].resolved ? AS_HAG_SUCCESS_INTERVAL
                                    : AS_HAG_FAILURE_INTERVAL);
            break;
        }
        entry = (ASHAGENTRYSTATE *)ellNext(&entry->node);
    }
    asHagScheduleNext(state);
    if(entryChanged) {
        after = asHagCollectMap((ASBASE *)pasbase, &afterCount);
        effectiveChanged = !asHagMapsEqual(before, beforeCount,
                                            after, afterCount);
        asHashRebuild((ASBASE *)pasbase);
        if(effectiveChanged)
            status = asComputeAllAsgPvt();
    }
    UNLOCK;
    if(changed)
        *changed = !!effectiveChanged;

done:
    if(updates) {
        for(i = 0; i < updateCount; i++)
            free(updates[i].source);
    }
    free(updates);
    free(before);
    free(after);
    epicsMutexUnlock(asHagRefreshLock);
    return status;
}

/*The dump routines do not lock. Thus they may get inconsistent data.*/
/*HOWEVER if they did lock and a user interrupts one of then then BAD BAD*/
static const char *asAccessName[] = {"NONE","READ","WRITE"};
static const char *asTrapOption[] = {"NOTRAPWRITE","TRAPWRITE"};
static const char *asLevelName[] = {"ASL0","ASL1"};
int epicsStdCall asDump(
        void (*memcallback)(struct asgMember *,FILE *),
        void (*clientcallback)(struct asgClient *,FILE *),
        int verbose)
{
    return asDumpFP(stdout,memcallback,clientcallback,verbose);
}

int epicsStdCall asDumpFP(
        FILE *fp,
        void (*memcallback)(struct asgMember *,FILE *),
        void (*clientcallback)(struct asgClient *,FILE *),
        int verbose)
{
    UAG         *puag;
    UAGNAME     *puagname;
    HAG         *phag;
    HAGNAME     *phagname;
    ASG         *pasg;
    ASGINP      *pasginp;
    ASGRULE     *pasgrule;
    ASGHAG      *pasghag;
    ASGUAG      *pasguag;
    ASGMEMBER   *pasgmember;
    ASGCLIENT   *pasgclient;

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
            int print_rule_end_brace = FALSE;
            if (pasgrule->ignore) goto next_rule;
            fprintf(fp,"\tRULE(%d,%s,%s)",
                pasgrule->level,asAccessName[pasgrule->access],
                asTrapOption[pasgrule->trapMask]);
            pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
            pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
            if(pasguag || pasghag || pasgrule->calc) {
                fprintf(fp," {\n");
                print_rule_end_brace = TRUE;
            } else {
                fprintf(fp,"\n");
                print_rule_end_brace = FALSE;
            }
            if(pasguag) fprintf(fp,"\t\tUAG(");
            while(pasguag) {
                fprintf(fp,"%s",pasguag->puag->name);
                pasguag = (ASGUAG *)ellNext(&pasguag->node);
                if(pasguag) fprintf(fp,","); else fprintf(fp,")\n");
            }
            if(pasghag) fprintf(fp,"\t\tHAG(");
            while(pasghag) {
                fprintf(fp,"%s",pasghag->phag->name);
                pasghag = (ASGHAG *)ellNext(&pasghag->node);
                if(pasghag) fprintf(fp,","); else fprintf(fp,")\n");
            }
            if(pasgrule->calc) {
                fprintf(fp,"\t\tCALC(\"%s\")",pasgrule->calc);
                if(verbose)
                    fprintf(fp," result=%s",(pasgrule->result==1 ? "TRUE" : "FALSE"));
                fprintf(fp,"\n");
            }
next_rule:
            if(print_rule_end_brace) fprintf(fp,"\t}\n");
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
                fprintf(fp,"\t\t\t %s %s",pasgclient->user,pasgclient->host);
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

static
void asDumpQuoted(FILE *fp, const char *s)
{
    fprintf(fp, "\"");
    (void)epicsStrPrintEscaped(fp, s, strlen(s));
    fprintf(fp, "\"");
}

int epicsStdCall asDumpUag(const char *uagname)
{
    return asDumpUagFP(stdout,uagname);
}

int epicsStdCall asDumpUagFP(FILE *fp,const char *uagname)
{
    UAG         *puag;
    UAGNAME     *puagname;

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
            asDumpQuoted(fp, puagname->user);
            puagname = (UAGNAME *)ellNext(&puagname->node);
            if(puagname) fprintf(fp,","); else fprintf(fp,"}\n");
        }
        puag = (UAG *)ellNext(&puag->node);
    }
    return(0);
}

int epicsStdCall asDumpHag(const char *hagname)
{
    return asDumpHagFP(stdout,hagname);
}

int epicsStdCall asDumpHagFP(FILE *fp,const char *hagname)
{
    HAG         *phag;
    HAGNAME     *phagname;

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
            asDumpQuoted(fp, phagname->host);
            phagname = (HAGNAME *)ellNext(&phagname->node);
            if(phagname) fprintf(fp,","); else fprintf(fp,"}\n");
        }
        phag = (HAG *)ellNext(&phag->node);
    }
    return(0);
}

int epicsStdCall asDumpRules(const char *asgname)
{
    return asDumpRulesFP(stdout,asgname);
}

int epicsStdCall asDumpRulesFP(FILE *fp,const char *asgname)
{
    ASG         *pasg;
    ASGINP      *pasginp;
    ASGRULE     *pasgrule;
    ASGHAG      *pasghag;
    ASGUAG      *pasguag;

    if(!asActive) return(0);
    pasg = (ASG *)ellFirst(&pasbase->asgList);
    if(!pasg) fprintf(fp,"No ASGs\n");
    while(pasg) {
        int print_end_brace = FALSE;

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
            int print_rule_end_brace = FALSE;
            if ( pasgrule->ignore) goto next_rule;
            fprintf(fp,"\tRULE(%d,%s,%s)",
                pasgrule->level,asAccessName[pasgrule->access],
                asTrapOption[pasgrule->trapMask]);
            pasguag = (ASGUAG *)ellFirst(&pasgrule->uagList);
            pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
            if(pasguag || pasghag || pasgrule->calc) {
                fprintf(fp," {\n");
                print_rule_end_brace = TRUE;
            } else {
                fprintf(fp,"\n");
                print_rule_end_brace = FALSE;
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
            if(pasgrule->calc) {
                fprintf(fp,"\t\tCALC(\"%s\")",pasgrule->calc);
                fprintf(fp," result=%s",(pasgrule->result==1 ? "TRUE" : "FALSE"));
                fprintf(fp,"\n");
            }
next_rule:
            if(print_rule_end_brace) fprintf(fp,"\t}\n");
            pasgrule = (ASGRULE *)ellNext(&pasgrule->node);
        }
        if(print_end_brace) fprintf(fp,"}\n");
        pasg = (ASG *)ellNext(&pasg->node);
    }
    return(0);
}

int epicsStdCall asDumpMem(const char *asgname,void (*memcallback)(ASMEMBERPVT,FILE *),
  int clients)
{
    return asDumpMemFP(stdout,asgname,memcallback,clients);
}

int epicsStdCall asDumpMemFP(FILE *fp,const char *asgname,
  void (*memcallback)(ASMEMBERPVT,FILE *),int clients)
{
    ASG         *pasg;
    ASGMEMBER   *pasgmember;
    ASGCLIENT   *pasgclient;

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

LIBCOM_API int epicsStdCall asDumpHash(void)
{
    return asDumpHashFP(stdout);
}

LIBCOM_API int epicsStdCall asDumpHashFP(FILE *fp)
{
    if(!asActive) return(0);
    gphDumpFP(fp,pasbase->phash);
    return(0);
}

/*Start of private routines*/
/* asCalloc is "friend" function */
LIBCOM_API void * epicsStdCall asCalloc(size_t nobj,size_t size)
{
    void *p;

    p=callocMustSucceed(nobj,size,"asCalloc");
    return(p);
}
LIBCOM_API char * epicsStdCall asStrdup(unsigned char *str)
{
        size_t len = strlen((char *) str);
        char *buf = asCalloc(1, len + 1);
        strcpy(buf, (char *) str);
        return buf;
}

static long asAddMemberPvt(ASMEMBERPVT *pasMemberPvt,const char *asgName)
{
    ASGMEMBER   *pasgmember;
    ASG         *pgroup;
    ASGCLIENT   *pasgclient;

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
    ASGRULE     *pasgrule;
    ASGMEMBER   *pasgmember;
    ASGCLIENT   *pasgclient;

    if(!asActive) return(S_asLib_asNotActive);
    pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
    while(pasgrule) {
        if ( pasgrule->ignore) goto next_rule;
        double  result = pasgrule->result;  /* set for VAL */
        long    status;

        if(pasgrule->calc && (pasg->inpChanged & pasgrule->inpUsed)) {
            status = calcPerform(pasg->pavalue,&result,pasgrule->rpcl);
            if(status) {
                pasgrule->result = 0;
                errMessage(status,"asComputeAsg");
            } else {
                pasgrule->result = ((result>.99) && (result<1.01)) ? 1 : 0;
            }
        }

next_rule:
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
    asAccessRights      access=asNOACCESS;
    int                 trapMask=0;
    ASGCLIENT           *pasgclient = asClientPvt;
    ASGMEMBER           *pasgMember;
    ASG                 *pasg;
    ASGRULE             *pasgrule;
    asAccessRights      oldaccess;
    GPHENTRY            *pgphentry;

    if(!asActive) return(S_asLib_asNotActive);
    if(!pasgclient) return(S_asLib_badClient);
    pasgMember = pasgclient->pasgMember;
    if(!pasgMember) return(S_asLib_badMember);
    pasg = pasgMember->pasg;
    if(!pasg) return(S_asLib_badAsg);
    oldaccess=pasgclient->access;
    pasgrule = (ASGRULE *)ellFirst(&pasg->ruleList);
    while(pasgrule) {
        if (pasgrule->ignore) goto next_rule;
        if(access == asWRITE) break;
        if(access>=pasgrule->access) goto next_rule;
        if(pasgclient->level > pasgrule->level) goto next_rule;
        /*if uagList is empty then no need to check uag*/
        if(ellCount(&pasgrule->uagList)>0){
            ASGUAG      *pasguag;
            UAG         *puag;

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
            ASGHAG      *pasghag;
            HAG         *phag;

            pasghag = (ASGHAG *)ellFirst(&pasgrule->hagList);
            while(pasghag) {
                if((phag = pasghag->phag)) {
                    pgphentry=gphFind(pasbase->phash,pasgclient->host,phag);
                    if(pgphentry) goto check_calc;
                }
                pasghag = (ASGHAG *)ellNext(&pasghag->node);
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
    UAG         *puag;
    UAGNAME     *puagname;
    HAG         *phag;
    HAGNAME     *phagname;
    ASG         *pasg;
    ASGINP      *pasginp;
    ASGRULE     *pasgrule;
    ASGHAG      *pasghag;
    ASGUAG      *pasguag;
    void        *pnext;

    if(!pasbase)
        return;

    asHagStateDestroy(pasbase);

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
    UAG         *pprev;
    UAG         *pnext;
    UAG         *puag;
    int         cmpvalue;
    ASBASE      *pasbase = (ASBASE *)pasbasenew;

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
    UAGNAME     *puagname;

    if(!puag) return(0);
    puagname = asCalloc(1,sizeof(UAGNAME)+strlen(user)+1);
    puagname->user = (char *)(puagname+1);
    strcpy(puagname->user,user);
    ellAdd(&puag->list,&puagname->node);
    return(0);
}

static HAG *asHagAdd(const char *hagName)
{
    HAG         *pprev;
    HAG         *pnext;
    HAG         *phag;
    int         cmpvalue;
    ASBASE      *pasbase = (ASBASE *)pasbasenew;

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
        char address[AS_HAG_IP_BUFSIZE];

        if(asHagParseNumeric(host, &addr) == 0) {
            asHagFormatAddress(&addr, address, sizeof(address));
            phagname = asCalloc(1, sizeof(*phagname) + sizeof(address) - 1u);
            epicsSnprintf(phagname->host, sizeof(address), "%s", address);
        } else {
            ASHAGBASESTATE *state = asHagStateFind(pasbasenew);
            ASHAGENTRYSTATE *entry;
            size_t hostLength = epicsStrnLen(host, AS_HAG_SOURCE_MAX);
            size_t unresolvedSize;
            size_t capacity;
            epicsUInt64 now = asHagNow();
            int resolved = asHagResolve(host, 0, &addr) == 0;

            if(hostLength == AS_HAG_SOURCE_MAX) {
                errlogPrintf("ACF: HAG host name exceeds %u bytes\n",
                    AS_HAG_SOURCE_MAX - 1u);
                return -1;
            }
            unresolvedSize = sizeof(AS_HAG_UNRESOLVED) + hostLength;
            capacity = unresolvedSize > AS_HAG_IP_BUFSIZE
                     ? unresolvedSize : AS_HAG_IP_BUFSIZE;

            phagname = asCalloc(1, sizeof(*phagname) + capacity - 1u);
            entry = asCalloc(1, sizeof(*entry));
            entry->group = phag;
            entry->name = phagname;
            entry->source = epicsStrDup(host);
            entry->capacity = capacity;
            entry->index = state ? (unsigned)ellCount(&state->entries) : 0u;
            entry->due = asHagAfter(now, resolved ? AS_HAG_SUCCESS_INTERVAL
                                                  : AS_HAG_FAILURE_INTERVAL);
            if(state)
                ellAdd(&state->entries, &entry->node);
            if(resolved) {
                asHagFormatAddress(&addr, address, sizeof(address));
                asHagSetEntry(entry, 1, address);
            } else {
                errlogPrintf("ACF: Unable to resolve host '%s'\n", host);
                asHagSetEntry(entry, 0, NULL);
            }
            asHagScheduleNext(state);
        }
    }
    ellAdd(&phag->list, &phagname->node);
    return 0;
}

static ASG *asAsgAdd(const char *asgName)
{
    ASG         *pprev;
    ASG         *pnext;
    ASG         *pasg;
    int         cmpvalue;
    ASBASE      *pasbase = (ASBASE *)pasbasenew;

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
    ASGINP      *pasginp;

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
    ASGRULE     *pasgrule;

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
    ASGUAG      *pasguag;
    UAG         *puag;
    ASBASE      *pasbase = (ASBASE *)pasbasenew;

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
    ASGHAG      *pasghag;
    HAG         *phag;
    ASBASE      *pasbase = (ASBASE *)pasbasenew;

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

/**
 * @brief Disable a rule if it contains unsupported elements
 * @param pasgrule the rule to disable
 * @return Non-zero if rule was not disabled
 */
static long asAsgRuleDisable(ASGRULE *pasgrule) {
    if (!pasgrule) return 1;
    pasgrule->ignore = 1;
    return 0;
}
