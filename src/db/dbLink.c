/* dbLink.c */
/* base/src/db $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            11-7-90
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
 * .01  07-26-91	mrk	Allow choices to be retrieved as numeric
 * .02  08-13-91	mrk	Support db_field_log for dbGetField
 * .03  09-30-91	mrk	Support for TPRO and DISP
 * .04  10-03-91	jba	Bug fix in putStringUchar
 * .05  11-06-91	jba	Moved processing message before record process
 * .06  11-26-91	jba	Added return to dbGetLink
 *              		Fixed bug in special processing of SPC_MOD (100) 
 * .07  12-02-91	jba	Writing to PROC will always force record process
 * .08  02-05-92	jba	Changed function arguments from paddr to precord 
 * .09  03-02-92	jba	Added function dbValueSize to replace db_value_size
 * .10  04-17-92	rcz	put in mrk's dbNameToAddr changes for dbBase
 * .11  05-18-92	mrk	Changes for database internal structures
 * .12  07-16-92	jba	Added disable alarm severity, ansi c changes
 * .13  08-05-92	jba	Removed all references to dbr_field_type
 * .14  09-18-92	jba	replaced get of disa code with recGblGetLinkValue call
 * .15  07-15-93	mrk	Changes for new dbStaticLib
 * .16  02-02-94	mrk	added dbPutNotify and caching
 * .17  02-02-94	mrk	added init code for tsel
 */


/*
 * dbGetLink			Get via a database link
 * dbPutLink			Put via a link
 * dbGetField			Get from outside database
 * dbPutField			Put from outside database
 * dbGet			Common get routine
 * dbPut			Common put routine
 *
 * dbBufferSize			Compute buffer size
 * dbValueSizeA			Compute size for a field
 */

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<stdio.h>
#include	<string.h>
#include	<taskLib.h>
#include	<vxLib.h>
#include	<tickLib.h>

#include	<fast_lock.h>
#include	<cvtFast.h>
#include	<alarm.h>
#include	<choice.h>
#include	<dbDefs.h>
#include	<dbBase.h>
#include	<dbAccess.h>
#include	<dbStaticLib.h>
#include	<dbScan.h>
#include	<dbCommon.h>
#include	<dbFldTypes.h>
#include 	<dbRecDes.h>
#include 	<dbRecType.h>
#include	<db_field_log.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>

extern struct dbBase *pdbBase;

/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();
long dbCommonInit();


long dbGetField(
struct dbAddr	*paddr,
short		dbrType,
void		*pbuffer,
long		*options,
long		*nRequest,
void		*pflin
)
{
	struct dbCommon *precord = (struct dbCommon *)(paddr->precord);
	long		status;

	dbScanLock(precord);
	status = dbGet(paddr,dbrType,pbuffer,options,nRequest,pflin);
	dbScanUnlock(precord);
	return(status);
}

long dbGetLink(
	struct db_link	*pdblink,
	struct dbCommon	*pdest,
	short  		dbrType,
	void		*pbuffer,
	long		*options,
	long		*nRequest
)
{
	struct dbAddr	*paddr=(struct dbAddr*)(pdblink->pdbAddr);
	struct dbCommon *psource=paddr->precord;
	long	status;

	if(pdblink->process_passive && psource->scan==0) {
		status=dbScanPassive(pdest,psource);
		if(status) return(status);
	}
	if(pdblink->maximize_sevr) recGblSetSevr(pdest,LINK_ALARM,psource->sevr);
	
	status= dbGet(paddr,dbrType,pbuffer,options,nRequest,NULL);
	if(status) recGblRecordError(status,(void *)pdest,"dbGetLink");
        return(status);
}

long dbPutLink(
	struct db_link	*pdblink,
	struct dbCommon	*psource,
	short		dbrType,
	void   		*pbuffer,
	long		nRequest
)
{
	struct dbAddr	*paddr=(struct dbAddr*)(pdblink->pdbAddr);
	struct dbCommon *pdest=paddr->precord;
	long	status;

	status=dbPut(paddr,dbrType,pbuffer,nRequest);
	if(pdblink->maximize_sevr) recGblSetSevr(pdest,LINK_ALARM,psource->sevr);
	if(status) return(status);

        if((paddr->pfield==(void *)&pdest->proc)
	|| (pdblink->process_passive && pdest->scan==0)) {
	    /*Note: If ppn then dbNotifyCancel will handle reprocessing*/
	    /*if dbPutField caused asyn record to process ask for reprocessing*/
	    if(!psource->ppn && pdest->putf) pdest->rpro = TRUE;
	    /* otherwise ask for the record to be processed*/
	    else status=dbScanLink(psource,pdest);
	}
	return(status);
}

long dbPutField(
       struct dbAddr   *paddr,
       short           dbrType,
       void            *pbuffer,
       long            nRequest
)
{
	long	status;
	struct fldDes *pfldDes=(struct fldDes *)(paddr->pfldDes);
	struct dbCommon *precord = (struct dbCommon *)(paddr->precord);

	/*check for putField disabled*/
	if(precord->disp) {
		if((void *)(&precord->disp) != paddr->pfield) return(0);
	}
	dbScanLock(precord);
	status=dbPut(paddr,dbrType,pbuffer,nRequest);
	if(status) recGblDbaddrError(status,paddr,"dbPutField");
	if(status==0){
        	if((paddr->pfield==(void *)&precord->proc)
		||(pfldDes->process_passive && precord->scan==0)) {
		    if(precord->pact) {
			precord->rpro = TRUE;
		    } else {
			/*indicate that dbPutField called dbProcess*/
			precord->putf = TRUE;
			status=dbProcess(precord);
		    }
		}
	}
	dbScanUnlock(precord);
	return(status);
}

