/* asLib.h */
/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file asLib.h
 * \brief An API to manage access security.
 * \author  Marty Kraimer
 * \date 09-27-93
 *
 * Several data structures and function prototypes are defined to manage access
 * security.
 */
#ifndef INCasLibh
#define INCasLibh

#include "libComAPI.h"
#include "ellLib.h"
#include "errMdef.h"
#include "errlog.h"

#ifdef __cplusplus
extern "C" {
#endif

 /** \brief Check the client's IP address during the access security checks.
  * 1: the actual client IP address will be used to perform the access checks
  * 0: the client-provided host name string will be used instead, HAG() are resolved to IPs at ACF load time.*/
LIBCOM_API extern int asCheckClientIP;

/** \brief Pointer to a structure representing a ASGMember. */
typedef struct asgMember *ASMEMBERPVT;
/** \brief Pointer to a structure representing a ASGClient. */
typedef struct asgClient *ASCLIENTPVT;
/** \brief A function pointer type, used as input function for asInitialize and the parser to provide the configuration content of the asLibrary. */
typedef int (*ASINPUTFUNCPTR)(char *buf,int max_size);
/** \brief Defines the possible status values that can be passed to the ASCLIENTCALLBACK function. For now its just "Change of access rights*" */
typedef enum{
    asClientCOAR
} asClientStatus;
/** \brief A function pointer type for a callback function that is called when a security client's access rights change. */
typedef void (*ASCLIENTCALLBACK) (ASCLIENTPVT,asClientStatus);

/** \brief Convenience macro - syntax: "long asCheckGet(ASCLIENTPVT asClientPvt);" */
#define asCheckGet(asClientPvt) \
    (!asActive || ((asClientPvt)->access >= asREAD))
/** \brief Convenience macro - syntax: "long asCheckPut(ASCLIENTPVT asClientPvt);" */
#define asCheckPut(asClientPvt) \
    (!asActive || ((asClientPvt)->access >= asWRITE))
/** \brief Convenience macro - syntax: "void *asTrapWriteWithData(ASCLIENTPVT asClientPvt,
     const char *userid, const char *hostid, void *addr,
     int dbrType, int no_elements, void *data);" */
#define asTrapWriteWithData(asClientPvt, user, host, addr, type, count, data) \
    ((asActive && (asClientPvt)->trapMask) \
    ? asTrapWriteBeforeWithData((user), (host), (addr), (type), (count), (data)) \
    : 0)
/** \brief Convenience macro - syntax: "void asTrapWriteAfter(ASCLIENTPVT asClientPvt);" */
#define asTrapWriteAfter(pvt) \
    if (pvt) asTrapWriteAfterWrite(pvt)
/** \brief This macro is for backwards compatibility, upgrade any code calling it to use asTrapWriteWithData() instead ASAP -
     syntax: void *asTrapWriteBefore(ASCLIENTPVT asClientPvt,
             const char *userid, const char *hostid, void *addr);
*/
#define asTrapWriteBefore(asClientPvt, user, host, addr) \
    asTrapWriteWithData(asClientPvt, user, host, addr, 0, 0, NULL)

/** \brief Initializes the access security library and specifies a callback function for obtaining input. */
LIBCOM_API long epicsStdCall asInitialize(ASINPUTFUNCPTR inputfunction);
/** \brief Initialize the access security library using an ACF from a file. */
LIBCOM_API long epicsStdCall asInitFile(
    const char *filename, const char *substitutions);
/** \brief Initialize the access security library using an ACF from a file pointer. */
LIBCOM_API long epicsStdCall asInitFP(FILE *fp,const char *substitutions);
/** \brief Initialize the access security library using an ACF from a memory buffer. */
LIBCOM_API long epicsStdCall asInitMem(const char *acf, const char *substitutions);
/** \brief This routine adds a new member to ASG asgName.
 * \warning caller must provide permanent storage for asgName */
LIBCOM_API long epicsStdCall asAddMember(
    ASMEMBERPVT *asMemberPvt,const char *asgName);
/** \brief This routine removes a member from an access control group. */
LIBCOM_API long epicsStdCall asRemoveMember(ASMEMBERPVT *asMemberPvt);
/** \brief CThis routine changes the group for an existing member. The access rights of all clients of the member are recomputed.
 * \warning caller must provide permanent storage for newAsgName */
LIBCOM_API long epicsStdCall asChangeGroup(
    ASMEMBERPVT *asMemberPvt,const char *newAsgName);
/** \brief For each member, the access system keeps a pointer that can be used by the caller. This routine returns the value of the pointer. */
LIBCOM_API void * epicsStdCall asGetMemberPvt(ASMEMBERPVT asMemberPvt);
/** \brief This routine is used to set the pointer returned by asGetMemberPvt. */
LIBCOM_API void epicsStdCall asPutMemberPvt(
    ASMEMBERPVT asMemberPvt,void *userPvt);
/** \brief This routine adds a client to an ASG member. The client must provide permanent storage for user and host. */
LIBCOM_API long epicsStdCall asAddClient(
    ASCLIENTPVT *asClientPvt,ASMEMBERPVT asMemberPvt,
    int asl,const char *user,char *host);
/** \brief This routine changes one or more of the values asl, user, and host for an existing client. The client must provide permanent storage for user and host. */
LIBCOM_API long epicsStdCall asChangeClient(
    ASCLIENTPVT asClientPvt,int asl,const char *user,char *host);
/** \brief This call removes a client. */
LIBCOM_API long epicsStdCall asRemoveClient(ASCLIENTPVT *asClientPvt);
/** \brief For each client, the access system keeps a pointer that can be used by the caller. This routine returns the value of the pointer. */
LIBCOM_API void * epicsStdCall asGetClientPvt(ASCLIENTPVT asClientPvt);
/** \brief TThis routine is used to set the pointer returned by asGetClientPvt. */
LIBCOM_API void epicsStdCall asPutClientPvt(
    ASCLIENTPVT asClientPvt,void *userPvt);
/** \brief This routine registers a callback that will be called whenever the access privilege of the client changes. */
LIBCOM_API long epicsStdCall asRegisterClientCallback(
    ASCLIENTPVT asClientPvt, ASCLIENTCALLBACK pcallback);
/** \brief This routine calls asComputeAsg for each access security group. */
LIBCOM_API long epicsStdCall asComputeAllAsg(void);
/** \brief This routine computes the access rights of a client.
 * After ASG declared: LIBCOM_API long epicsStdCall asComputeAsg(ASG *pasg); */
LIBCOM_API long epicsStdCall asCompute(ASCLIENTPVT asClientPvt);
/** \brief These routine print the current access security database.*/
LIBCOM_API int epicsStdCall asDump(
    void (*memcallback)(ASMEMBERPVT,FILE *),
    void (*clientcallback)(ASCLIENTPVT,FILE *),int verbose);
/** \brief These routine print the current access security database.*/
LIBCOM_API int epicsStdCall asDumpFP(FILE *fp,
    void (*memcallback)(ASMEMBERPVT,FILE *),
    void (*clientcallback)(ASCLIENTPVT,FILE *),int verbose);
/** \brief These routines display the speciﬁed UAG or if uagname is NULL each UAG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpUag(const char *uagname);
/** \brief These routines display the speciﬁed UAG or if uagname is NULL each UAG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpUagFP(FILE *fp,const char *uagname);
/** \brief These routines display the speciﬁed HAG or if hagname is NULL each HAG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpHag(const char *hagname);
/** \brief These routines display the speciﬁed HAG or if hagname is NULL each HAG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpHagFP(FILE *fp,const char *hagname);
/** \brief These routines display the rules for the speciﬁed ASG or if asgname is NULL the rules for each ASG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpRules(const char *asgname);
/** \brief These routines display the rules for the speciﬁed ASG or if asgname is NULL the rules for each ASG deﬁned in the access security database. */
LIBCOM_API int epicsStdCall asDumpRulesFP(FILE *fp,const char *asgname);
/** \brief This routine displays the member and, if clients is TRUE, client information for the speciﬁed ASG or if asgname is NULL the member and client information for each ASG deﬁned in the access security database. It also calls memcallback for each member if this argument is not NULL. */
LIBCOM_API int epicsStdCall asDumpMem(const char *asgname,
    void (*memcallback)(ASMEMBERPVT,FILE *),int clients);
/** \brief This routine displays the member and, if clients is TRUE, client information for the speciﬁed ASG or if asgname is NULL the member and client information for each ASG deﬁned in the access security database. It also calls memcallback for each member if this argument is not NULL. */
LIBCOM_API int epicsStdCall asDumpMemFP(FILE *fp,const char *asgname,
    void (*memcallback)(ASMEMBERPVT,FILE *),int clients);
/** \brief These show the contents of the hash table used to locate UAGs and HAGs, */
LIBCOM_API int epicsStdCall asDumpHash(void);
/** \brief These show the contents of the hash table used to locate UAGs and HAGs, */
LIBCOM_API int epicsStdCall asDumpHashFP(FILE *fp);
/** \brief These routines must be called before any write performed for a client, to permit any registered listeners to be notiﬁed.*/
LIBCOM_API void * epicsStdCall asTrapWriteBeforeWithData(
    const char *userid, const char *hostid, void *addr,
    int dbrType, int no_elements, void *data);
/** \brief These routines must be called after any write performed for a client, to permit any registered listeners to be notiﬁed.*/
LIBCOM_API void epicsStdCall asTrapWriteAfterWrite(void *pvt);

#define S_asLib_clientsExist    (M_asLib| 1) /**< \brief Client Exists*/
#define S_asLib_noUag           (M_asLib| 2) /**< \brief User Access Group does not exist*/
#define S_asLib_noHag           (M_asLib| 3) /**< \brief Host Access Group does not exist*/
#define S_asLib_noAccess        (M_asLib| 4) /**< \brief access security: no access allowed*/
#define S_asLib_noModify        (M_asLib| 5) /**< \brief access security: no modification allowed*/
#define S_asLib_badConfig       (M_asLib| 6) /**< \brief access security: bad configuration file*/
#define S_asLib_badCalc         (M_asLib| 7) /**< \brief access security: bad calculation espression*/
#define S_asLib_dupAsg          (M_asLib| 8) /**< \brief Duplicate Access Security Group */
#define S_asLib_InitFailed      (M_asLib| 9) /**< \brief access security: Init failed*/
#define S_asLib_asNotActive     (M_asLib|10) /**< \brief access security is not active*/
#define S_asLib_badMember       (M_asLib|11) /**< \brief access security: bad ASMEMBERPVT*/
#define S_asLib_badClient       (M_asLib|12) /**< \brief access security: bad ASCLIENTPVT*/
#define S_asLib_badAsg          (M_asLib|13) /**< \brief access security: bad ASG*/
#define S_asLib_noMemory        (M_asLib|14) /**< \brief access security: no Memory */

/** \brief Private declarations */
LIBCOM_API extern int asActive;

/** \brief Definition of access rights */
typedef enum{asNOACCESS,asREAD,asWRITE} asAccessRights;

struct gphPvt;

/** \brief Contains the list head for lists of UAGs, HAGs, and ASGs, so the Base pointers for access security */
typedef struct asBase{
    ELLLIST         uagList;
    ELLLIST         hagList;
    ELLLIST         asgList;
    struct gphPvt   *phash;
} ASBASE;

LIBCOM_API extern volatile ASBASE *pasbase;

/** \brief Defs for User Access Groups */
typedef struct{
    ELLNODE         node;
    char            *user;
} UAGNAME;
typedef struct uag{
    ELLNODE         node;
    char            *name;
    ELLLIST         list;   /**< \brief list of UAGNAME */
} UAG;
/** \brief Defs for Host Access Groups */
typedef struct{
    ELLNODE         node;
    char            host[1];
} HAGNAME;
typedef struct hag{
    ELLNODE         node;
    char            *name;
    ELLLIST         list;   /**< \brief list of HAGNAME */
} HAG;
/** \brief Defs for Access SecurityGroups */
typedef struct {
    ELLNODE         node;
    UAG             *puag;
}ASGUAG;
typedef struct {
    ELLNODE         node;
    HAG             *phag;
}ASGHAG;
#define AS_TRAP_WRITE 1
/** \brief Contains the information for a rule. */
typedef struct{
    ELLNODE         node;
    asAccessRights  access;
    int             level;
    unsigned long   inpUsed; /**< \brief bitmap of which inputs are used */
    int             result;  /**< \brief Result of calc converted to TRUE/FALSE */
    char            *calc;
    void            *rpcl;
    ELLLIST         uagList; /**< \brief List of ASGUAG */
    ELLLIST         hagList; /**< \brief List of ASGHAG */
    int             trapMask;
} ASGRULE;
/** \brief Contains the information for an INPx. */
typedef struct{
    ELLNODE         node;
    char            *inp;
    void            *capvt;
    struct asg      *pasg;
    int             inpIndex;
}ASGINP;

/** \brief An access secuity group. It contains the list head for ASGINPs, ASGRULEs, and ASGMEMBERs */
typedef struct asg{
    ELLNODE         node;
    char            *name;
    ELLLIST         inpList;
    ELLLIST         ruleList;
    ELLLIST         memberList;
    double          *pavalue;   /**< \brief pointer to array of input values */
    unsigned long   inpBad;     /**< \brief bitmap of which inputs are bad */
    unsigned long   inpChanged; /**< \brief bitmap of inputs that changed */
} ASG;
/** \brief Contains the information for a member of an access secururity group. It contains the list head for ASGCLIENTs. */
typedef struct asgMember {
    ELLNODE         node;
    ASG             *pasg;
    ELLLIST         clientList;
    const char      *asgName;
    void            *userPvt;
} ASGMEMBER;
/** \brief AContains the information of a client of an access securitz group. */
typedef struct asgClient {
    ELLNODE         node;
    ASGMEMBER       *pasgMember;
    const char      *user;
    char            *host;
    void            *userPvt;
    ASCLIENTCALLBACK pcallback;
    int             level;
    asAccessRights  access;
    int             trapMask;
} ASGCLIENT;

/** \brief This routine calculates all CALC entries for the ASG and calls asCompute for each client of each member of the speciﬁed access security group. */
LIBCOM_API long epicsStdCall asComputeAsg(ASG *pasg);
/** \brief part of is "friend" function */
LIBCOM_API void * epicsStdCall asCalloc(size_t nobj,size_t size);
/** \brief part of is "friend" function */
LIBCOM_API char * epicsStdCall asStrdup(unsigned char *str);
/** \brief part of is "friend" function */
LIBCOM_API void asFreeAll(ASBASE *pasbase);
#ifdef __cplusplus
}
#endif

#endif /*INCasLibh*/
