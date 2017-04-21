/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbAccessDefs.h	*/

#ifndef INCdbAccessDefsh
#define INCdbAccessDefsh

#ifdef epicsExportSharedSymbols
#   define INCLdb_accessh_epicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include "epicsTypes.h"
#include "epicsTime.h"
#include "dbBase.h"
#include "dbAddr.h"
#include "recSup.h"

#ifdef INCLdb_accessh_epicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   include "shareLib.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


epicsShareExtern struct dbBase *pdbbase;
epicsShareExtern volatile int interruptAccept;

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
 *	epicsFloat32 value[10]
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
 *	epicsFloat32 value[10]
 * } MYBUFFER;
 *
 * With this definition you can give definitions such as the following:
 *
 * MYBUFFER *pbuf1;
 * MYBUFFER buf;
 *************************************************************************/

/* Macros for defining each option */
#define DBRstatus \
	epicsUInt16	status;		/* alarm status */\
	epicsUInt16	severity;	/* alarm severity*/\
	epicsUInt16	acks;		/* alarm ack severity*/\
	epicsUInt16	ackt;		/* Acknowledge transient alarms?*/
#define DB_UNITS_SIZE   16
#define DBRunits \
	char		units[DB_UNITS_SIZE];	/* units	*/
#define DBRprecision union { \
        long            dp;      /* number of decimal places*/\
        double          unused;  /* for alignment */\
        } precision;
        /* precision.dp must be long to match the pointer arguments to
         * RSET->get_precision() and recGblGetPrec(), which it's
         * too late to change now.  DBRprecision must be padded to
         * maintain 8-byte alignment. */
#define DBRtime \
	epicsTimeStamp	time;		/* time stamp*/
#define DBRenumStrs \
	epicsUInt32	no_str;		/* number of strings*/\
	epicsInt32	padenumStrs;	/*padding to force 8 byte align*/\
	char		strs[DB_MAX_CHOICES][MAX_STRING_SIZE];	/* string values    */
#define DBRgrLong \
        epicsInt32      upper_disp_limit;       /*upper limit of graph*/\
        epicsInt32      lower_disp_limit;       /*lower limit of graph*/
#define DBRgrDouble \
        epicsFloat64    upper_disp_limit;       /*upper limit of graph*/\
        epicsFloat64    lower_disp_limit;       /*lower limit of graph*/
#define DBRctrlLong \
        epicsInt32      upper_ctrl_limit;       /*upper limit of graph*/\
        epicsInt32      lower_ctrl_limit;       /*lower limit of graph*/
#define DBRctrlDouble \
        epicsFloat64    upper_ctrl_limit;       /*upper limit of graph*/\
        epicsFloat64    lower_ctrl_limit;       /*lower limit of graph*/
#define DBRalLong \
        epicsInt32      upper_alarm_limit;\
        epicsInt32      upper_warning_limit;\
        epicsInt32      lower_warning_limit;\
        epicsInt32      lower_alarm_limit;
#define DBRalDouble \
        epicsFloat64    upper_alarm_limit;\
        epicsFloat64    upper_warning_limit;\
        epicsFloat64    lower_warning_limit;\
        epicsFloat64    lower_alarm_limit;

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
#define S_db_noLSET     (M_dbAccess|21) /*No link support table or entry*/
#define S_db_noRSET 	(M_dbAccess|31) /*missing record support entry table*/
#define S_db_noSupport 	(M_dbAccess|33) /*RSET or DSXT routine not defined*/
#define S_db_BadSub 	(M_dbAccess|35) /*Subroutine not found*/
/*!!!! Do not change next line without changing src/rsrv/server.h!!!!!!!!*/
#define S_db_Pending 	(M_dbAccess|37) /*Request is pending*/

#define S_db_Blocked 	(M_dbAccess|39) /*Request is Blocked*/
#define S_db_putDisabled (M_dbAccess|41) /*putFields are disabled*/
#define S_db_badHWaddr  (M_dbAccess|43) /*Hardware link type not on INP/OUT*/
#define S_db_bkptSet    (M_dbAccess|53) /*Breakpoint already set*/
#define S_db_bkptNotSet (M_dbAccess|55) /*No breakpoint set in record*/
#define S_db_notStopped (M_dbAccess|57) /*Record not stopped*/
#define S_db_errArg     (M_dbAccess|59) /*Error in argument*/
#define S_db_bkptLogic  (M_dbAccess|61) /*Logic error in breakpoint routine*/
#define S_db_cntSpwn    (M_dbAccess|63) /*Cannot spawn dbContTask*/
#define S_db_cntCont    (M_dbAccess|65) /*Cannot resume dbContTask*/
#define S_db_noMemory   (M_dbAccess|66) /*unable to allocate data structure from pool*/
#define S_db_notInit    (M_dbAccess|67) /*Not initialized*/
#define S_db_bufFull    (M_dbAccess|68) /*Buffer full*/

epicsShareFunc long dbPutSpecial(struct dbAddr *paddr,int pass);
epicsShareFunc rset * dbGetRset(const struct dbAddr *paddr);
epicsShareFunc long dbPutAttribute(
    const char *recordTypename,const char *name,const char*value);
epicsShareFunc int dbIsValueField(const struct dbFldDes *pdbFldDes);
epicsShareFunc int dbGetFieldIndex(const struct dbAddr *paddr);
epicsShareFunc long dbScanPassive(
    struct dbCommon *pfrom,struct dbCommon *pto);
epicsShareFunc long dbProcess(struct dbCommon *precord);
epicsShareFunc long dbNameToAddr(
    const char *pname,struct dbAddr *);
epicsShareFunc devSup* dbDTYPtoDevSup(dbRecordType *prdes, int dtyp);
epicsShareFunc devSup* dbDSETtoDevSup(dbRecordType *prdes, struct dset *pdset);
epicsShareFunc long dbGetField(
    struct dbAddr *,short dbrType,void *pbuffer,long *options,
    long *nRequest,void *pfl);
epicsShareFunc long dbGet(
    struct dbAddr *,short dbrType,void *pbuffer,long *options,
    long *nRequest,void *pfl);
epicsShareFunc long dbPutField(
    struct dbAddr *,short dbrType,const void *pbuffer,long nRequest);
epicsShareFunc long dbPut(
    struct dbAddr *,short dbrType,const void *pbuffer,long nRequest);

typedef void(*SPC_ASCALLBACK)(struct dbCommon *);
/*dbSpcAsRegisterCallback called by access security */
epicsShareFunc void dbSpcAsRegisterCallback(SPC_ASCALLBACK func);
epicsShareFunc long dbBufferSize(
    short dbrType,long options,long nRequest);
epicsShareFunc long dbValueSize(short dbrType);

/* Hook Routine */

typedef void (*DB_LOAD_RECORDS_HOOK_ROUTINE)(const char* filename,
    const char* substitutions);
epicsShareExtern DB_LOAD_RECORDS_HOOK_ROUTINE dbLoadRecordsHook;

epicsShareFunc int  dbLoadDatabase(
    const char *filename, const char *path, const char *substitutions);
epicsShareFunc int dbLoadRecords(
    const char* filename, const char* substitutions);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAccessDefsh*/