static void notifyCallback(CALLBACK *pcallback)
{
    PUTNOTIFY	*ppn=NULL;
    long	status;

    callbackGetUser(ppn,pcallback);
    if(ppn->cmd==notifyCmdRepeat) {
	status = dbPutNotify(ppn);
    } else if(ppn->cmd==notifyCmdCallUser) {
	(ppn->userCallback)(ppn);
    } else {/*illegal request*/
	recGblRecordError(-1,ppn->paddr->precord,"dbNotifyCompletion: illegal callback request");
    }
}

static void notifyCancel(PUTNOTIFY *ppn)
{
    struct dbCommon *precord = ppn->list;

    while(precord) {
	void	*pnext;

	if(precord->rpro) {
	    precord->rpro = FALSE;
	    scanOnce(precord);
	}
	precord->ppn = NULL;
	pnext = precord->ppnn;
	precord->ppnn = NULL;
	precord = pnext;
    }
    ppn->list = NULL;
}

void dbNotifyCancel(PUTNOTIFY *ppn)
{
    struct dbCommon *precord = ppn->list;

    dbScanLock(precord);
    notifyCancel(ppn);
    dbScanUnlock(precord);
}

long dbPutNotify(PUTNOTIFY *ppn)
{
    struct dbAddr *paddr = ppn->paddr;
    short         dbrType = ppn->dbrType;
    void          *pbuffer = ppn->pbuffer;
    long          nRequest = ppn->nRequest;
    long	  status=0;
    struct fldDes *pfldDes=(struct fldDes *)(paddr->pfldDes);
    struct dbCommon *precord = (struct dbCommon *)(paddr->precord);

    callbackSetCallback(notifyCallback,&ppn->callback);
    callbackSetUser(ppn,&ppn->callback);
    callbackSetPriority(priorityLow,&ppn->callback);
    /*check for putField disabled*/
    if(precord->disp) {
	if((void *)(&precord->disp) != paddr->pfield) {
	    ppn->cmd = notifyCmdCallUser;
	    ppn->status = S_db_putDisabled;
	    notifyCallback(&ppn->callback);
	    return(S_db_putDisabled);
	}
    }
    dbScanLock(precord);
    status=dbPut(paddr,dbrType,pbuffer,nRequest);
    if(status) recGblDbaddrError(status,paddr,"dbPutField");
    ppn->status = status;
    if(status==0){
       	if((paddr->pfield==(void *)&precord->proc)
	||(pfldDes->process_passive && precord->scan==0)) {
	    if(precord->ppn) {
		/*record already has attached ppn. Blocked*/

		ppn->status = status = S_db_Blocked;
		return(status);
	    }
	    ppn->nwaiting = 1;
	    ppn->rescan = FALSE;
	    ppn->list = NULL;
	    precord->ppn = ppn;
	    precord->ppnn = NULL;
	    if(precord->pact) {/*blocked wait for dbNotifyCompletion*/
		ppn->rescan = TRUE;
		ppn->status = status = S_db_Pending;
		return(status);
	    }
	    status=dbProcess(precord);
	    if(status==0) {
		if(!precord->pact) {
		    precord->ppn = NULL;
		} else {
		    precord->ppnn = ppn->list;
		    ppn->list = precord;
		}
		ppn->status = status = ((ppn->nwaiting == 0) ? 0 : S_db_Pending);
	    } else {
		ppn->status = status;
		notifyCancel(ppn);
	    }
	} else { /*Make callback immediately*/
	    ppn->cmd = notifyCmdCallUser;
	    ppn->status = 0;
	    notifyCallback(&ppn->callback);
	}
    }
    dbScanUnlock(precord);
    return(status);
}

void dbNotifyCompletion(PUTNOTIFY *ppn)
{

    if(ppn->status!=0 && ppn->status!=S_db_Pending) {
	ppn->cmd = notifyCmdCallUser;
	notifyCancel(ppn);
	callbackRequest(&ppn->callback);
	return;
    }
    /*decrement number of records being waited on*/
    if(ppn->nwaiting<=0) {
	recGblRecordError(-1,ppn->paddr->precord,"dbNotifyCompletion: nwaiting<-0 LOGIC");
	return;
    }
    if(--ppn->nwaiting == 0) {/*original request completed*/
	notifyCancel(ppn);
	if(ppn->rescan) {
	    ppn->cmd = notifyCmdRepeat;
	    callbackRequest(&ppn->callback);
	} else {
	    /*issue completion callback*/
	    ppn->cmd = notifyCmdCallUser;
	    if(ppn->status==S_db_Pending) ppn->status = 0;
	    callbackRequest(&ppn->callback);
	}
    }
}

long dbValueSize(
	short     dbr_type
)
{
     /* sizes for value associated with each DBR request type */
     static long size[] = {
        MAX_STRING_SIZE,                /* STRING       */
        sizeof(char),                   /* CHAR         */
        sizeof(unsigned char),          /* UCHAR        */
        sizeof(short),                  /* SHORT        */
        sizeof(unsigned short),         /* USHORT       */
        sizeof(long),                   /* LONG         */
        sizeof(unsigned long),          /* ULONG        */
        sizeof(float),                  /* FLOAT        */
        sizeof(double),                 /* DOUBLE       */
        sizeof(unsigned short)};        /* ENUM         */

     return(size[dbr_type]);
}

long dbBufferSize(
     short     dbr_type,
     long      options,
     long      no_elements
)
{
    long nbytes=0;

    nbytes += dbValueSize(dbr_type) * no_elements;
    if(options & DBR_STATUS)	nbytes += dbr_status_size;
    if(options & DBR_UNITS)	nbytes += dbr_units_size;
    if(options & DBR_PRECISION) nbytes += dbr_precision_size;
    if(options & DBR_TIME)	nbytes += dbr_time_size;
    if(options & DBR_ENUM_STRS)	nbytes += dbr_enumStrs_size;
    if(options & DBR_GR_LONG)	nbytes += dbr_grLong_size;
    if(options & DBR_GR_DOUBLE)	nbytes += dbr_grDouble_size;
    if(options & DBR_CTRL_LONG) nbytes += dbr_ctrlLong_size;
    if(options & DBR_CTRL_DOUBLE)nbytes += dbr_ctrlDouble_size;
    if(options & DBR_AL_LONG)   nbytes += dbr_alLong_size;
    if(options & DBR_AL_DOUBLE) nbytes += dbr_alDouble_size;
    return(nbytes);
}

