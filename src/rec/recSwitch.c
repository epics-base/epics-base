/* recSwitch.c */
/* base/src/rec  $Id$ */

/* recSwitch.c - Record Support Routines for Switch records */
/*
 *      Author:          Matthew Needes
 *      Date:            9-21-93
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
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include        <wdLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include        <callback.h>
#include	<switchRecord.h>

/* RSET - Record Support Entry Table */
#define report             NULL
#define initialize         NULL
static  long init_record();
static  long process();
static  long special();
static  long get_value();
#define cvt_dbaddr         NULL
#define get_array_info     NULL
#define put_array_info     NULL
#define get_units          NULL
static  long get_precision();
static  long get_enum_str();
static  long get_enum_strs();
#define put_enum_str       NULL
#define get_graphic_double NULL
#define get_control_double NULL
#define get_alarm_double   NULL

struct rset switchRSET = {
	RSETNUMBER,
	report,
	initialize,
	init_record,
	process,
	special,
	get_value,
	cvt_dbaddr,
	get_array_info,
	put_array_info,
	get_units,
	get_precision,
	get_enum_str,
	get_enum_strs,
	put_enum_str,
	get_graphic_double,
	get_control_double,
	get_alarm_double };

static long init_state();
static long output_and_watch();
static long check_readback();
static void monitor();
static void callback();
static void watchdog();

/* defs for initialization choices */
#define INIT_RESET        0
#define INIT_SET          1
#define INIT_READBACK     2

/* defs for readback set threshold */
#define ABOVE             0
#define BELOW             1

/* defs for transition fail action choices */
#define FAIL_FOLLOW_RDBK  0
#define FAIL_KEEP_VALUE   1

/* state definitions for switch */
#define UNINITIALIZED     0
#define SET               1
#define RESET             2
#define RESET_TO_SET      3
#define SET_TO_RESET      4

/* states above this number signify transition */
#define TRANS_STATE       2

/* number of strings */
#define STRING_COUNT      5

/*
 *  Bitmask to check to see if the switch
 *    is set or in the process of setting.
 */
#define TO_SET            0x01

/* private structure in record */
struct PVT {
    CALLBACK callback;
    WDOG_ID watchdog;
};

/* post an event on a field */
#define MON_FIELD(CUR,PREV) \
     if ((CUR) != (PREV)) { \
         db_post_events(pswitch, &(CUR), DBE_VALUE); \
         (PREV) = (CUR); }

/* initialize record */
static long init_record(
     struct switchRecord *pswitch,
     int pass
)
{
  long status;

 /* ignore second pass */
  if (!pass)
    return(0);

 /* initialize readback input link */
  status = recGblInitFastInLink(&pswitch->rbkl, (void *) pswitch, DBR_DOUBLE, "RBKV");
  if (status)
     return(status);
  
 /* initialize master reset link */
  status = recGblInitFastInLink(&pswitch->mrsl, (void *) pswitch, DBR_UCHAR, "MRSV");
  if (status)
     return(status);

 /* initialize master set link */
  status = recGblInitFastInLink(&pswitch->mstl, (void *) pswitch, DBR_UCHAR, "MSTV");
  if (status)
     return(status);

 /* initialize reset link */
  status = recGblInitFastInLink(&pswitch->rstl, (void *) pswitch, DBR_UCHAR, "RSTV");
  if (status)
     return(status);

 /* initialize set link */
  status = recGblInitFastInLink(&pswitch->setl, (void *) pswitch, DBR_UCHAR, "SETV");
  if (status)
     return(status);

 /* initialize toggle link */
  status = recGblInitFastInLink(&pswitch->togl, (void *) pswitch, DBR_UCHAR, "TOGV");
  if (status)
     return(status);

 /* clear alarm severity */
  pswitch->sevr = NO_ALARM;

 /*
  *  Allocate private structure
  */
  pswitch->pvt = (void *) malloc(sizeof(struct PVT));

  if (!pswitch->pvt)
     return(S_rec_outMem);

 /*
  * Create watchdog
  */
  ((struct PVT *)pswitch->pvt)->watchdog = (void *) wdCreate();

  if (!((struct PVT *)pswitch->pvt)->watchdog)
     return(S_rec_outMem);

 /*
  *  Initialize EPICS callback
  */
  callbackSetCallback(callback, &((struct PVT *)pswitch->pvt)->callback);
  callbackSetUser(pswitch, &((struct PVT *)pswitch->pvt)->callback);
  callbackSetPriority(pswitch->prio, &((struct PVT *)pswitch->pvt)->callback);

  return(0);
}

