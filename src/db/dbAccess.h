/* dbAccess.h	*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************
 
(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO
 
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/* $Id$ 
 *
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 * Modification Log:
 * -----------------
 * .01  12-18-91        jba     Changed caddr_t to void *
 * .02  03-04-92        jba     Replaced dbr_value_size with dbrValueSize in dbAccess 
 * .03  05-28-92        mrk     cleanup
 * .04  08-19-92        jba     added prototypes dbCaAddInlink,dbCaAddOutlink
 * .05  08-19-92        jba     added prototypes dbCaPutLink,dbCaGetLink
 * .06  02-02-94	mrk	added definitions for dbPutNotify
 * .07	03-18-94	mcn	added breakpoint codes, fast link protos
 */

#ifndef INCdbAccessh
#define INCdbAccessh

#include "shareLib.h"

epicsShareExtern struct dbBase *pdbbase;

/*  The database field and request types are defined in dbFldTypes.h*/
/* Data Base Request Options	*/
#define DBR_STATUS      0x00000001
#define DBR_UNITS       0x00000002
#define DBR_PRECISION   0x00000004
#define DBR_TIME        0x00000008
#define DBR_ENUM_STRS   0x00000010
#define DBR_GR_LONG     0x00000020
#define DBR_GR_DOUBLE   0x00000040
#define DBR_CTRL_LONG   0x00000080
#define DBR_CTRL_DOUBLE 0x00000100
#define DBR_AL_LONG     0x00000200
#define DBR_AL_DOUBLE   0x00000400

/**********************************************************************
 * The next page contains macros for defining requests.
 * As an example the following defines a buffer to accept an array
 * of 10 float values + DBR_STATUS and DBR_TIME options
 *
 * struct {
 *	DBRstatus
 *	DBRtime
 *	float value[10]
 * } buffer;
 *
 * IMPORTANT!! The DBRoptions must be given in the order that they
 *             appear in the Data Base Request Options #defines
 *
 * The associated dbGetField call is:
 *
 * long options,number_elements;
 * ...
 * options = DBR_STATUS|DBR_TIME;
 * number_elements = 10;
 * rtnval=dbGetField(paddr,DBR_FLOAT,&buffer,&options,&number_elements);
 *	
 * When dbGetField returns:
 *	rtnval is error status (0 means success)
 *	options has a bit set for each option that was accepted
 *	number_elements is actual number of elements obtained
 *
 * The individual items can be refered to by the expressions::
 *
 * buffer.status
 * buffer.severity
 * buffer.err_status
 * buffer.epoch_seconds
 * buffer.nano_seconds
 * buffer.value[i]
 *
 * The following is also a valid declaration:
 *
 * typedef struct {
 *	DBRstatus
 *	DBRtime
 *	float value[10]
 * } MYBUFFER;
 *
 * With this definition you can give definitions such as the following:
 *
 * MYBUFFER *pbuf1;
 * MYBUFFER buf;
 *************************************************************************/

/* Macros for defining each option */
#define DBRstatus \
	unsigned short	status;		/* alarm status */\
	unsigned short	severity;	/* alarm severity*/\
	unsigned short	acks;		/* alarm ack severity*/\
	unsigned short	ackt;		/* Acknowledge transient alarms?*/
#define DB_UNITS_SIZE   16
#define DBRunits \
	char		units[DB_UNITS_SIZE];	/* units	*/
#define DBRprecision \
        long            precision;      /* number of decimal places*/\
        long            field_width;    /* field width             */
#define DBRtime \
	TS_STAMP	time;		/* time stamp*/
#define DBRenumStrs \
	unsigned long	no_str;		/* number of strings*/\
	long		padenumStrs;	/*padding to force 8 byte align*/\
	char		strs[DB_MAX_CHOICES][MAX_STRING_SIZE];	/* string values    */
#define DBRgrLong \
        long            upper_disp_limit;       /*upper limit of graph*/\
        long            lower_disp_limit;       /*lower limit of graph*/