/* DATABASE ACCESS GET CONVERSION SUPPORT */

static long getStringString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *psrc=paddr->pfield;
    short size=paddr->field_size;
    short sizeto;

    /* always force result string to be null terminated*/
    sizeto = size;
    if(sizeto>=MAX_STRING_SIZE) sizeto = MAX_STRING_SIZE-1;

    if(nRequest==1 && offset==0) {
	strncpy(pbuffer,psrc,sizeto);
	*(pbuffer+sizeto) = 0;
	return(0);
    }
    psrc+= (size*offset);
    while (nRequest) {
        strncpy(pbuffer,psrc,sizeto);
	*(pbuffer+sizeto) = 0;
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=paddr->pfield;
	else
    		psrc  += size;
	nRequest--;
    }
    return(0);
}

static long getCharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtCharToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtCharToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getCharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getCharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getCharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char *psrc=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUcharToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUcharToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned char *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUcharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUcharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUcharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *psrc=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtShortToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtShortToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getShortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getShortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getShortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getShortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short *psrc=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUshortToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUshortToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned short *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUshortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
static long getUshortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUshortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUshortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtLongToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtLongToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(long *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getLongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getLongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getLongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long *psrc=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	cvtUlongToString(*psrc,pbuffer);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtUlongToString(*psrc,pbuffer);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(unsigned long *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getUlongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getUlongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getUlongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long *psrc=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float	*psrc=(float *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbGet","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	cvtFloatToString(*psrc,pbuffer,precision);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtFloatToString(*psrc,pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(float *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getFloatChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);
    long  ltemp;	/*vxWorks does not support float to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *psrc;
	*pbuffer = ltemp;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ltemp = *psrc++;
	*pbuffer++ = ltemp;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getFloatDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getFloatEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float *psrc=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbGet","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	cvtDoubleToString(*psrc,pbuffer,precision);
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	cvtDoubleToString(*psrc,pbuffer,precision);
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		psrc=(double *)paddr->pfield;
	else
    		psrc++;
	nRequest--;
    }
    return(0);
}

static long getDoubleChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);
    long   ltemp; /*vxWorks does not support double to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *psrc;
	*pbuffer = ltemp;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	ltemp = *psrc++;
	*pbuffer++ = ltemp;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getDoubleDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getDoubleEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double *psrc=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    struct rset	*prset;
    short	record_type=(paddr->record_type);
    long	status;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->get_enum_str))
        return( (*prset->get_enum_str)(paddr,pbuffer) );
    status=S_db_noRSET;
    recGblRecSupError(status,paddr,"dbGet","get_enum_str");
    return(S_db_badDbrtype);
}

static long getEnumChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long getEnumDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getEnumEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short *psrc=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pbuffer = *psrc;
	return(0);
    }
    psrc += offset;
    while (nRequest) {
	*pbuffer++ = *psrc++;
	if(++offset==no_elements) psrc=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long getGchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short		choice_set=paddr->choice_set;
    short		choice_ind= *((short*)paddr->pfield);
    char		*pchoice;
    struct choiceSet	*pchoiceSet;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getGchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(pchoiceSet=GET_PCHOICE_SET(pdbBase->pchoiceGbl,choice_set)))
    || (!(pchoice=GET_CHOICE(pchoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getGchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}

static long getCchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short	choice_ind= *((short*)paddr->pfield);
    char	*pchoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getCchoiceString)");
        return(S_db_onlyOne);
    }
    if (!(pchoice=GET_CHOICE(pdbBase->pchoiceCvt,choice_ind))) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getCchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}


static long getRchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short      		choice_set=paddr->choice_set;
    short      		choice_ind= *((short*)paddr->pfield);
    struct choiceSet	*pchoiceSet;
    struct arrChoiceSet *parrChoiceSet;
    char       		*pchoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getRchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(parrChoiceSet=GET_PARR_CHOICE_SET(pdbBase->pchoiceRec,(paddr->record_type))))
    || (!(pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,choice_set)))
    || (!(pchoice=GET_CHOICE(pchoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getRchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pchoice,MAX_STRING_SIZE);
    return(0);
}


static long getDchoiceString(paddr,pbuffer,nRequest,no_elements,offset)
    struct dbAddr	*paddr;
    char		*pbuffer;
    long		nRequest;
    long		no_elements;
    long		offset;
{
    short      		choice_ind= *((short*)paddr->pfield);
    struct devChoiceSet *pdevChoiceSet;
    struct devChoice	*pdevChoice;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbGet(getDchoiceString)");
        return(S_db_onlyOne);
    }
    if((!(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev,paddr->record_type)))
    || (!(pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,choice_ind))) ) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbGet(getRchoiceString)");
        return(S_db_badChoice);
    }
    strncpy(pbuffer,pdevChoice->pchoice,MAX_STRING_SIZE);
    return(0);
}

/* This is the table of routines for converting database fields */
/* the rows represent the field type of the database field */
/* the columns represent the types of the buffer in which they are placed */

/* buffer types are********************************************************
 DBR_STRING,      DBR_CHR,         DBR_UCHAR,       DBR_SHORT,       DBR_USHORT,
 DBR_LONG,        DBR_ULONG,       DBR_FLOAT,       DBR_DOUBLE,      DBR_ENUM
 ***************************************************************************/

long (*get_convert_table[DBF_DEVCHOICE+1][DBR_ENUM+1])() = {

/* source is a DBF_STRING		*/
{getStringString, NULL,            NULL,            NULL,            NULL,
    NULL,         NULL,            NULL,            NULL,            NULL},
/* source is a DBF_CHAR 		*/
{getCharString,   getCharChar,     getCharUchar,    getCharShort,    getCharUshort,
 getCharLong,     getCharUlong,    getCharFloat,    getCharDouble,   getCharEnum},
/* source is a DBF_UCHAR		*/
{getUcharString,  getUcharChar,    getUcharUchar,   getUcharShort,   getUcharUshort,
 getUcharLong,    getUcharUlong,   getUcharFloat,   getUcharDouble,  getUcharEnum},
/* source is a DBF_SHORT		*/
{getShortString,  getShortChar,    getShortUchar,   getShortShort,   getShortUshort,
 getShortLong,    getShortUlong,   getShortFloat,   getShortDouble,  getShortEnum},
/* source is a DBF_USHORT		*/
{getUshortString, getUshortChar,   getUshortUchar,  getUshortShort,  getUshortUshort,
 getUshortLong,   getUshortUlong,  getUshortFloat,  getUshortDouble, getUshortEnum},
/* source is a DBF_LONG		*/
{getLongString,   getLongChar,     getLongUchar,    getLongShort,    getLongUshort,
 getLongLong,     getLongUlong,    getLongFloat,    getLongDouble,   getLongEnum},
/* source is a DBF_ULONG		*/
{getUlongString,  getUlongChar,    getUlongUchar,   getUlongShort,   getUlongUshort,
 getUlongLong,    getUlongUlong,   getUlongFloat,   getUlongDouble,  getUlongEnum},
/* source is a DBF_FLOAT		*/
{getFloatString,  getFloatChar,    getFloatUchar,   getFloatShort,   getFloatUshort,
 getFloatLong,    getFloatUlong,   getFloatFloat,   getFloatDouble,  getFloatEnum},
/* source is a DBF_DOUBLE		*/
{getDoubleString, getDoubleChar,   getDoubleUchar,  getDoubleShort,  getDoubleUshort,
 getDoubleLong,   getDoubleUlong,  getDoubleFloat,  getDoubleDouble, getDoubleEnum},
/* source is a DBF_ENUM		*/
{getEnumString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_GBLCHOICE	*/
{getGchoiceString,getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_CVTCHOICE	*/
{getCchoiceString,getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_RECCHOICE	*/
{getRchoiceString,getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_DEVCHOICE	*/
{getDchoiceString,getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumFloat,    getEnumDouble,   getEnumEnum}
};


/* forward references for private routines used by dbGet */
static void get_enum_strs(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options);
static void get_graphics(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options);
static void get_control(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options);
static void get_alarm(struct dbAddr *paddr,void	**ppbuffer,
	struct rset *prset,long	*options);

long dbGet(
struct dbAddr	*paddr,
short		dbrType,
void		*pbuffer,
long		*options,
long		*nRequest,
void		*pflin
)
{
	db_field_log	*pfl= (db_field_log *)pflin;
	long		no_elements=paddr->no_elements;
	long 		offset;
	struct rset	*prset;
	short		field_type=paddr->field_type;
	long		(*pconvert_routine)();
	struct dbCommon *pcommon;
	long		status;
	long		*perr_status=NULL;


	prset=GET_PRSET(pdbBase->precSup,paddr->record_type);

	if(!(*options)) goto GET_DATA;

	/* Process options */
	pcommon = (struct dbCommon *)(paddr->precord);
	if( (*options) & DBR_STATUS ) {
	    if(pfl!=NULL) {
		*((unsigned short *)pbuffer)++ = pfl->stat;
		*((unsigned short *)pbuffer)++ = pfl->sevr;
	    } else {
		*((unsigned short *)pbuffer)++ = pcommon->stat;
		*((unsigned short *)pbuffer)++ = pcommon->sevr;
	    }
	    *((unsigned short *)pbuffer)++ = pcommon->acks;
	    *((unsigned short *)pbuffer)++ = pcommon->ackt;
	    *perr_status = 0;
	}
	if( (*options) & DBR_UNITS ) {
	    if( prset && prset->get_units ){ 
		(*prset->get_units)(paddr,pbuffer);
	    } else {
		memset(pbuffer,'\0',dbr_units_size);
		*options = (*options) ^ DBR_UNITS; /*Turn off DBR_UNITS*/
	    }
	    pbuffer += dbr_units_size;
	}
	if( (*options) & DBR_PRECISION ) {
	    struct dbr_precision *pdbr_precision=( struct dbr_precision *)pbuffer;

	    if((field_type==DBF_FLOAT || field_type==DBF_DOUBLE) &&  prset && prset->get_precision ){ 
		(*prset->get_precision)(paddr,pbuffer);
		if(pdbr_precision->field_width<=0)
			pdbr_precision->field_width = pdbr_precision->precision + 5;
	    } else {
		memset(pbuffer,'\0',dbr_precision_size);
		*options = (*options) ^ DBR_PRECISION; /*Turn off DBR_PRECISION*/
	    }
	    pbuffer += dbr_precision_size;
	}
	if( (*options) & DBR_TIME ) {
	    if(pfl!=NULL) {
		*((unsigned long *)pbuffer)++ = pfl->time.secPastEpoch;
		*((unsigned long *)pbuffer)++ = pfl->time.nsec;
	    } else {
		*((unsigned long *)pbuffer)++ = pcommon->time.secPastEpoch;
		*((unsigned long *)pbuffer)++ = pcommon->time.nsec;
	    }
	}
	if( (*options) & DBR_ENUM_STRS ) get_enum_strs(paddr,&pbuffer,prset,options);
	if( (*options) & (DBR_GR_LONG|DBR_GR_DOUBLE ))
			get_graphics(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_CTRL_LONG | DBR_CTRL_DOUBLE ))
			get_control(paddr,&pbuffer,prset,options);
	if((*options) & (DBR_AL_LONG | DBR_AL_DOUBLE ))
			get_alarm(paddr,&pbuffer,prset,options);


GET_DATA:

        if(*nRequest==0) return(0);
	/* Check for valid request */
	if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVCHOICE) ){
		char message[80];

		sprintf(message,"dbGet - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		if(perr_status) *perr_status = S_db_badDbrtype;
		return(S_db_badDbrtype);
	}
	
	/* check for array			*/
	if( no_elements>1 && prset && (prset->get_array_info) ) {
		status = (*prset->get_array_info)(paddr,&no_elements,&offset);
	}
	else offset=0;
	if(no_elements<(*nRequest)) *nRequest = no_elements;
	if(!(pconvert_routine=get_convert_table[field_type][dbrType])) {
		char message[80];

		sprintf(message,"dbGet - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		if(perr_status) *perr_status = S_db_badDbrtype;
		return(S_db_badDbrtype);
	}
	/* convert database field to buffer type and place it in the buffer */
	if(pfl!=NULL) {
	    struct dbAddr	localAddr;

	    memcpy(&localAddr,paddr,sizeof(localAddr));
	    /*Use longest field size*/
	    localAddr.pfield = (char *)&pfl->field;
	    status=(*pconvert_routine)(&localAddr,pbuffer,*nRequest,
			no_elements,offset);
	} else {
	    status=(*pconvert_routine)(paddr,pbuffer,*nRequest,
			no_elements,offset);
	}
	if(perr_status) *perr_status = status;
        return(status);
}

static void get_enum_strs(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	short			field_type=paddr->field_type;
	struct choiceSet	*pchoiceSet;
	struct arrChoiceSet	*parrChoiceSet;
	struct devChoice	*pdevChoice;
	struct devChoiceSet	*pdevChoiceSet;
	unsigned long		no_str;
	char			*ptemp;
	struct dbr_enumStrs	*pdbr_enumStrs=(struct dbr_enumStrs*)(*ppbuffer);
	int			i;

	memset(pdbr_enumStrs,'\0',dbr_enumStrs_size);
	switch(field_type) { case DBF_ENUM: if( prset && prset->get_enum_strs ) {
			(*prset->get_enum_strs)(paddr,pdbr_enumStrs);
		    } else {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    }
		    break;
		case DBF_GBLCHOICE:
		    pchoiceSet=GET_PCHOICE_SET(pdbBase->pchoiceGbl,paddr->choice_set);
		    goto choice_common;
		case DBF_CVTCHOICE:
		    pchoiceSet=pdbBase->pchoiceCvt;
		    goto choice_common;
		case DBF_RECCHOICE:
		    parrChoiceSet=GET_PARR_CHOICE_SET(pdbBase->pchoiceRec,
			paddr->record_type);
		    pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,paddr->choice_set);
choice_common:
		    if(pchoiceSet==NULL) {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
			break;
		    }
		    i = sizeof(pdbr_enumStrs->strs)/sizeof(pdbr_enumStrs->strs[0]);
		    no_str=min(pchoiceSet->number,i);
		    pdbr_enumStrs->no_str = no_str;
		    ptemp = &(pdbr_enumStrs->strs[0][0]);
		    for (i=0; i<no_str; i++) {
			if(pchoiceSet->papChoice[i]==NULL) *ptemp=0;
			else {
			    strncpy(ptemp,pchoiceSet->papChoice[i],sizeof(pdbr_enumStrs->strs[0]));
			    *(ptemp+sizeof(pdbr_enumStrs->strs[0])-1) = 0;
			}
			ptemp += sizeof(pdbr_enumStrs->strs[0]);
		    }
		    break;
		case DBF_DEVCHOICE:
		    pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev,
			paddr->record_type);
		    if(pdevChoiceSet==NULL) {
			*options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
			break;
		    }
		    i = sizeof(pdbr_enumStrs->strs)/sizeof(pdbr_enumStrs->strs[0]);
		    no_str=min(pdevChoiceSet->number,i);
		    pdbr_enumStrs->no_str = no_str;
		    ptemp = &(pdbr_enumStrs->strs[0][0]);
		    for (i=0; i<no_str; i++) {
			pdevChoice=GET_DEV_CHOICE(pdevChoiceSet,i);
			if(pdevChoice==NULL || pdevChoice->pchoice==NULL)
			    *ptemp=0;
			else {
			    strncpy(ptemp,pdevChoice->pchoice,sizeof(pdbr_enumStrs->strs[0]));
			    *(ptemp+sizeof(pdbr_enumStrs->strs[0])-1) = 0;
			}
			ptemp += sizeof(pdbr_enumStrs->strs[0]);
		    }
		    break;
		default:
		    *options = (*options)^DBR_ENUM_STRS;/*Turn off option*/
		    break;
	}
	*ppbuffer += dbr_enumStrs_size;
	return;
}

static void get_graphics(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	struct			dbr_grDouble grd;
	int			got_data=FALSE;

	if( prset && prset->get_graphic_double ) {
		(*prset->get_graphic_double)(paddr,&grd);
		got_data=TRUE;
	}
	if( (*options) & (DBR_GR_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grLong *pgr=(struct dbr_grLong*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    memset(pbuffer,'\0',dbr_grLong_size);
		    *options = (*options) ^ DBR_GR_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_grLong_size;
	}
	if( (*options) & (DBR_GR_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_grDouble *pgr=(struct dbr_grDouble*)pbuffer;
		    pgr->upper_disp_limit = grd.upper_disp_limit;
		    pgr->lower_disp_limit = grd.lower_disp_limit;
		} else {
		    memset(pbuffer,'\0',dbr_grDouble_size);
		    *options = (*options) ^ DBR_GR_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_grDouble_size;
	}
	return;
}

static void get_control(struct dbAddr *paddr,void **ppbuffer,
	struct rset *prset,long	*options)
{
	struct dbr_ctrlDouble	ctrld;
	int			got_data=FALSE;

	if( prset && prset->get_control_double ) {
		(*prset->get_control_double)(paddr,&ctrld);
		got_data=TRUE;
	}
	if( (*options) & (DBR_CTRL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_ctrlLong *pctrl=(struct dbr_ctrlLong*)pbuffer;
		    pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		    pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    memset(pbuffer,'\0',dbr_ctrlLong_size);
		    *options = (*options) ^ DBR_CTRL_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_ctrlLong_size;
	}
	if( (*options) & (DBR_CTRL_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		   struct dbr_ctrlDouble *pctrl=(struct dbr_ctrlDouble*)pbuffer;
		   pctrl->upper_ctrl_limit = ctrld.upper_ctrl_limit;
		   pctrl->lower_ctrl_limit = ctrld.lower_ctrl_limit;
		} else {
		    memset(pbuffer,'\0',dbr_ctrlDouble_size);
		    *options = (*options) ^ DBR_CTRL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_ctrlDouble_size;
	}
	return;
}

static void get_alarm(struct dbAddr *paddr,void	**ppbuffer,
	struct rset *prset,long	*options)
{
	struct			dbr_alDouble ald;
	int			got_data=FALSE;

	if( prset && prset->get_alarm_double ) {
		(*prset->get_alarm_double)(paddr,&ald);
		got_data=TRUE;
	}
	if( (*options) & (DBR_AL_LONG) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alLong *pal=(struct dbr_alLong*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    memset(pbuffer,'\0',dbr_alLong_size);
		    *options = (*options) ^ DBR_AL_LONG; /*Turn off option*/
		}
		*ppbuffer += dbr_alLong_size;
	}
	if( (*options) & (DBR_AL_DOUBLE) ) {
		char	*pbuffer=*ppbuffer;

		if(got_data) {
		    struct dbr_alDouble *pal=(struct dbr_alDouble*)pbuffer;
		    pal->upper_alarm_limit = ald.upper_alarm_limit;
		    pal->upper_warning_limit = ald.upper_warning_limit;
		    pal->lower_warning_limit = ald.lower_warning_limit;
		    pal->lower_alarm_limit = ald.lower_alarm_limit;
		} else {
		    memset(pbuffer,'\0',dbr_alDouble_size);
		    *options = (*options) ^ DBR_AL_DOUBLE; /*Turn off option*/
		}
		*ppbuffer += dbr_alDouble_size;
	}
	return;
}

/* DATABASE ACCESS PUT CONVERSION SUPPORT */

static long putStringString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=paddr->pfield;
    short size=paddr->field_size;

    if(nRequest==1 && offset==0) {
	strncpy(pdest,pbuffer,size);
	*(pdest+size-1) = 0;
	return(0);
    }
    pdest+= (size*offset);
    while (nRequest) {
        strncpy(pdest,pbuffer,size);
	*(pdest+size-1) = 0;
	pbuffer += MAX_STRING_SIZE;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest  += size;
	nRequest--;
    }
    return(0);
}

static long putStringChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = (char)value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)paddr->pfield;
    unsigned short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = (unsigned char)value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)paddr->pfield;
    short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hd",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)paddr->pfield;
    unsigned short  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%hu",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)paddr->pfield;
    long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%ld",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)paddr->pfield;
    unsigned long  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%lu",&value) == 1) {
		*pdest = value;
		return(0);
	}
	else return(-1);
    }
    return(-1);
}

static long putStringFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)paddr->pfield;
    float  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%f",&value) == 1) {
		*pdest = value;
		return(0);
	}
    }
    return(-1);
}