/* initialize record's state */
static long init_state(struct switchRecord *pswitch)
{
  long status = 0;

 /* set initial state */
  switch (pswitch->ini) {
     case INIT_RESET:
        pswitch->val  = SET_TO_RESET;
        pswitch->pint = RESET;
        pswitch->outv = pswitch->offv;
        status = output_and_watch(pswitch);
        if (status)
           return(status);

        break;
     case INIT_SET:
        pswitch->val  = RESET_TO_SET;
        pswitch->pint = SET;
        pswitch->outv = pswitch->onv;
        status = output_and_watch(pswitch);
        if (status)
           return(status);

        break;
     case INIT_READBACK:
       /*
        *  If uninitialized, get state from readback
        */
        status = recGblGetFastLink(&pswitch->rbkl, (void *) pswitch,
                                       &pswitch->rbkv);
        if (status)
           return(status);

        if (pswitch->dthr == BELOW)
           pswitch->pint = pswitch->val = (pswitch->rbkv <= pswitch->rthr) ? SET : RESET;
        else
           pswitch->pint = pswitch->val = (pswitch->rbkv >= pswitch->rthr) ? SET : RESET;

        pswitch->outv = (pswitch->val == SET) ? pswitch->onv : pswitch->offv;

       /*
        *  Drive output
        */
        if (pswitch->swtc.type == DB_LINK) {
           dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
        }

        break;
     default:
        return(0);
        break;
  }

  pswitch->udf = FALSE;

  pswitch->intn = 0;
  return(0);
}