#define DBRgrDouble \
        double          upper_disp_limit;       /*upper limit of graph*/\
        double          lower_disp_limit;       /*lower limit of graph*/
#define DBRctrlLong \
        long            upper_ctrl_limit;       /*upper limit of graph*/\
        long            lower_ctrl_limit;       /*lower limit of graph*/
#define DBRctrlDouble \
        double          upper_ctrl_limit;       /*upper limit of graph*/\
        double          lower_ctrl_limit;       /*lower limit of graph*/
#define DBRalLong \
        long            upper_alarm_limit;\
        long            upper_warning_limit;\
        long            lower_warning_limit;\
        long            lower_alarm_limit;
#define DBRalDouble \
        double          upper_alarm_limit;\
        double          upper_warning_limit;\
        double          lower_warning_limit;\
        double          lower_alarm_limit;

/*  structures for each option type             */
struct dbr_status       {DBRstatus};
struct dbr_units        {DBRunits};
struct dbr_precision    {DBRprecision};
struct dbr_time         {DBRtime};
struct dbr_enumStrs     {DBRenumStrs};
struct dbr_grLong       {DBRgrLong};
struct dbr_grDouble     {DBRgrDouble};
struct dbr_ctrlLong     {DBRctrlLong};
struct dbr_ctrlDouble   {DBRctrlDouble};
struct dbr_alLong       {DBRalLong};
struct dbr_alDouble     {DBRalDouble};
/* sizes for each option structure              */
#define dbr_status_size sizeof(struct dbr_status)
#define dbr_units_size sizeof(struct dbr_units)
#define dbr_precision_size sizeof(struct dbr_precision)
#define dbr_time_size sizeof(struct dbr_time)
#define dbr_enumStrs_size sizeof(struct dbr_enumStrs)
#define dbr_grLong_size sizeof(struct dbr_grLong)
#define dbr_grDouble_size sizeof(struct dbr_grDouble)
#define dbr_ctrlLong_size sizeof(struct dbr_ctrlLong)
#define dbr_ctrlDouble_size sizeof(struct dbr_ctrlDouble)
#define dbr_alLong_size sizeof(struct dbr_alLong)
#define dbr_alDouble_size sizeof(struct dbr_alDouble)

#ifndef INCerrMdefh
#include "errMdef.h"
#endif
#define S_db_notFound 	(M_dbAccess| 1) /*Process Variable Not Found*/
#define S_db_badDbrtype	(M_dbAccess| 3) /*Illegal Database Request Type*/
#define S_db_noMod 	(M_dbAccess| 5) /*Attempt to modify noMod field*/
#define S_db_badLset 	(M_dbAccess| 7) /*Illegal Lock Set*/
#define S_db_precision 	(M_dbAccess| 9) /*get precision failed */
#define S_db_onlyOne 	(M_dbAccess|11) /*Only one element allowed*/
#define S_db_badChoice 	(M_dbAccess|13) /*Illegal choice*/
#define S_db_badField 	(M_dbAccess|15) /*Illegal field value*/
#define S_db_lsetLogic 	(M_dbAccess|17) /*Logic error generating lock sets*/
#define S_db_noRSET 	(M_dbAccess|31) /*missing record support entry table*/
#define S_db_noSupport 	(M_dbAccess|33) /*RSET routine not defined*/
#define S_db_BadSub 	(M_dbAccess|35) /*Subroutine not found*/
/*!!!! Do not change next two lines without changing src/rsrv/server.h!!!!!!!!*/
#define S_db_Pending 	(M_dbAccess|37) /*Request is pending*/
#define S_db_Blocked 	(M_dbAccess|39) /*Request is Blocked*/
#define S_db_putDisabled (M_dbAccess|41) /*putFields are disabled*/
#define S_db_bkptSet    (M_dbAccess|53) /*Breakpoint already set*/
#define S_db_bkptNotSet (M_dbAccess|55) /*No breakpoint set in record*/
#define S_db_notStopped (M_dbAccess|57) /*Record not stopped*/
#define S_db_errArg     (M_dbAccess|59) /*Error in argument*/
#define S_db_bkptLogic  (M_dbAccess|61) /*Logic error in breakpoint routine*/
#define S_db_cntSpwn    (M_dbAccess|63) /*Cannot spawn dbContTask*/
#define S_db_cntCont    (M_dbAccess|65) /*Cannot resume dbContTask*/