static long putStringDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)paddr->pfield;
    double  value;

    if(nRequest==1 && offset==0) {
	if(sscanf(pbuffer,"%lf",&value) == 1) {
		*pdest = value;
		return(0);
	}
    }
    return(-1);
}

static long putStringEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    struct rset *prset;
    short       record_type=(paddr->record_type);
    long        status;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->put_enum_str))
        return( (*prset->put_enum_str)(paddr,pbuffer) );
    status=S_db_noRSET;
    recGblRecSupError(status,paddr,"dbPutField","put_enum_str");
    return(S_db_badDbrtype);
}

static long putStringGchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short               choice_set=paddr->choice_set;
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringGchoice)");
        return(S_db_onlyOne);
    }
    if(pchoiceSet=GET_PCHOICE_SET(pdbBase->pchoiceGbl,choice_set)) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringGchoice)");
    return(S_db_badChoice);
}

static long putStringCchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringCchoice)");
        return(S_db_onlyOne);
    }
    if(pchoiceSet=pdbBase->pchoiceCvt) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringCchoice)");
    return(S_db_badChoice);
}

static long putStringRchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short               choice_set=paddr->choice_set;
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    char                *pchoice;
    struct choiceSet    *pchoiceSet;
    struct arrChoiceSet *parrChoiceSet;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringRchoice)");
        return(S_db_onlyOne);
    }
    if((parrChoiceSet=GET_PARR_CHOICE_SET(pdbBase->pchoiceRec,(paddr->record_type)))
    && (pchoiceSet=GET_PCHOICE_SET(parrChoiceSet,choice_set))) {
	for(i=0; i<pchoiceSet->number; i++) {
	    if(!(pchoice=pchoiceSet->papChoice[i])) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringRchoice)");
    return(S_db_badChoice);
}