/* process record */
static long process(struct switchRecord *pswitch)
{
  long status = 0;
  unsigned char value = 0;

  pswitch->pact = TRUE;

 /* initialize record if not already initialized */
  if (pswitch->val == UNINITIALIZED)
     status = init_state(pswitch);

 /*
  *  Check master set and reset links.
  */
  status = recGblGetFastLink(&pswitch->mrsl, (void *) pswitch,
                             &pswitch->mrsv);

  if (status)
     return(status);

  MON_FIELD(pswitch->mrsv, pswitch->lmrs);

  status = recGblGetFastLink(&pswitch->mstl, (void *) pswitch,
                             &pswitch->mstv);

  if (status)
     return(status);

  MON_FIELD(pswitch->mstv, pswitch->lmst);

 /* Check master reset value */
  if (pswitch->mrsv) {
      pswitch->pint = RESET;

      if (pswitch->val & TO_SET) {
         if (pswitch->val > TRANS_STATE) {
            wdCancel((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog);
         }
         pswitch->val  = SET_TO_RESET;
         pswitch->outv = pswitch->offv;
         status = output_and_watch(pswitch);
         if (status)
            return(status);
      }
      else if (pswitch->aftt == FAIL_KEEP_VALUE) {
         pswitch->outv = pswitch->offv;
         pswitch->nsta = pswitch->nsev = 0;
         if (pswitch->swtc.type == DB_LINK) {
            dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
         }
      }

      pswitch->setv = pswitch->rstv = pswitch->togv = 0;
  }
  else {
    /*
     *  Check master set link
     */
     if (pswitch->mstv) {
        pswitch->pint = SET;
        if (!(pswitch->val & TO_SET)) {
           if (pswitch->val > TRANS_STATE) {
              wdCancel((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog);
           }
           pswitch->val  = RESET_TO_SET;
           pswitch->outv = pswitch->onv;
           status = output_and_watch(pswitch);
           if (status)
              return(status);
        }
        else if (pswitch->aftt == FAIL_KEEP_VALUE) {
           pswitch->outv = pswitch->onv;   
           pswitch->nsta = pswitch->nsev = 0;
           if (pswitch->swtc.type == DB_LINK) {
              dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
           }
        }

        pswitch->setv = pswitch->rstv = pswitch->togv = 0;
     }
    /*
     *  Check input links if record is in closed loop mode
     */
     else {
        if (pswitch->omsl == CLOSED_LOOP) {
          /*
           *  Check set/reset/toggle input links - and check monitor
           */
           status = recGblGetFastLink(&pswitch->rstl, (void *) pswitch, &pswitch->rstv);

           if (status)
              return(status);

           MON_FIELD(pswitch->rstv, pswitch->lrst);

           status = recGblGetFastLink(&pswitch->setl, (void *) pswitch, &pswitch->setv);

           if (status)
              return(status);

           MON_FIELD(pswitch->setv, pswitch->lsst);

           status = recGblGetFastLink(&pswitch->togl, (void *) pswitch, &pswitch->togv);

           if (status)
              return(status);

           MON_FIELD(pswitch->togv, pswitch->ltog);

           pswitch->intn = 1;
        }
       /*
        *  If "intention" is set, a non-master switch has been
        *    modified (by a put, for example).  Check the switches.
        */
        if (pswitch->intn) {
          /*
           *  Ignore command if in transition state and ignore flag set
           */
           if (!((pswitch->val > TRANS_STATE) && pswitch->iti)) {
              if (pswitch->rstv) {
                /* reset */
                 pswitch->pint = RESET;
                 if (pswitch->val & TO_SET) {
                    if (pswitch->val > TRANS_STATE) {
                       wdCancel((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog);
                    }
                    pswitch->val  = SET_TO_RESET;
                    pswitch->outv = pswitch->offv;
                    status = output_and_watch(pswitch);
                 }
                 else if (pswitch->aftt == FAIL_KEEP_VALUE) {
                    pswitch->outv = pswitch->offv;
                    pswitch->nsta = pswitch->nsev = 0;
                    if (pswitch->swtc.type == DB_LINK) {
                       dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
                    }
                 }
              }
              else if (pswitch->setv) {
                /* set */
                 pswitch->pint = SET;
                 if (!(pswitch->val & TO_SET)) {
                    if (pswitch->val > TRANS_STATE) {
                       wdCancel((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog);
                    }
                    pswitch->val  = RESET_TO_SET;
                    pswitch->outv = pswitch->onv;
                    status = output_and_watch(pswitch);
                 }
                 else if (pswitch->aftt == FAIL_KEEP_VALUE) {
                    pswitch->outv = pswitch->onv;
                    pswitch->nsta = pswitch->nsev = 0;
                    if (pswitch->swtc.type == DB_LINK) {
                       dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
                    }
                 }
              }
              else if (pswitch->togv) {
                /* toggle */
                 if (pswitch->val > TRANS_STATE) {
                    wdCancel((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog);
                 }
                 if (pswitch->val & TO_SET) {
                    pswitch->pint = RESET;
                    pswitch->val  = SET_TO_RESET;
                    pswitch->outv = pswitch->offv;
                 }
                 else {
                    pswitch->pint = SET;
                    pswitch->val  = RESET_TO_SET;
                    pswitch->outv = pswitch->onv;
                 }
                 status = output_and_watch(pswitch);
              }
              if (status)
                 return(status);
           }
           pswitch->setv = pswitch->rstv = pswitch->togv = 0;

          /*
           *  Intention resets when setv, rstv, togv are activated,
           *    and only after mstv/mrsv are cleared
           */
           pswitch->intn = 0;
        }
     }
  }

 /*
  *  Do not check readback if the link isn't specified or
  *    if a watchdog is in existence.
  */
  if (pswitch->rbkl.type != CONSTANT && pswitch->val <= TRANS_STATE) {
     status = check_readback(pswitch);

     if (status)
        return(status);
  }

 /* store time in record */
  recGblGetTimeStamp(pswitch);

 /* alarm checking is done in check_readback */

 /*
  *  check event list - though some monitors are
  *   performed in other areas of the code.
  */
  monitor(pswitch);

 /* process forward link */
  recGblFwdLink(pswitch);

  pswitch->pact = FALSE;
  return(status);
}

/* special processing */
static long special(
     struct dbAddr *paddr,
     int after
)
{
  if (after && ((struct switchRecord *) paddr->precord)->omsl == SUPERVISORY) {
       ((struct switchRecord *) paddr->precord)->intn = 1;
  }

  return(0);
}

/* monitor fields */
static void monitor(struct switchRecord *pswitch)
{
  int monitor_mask = 0;
  short stat, sevr, nsta, nsev;

  if (pswitch->aftt == FAIL_FOLLOW_RDBK) {
     monitor_mask = recGblResetAlarms(pswitch);
  }
  else {
    /*
     *  recGblResetAlarms() is not used here, because alarm resets
     *    are not supposed to be done when this record is processed.
     *    (i.e.  nsta and nsev are not set to zero)  However, these
     *    alarms are reset in check_readback().
     */
     stat = pswitch->stat; sevr = pswitch->sevr; nsta = pswitch->nsta;
     nsev = pswitch->nsev; pswitch->stat = nsta; pswitch->sevr = nsev;

    /*
     *  Check alarm condition -
     *    normally done automatically by recGblResetAlarms()
     */
     if (stat != nsta || sevr != nsev) {
        monitor_mask |= DBE_ALARM;
        db_post_events(pswitch, &pswitch->stat, DBE_VALUE);
        db_post_events(pswitch, &pswitch->sevr, DBE_VALUE);
     }
  }

 /* check value */
  if (pswitch->val != pswitch->mlst) {
    /* trigger monitor on value */
     monitor_mask |= (DBE_VALUE  | DBE_LOG);
     pswitch->mlst = pswitch->val;
  }

  if (monitor_mask) {
     db_post_events(pswitch, &pswitch->val, monitor_mask);
  }

 /* check outv */
  MON_FIELD(pswitch->outv, pswitch->lout);
}

/* get state */
static long get_value(
     struct switchRecord *pswitch,
     struct valueDes *pvdes
)
{
  pvdes->field_type = DBF_ENUM;
  pvdes->no_elements = 1;
  (unsigned short *)(pvdes->pvalue) = &pswitch->val;
  return(0);
}

/* get precision */
static long get_precision(
     struct dbAddr *paddr,
     long *precision
)
{
  struct switchRecord *pswitch = (struct switchRecord *) paddr->precord;

  if (paddr->pfield == &pswitch->xtim)
     *precision = pswitch->prec;
  else
     *precision = 0;

  return(0);
}

/* get state string */
static long get_enum_str(
     struct dbAddr *paddr,
     char *dest
)
{
  struct switchRecord *pswitch = (struct switchRecord *) paddr->precord;
  char *string;

  if (pswitch->val < STRING_COUNT-1) {
    string = pswitch->ssi + (pswitch->val * sizeof(pswitch->ssi));
    strncpy(dest, string, sizeof(pswitch->ssi));
  }
  else {
    strcpy(dest, "Illegal Value");
  }

  return(0);
}

/* get all strings */
static long get_enum_strs(
     struct dbAddr *paddr,
     struct dbr_enumStrs *pes
)
{
  struct switchRecord *pswitch = (struct switchRecord *) paddr->precord;
  char *string;
  int i;
  short count = 0;

  memset(pes->strs, '\0', sizeof(pes->strs));
  string = pswitch->ssi;
  for (i=0; i<STRING_COUNT; i++) {
     strncpy(pes->strs[i], string, sizeof(pswitch->ssi));
     if (*string)
         count = i + 1;
     string += sizeof(pswitch->ssi);
  }
  pes->no_str = count;

  return(0);
}

/* output to "switch" link, and set watchdog */
static long output_and_watch(struct switchRecord *pswitch)
{
  long status;

 /* Process switch forward link */
  if (pswitch->swtc.type == DB_LINK) {
     dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
  }

  if (pswitch->rbkl.type == CONSTANT) {
     pswitch->val = (pswitch->val == SET_TO_RESET) ? RESET : SET;
  }
  else {
     if (pswitch->xtim) {
       /* activate watchdog */
        wdStart((WDOG_ID) ((struct PVT *)pswitch->pvt)->watchdog,
                     (int) pswitch->xtim * sysClkRateGet(),
                     (FUNCPTR) watchdog, (int) pswitch);
     }
     else {
       /* check readback immediately */
        status = check_readback(pswitch);
        if (status)
           return(status);
     }
  }
  return(0);
}

/* check readback value */
static long check_readback(struct switchRecord *pswitch)
{
  long status;
  int condition;

 /* get readback */
  status = recGblGetFastLink(&pswitch->rbkl, (void *) pswitch, &pswitch->rbkv);
  if (status)
     return(status);

 /* choose which condition to check */
  condition = (pswitch->dthr == BELOW) ? (pswitch->rbkv <= pswitch->rthr) : (pswitch->rbkv >= pswitch->rthr);

 /* check readback value */
  if (condition) {
     if (pswitch->pint == SET) {
       /* Switch set properly */
        pswitch->val = SET;
        pswitch->nsta = pswitch->nsev = 0;
     }
     else {
       /* SET ALARM - Was supposed to be reset, but was found to be set */
        recGblSetSevr(pswitch, STATE_ALARM, pswitch->srfs);
        pswitch->val = SET;
     }

    /* Drive output to what it should be given readback, if FOLLOW RDBK set */
     if (pswitch->aftt == FAIL_FOLLOW_RDBK) {
        if (pswitch->outv != pswitch->onv) {
           pswitch->outv = pswitch->onv;

          /* drive output */
           if (pswitch->swtc.type == DB_LINK) {
              dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
           }
        }
        pswitch->pint = SET;
     }
  }
  else {
     if (pswitch->pint == RESET) {
       /* switch reset properly */
        pswitch->val = RESET;
        pswitch->nsta = pswitch->nsev = 0;
     }
     else {
       /* SET ALARM - Was supposed to be set, but was found to be reset */
        recGblSetSevr(pswitch, STATE_ALARM, pswitch->rsfs);
        pswitch->val = RESET;
     }

    /* Drive output to what it should be given readback, if FOLLOW RDBK set */
     if (pswitch->aftt == FAIL_FOLLOW_RDBK) {
        if (pswitch->outv != pswitch->offv) {
           pswitch->outv = pswitch->offv;

          /* drive output */
           if (pswitch->swtc.type == DB_LINK) {
              dbScanPassive((void *) pswitch, ((struct dbAddr *)pswitch->swtc.value.db_link.pdbAddr)->precord);
           }
        }
        pswitch->pint = RESET;
     }
  }
  return(0);
}

/* callback */
static void callback(CALLBACK *pcallback)
{
  struct switchRecord *pswitch;

 /*
  *  Retrieve pointer to record from callback structure.
  *    This value is fixed at initialization, and will
  *    not be changed, so the database does not have to
  *    be locked for this actiion.
  */
  callbackGetUser((void *) pswitch, pcallback);

 /* lock the database */
  dbScanLock((struct dbCommon *) pswitch);

 /* check the readback value */
  check_readback(pswitch);

 /* monitor any changes */
  monitor(pswitch);

 /* unlock database */
  dbScanUnlock((struct dbCommon *) pswitch);
}

/* watchdog process adds callback to queue */
static void watchdog(struct switchRecord *pswitch)
{
  callbackRequest(&((struct PVT *)pswitch->pvt)->callback);
}