/* Global Database Access Routines*/
#define dbGetLink(PLNK,DBRTYPE,PBUFFER,OPTIONS,NREQUEST) \
    (((((PLNK)->type == CONSTANT)&&(!(NREQUEST))) ||\
	(! (((PLNK)->type == DB_LINK) || ((PLNK)->type == CA_LINK)) ))\
    ? 0\
    : dbGetLinkValue((PLNK),(DBRTYPE), \
	(void *)(PBUFFER),(OPTIONS),(NREQUEST)))
#define dbPutLink(PLNK,DBRTYPE,PBUFFER,NREQUEST) \
    ((((PLNK)->type == CONSTANT) ||\
	(! (((PLNK)->type == DB_LINK) || ((PLNK)->type == CA_LINK)) ))\
    ? 0\
    : dbPutLinkValue((PLNK),(DBRTYPE),(void *)(PBUFFER),(NREQUEST)))
#define dbGetPdbAddrFromLink(PLNK) \
    (\
        ((PLNK)->type != DB_LINK) \
        ? 0\
        : (((DBADDR*)((PLNK)->value.pv_link.pvt))) \
    )

epicsShareFunc struct rset * epicsShareAPI dbGetRset(struct dbAddr *paddr);
epicsShareFunc long epicsShareAPI dbPutAttribute(
    char *recordTypename,char *name,char*value);
epicsShareFunc int epicsShareAPI dbIsValueField(struct dbFldDes *pdbFldDes);
epicsShareFunc int epicsShareAPI dbGetFieldIndex(struct dbAddr *paddr);
epicsShareFunc long epicsShareAPI dbGetNelements(
    struct link *plink,long *nelements);
epicsShareFunc int epicsShareAPI dbIsLinkConnected(struct link *plink);
epicsShareFunc int epicsShareAPI dbGetLinkDBFtype(struct link *plink);
epicsShareFunc long epicsShareAPI dbScanLink(
    struct dbCommon *pfrom, struct dbCommon *pto);
epicsShareFunc long epicsShareAPI dbScanPassive(
    struct dbCommon *pfrom,struct dbCommon *pto);
epicsShareFunc void epicsShareAPI dbScanFwdLink(struct link *plink);
epicsShareFunc long epicsShareAPI dbProcess(struct dbCommon *precord);
epicsShareFunc long epicsShareAPI dbNameToAddr(
    const char *pname,struct dbAddr *);
epicsShareFunc long epicsShareAPI dbGetLinkValue(
    struct link *,short dbrType,void *pbuffer,long *options,long *nRequest);
epicsShareFunc long epicsShareAPI dbGetField(
    struct dbAddr *,short dbrType,void *pbuffer,long *options,
    long *nRequest,void *pfl);
epicsShareFunc long epicsShareAPI dbGet(
    struct dbAddr *,short dbrType,void *pbuffer,long *options,
    long *nRequest,void *pfl);
epicsShareFunc long epicsShareAPI dbPutLinkValue(
    struct link *,short dbrType,const void *pbuffer,long nRequest);
epicsShareFunc long epicsShareAPI dbPutField(
    struct dbAddr *,short dbrType,const void *pbuffer,long nRequest);
epicsShareFunc long epicsShareAPI dbPut(
    struct dbAddr *,short dbrType,const void *pbuffer,long nRequest);
typedef void(*SPC_ASCALLBACK)(struct dbCommon *);
/*dbSpcAsRegisterCallback called by access security */
epicsShareFunc void epicsShareAPI dbSpcAsRegisterCallback(SPC_ASCALLBACK func);
epicsShareFunc long epicsShareAPI dbBufferSize(
    short dbrType,long options,long nRequest);
epicsShareFunc long epicsShareAPI dbValueSize(short dbrType);

#endif /*INCdbAccessh*/