static long putStringDchoice(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short      *pfield= (unsigned short*)(paddr->pfield);
    struct devChoiceSet *pdevChoiceSet;
    char		*pchoice;
    unsigned short	i;

    if(no_elements!=1){
        recGblDbaddrError(S_db_onlyOne,paddr,"dbPut(putStringDchoice)");
        return(S_db_onlyOne);
    }
    if(pdevChoiceSet=GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev,paddr->record_type)) {
	for(i=0; i<pdevChoiceSet->number; i++) {
	    if(!(pchoice=pdevChoiceSet->papDevChoice[i]->pchoice)) continue;
	    if(strcmp(pchoice,pbuffer)==0) {
		*pfield=i;
		return(0);
	    }
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbPut(putStringDchoice)");
    return(S_db_badChoice);
}

static long putCharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtCharToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtCharToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putCharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putCharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putCharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
char 		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUcharToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUcharToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUcharChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUcharDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUcharEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned char 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtShortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtShortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putShortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putShortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putShortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
short	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUshortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUshortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUshortChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUshortDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUshortEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtLongToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtLongToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putLongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putLongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putLongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
long	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUlongToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUlongToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putUlongChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putUlongDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putUlongEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned long 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char	*pdest=(char *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;
    short size=paddr->field_size;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbPutField","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	cvtFloatToString(*pbuffer,pdest,precision);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtFloatToString(*pbuffer,pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putFloatChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);
    long	   ltemp;/*vxWorks does not support float to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *pbuffer;
	*pdest = ltemp;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	ltemp = *pbuffer++;
	*pdest++ = ltemp;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putFloatDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putFloatEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
float	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char	*pdest=(char *)(paddr->pfield);
    long	status;
    int		precision;
    short	record_type=paddr->record_type;
    struct rset	*prset;
    short size=paddr->field_size;

    if((prset=GET_PRSET(pdbBase->precSup,record_type)) && (prset->get_precision))
	status = (*prset->get_precision)(paddr,&precision);
    else
	status=S_db_precision;
    if(!RTN_SUCCESS(status)) {
	recGblRecSupError(status,paddr,"dbPutField","get_precision");
	return(status);
    }

    if(nRequest==1 && offset==0) {
	cvtDoubleToString(*pbuffer,pdest,precision);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtDoubleToString(*pbuffer,pdest,precision);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putDoubleChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char   *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char   *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);
    long	   ltemp;/*vxWorks does not support double to unsigned long*/

    if(nRequest==1 && offset==0) {
	ltemp = *pbuffer;
	*pdest = ltemp;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	ltemp = *pbuffer++;
	*pdest++ = ltemp;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putDoubleDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double		*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putDoubleEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
double	 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumString(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);
    short size=paddr->field_size;


    if(nRequest==1 && offset==0) {
	cvtUshortToString(*pbuffer,pdest);
	return(0);
    }
    pdest += (size*offset);
    while (nRequest) {
	cvtUshortToString(*pbuffer,pdest);
	pbuffer++;
	if(++offset==no_elements)
		pdest=(char *)paddr->pfield;
	else
    		pdest += size;
	nRequest--;
    }
    return(0);
}

static long putEnumChar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    char  *pdest=(char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUchar(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned char  *pdest=(unsigned char *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned char *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumShort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    short  *pdest=(short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUshort(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumLong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    long  *pdest=(long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumUlong(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned long  *pdest=(unsigned long *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned long *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumFloat(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    float  *pdest=(float *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(float *)paddr->pfield;
	nRequest--;
    }
    return(0);
}
 
static long putEnumDouble(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    double  *pdest=(double *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(double *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

static long putEnumEnum(paddr,pbuffer,nRequest,no_elements,offset)
struct dbAddr	*paddr;
unsigned short 	*pbuffer;
long		nRequest;
long		no_elements;
long		offset;
{
    unsigned short  *pdest=(unsigned short *)(paddr->pfield);

    if(nRequest==1 && offset==0) {
	*pdest = *pbuffer;
	return(0);
    }
    pdest += offset;
    while (nRequest) {
	*pdest++ = *pbuffer++;
	if(++offset==no_elements) pdest=(unsigned short *)paddr->pfield;
	nRequest--;
    }
    return(0);
}

/* This is the table of routines for converting database fields */
/* the rows represent the buffer types				*/
/* the columns represent the field types			*/

/* field types are********************************************************
 DBF_STRING,      DBF_CHAR,        DBF_UCHAR,       DBF_SHORT,       DBF_USHORT,
 DBF_LONG,        DBF_ULONG,       DBF_FLOAT,       DBF_DOUBLE,      DBF_ENUM
 DBF_GBLCHOICE,   DBF_CVTCHOICE,   DBF_RECCHOICE,   DBF_DEVCHOICE
 ***************************************************************************/

long (*put_convert_table[DBR_ENUM+1][DBF_DEVCHOICE+1])() = {
/* source is a DBR_STRING		*/
{putStringString, putStringChar,   putStringUchar,  putStringShort,  putStringUshort,
 putStringLong,   putStringUlong,  putStringFloat,  putStringDouble, putStringEnum,
 putStringGchoice,putStringCchoice,putStringRchoice,putStringDchoice},
/* source is a DBR_CHAR		*/
{putCharString,   putCharChar,     putCharUchar,    putCharShort,    putCharUshort,
 putCharLong,     putCharUlong,    putCharFloat,    putCharDouble,   putCharEnum,
 putCharEnum,     putCharEnum,     putCharEnum,     putCharEnum},
/* source is a DBR_UCHAR		*/
{putUcharString,  putUcharChar,    putUcharUchar,   putUcharShort,   putUcharUshort,
 putUcharLong,    putUcharUlong,   putUcharFloat,   putUcharDouble,  putUcharEnum,
 putUcharEnum,    putUcharEnum,    putUcharEnum,    putUcharEnum},
/* source is a DBR_SHORT		*/
{putShortString,  putShortChar,    putShortUchar,   putShortShort,   putShortUshort,
 putShortLong,    putShortUlong,   putShortFloat,   putShortDouble,  putShortEnum,
 putShortEnum,    putShortEnum,    putShortEnum,    putShortEnum},
/* source is a DBR_USHORT		*/
{putUshortString, putUshortChar,   putUshortUchar,  putUshortShort,  putUshortUshort,
 putUshortLong,   putUshortUlong,  putUshortFloat,  putUshortDouble, putUshortEnum,
 putUshortEnum,   putUshortEnum,   putUshortEnum,   putUshortEnum},
/* source is a DBR_LONG		*/
{putLongString,   putLongChar,     putLongUchar,    putLongShort,    putLongUshort,
 putLongLong,     putLongUlong,    putLongFloat,    putLongDouble,   putLongEnum,
 putLongEnum,     putLongEnum,     putLongEnum,     putLongEnum},
/* source is a DBR_ULONG		*/
{putUlongString,  putUlongChar,    putUlongUchar,   putUlongShort,   putUlongUshort,
 putUlongLong,    putUlongUlong,   putUlongFloat,   putUlongDouble,  putUlongEnum,
 putUlongEnum,    putUlongEnum,    putUlongEnum,    putUlongEnum},
/* source is a DBR_FLOAT		*/
{putFloatString,  putFloatChar,    putFloatUchar,   putFloatShort,   putFloatUshort,
 putFloatLong,    putFloatUlong,   putFloatFloat,   putFloatDouble,  putFloatEnum,
 putFloatEnum,    putFloatEnum,    putFloatEnum,    putFloatEnum},
/* source is a DBR_DOUBLE		*/
{putDoubleString, putDoubleChar,   putDoubleUchar,  putDoubleShort,  putDoubleUshort,
 putDoubleLong,   putDoubleUlong,  putDoubleFloat,  putDoubleDouble, putDoubleEnum,
 putDoubleEnum,   putDoubleEnum,   putDoubleEnum,   putDoubleEnum},
/* source is a DBR_ENUM		*/
{putEnumString,   putEnumChar,     putEnumUchar,    putEnumShort,    putEnumUshort,
 putEnumLong,     putEnumUlong,    putEnumFloat,    putEnumDouble,   putEnumEnum,
 putEnumEnum,     putEnumEnum,     putEnumEnum,     putEnumEnum}
};

long dbPut(
       struct dbAddr   *paddr,
       short           dbrType,
       void            *pbuffer,
       long            nRequest
)
{
	long		no_elements=paddr->no_elements;
	long		dummy;
	long		offset;
	long		(*pconvert_routine)();
	long int	(*pspecial)()=NULL;
	struct rset	*prset;
	struct dbCommon *precord=(struct dbCommon *)(paddr->precord);
	long		status=0;
	static   char	val[4]={'V','A','L',' '};
	long		*pval=(long *)&val[0];
	struct fldDes	*pfldDes;
	long		*pfield_name;
	long		special=paddr->special;
	short		field_type=paddr->field_type;
	unsigned short	acks=precord->acks;

	/* Check for valid request */
	if( INVALID_DB_REQ(dbrType) || (field_type>DBF_DEVCHOICE)
	|| (!(pconvert_routine=put_convert_table[dbrType][field_type])) )
	{
		char message[80];

		sprintf(message,"dbPut - database request type is %d",dbrType);
		recGblDbaddrError(S_db_badDbrtype,paddr,message);
		return(S_db_badDbrtype);
	}
	
	prset=GET_PRSET(pdbBase->precSup,paddr->record_type);
	
	/* check for special processing	is required */
	if(special) {
	    if(special<100) { /*global processing*/
		if(special==SPC_NOMOD) {
		    return(S_db_noMod);
		}else if(special==SPC_SCAN){
		    scanDelete(precord);
		}
	    }
	    else {
		if( prset && (pspecial = (prset->special))) {
		    status=(*pspecial)(paddr,0);
		    if(!RTN_SUCCESS(status)) return(status);
		} else {
		    recGblRecSupError(S_db_noSupport,paddr,"dbPut",
			"special");
		    return(S_db_noSupport);
		}
	    }
	}

	/* check for array			*/
	if( no_elements>1 && prset && (prset->get_array_info) ) {
		status= (*prset->get_array_info)(paddr,&dummy,&offset);
	}
	else offset=0;
	if(no_elements<(nRequest)) nRequest = no_elements;

	/* convert database field to buffer type and place it in the buffer */
	status=(*pconvert_routine)(paddr,pbuffer,nRequest,no_elements,offset);

	/* update array info	*/
	if( no_elements>1 && prset && (prset->put_array_info) ) {
		status= (*prset->put_array_info)(paddr,nRequest);
	}

	if(!RTN_SUCCESS(status)) return(status);

	/* check for special processing	is required */
	if(special) {
	    if(special<100) { /*global processing*/
		if(special==SPC_SCAN) {
		    scanAdd(precord);
		} else if(special==SPC_ALARMACK) {
		    if(paddr->pfield == (void *)&precord->acks) {
			if(acks>0 && acks<=precord->acks) {
			    precord->acks = 0;
			    db_post_events(precord,&precord->acks,DBE_VALUE);
			} else { /*Undo change*/
			    precord->acks = acks;
			}
		    }else if(paddr->pfield == (void *)&precord->ackt) {
			if(!precord->ackt && precord->acks>precord->sevr) {
			    precord->acks = precord->sevr;
			    db_post_events(precord,&precord->acks,DBE_VALUE);
			}
		    }
		} else if(special==SPC_AS) {
		    asChangeGroup(&precord->asp,precord->asg);
		}
	    }
	    else {
		status=(*pspecial)(paddr,1);
		if(!RTN_SUCCESS(status)) return(status);
	    }
	}

	/* propagate events for this field */
	/* if the field is VAL and process_passive is true dont propagate*/
	pfldDes = (struct fldDes *)(paddr->pfldDes);
	pfield_name = (long *)&(pfldDes->fldname[0]);
	/* if field is val set udf FALSE */
	if (*pval == *pfield_name) precord->udf=FALSE;
	if(precord->mlis.count &&
	((*pval != *pfield_name) || (!pfldDes->process_passive)))
		db_post_events(precord,paddr->pfield,DBE_VALUE);

	return(status);
}
