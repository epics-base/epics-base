
/*
 *      Original Author: Ned D. Arnold
 *      Date:            07-18-94
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 *
 * Modification Log:
 * -----------------
 * .01  07-18-94  nda   significantly expanded functionality from prototype
 * .02  08-18-94  nda   Starting with R3.11.6, dbGetField locks the record
 *                      before fetching the data. This can cause deadlocks
 *                      within a database. Change all dbGetField() to dbGet()
 * .03  08-23-94  nda   added code for checking/adjusting linear scan
 *                      params (it gets a little messy !)
 * .04  08-25-94  nda   Added check of scan positions vs Positioner Control
 *                      Limits
 * .05  08-29-94  nda   Added "viewScanPos" that puts desired positions
 *                      in D1 array any time a scan parameter is changed
 * .06  10-03-94  nda   added code for enumerated string .CMND. Changed 
 *                      .EXSC to a SHORT in record definition. Added VERSION
 *                      for .VERS field (1.06) to keep track of version. 
 * .07  10-21-94  nda   changed linear scan parameter algorithms so changing
 *                      start/end modifies step/width unless frozen. This
 *                      seems more intuitive.
 * .08  12-06-94  nda   added support for .FFO .When set to 1, frzFlag values
 *                      are saved in recPvtStruct. Restored when FFO set to 0.
 * .09  02-02-95  nda   fixed order of posting monitors to be what people
 *                      expect (i.e. .cpt, .pxdv, .dxcv)
 * .10  02-10-95  nda   fixed on-the-fly so 1st step is to end position
 * .11  02-21-95  nda   added "Return To Start" flag. If set, positioners
 *                      will be commanded to the start pos after the scan.
 * .12  03-02-95  nda   added positioner readback arrays (PxRA). These arrays
 *                      will contain actual readback positions (if RxPV are
 *                      defined. If not, the desired positions will be loaded
 *                      into them.
 * .13  03-02-95  nda   Post .val field when a new point is complete during a
 *                      scan. This will assist in poin by point plots.
 * .14  03-02-95  nda   Bug fix on filling PxRA's. ALWAYS CALL CHECK_MOTORS.
 * .15  03-15-95  nda   If no readback PV (RxPV) is specified, copy desired
 *                      value (PxDV) to current value (RxCV). Now, plotting
 *                      programs can always monitor RxCV.
 * .16  04-03-95  nda   If PV field = DESC, change to VAL                  
 * 
 * 3.00 08-28-95  nda   > Significant rewrite to add Channel Access
 *                      for dynamic links using recDynLink.c . All
 *                      inputs are now "monitored" via Channel Access.
 *                      > Name Valid field is used to keep track of PV
 *                      connection status: 0-PV_OK,
 *                      1-NotConnected,  2-NO_PV
 *                      > added relative/absolute mode on positioners 
 *                      > added retrace options of stay/start/prior 
 *                      > supports 15 detectors
 *                      > added "preview scan"
 *                      > added "Before Scan" and "After Scan" links
 *                      > fetch DRVL/DRVH/prec/egu from dynamic links
 *                      > when posting arrays at end of scan, use DBE_LOG, too
 *                      > Record timeStamp only at beginning of scan
 *                      > Record positioner readbacks when reading detectors
 *                      > "TIME" or "time" in a readback PV records time 
 *
 * 3.01 03-07-96  nda   Rearrange order of precedence of linear scan parameters
 *                      per Tim Mooney's memo duplicated at end of this file
 *
 * 3.02 03-12-96  nda   If scan request and any PV's are PV_NC, callback 3
 *                      seconds later ... if still PV_NC, abort. Also, if any
 *                      control PV's (positioners, readbacks, detctr triggers)
 *                      lose connection during a scan, abort scan.
 *                      
 * 3.02 03-12-96  nda   Changed detector reading to accommodate the possibility
 *                      of connections "coming and going" during a scan: If a
 *                      detector PV is valid at the beginning of the scan, the
 *                      value will be read each point. If the PV is not 
 *                      connected for a given point, a zero will be stored. 
 *                      Previously, nothing would be stored, which left old
 *                      data in the array for those points. Any detector PV's
 *                      added during a scan will be ignored until the next scan 
 *
 * 3.02 03-12-96  nda   Writing a 3 to the .CMND field will clear all PV's,  
 *                      set all positioners to LINEAR & ABSOLUTE, resets all
 *                      freeze flags & .FFO, sets .SCAN to Passive,RETRACE=stay
 *
 * 3.03 04-26-96  tmm   Writing a 4 to the .CMND field will clear positioner
 *                      PV's, set all positioners to LINEAR & ABSOLUTE, reset
 *                      all freeze flags & .FFO, set .SCAN to Passive, set
 *                      RETRACE=stay
 *
 * 3.13 _______   nda   changes for 3.13
 *      03-15-96        - remove pads from detFields structure
 *                      - use fieldOffset indexes
 */

#define VERSION 3.13



#include	<vxWorks.h> 
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<math.h>
#include	<tickLib.h>
#include	<semLib.h>
#include	<taskLib.h>
#include        <wdLib.h>
#include        <sysLib.h>

#include "dbDefs.h"
#include "epicsPrint.h"
#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbEvent.h>
#include	<dbScan.h>
#include        <dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<choiceScan.h>
#define GEN_SIZE_OFFSET
#include	<scanRecord.h>
#undef  GEN_SIZE_OFFSET
#include	<callback.h>
#include	<taskwd.h>
#include        <drvTS.h>   /* also includes timers.h and tsDefs.h */

#include        <recDynLink.h>


/***************************
  Declare constants
***************************/
#define DEF_WF_SIZE	100
#define PVN_SIZE        40

#define POSITIONER      0
#define READBACK        1
#define DETECTOR        2
#define TRIGGER         3
#define FWD_LINK        4

#define NUM_POS         4
#define NUM_RDKS        4
#define NUM_DET         15
#define NUM_TRGS        2 
#define NUM_MISC        2  /* Before Scan PV, After Scan PV */
#define NUM_PVS         (NUM_POS + NUM_RDKS + NUM_DET + NUM_TRGS + NUM_MISC)
/* Determine total # of dynLinks (each positioner requires two: IN & OUT */
#define NUM_LINKS       (NUM_PVS + NUM_POS)

/* Define special type constants for first and last dynamic PV name */
#define FIRST_PV    REC_SC_P1
#define LAST_PV     REC_SC_AS

/* Predefine some index numbers */
#define P1_IN       0
#define P2_IN       1
#define P3_IN       2
#define P4_IN       3
#define R1_IN       (NUM_POS)
#define R2_IN       (NUM_POS + 1)
#define R3_IN       (NUM_POS + 2)
#define R4_IN       (NUM_POS + 3)
#define D1_IN       (NUM_POS + NUM_RDKS)

#define T1_OUT      (D1_IN + NUM_DET)
#define T2_OUT      (T1_OUT + 1)

#define BS_OUT      (T1_OUT + NUM_TRGS)
#define AS_OUT      (BS_OUT + 1)

/* Added four recDynLinks at the end of PV's for positioner outs */
#define P1_OUT      (NUM_PVS)
#define P2_OUT      (P1_OUT + 1)
#define P3_OUT      (P2_OUT + 1)
#define P4_OUT      (P3_OUT + 1)

#define MIN_MON         3 /* # of ticks between monitor postings. 3 = 50 ms */

#define PV_OK           REC_SCAN_DYNL_OK     /* from choiceScan.h */
#define PV_NC           REC_SCAN_DYNL_NC     /* from choiceScan.h */
#define NO_PV           REC_SCAN_DYNL_NO_PV  /* from choiceScan.h */

#define A_BUFFER        0
#define B_BUFFER        1

#define IDLE            0
#define INIT_SCAN       1 
#define BEFORE_SCAN     2 
#define MOVE_MOTORS     3
#define CHECK_MOTORS    4
#define TRIG_DETCTRS    5
#define READ_DETCTRS    6
#define AFTER_SCAN      7 
#define PREVIEW         8 
#define SCAN_PENDING    9 

#define CLEAR_MSG       0
#define CHECK_LIMITS    1
#define PREVIEW_SCAN    2 /* Preview the SCAN positions */
#define CLEAR_RECORD    3 /* Clear PV's, frzFlags, modes, abs/rel, etc */
#define CLEAR_POSITIONERS     4 /* Clear positioner PV's, frzFlags, modes, abs/rel, etc */

#define DBE_VAL_LOG     (DBE_VALUE | DBE_LOG)


/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
static long get_enum_str();
static long get_enum_strs();
static long put_enum_str();
static long get_graphic_double();
static long get_control_double();
static long get_alarm_double(); 
 
struct rset scanRSET={
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
	get_alarm_double
};

typedef struct recDynLinkPvt {
    struct scanRecord *pscan;     /* pointer to scan record */
    unsigned short     linkIndex; /* specifies which dynamic link */
    unsigned short     linkType;  /* Positioner, Rdbk, Trig, Det */
    struct   dbAddr   *pAddr;     /* Pointer to dbAddr for local PV's */
    long               dbAddrNv;  /* Zero if dbNameToAddr succeeded */
    unsigned long      nelem;     /* # of elements for this PV  */
    unsigned short     ts;        /* if 1, use timestamp as value */
    short              fldOffset; /* For arrays, offset from pscan, used
                                     for get_array_info, cvt_dbaddr */
}recDynLinkPvt;

typedef struct dynLinkInfo {
    DBRunits
    DBRprecision
    DBRgrDouble
    DBRctrlDouble
    float        value;
}dynLinkInfo;

typedef struct posBuffers {
               double *pFill;
               double *pBufA;
               double *pBufB;
} posBuffers;
    
typedef struct detBuffers {
               float  *pFill;
               float  *pBufA;
               float  *pBufB;
               int     size;
               short   vector;
} detBuffers;
    

/* the following structure must match EXACTLY with the order and type of
   fields defined in scanRecord.h for each positioner (even including
   the "Created Pad"s  */

typedef struct posFields {
        double          p_pp;           /* P1 Previous Position */
        double          p_cv;           /* P1 Current Value */
        double          p_dv;           /* P1 Desired Value */
        double          p_lv;           /* P1 Last Value Posted */
        double          p_sp;           /* P1 Start Position */
        double          p_si;           /* P1 Step Increment */
        double          p_ep;           /* P1 End Position */
        double          p_cp;           /* P1 Center Position */
        double          p_wd;           /* P1 Scan Width */
        double          r_cv;           /* P1 Readback Value */
        double          r_lv;           /* P1 Rdbk Last Val Pst */
        double          r_dl;           /* P1 Readback Delta */
        double          p_hr;           /* P1 High Oper Range */
        double          p_lr;           /* P1 Low  Oper Range */
        double *         p_pa;          /* P1 Step Array */
        double *         p_ra;          /* P1 Readback Array */
        unsigned short  p_fs;           /* P1 Freeze Start Pos */
        unsigned short  p_fi;           /* P1 Freeze Step Inc */
        unsigned short  p_fe;           /* P1 Freeze End Pos */
        unsigned short  p_fc;           /* P1 Freeze Center Pos */
        unsigned short  p_fw;           /* P1 Freeze Width */
        unsigned short  p_sm;           /* P1 Step Mode */
        unsigned short  p_ar;           /* P1 Absolute/Relative */
        char            p_eu[16];               /* P1 Engineering Units */
        short           p_pr;           /* P1 Display Precision */
}posFields;
        
/* the following structure must match EXACTLY with the order and type of
   fields defined in scanRecord.h for each detector (even including
   the "Created Pad"s  */

typedef struct detFields {
        double          d_hr;           /* D1 High Oper Range */
        double          d_lr;           /* D1 Low  Oper Range */
        float *          d_da;          /* D1 Data Array */
        float           d_cv;           /* D1 Current Value */
        float           d_lv;           /* D1 Last Value Posted */
        unsigned long   d_ne;           /* D1 # of Elements/Pt */
        char            d_eu[16];               /* D1 Engineering Units */
        short           d_pr;           /* D1 Display Precision */
}detFields;


typedef struct recPvtStruct {
    /* THE CODE ASSUMES doPutsCallback is THE FIRST THING IN THIS STRUCTURE!*/
    CALLBACK           doPutsCallback;/* do output links callback structure */
    WDOG_ID            wd_id;       /* watchdog for delay if PV_NC and .EXSC */
    struct scanRecord *pscan;        /* ptr to record which needs work done */
    short              phase;        /* what to do to the above record */
    short              validBuf;      /* which data array buffer is valid */
    recDynLink         caLinkStruct[NUM_LINKS]; /* req'd for recDynLink */
    posBuffers         posBufPtr[NUM_POS];
    detBuffers         detBufPtr[NUM_DET];
    unsigned short     valPosPvs;    /* # of valid positioners */
    unsigned short     valRdbkPvs;   /* # of valid Readbacks   */
    unsigned short     valDetPvs;    /* # of valid Detectors */
    unsigned short     valTrigPvs;   /* # of valid Det Triggers  */
    unsigned short     acqDet[NUM_DET]; /* which detectors to acquire */
    dynLinkInfo       *pDynLinkInfo;
    short              pffo;         /* previous state of ffo */
    short              fpts;         /* backup copy of all freeze flags */
    unsigned short     prevSm[NUM_POS];   /* previous states of p_sm */
    posFields          posParms[NUM_POS]; /* backup copy of all pos. parms */ 
    unsigned long      tablePts[NUM_POS]; /* # of pts loaded in P_PA */
    short              onTheFly;
    float             *nullArray;
    unsigned long      tickStart;    /* used to time the scan */
    char               movPos[NUM_POS]; /* used to indicate interactive move */
    unsigned char      scanErr;
    unsigned char      badOutputPv;  /* positioner, detector trig, readbk */
    unsigned char      badInputPv;   /* detector BAD_PV */
    char               nptsCause;    /* who caused the "# of points to change:
                                        -1:operator; 0-3 Positioners*/
}recPvtStruct;

/*  forward declarations */

static void checkMonitors();
static long initScan();
static void contScan();
static void endScan();
static void doPuts();
static void adjLinParms();
static void changedNpts();
static long checkScanLimits();
static void saveFrzFlags();
static void resetFrzFlags();
static void restoreFrzFlags();

static void previewScan(struct scanRecord *pscan);

static void lookupPV(struct scanRecord *pscan, unsigned short i); 
static void checkConnections(struct scanRecord *pscan); 
static void pvSearchCallback(recDynLink *precDynLink);
static void posMonCallback(recDynLink *precDynLink);
static void restorePosParms(struct scanRecord *pscan, unsigned short i);
static void savePosParms(struct scanRecord *pscan, unsigned short i);
static void zeroPosParms(struct scanRecord *pscan, unsigned short i);

/* variables ... */
long    recScanDebug=0;
long    recScanViewPos=0; 
long    recScanDontCheckLimits=0; 


static long init_record(pscan,pass)
    struct scanRecord	*pscan;
    int pass;
{
    recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields    *pPosFields;
    detFields    *pDetFields;

    char            *ppvn[PVN_SIZE];
    unsigned short  *pPvStat;
    unsigned short  i;

    recDynLinkPvt   *puserPvt;


    if (pass==0) {
      pscan->vers = VERSION;
      if(pscan->mpts < 100) pscan->mpts = DEF_WF_SIZE;

      /* First time through, rpvt needs initialized */
      pscan->rpvt = calloc(1, sizeof(recPvtStruct));
      precPvt = (recPvtStruct *)pscan->rpvt;

      precPvt->phase = IDLE;

      precPvt->prevSm[0] = pscan->p1sm;
      precPvt->prevSm[1] = pscan->p2sm;
      precPvt->prevSm[2] = pscan->p3sm;
      precPvt->prevSm[3] = pscan->p4sm;

      precPvt->pscan = pscan;

      precPvt->pDynLinkInfo = (dynLinkInfo *)calloc(1, sizeof(dynLinkInfo));

      /* init the private area of the caLinkStruct's   */
      for(i=0;i<(NUM_LINKS); i++) {
        precPvt->caLinkStruct[i].puserPvt 
                 = calloc(1,sizeof(struct recDynLinkPvt));
        puserPvt = (recDynLinkPvt *)precPvt->caLinkStruct[i].puserPvt;
        puserPvt->pscan = pscan;
        puserPvt->linkIndex = i;
        puserPvt->pAddr = calloc(1, sizeof(struct dbAddr));
        puserPvt->dbAddrNv  = -1;
        if     (i < R1_IN)  puserPvt->linkType = POSITIONER;
        else if(i < D1_IN)  puserPvt->linkType = READBACK;
        else if(i < T1_OUT) puserPvt->linkType = DETECTOR;
        else if(i < BS_OUT) puserPvt->linkType = TRIGGER;
        else if(i < P1_OUT) puserPvt->linkType = FWD_LINK;
      }

      pscan->p1pa = (double *) calloc(pscan->mpts, sizeof(double));
      pscan->p2pa = (double *) calloc(pscan->mpts, sizeof(double));
      pscan->p3pa = (double *) calloc(pscan->mpts, sizeof(double));
      pscan->p4pa = (double *) calloc(pscan->mpts, sizeof(double));

      /* Readbacks need double buffering. Allocate space and initialize */
      /* fill pointer and readback array pointers */
      precPvt->validBuf = A_BUFFER;
      pPosFields = (posFields *)&pscan->p1pp;
      for(i=0; i<NUM_RDKS; i++, pPosFields++) {
        precPvt->posBufPtr[i].pBufA =
            (double *) calloc(pscan->mpts, sizeof(double));
        precPvt->posBufPtr[i].pBufB =
            (double *) calloc(pscan->mpts, sizeof(double));

        pPosFields->p_ra = precPvt->posBufPtr[i].pBufA;
        precPvt->posBufPtr[i].pFill = precPvt->posBufPtr[i].pBufB;
      }

      /* allocate arrays for 4 detectors assuming nelem = 1 */
      /* (previewScan uses the first four detectors) */
      /* For now, just point other detectors to a NULL array */

      precPvt->nullArray = (float *) calloc(pscan->mpts, sizeof(float));
      pDetFields = (detFields *)&pscan->d1hr;
      for(i=0; i<NUM_DET; i++, pDetFields++) {
          puserPvt = (recDynLinkPvt *)precPvt->caLinkStruct[D1_IN+i].puserPvt;
          if(i<4) {
              precPvt->detBufPtr[i].pBufA = 
                  (float *) calloc(pscan->mpts, sizeof(float));
              precPvt->detBufPtr[i].pBufB = 
                  (float *) calloc(pscan->mpts, sizeof(float));
              puserPvt->nelem = 1;
          }
          else {
              precPvt->detBufPtr[i].pBufA = precPvt->nullArray;
              precPvt->detBufPtr[i].pBufB = precPvt->nullArray;
              puserPvt->nelem = 0;
          }
          pDetFields->d_da = precPvt->detBufPtr[i].pBufA;
          precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufB;
      }

      return(0);
    }

    callbackSetCallback(doPuts, &precPvt->doPutsCallback);
    callbackSetPriority(pscan->prio, &precPvt->doPutsCallback);
    precPvt->wd_id = wdCreate();

    /* initialize all linear scan fields */
    precPvt->nptsCause = -1; /* resolve all positioner parameters */
    changedNpts(pscan);

    if(pscan->ffo) {
       saveFrzFlags(pscan);
       resetFrzFlags(pscan);
    }

    /* init field values */
    pscan->exsc = 0;
    pscan->pxsc = 0;

    *ppvn = &pscan->p1pv[0];
    pPvStat = &pscan->p1nv;

    /* check all dynLink PV's for non-NULL  */
    for(i=0;i<NUM_PVS; i++, pPvStat++, *ppvn += PVN_SIZE) {
        if(*ppvn[0] != NULL) {
            *pPvStat = PV_NC;
            lookupPV(pscan, i);
        }
        else {
            *pPvStat = NO_PV;
        }
    }

    
    return(0);
}

static long process(pscan)
	struct scanRecord	*pscan;
{
        recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
        long	status = 0;


	/* Decide what to do. */
        if(precPvt->phase == SCAN_PENDING) return(status);
        /* Brand new scan */
	if ((pscan->pxsc == 0) && (pscan->exsc == 1)) { 
          precPvt->phase = INIT_SCAN;
          /* use TimeStamp to record beginning of scan */
          recGblGetTimeStamp(pscan);
          initScan(pscan);
        } 

        /* Before Scan Fwd Link is complete, initScan again */
        else if(precPvt->phase == BEFORE_SCAN) {
          initScan(pscan);
        }

        else if ((pscan->pxsc == 1) && (pscan->exsc == 0)) {
          sprintf(pscan->smsg,"Scan aborted by operator");
          db_post_events(pscan,&pscan->smsg,DBE_VALUE);
          endScan(pscan);
        }

        else if (pscan->exsc == 1) {
          if(precPvt->badOutputPv) {
              pscan->alrt = 1;
              db_post_events(pscan,&pscan->alrt, DBE_VALUE);
              sprintf(pscan->smsg,"Lost connection to Control PV");
              db_post_events(pscan,&pscan->smsg,DBE_VALUE);
              pscan->exsc = 0;
              db_post_events(pscan,&pscan->exsc, DBE_VALUE);
              endScan(pscan);
          }
          else {
              contScan(pscan);
          }
        }

        checkMonitors(pscan);

        /* do forward link on last scan aquisition */
        if ((pscan->pxsc == 1) && (pscan->exsc == 0)) {
	    recGblFwdLink(pscan);   /* process the forward scan link record */
            if(recScanDebug) {
              printf("Scan Time = %.2f ms\n",
                     (float)((tickGet()-(precPvt->tickStart))*16.67)); 
            }

	}

        pscan->pxsc = pscan->exsc;
        recGblResetAlarms(pscan);

	pscan->pact = FALSE;
	return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    struct scanRecord  	*pscan = (struct scanRecord *)(paddr->precord);
    recPvtStruct        *precPvt = (recPvtStruct *)pscan->rpvt;
    recDynLinkPvt       *puserPvt;
    posFields           *pPosFields;
    int           	 special_type = paddr->special;
    char                *ppvn[PVN_SIZE];
    unsigned short      *pPvStat;
    unsigned short       oldStat;
    short                i = -1;
    long status;
    unsigned char        prevAlrt;

    if(!after) {
        precPvt->pffo = pscan->ffo; /* save previous ffo flag */
        return(0);
    }

    if(recScanDebug > 5) printf("special(),special_type = %d\n",special_type); 
    switch(special_type) {
    case(SPC_MOD):   
        if(paddr->pfield==(void *)&pscan->exsc) {
            /* If aborting, scanOnce immediately */
            if(!pscan->exsc) {
                scanOnce(pscan);
            /* If .EXSC, only scanOnce if IDLE OR PREVIEW */
            } else if((precPvt->phase == IDLE) || (precPvt->phase == PREVIEW)) {
                checkConnections(pscan);
                if(precPvt->badOutputPv || precPvt->badInputPv) {
                    /* postpone the scan for 5 seconds via watchdog */
                    pscan->alrt = 1;
                    db_post_events(pscan,&pscan->alrt,DBE_VALUE);
		    strcpy(pscan->smsg,"Some PV's not connected ...");
                    db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                    precPvt->phase = SCAN_PENDING;
                    wdStart(precPvt->wd_id,(3 * sysClkRateGet()), 
                            (FUNCPTR)callbackRequest, 
                            (int)(&precPvt->doPutsCallback));
                }
                else {
                    pscan->alrt = 0;
                    db_post_events(pscan,&pscan->alrt, DBE_VALUE);
                    scanOnce(pscan);
                }
            }
            return(0);
        } else if(paddr->pfield==(void *)&pscan->cmnd) { 
            if(pscan->cmnd == CLEAR_MSG) {
                pscan->alrt = 0;
                db_post_events(pscan,&pscan->alrt,DBE_VALUE);
		strcpy(pscan->smsg,"");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
            }
            else if((pscan->cmnd == CHECK_LIMITS) && !(pscan->exsc)) {
                prevAlrt = pscan->alrt;
                pscan->alrt = 0;
                checkScanLimits(pscan);
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                if(pscan->alrt != prevAlrt) {
                    db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                } 
            }
            else if((pscan->cmnd == PREVIEW_SCAN) && !(pscan->exsc)) {
                /* get_array_info() needs to know that we're previewing */
                precPvt->phase = PREVIEW;
                previewScan(pscan);
            }
            else if(((pscan->cmnd == CLEAR_RECORD) ||
                     (pscan->cmnd == CLEAR_POSITIONERS)) &&
                    !(pscan->exsc)) {
                /* clear PV's, frzFlags, modes, etc */
                pscan->scan = 0;
                db_post_events(pscan,&pscan->scan,DBE_VALUE);
                resetFrzFlags(pscan);
                pscan->p1sm = 0;
                db_post_events(pscan,&pscan->p1sm,DBE_VALUE);
                pscan->p1ar = 0;
                db_post_events(pscan,&pscan->p1ar,DBE_VALUE);
                pscan->p2sm = 0;
                db_post_events(pscan,&pscan->p2sm,DBE_VALUE);
                pscan->p2ar = 0;
                db_post_events(pscan,&pscan->p2ar,DBE_VALUE);
                pscan->p3sm = 0;
                db_post_events(pscan,&pscan->p3sm,DBE_VALUE);
                pscan->p3ar = 0;
                db_post_events(pscan,&pscan->p3ar,DBE_VALUE);
                pscan->p4sm = 0;
                db_post_events(pscan,&pscan->p4sm,DBE_VALUE);
                pscan->p4ar = 0;
                db_post_events(pscan,&pscan->p4ar,DBE_VALUE);
                pscan->pasm = 0;
                db_post_events(pscan,&pscan->pasm,DBE_VALUE);
                pscan->ffo = 0;
                db_post_events(pscan,&pscan->ffo,DBE_VALUE);
                for(i=0; i<NUM_PVS; i++) {
                    puserPvt = (recDynLinkPvt *)precPvt->caLinkStruct[i].puserPvt;
                    if((pscan->cmnd == CLEAR_RECORD) ||
                       ((pscan->cmnd == CLEAR_POSITIONERS) &&
                       (puserPvt->linkType == POSITIONER))) {
                        /* clear this PV */
                        pPvStat = &pscan->p1nv + i; /* pointer arithmetic */
                        oldStat = *pPvStat;
                        *ppvn = &pscan->p1pv[0] + (i*PVN_SIZE);
                        *ppvn[0] = NULL;
                        db_post_events(pscan,*ppvn,DBE_VALUE);
                        if(*pPvStat != NO_PV) {
                           /* PV is now NULL but didn't used to be */
                           *pPvStat = NO_PV;              /* PV just cleared */
                            if(*pPvStat != oldStat) {
                               db_post_events(pscan,pPvStat,DBE_VALUE);
                            }
                            if(puserPvt->dbAddrNv) { /*Clear if non-local */
                                recDynLinkClear(&precPvt->caLinkStruct[i]);
                                 /* Positioners have two recDynLinks */
                                if(i<NUM_POS)
                                    recDynLinkClear(&precPvt->caLinkStruct[i+NUM_PVS]);
                            }
                            else puserPvt->dbAddrNv = -1;
                        }
                        else {
                            /* PV is NULL, but previously MAY have been "time" */
                            puserPvt->ts = 0;
                        }
                    }
                }
            }
            else if(paddr->pfield==(void *)&pscan->prio) {
                callbackSetPriority(pscan->prio, &precPvt->doPutsCallback);
                return(0);
            }
            return(0);
        } 
        /* Check to see if it was a command to send to the positioner */
        else if(paddr->pfield==(void *)&pscan->p1dv) i = P1_IN;
        else if(paddr->pfield==(void *)&pscan->p2dv) i = P2_IN; 
        else if(paddr->pfield==(void *)&pscan->p3dv) i = P3_IN; 
        else if(paddr->pfield==(void *)&pscan->p4dv) i = P4_IN; 

        /* if a positioner command and not scanning .... */
        if((i>=0) && ((precPvt->phase==IDLE) || (precPvt->phase==PREVIEW))) {
           pPvStat = &precPvt->pscan->p1nv + i;
           pPosFields = (posFields *)&precPvt->pscan->p1pp + i;
           if(*pPvStat == PV_OK) {
               puserPvt = precPvt->caLinkStruct[i].puserPvt;
               if(puserPvt->dbAddrNv) {
                  status = recDynLinkPut(&precPvt->caLinkStruct[i+P1_OUT],
                              &(pPosFields->p_dv), 1);
               } else {
                  /* Must use the callback task ... */
                  precPvt->movPos[i] = 1;
                  callbackRequest(&precPvt->doPutsCallback);
               }
           }
        }


        break;

    case(SPC_SC_MO): /* Step Mode changed for a positioner */
        
        pPosFields = (posFields *)&precPvt->pscan->p1pp;
        for(i=0; i<NUM_POS; i++, pPosFields++) {
            if(paddr->pfield==(void *)&pPosFields->p_sm) {
                /* Entering Table mode ? */
                if((pPosFields->p_sm == REC_SCAN_MO_TAB) &&
                   (precPvt->prevSm[i] != REC_SCAN_MO_TAB)) {
                    savePosParms(pscan, (unsigned short) i);
                    zeroPosParms(pscan, (unsigned short) i);
                    precPvt->prevSm[i] = pPosFields->p_sm;
                    if(precPvt->tablePts[i] < pscan->npts) {
                      sprintf(pscan->smsg,"Pts in P%d Table < # of Steps",i+1);
                      db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                      if(!pscan->alrt) {
                          pscan->alrt = 1;
                          db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                      }
                    }
                }
                /* Leaving Table mode ? */
                else if((pPosFields->p_sm != REC_SCAN_MO_TAB) &&
                        (precPvt->prevSm[i] == REC_SCAN_MO_TAB)) {
                    restorePosParms(pscan, (unsigned short) i);
                    prevAlrt = pscan->alrt;
                    precPvt->nptsCause = -1;
                    changedNpts(pscan);
                    db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                    if(pscan->alrt != prevAlrt) {
                        db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                    }
                    precPvt->prevSm[i] = pPosFields->p_sm;
                }
            }
        }
        break;
            
    case(SPC_SC_S):  /* linear scan parameter change */
    case(SPC_SC_I):  
    case(SPC_SC_E):  
    case(SPC_SC_C):  
    case(SPC_SC_W):  
        /* resolve linear scan parameters affected by this fields change */
        prevAlrt = pscan->alrt;
        pscan->alrt = 0;
	strcpy(pscan->smsg,"");
        adjLinParms(paddr);       
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(pscan->alrt != prevAlrt) {
            db_post_events(pscan,&pscan->alrt,DBE_VALUE);
        }
        if(recScanViewPos) {
            previewScan(pscan);
        }
        break;

    case(SPC_SC_N):  
        /* adjust all linear scan parameters per new # of steps */
        if(pscan->npts > pscan->mpts) {
            pscan->npts = pscan->mpts;
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
        }
        prevAlrt = pscan->alrt;
        pscan->alrt = 0;
	strcpy(pscan->smsg,"");
        precPvt->nptsCause = -1; /* resolve all positioner parameters */
        changedNpts(pscan);       
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(pscan->alrt != prevAlrt) {
            db_post_events(pscan,&pscan->alrt,DBE_VALUE);
        } 
        if(recScanViewPos) {
            previewScan(pscan);
        } 
        break;

    case(SPC_SC_FFO):
        /* Freeze Flag Override field */
        if((pscan->ffo) && (!precPvt->pffo)) {
           saveFrzFlags(pscan);
           resetFrzFlags(pscan);
        }
        else if(!pscan->ffo && precPvt->pffo) /* only on 1->0 */
           restoreFrzFlags(pscan);
    
        break;

    case(SPC_SC_F):
        /* Freeze Flag Override field */
        if(pscan->ffo)
           resetFrzFlags(pscan);

        break;


    default:
        /* Check for a dynamic link PV change */
        if((special_type >= FIRST_PV) && (special_type <= LAST_PV)) {
            i = special_type - FIRST_PV;
            puserPvt = (recDynLinkPvt *)precPvt->caLinkStruct[i].puserPvt;
            pPvStat = &pscan->p1nv + i; /* pointer arithmetic */
            oldStat = *pPvStat;
            *ppvn = &pscan->p1pv[0] + (i*PVN_SIZE);
            if(*ppvn[0] != NULL) {
                if(recScanDebug > 5) printf("Search during special \n");
                *pPvStat = PV_NC;
                /* force flags to indicate PV_NC until callback happens */
                if((i<D1_IN) || (i==T1_OUT) || (i==T2_OUT)) {
                    precPvt->badOutputPv = 1;
                }
                else { 
                    precPvt->badInputPv = 1;
                }
                /* need to post_event before recDynLinkAddXxx because
                   SearchCallback could happen immediatley */
                if(*pPvStat != oldStat) {
                    db_post_events(pscan,pPvStat,DBE_VALUE);
                }
                lookupPV(pscan, i);
            }
            else if(*pPvStat != NO_PV) {
                /* PV is now NULL but didn't used to be */
                *pPvStat = NO_PV;              /* PV just cleared */
                if(*pPvStat != oldStat) {
                    db_post_events(pscan,pPvStat,DBE_VALUE);
                }
                if(puserPvt->dbAddrNv) { /*Clear if non-local */
                    recDynLinkClear(&precPvt->caLinkStruct[i]);
                    /* Positioners have two recDynLinks */
                    if(i<NUM_POS) 
                        recDynLinkClear(&precPvt->caLinkStruct[i+NUM_PVS]);
                }
                else puserPvt->dbAddrNv = -1;
            }
            else {
                /* PV is NULL, but previously MAY have been "time" */
                puserPvt->ts = 0;
            }
         }
    }
return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct scanRecord *pscan=(struct scanRecord *)paddr->precord;
    
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    detFields          *pDetFields = (detFields *)&pscan->d1hr;
    short               fieldOffset;
    unsigned short      i;

    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;

    /* for the double buffered arrays, must use the field offset
       because you don't know what the pointer will be */

    for(i=0; i < NUM_DET; i++, pDetFields++) {
        if(((char *)&pDetFields->d_da - (char *)pscan) == fieldOffset) {
            paddr->pfield = pDetFields->d_da;
            paddr->no_elements = pscan->mpts;
            paddr->field_type = DBF_FLOAT;
            paddr->field_size = sizeof(float);
            paddr->dbr_field_type = DBF_FLOAT;
            return(0);
        }
    }

    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if(((char *)&pPosFields->p_pa - (char *)pscan) == fieldOffset) { 
               paddr->pfield = (void *)(pPosFields->p_pa);
               paddr->no_elements = pscan->mpts;
               paddr->field_type = DBF_DOUBLE;
               paddr->field_size = sizeof(double);
               paddr->dbr_field_type = DBF_DOUBLE;
        } else if(((char *)&pPosFields->p_ra - (char *)pscan) == fieldOffset) {
               paddr->pfield = (void *)(pPosFields->p_ra);
               paddr->no_elements = pscan->mpts;
               paddr->field_type = DBF_DOUBLE;
               paddr->field_size = sizeof(double);
               paddr->dbr_field_type = DBF_DOUBLE;
        }
    }
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long          *no_elements;
    long          *offset;
{
    struct scanRecord  *pscan=(struct scanRecord *)paddr->precord;
    recPvtStruct       *precPvt = (recPvtStruct *)pscan->rpvt;
    detFields          *pDetFields = (detFields *)&pscan->d1hr;
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    short               fieldOffset;
    unsigned short     *pPvStat; 
    unsigned short           i;

    /* This routine is called because someone wants an array. Determine
       which array they are interested by comparing the address of the 
       field to the array pointers */          

    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;

    pPvStat = &precPvt->pscan->d1nv;
    for(i=0; i < NUM_DET; i++, pDetFields++, pPvStat++) {
        if(((char *)&pDetFields->d_da - (char *)pscan) == fieldOffset) {
            if((precPvt->acqDet[i]) ||
               ((i<NUM_POS) && (precPvt->phase == PREVIEW))) {
                if(precPvt->validBuf)
                    paddr->pfield = precPvt->detBufPtr[i].pBufB;
                else
                    paddr->pfield = precPvt->detBufPtr[i].pBufA;
            } else {
                paddr->pfield = precPvt->nullArray;
            }
            *no_elements = pscan->mpts;
            *offset = 0;
            return(0);
        }
    }
    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if(((char *)&pPosFields->p_ra - (char *)pscan) == fieldOffset) {
            if(precPvt->validBuf) 
                paddr->pfield = precPvt->posBufPtr[i].pBufB;
            else
                paddr->pfield = precPvt->posBufPtr[i].pBufA;
                
            *no_elements = pscan->mpts;
            *offset = 0;
            return(0);
        }
    }

    if (paddr->pfield == pscan->p1pa) {
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if (paddr->pfield == pscan->p2pa) {
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if (paddr->pfield == pscan->p3pa) {
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if (paddr->pfield == pscan->p4pa) {
        *no_elements = pscan->mpts;
        *offset = 0;
    } else {
        *no_elements = 0;
        *offset = 0;
    }

    return(0);
}

static long put_array_info(paddr,nNew)
    struct dbAddr *paddr;
    long          nNew;
{
    struct scanRecord  *pscan=(struct scanRecord *)paddr->precord;
    recPvtStruct       *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    short               fieldOffset;
    unsigned short      i;

    /* This routine is called because someone wrote a table to the
       "positioner" array p_pa. Determine which positioner and store
       nelem for future use. Also check against current npts  */

    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;
    
    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if(((char *)&pPosFields->p_pa - (char *)pscan) == fieldOffset) {
            precPvt->tablePts[i] = nNew;
            if(nNew < pscan->npts) {
                sprintf(pscan->smsg,"Pts in P%d Table < # of Steps", i+1);
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                if(!pscan->alrt) {
                    pscan->alrt = 1;
                    db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                }
            } else {
		strcpy(pscan->smsg,"");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                if(pscan->alrt) {
                    pscan->alrt = 0;
                    db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                }
            }
        return(0);
        }
    }
    return(0);
}


static long get_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct scanRecord     *pscan=(struct scanRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pscan->cmnd)
    {
        sprintf(pstring, "%d",pscan->cmnd);
    }
    else
    {
        strcpy(pstring,"No string");
    }
    return(0);
}
static long get_enum_strs(paddr,pes)
    struct dbAddr *paddr;
    struct dbr_enumStrs *pes;
{
    struct scanRecord     *pscan=(struct scanRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pscan->cmnd)
    {
        memset(pes->strs,'\0',sizeof(pes->strs));
        strncpy(pes->strs[0],"0-Clear Msg",sizeof("0-Clear Msg"));
        strncpy(pes->strs[1],"1-Check Limits",sizeof("1-Check Limits"));
        pes->no_str = 2;
    }
    else
    {
        strcpy(pes->strs[0],"No string");
        pes->no_str=1;
    }

    return(0);
}

static long put_enum_str(paddr,pstring)
    struct dbAddr *paddr;
    char          *pstring;
{
    struct scanRecord     *pscan=(struct scanRecord *)paddr->precord;

    if(paddr->pfield==(void *)&pscan->cmnd)
    {
        if(sscanf(pstring,"%hu",&pscan->cmnd)<=0)
            return(S_db_badChoice);
    }
    else
    {
        return(S_db_badChoice);
    }

    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct scanRecord  *pscan=(struct scanRecord *)paddr->precord;
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    detFields          *pDetFields = (detFields *)&pscan->d1hr;
    short               fieldOffset;
    short               i;

   
    /* Use the field offset to see which detector or positioner it is */
    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;

    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if((fieldOffset >= ((char *)&pPosFields->p_pp - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pPosFields->p_pr - (char *)pscan))) {
               strncpy(units, pPosFields->p_eu, 7);
               units[7] = NULL;
               return(0);
        }
    }
    for(i=0; i < NUM_DET; i++, pDetFields++) {
        if((fieldOffset >= ((char *)&pDetFields->d_hr - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pDetFields->d_pr - (char *)pscan))) {
               strncpy(units, pDetFields->d_eu, 7);
               units[7] = NULL;
               return(0);
        }
    }

    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct scanRecord  *pscan=(struct scanRecord *)paddr->precord;
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    detFields          *pDetFields = (detFields *)&pscan->d1hr;
    short               fieldOffset;
    short               i;

    
    /* Use the field offset to see which detector or positioner it is */
    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;

    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if((fieldOffset >= ((char *)&pPosFields->p_pp - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pPosFields->p_pr - (char *)pscan))) {
               *precision = pPosFields->p_pr;
               return(0);
        }
    }
    for(i=0; i < NUM_DET; i++, pDetFields++) {
        if((fieldOffset >= ((char *)&pDetFields->d_hr - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pDetFields->d_pr - (char *)pscan))) {
               *precision = pDetFields->d_pr;
               return(0);
        }
    }
    
    *precision = 2;
    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct scanRecord  *pscan=(struct scanRecord *)paddr->precord;
    posFields          *pPosFields = (posFields *)&pscan->p1pp;
    detFields          *pDetFields = (detFields *)&pscan->d1hr;
    short               fieldOffset;
    short               i;

   
    /* Use the field offset to see which detector or positioner it is */
    fieldOffset = ((dbFldDes *)(paddr->pfldDes))->offset;

    for(i=0; i < NUM_POS; i++, pPosFields++) {
        if((fieldOffset >= ((char *)&pPosFields->p_pp - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pPosFields->p_pr - (char *)pscan))) {
               pgd->upper_disp_limit = pPosFields->p_hr;
               pgd->lower_disp_limit = pPosFields->p_lr;
               return(0);
        }
    }
    for(i=0; i < NUM_DET; i++, pDetFields++) {
        if((fieldOffset >= ((char *)&pDetFields->d_hr - (char *)pscan)) &&
           (fieldOffset <= ((char *)&pDetFields->d_pr - (char *)pscan))) {
               pgd->upper_disp_limit = pDetFields->d_hr;
               pgd->lower_disp_limit = pDetFields->d_lr;
               return(0);
        }
    }

    recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    /* for testing .... */
    /* return upper_disp_limit and lower_disp_limit as control limits */
    struct dbr_grDouble grDblStruct;
    long status;

    status = get_graphic_double(paddr,&grDblStruct);
    pcd->upper_ctrl_limit = grDblStruct.upper_disp_limit;
    pcd->lower_ctrl_limit = grDblStruct.lower_disp_limit;

    return(0);
}

static long get_alarm_double(paddr,pad)
    struct dbAddr *paddr;
    struct dbr_alDouble *pad;
{
    recGblGetAlarmDouble(paddr,pad);
    return(0);
}


static void checkMonitors(pscan)
    struct scanRecord   *pscan;
{
    recPvtStruct        *precPvt = (recPvtStruct *)pscan->rpvt;
    detFields           *pDetFields = (detFields *)&pscan->d1hr;
    posFields           *pPosFields = (posFields *)&pscan->p1pp;
    unsigned long        now;
    int                  i;

    /* If last posting time is > MIN_MON, check to see if any dynamic fields 
       have changed (also post monitors on end of scan)*/

    now=tickGet();
    if(((now - pscan->tolp) > MIN_MON) || 
       ((pscan->pxsc == 1) && (pscan->exsc == 0))) {
        pscan->tolp = now;

        /* check all detectors */
        for(i=0; i < NUM_DET; i++, pDetFields++) {
          if(fabs(pDetFields->d_lv - pDetFields->d_cv) > 0) {
            db_post_events(pscan,&pDetFields->d_cv, DBE_VALUE);
            pDetFields->d_lv = pDetFields->d_cv;
          }
        }

        /* check all positioners and readbacks  */
        for(i=0; i < NUM_POS; i++, pPosFields++) {
          if(fabs(pPosFields->p_lv - pPosFields->p_dv) > 0) {
            db_post_events(pscan,&pPosFields->p_dv, DBE_VALUE);
            pPosFields->p_lv = pPosFields->p_dv;
          }
          if(fabs(pPosFields->r_lv - pPosFields->r_cv) > 0) {
            db_post_events(pscan,&pPosFields->r_cv, DBE_VALUE);
            pPosFields->r_lv = pPosFields->r_cv;
          }
        }

        if(pscan->pcpt != pscan->cpt) {
            db_post_events(pscan,&pscan->cpt, DBE_VALUE);
            pscan->pcpt = pscan->cpt;
        }

        /* Post a monitor on VAL to indicate new point */
        /* (checking pxsc skips the first pass when D_CV's are
           not valid and also catches the last point after exsc == 0 */
        if(pscan->pxsc) {
            db_post_events(pscan,&pscan->val, DBE_VALUE); 
        }
    }

    /* if this is the end of a scan, post data arrays */
    /* post these with DBE_LOG option for archiver    */
    if ((pscan->pxsc == 1) && (pscan->exsc == 0)) { 
 
        /* Must post events on both pointers, since toggle */
        db_post_events(pscan,precPvt->posBufPtr[0].pBufA, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[0].pBufB, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[1].pBufA, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[1].pBufB, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[2].pBufA, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[2].pBufB, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[3].pBufA, DBE_VAL_LOG);
        db_post_events(pscan,precPvt->posBufPtr[3].pBufB, DBE_VAL_LOG);

        for(i=0; i<NUM_DET; i++) {
            db_post_events(pscan,precPvt->detBufPtr[i].pBufA, DBE_VAL_LOG);
            db_post_events(pscan,precPvt->detBufPtr[i].pBufB, DBE_VAL_LOG);
        }
        /* I must also post a monitor on the NULL array, because some
           clients connected to D?PV's without valid PV's ! */
        db_post_events(pscan, precPvt->nullArray, DBE_VALUE);

        /* post alert if changed */
        if(precPvt->scanErr) {
            pscan->alrt = precPvt->scanErr;
            db_post_events(pscan,&pscan->alrt, DBE_VALUE);
        }

    if (pscan->pxsc != pscan->exsc) 
        db_post_events(pscan,&pscan->exsc, DBE_VALUE);

    }
}


/**************************************************************
 * lookupPV()
 * This is the routine that looks up newly entered "dynamic"
 * links. A dbNameToAddr call is done first to
 * see if the PV resides on this IOC. Much better performance is
 * obtained by using dbGet/dbPutField rather than the recDynLink
 * library for local PV's. If the dbNameToAddr fails, 
 * recDynLinkAddInput is used.
 *
 * For positioners:     
 *     If the PV is local, the dbAddr is used for puts and gets (
 *        to get the "previous positioner" before a scan starts. A 
 *        recDynLinkInput is also used to monitor the positioner
 *        for changes made from other places.
 *        This value is stored in p_cv.
 *     If the PV is not local, a recDynLinkOutput and a 
 *        recDynLinkInput are used. The recDynLinkInput has a 
 *        monitor callback routine for p_cv, but a get is used
 *        to find the previous position.
 *
 * For Readbacks:
 *     If the PV name starts with TIME or time, set a flag that
 *        indicates that the timestamp should be used as the
 *        readback value.
 *     Otherwise, do a PV lookup.
 **************************************************************/
static void lookupPV(struct scanRecord *pscan, unsigned short i)
{
    recPvtStruct  *precPvt = (recPvtStruct *)pscan->rpvt;
    recDynLinkPvt *puserPvt;
    char          *ppvn[PVN_SIZE];
    char          *pdesc = ".DESC";
    char          *pval  = ".VAL";
    char          *pTIME = "TIME";
    char          *ptime = "time";
    char          *pdot;
    unsigned short *pPvStat;

    /* Point to correct places by using index i */
    *ppvn = &pscan->p1pv[0] + (i * PVN_SIZE);
    pPvStat = &pscan->p1nv + i; /* pointer arithmetic */
    puserPvt = (recDynLinkPvt *)precPvt->caLinkStruct[i].puserPvt;

    /* If PV field name = DESC, change to VAL */
    pdot = strrchr(*ppvn, '.');
    if(pdot!=NULL) {
        if(strncmp(pdot, pdesc, 5) == 0) {
            strcpy(pdot, pval);
            db_post_events(pscan, *ppvn, DBE_VALUE);
        }
    }

    /* See if it's a local PV */
    puserPvt->dbAddrNv = dbNameToAddr(*ppvn, puserPvt->pAddr);
    if(recScanDebug) {
      printf("PV: %s ,dbNameToAddr returned %lx\n",*ppvn, puserPvt->dbAddrNv);
    }

    switch(puserPvt->linkType) {
      case POSITIONER:
        if(puserPvt->dbAddrNv) {
            recDynLinkAddOutput(&precPvt->caLinkStruct[i+NUM_PVS], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback);
            recDynLinkAddInput(&precPvt->caLinkStruct[i], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback, posMonCallback);
        }else {
            /* Add a monitor anyway, so p_cv can be kept up to date */
            recDynLinkAddInput(&precPvt->caLinkStruct[i], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback, posMonCallback);
        }
        break;

      case READBACK:
        /* Check to see if it equals "time" */
        if((strncmp(*ppvn, pTIME, 4)==0) || (strncmp(*ppvn, ptime, 4)==0)) {
            puserPvt->ts = 1;
            *pPvStat = NO_PV;
            db_post_events(pscan, pPvStat, DBE_VALUE);
            break;  /* don't do lookups or pvSearchCallback */
        } else {
            puserPvt->ts = 0;
        }
        if(puserPvt->dbAddrNv) {
            recDynLinkAddInput(&precPvt->caLinkStruct[i], *ppvn,
                DBR_DOUBLE, rdlSCALAR, pvSearchCallback, NULL);
        }else {
            pvSearchCallback(&precPvt->caLinkStruct[i]);
        }
        break;

      case DETECTOR:
        if(puserPvt->dbAddrNv) {
            recDynLinkAddInput(&precPvt->caLinkStruct[i], *ppvn,
                DBR_FLOAT, rdlSCALAR, pvSearchCallback, NULL);
        }else {
            pvSearchCallback(&precPvt->caLinkStruct[i]);
        }
        break;

      case TRIGGER:
      case FWD_LINK:
        if(puserPvt->dbAddrNv) {
            recDynLinkAddOutput(&precPvt->caLinkStruct[i], *ppvn,
                DBR_FLOAT, rdlSCALAR, pvSearchCallback);
        }else {
            pvSearchCallback(&precPvt->caLinkStruct[i]);
        }
        break;
    }
}


LOCAL void pvSearchCallback(recDynLink *precDynLink)
{

    recDynLinkPvt     *puserPvt = (recDynLinkPvt *)precDynLink->puserPvt;
    struct scanRecord *pscan    = puserPvt->pscan;
    recPvtStruct      *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields         *pPosFields = (posFields *)&pscan->p1pp;
    detFields         *pDetFields = (detFields *)&pscan->d1hr;
    unsigned short     index    = puserPvt->linkIndex;
    unsigned short     detIndex;
    unsigned short    *pPvStat;
    unsigned short     oldValid;
    size_t             nelem;
    long               status;
    long               nRequest = 1;
    long               options = DBR_UNITS | DBR_PRECISION | DBR_GR_DOUBLE |
                                 DBR_CTRL_DOUBLE ;
    int                precision;

    pPvStat = &pscan->p1nv + index;    /* pointer arithmetic */

    oldValid = *pPvStat;
    if(puserPvt->dbAddrNv && recDynLinkConnectionStatus(precDynLink)) {
        *pPvStat = PV_NC;
        checkConnections(pscan);
        if(recScanDebug) printf("Search Callback: No Connection\n");
        if(*pPvStat != oldValid) {
            db_post_events(pscan, pPvStat, DBE_VALUE);
        }
        return;
    }
    else {
        *pPvStat = PV_OK;
        checkConnections(pscan);
        if(recScanDebug) printf("Search Callback: Success\n");
    }
    if(*pPvStat != oldValid) {
        db_post_events(pscan, pPvStat, DBE_VALUE);
    }

    /* Do other stuff, depending on the Link Type */
    switch(puserPvt->linkType) {
      case POSITIONER: /* indexes 0,1,2,3 and 25,26,27,28 */
        /* Positioners have an IN and an OUT. Only do the in's */
        if(index > P4_IN)
            break;
        else {
            pPosFields += index;
        }

        /* Get control limits, precision, and egu */
        if(puserPvt->dbAddrNv) {
            /* use the recDynLink lib */
            recDynLinkGetUnits(precDynLink, pPosFields->p_eu, 8);
            recDynLinkGetPrecision(precDynLink, &precision);
            pPosFields->p_pr = precision;
            recDynLinkGetControlLimits(precDynLink,
                &pPosFields->p_lr, &pPosFields->p_hr);
        } else {
            status = dbGet(puserPvt->pAddr, DBR_FLOAT,
                           precPvt->pDynLinkInfo,
                           &options, &nRequest, NULL);
            if(status == OK) {
               strcpy(pPosFields->p_eu, precPvt->pDynLinkInfo->units);
               pPosFields->p_pr = precPvt->pDynLinkInfo->precision;
               pPosFields->p_hr = precPvt->pDynLinkInfo->upper_ctrl_limit;
               pPosFields->p_lr = precPvt->pDynLinkInfo->lower_ctrl_limit;
            }
        }
        db_post_events(pscan,&pPosFields->p_eu, DBE_VALUE);
        db_post_events(pscan,&pPosFields->p_pr, DBE_VALUE);
        db_post_events(pscan,&pPosFields->p_hr, DBE_VALUE);
        db_post_events(pscan,&pPosFields->p_lr, DBE_VALUE);
 
        break;

      case READBACK:
        break;

      case DETECTOR:
        /* Derive pointer to appropriate detector fields */
        detIndex = index - D1_IN;
        pDetFields += detIndex; /* pointer arithmetic */

        /* Get display limits, precision, and egu */
        if(puserPvt->dbAddrNv) {
            /* use the recDynLink lib */
            recDynLinkGetUnits(precDynLink, pDetFields->d_eu, 8);
            recDynLinkGetPrecision(precDynLink, &precision);
            pDetFields->d_pr = precision;
            recDynLinkGetGraphicLimits(precDynLink,
                &pDetFields->d_lr, &pDetFields->d_hr);
        } else {
            status = dbGet(puserPvt->pAddr, DBR_FLOAT,
                           precPvt->pDynLinkInfo,
                           &options, &nRequest, NULL);
            if(status == OK) {
               strcpy(pDetFields->d_eu, precPvt->pDynLinkInfo->units);
               pDetFields->d_pr = precPvt->pDynLinkInfo->precision;
               pDetFields->d_hr = precPvt->pDynLinkInfo->upper_disp_limit;
               pDetFields->d_lr = precPvt->pDynLinkInfo->lower_disp_limit;
            }
        }
        db_post_events(pscan,&pDetFields->d_eu, DBE_VALUE);
        db_post_events(pscan,&pDetFields->d_pr, DBE_VALUE);
        db_post_events(pscan,&pDetFields->d_hr, DBE_VALUE);
        db_post_events(pscan,&pDetFields->d_lr, DBE_VALUE);
  
        /* get # of elements, re-allocate array as necessary */
        if(puserPvt->dbAddrNv) {
            status = recDynLinkGetNelem(precDynLink, &nelem);
        }
        else {
            nelem = puserPvt->pAddr->no_elements;
        }
  
        if(nelem > 1) {
            printf("NOT READY FOR VECTORS YET !!!\n");
            nelem = 1;  /* force it to be a scalar for now */
        }
        if(nelem > puserPvt->nelem) {
            /*
            if(puserPvt->nelem) {
                printf("Freeing memory for dynLink %d  \n", index);
                free(pDetFields->d_da);
            }
            */
            
            printf("Allocating memory for dynLink %d\n  ", index);  
            pDetFields->d_da=(float *)calloc(pscan->mpts*nelem,sizeof(float));
            precPvt->detBufPtr[detIndex].pBufA =
               (float *) calloc(pscan->mpts, sizeof(float));
            precPvt->detBufPtr[detIndex].pBufB =
               (float *) calloc(pscan->mpts, sizeof(float));
            
            if(!(precPvt->detBufPtr[detIndex].pBufA) ||
               !(precPvt->detBufPtr[detIndex].pBufB)) {
                /* MEMORY ALLOCATION FAILED */
                printf("MEMORY ALLOCATION FAILED \n");
                puserPvt->nelem = 0;
            }
            else {
                puserPvt->nelem = nelem;
                if(precPvt->validBuf)
                    precPvt->detBufPtr[detIndex].pFill = 
                        precPvt->detBufPtr[detIndex].pBufA;
                else
                    precPvt->detBufPtr[detIndex].pFill = 
                        precPvt->detBufPtr[detIndex].pBufB;
            }
        }
        break;

      case TRIGGER:
      case FWD_LINK:
        break;
    }

       
}

LOCAL void posMonCallback(recDynLink *precDynLink)
{

    recDynLinkPvt     *puserPvt = (recDynLinkPvt *)precDynLink->puserPvt;
    struct scanRecord *pscan    = puserPvt->pscan;
    recPvtStruct      *precPvt = (recPvtStruct *)pscan->rpvt;
    unsigned short     index    = puserPvt->linkIndex;
    posFields         *pPosFields = (posFields *)&pscan->p1pp + index;
    long               status;
    size_t	       nRequest = 1;


    /* update p_cv with current positioner value */
    /* NOTE: For fast scans, these will get lost, because it is the low
       priority channel access task that is doing these callbacks */
    status = recDynLinkGet(&precPvt->caLinkStruct[index],
                       &pPosFields->p_cv, &nRequest, 0, 0, 0);
    db_post_events(pscan,&pPosFields->p_cv, DBE_VALUE);
}


static void checkConnections(struct scanRecord *pscan)
{
  recPvtStruct   *precPvt = (recPvtStruct *)pscan->rpvt;
  unsigned short *pPvStat;
  unsigned char   badOutputPv = 0;
  unsigned char   badInputPv = 0;
  unsigned short  i;

  pPvStat = &pscan->p1nv;

  for(i=1; i <= NUM_POS; i++, pPvStat++) {
      if(*pPvStat == PV_NC) badOutputPv = 1;
  }
  /* let pPvStat continue to increment into Readbacks */
  for(i=1; i <= NUM_POS; i++, pPvStat++) {
      if(*pPvStat == PV_NC) badOutputPv = 1;
  }
  /* let pPvStat continue to increment into Detectors */
  for(i=1; i <= NUM_DET; i++, pPvStat++) {
      if(*pPvStat == PV_NC) badInputPv = 1;
  }
  /* let pPvStat continue to increment into Triggers */
  for(i=1; i <= NUM_TRGS; i++, pPvStat++) {
      if(*pPvStat == PV_NC) badOutputPv = 1;
  }

  precPvt->badOutputPv = badOutputPv;
  precPvt->badInputPv  = badInputPv;
}



/********************** SCAN Initialization  ********************************/
/*
 * If a "Before Scan Link PV" is defined, this routine will be called twice,
 * once before the link is processed and once after. The starting positions
 * and limits are calculated each time, in case the link caused something to
 * change.
 *
 */
static long initScan(pscan)
struct scanRecord *pscan;
{
  recPvtStruct    *precPvt = (recPvtStruct *)pscan->rpvt;
  posFields       *pPosFields;
  long  status;
  unsigned short  *pPvStat;
  int   i;


  /* General initialization ... */
  if(recScanDebug) {
      precPvt->tickStart = tickGet();
  }

  pscan->cpt = 0;   /* reset point counter */
  precPvt->scanErr = 0;

  /* determine highest valid positioner, readback, trigger, and detector */
  precPvt->valPosPvs = 0;
  precPvt->valRdbkPvs = 0;
  precPvt->valDetPvs = 0;
  precPvt->valTrigPvs = 0;
  pPvStat = &pscan->p1nv;  

  for(i=1; i <= NUM_POS; i++, pPvStat++) {
      if(*pPvStat == PV_OK) precPvt->valPosPvs = i;
  }
  /* let pPvStat continue to increment into Readbacks */
  for(i=1; i <= NUM_POS; i++, pPvStat++) {
      if(*pPvStat == PV_OK) precPvt->valRdbkPvs = i;
  }
  /* let pPvStat continue to increment into Detectors */
  /* flag which detectors should be acquired during scan */
  for(i=1; i <= NUM_DET; i++, pPvStat++) {
      if(*pPvStat == PV_OK) {
          precPvt->valDetPvs = i;
          precPvt->acqDet[i-1] = 1;
      }
      else {
          precPvt->acqDet[i-1] = 0;
      }
  }
  /* let pPvStat continue to increment into Triggers */
  for(i=1; i <= NUM_TRGS; i++, pPvStat++) {
      if(*pPvStat == PV_OK) precPvt->valTrigPvs = i;
  }

  /* if more readbacks than positioners, set valPosPvs to valRdbkPvs */
  if(precPvt->valRdbkPvs > precPvt->valPosPvs) {
      precPvt->valPosPvs = precPvt->valRdbkPvs;
  }
      
  if(recScanDebug > 10) {
      printf("Positioners  : %u\n",precPvt->valPosPvs);
      printf("Readbacks    : %u\n",precPvt->valRdbkPvs);
      printf("Detectors    : %u\n",precPvt->valDetPvs);
      printf("Triggers     : %u\n",precPvt->valTrigPvs);
  }

  /* checkScanLimits must be called to update the current value of each 
     positioner into p_pp. If recScanDontCheckLimits is set, it will return
     successful w/o actually checking */

  if(status = checkScanLimits(pscan)) {
      pscan->exsc = 0;   /* limits didn't pass, abort scan */
      db_post_events(pscan,&pscan->smsg,DBE_VALUE);
      db_post_events(pscan,&pscan->alrt,DBE_VALUE);
      db_post_events(pscan,&pscan->exsc,DBE_VALUE);
      precPvt->phase = IDLE;
      return(status);
  }

  /* Then calculate the starting position */
  precPvt->onTheFly = 0;  /* clear onTheFly flag */
  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  for(i=0; i < precPvt->valPosPvs; i++, pPosFields++, pPvStat++) {
    if(*pPvStat == PV_OK) {
      /* Figure out starting positions for each positioner */
      if(pPosFields->p_sm == REC_SCAN_MO_TAB) {
          if(pPosFields->p_ar) {
              pPosFields->p_dv = pPosFields->p_pp + pPosFields->p_pa[0];
          }
          else {
              pPosFields->p_dv = pPosFields->p_pa[0];
          }
      }
      else {
          if(pPosFields->p_ar) {
              pPosFields->p_dv = pPosFields->p_pp + pPosFields->p_sp;
          }
          else {
              pPosFields->p_dv = pPosFields->p_sp;
          }
      }
      db_post_events(pscan,&pPosFields->p_dv,DBE_VALUE);
      if(pPosFields->p_sm == REC_SCAN_MO_OTF) {
          precPvt->onTheFly |= 1;  /* set flag if onTheFly */
      }
    }
  }
      
  if((pscan->bsnv == PV_OK) && (precPvt->phase == INIT_SCAN)) {
      precPvt->phase = BEFORE_SCAN;
      sprintf(pscan->smsg,"Before Scan FLNK ...");
      db_post_events(pscan,&pscan->smsg,DBE_VALUE);
  }
  else {
      precPvt->phase = MOVE_MOTORS;
      sprintf(pscan->smsg,"Scanning ...");
      db_post_events(pscan,&pscan->smsg,DBE_VALUE);
  }
  /* request callback to do dbPutFields */
  callbackRequest(&precPvt->doPutsCallback);
  return(OK);
}

static void contScan(pscan)
struct scanRecord *pscan;
{
  recPvtStruct   *precPvt = (recPvtStruct *)pscan->rpvt;
  posFields      *pPosFields = (posFields *)&pscan->p1pp;
  detFields      *pDetFields = (detFields *)&pscan->d1hr;
  recDynLinkPvt  *puserPvt;
  TS_STAMP       currentTs;
  unsigned short *pPvStat;
  unsigned short *pPvStatPos;
  unsigned short i;          
  long  status;
  size_t  nRequest = 1;

  switch (precPvt->phase) {
    case TRIG_DETCTRS:
        /* go output to detector trigger fields */
        callbackRequest(&precPvt->doPutsCallback);
        return;

    case CHECK_MOTORS:
        if(recScanDebug >=5) {
            printf("CHECK_MOTORS  - Point %d\n",precPvt->pscan->cpt);
        }
        /* check if a readback PV and a delta are specified */
        pPvStat = &pscan->r1nv;
        pPvStatPos  = &pscan->p1nv;
        for(i=0; i < NUM_POS; i++, pPosFields++, pPvStatPos++, pPvStat++) {
          if((pPosFields->r_dl != 0) && 
             (*pPvStat == PV_OK) &&
             (*pPvStatPos == PV_OK)) {
            puserPvt = precPvt->caLinkStruct[i+NUM_POS].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkGet(&precPvt->caLinkStruct[i+NUM_POS],
                       &pPosFields->r_cv, &nRequest, 0, 0, 0);
            }
            else {
                status = dbGet(puserPvt->pAddr,DBR_DOUBLE, &pPosFields->r_cv,
                           0,0,NULL);
            }
 
            if((pPosFields->r_dl > 0) &&
              (fabs(pPosFields->p_dv-pPosFields->r_cv) > pPosFields->r_dl)) {
                sprintf(pscan->smsg,"SCAN Aborted: P1 Readback > delta");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                precPvt->scanErr = 1;
                endScan(pscan);
                return;
            }
          } 
        }

        /* if there is a valid detector trigger ...   */
        /*    AND we are NOT in onTheFly              */
        /*       OR  we are doing Point 0             */
        /* go trigger detectors and wait for the next process */
        if(precPvt->valTrigPvs && (!precPvt->onTheFly || (pscan->cpt==0))) {
            /* do detector trigger fields */
            precPvt->phase = TRIG_DETCTRS;
            callbackRequest(&precPvt->doPutsCallback);
            return;
        }

        /* if no validTrigPvs, fall through to READ_DETCTRS */
        /* if on-the-fly mode, fall through to READ_DETCTRS */
        
    case READ_DETCTRS: 
        if(recScanDebug >=5) {
            printf("READ_DETCTRS - Point %d\n",precPvt->pscan->cpt);
        }
        /* Store the appropriate value into the positioner readback array */
        /* from RxCV or PxDV or TIME */
        pPvStat = &pscan->r1nv;
        pPvStatPos  = &pscan->p1nv;
        pPosFields = (posFields *)&pscan->p1pp;
        for(i=0; i < NUM_POS; i++, pPosFields++, pPvStatPos++, pPvStat++) {
          /* if readback PV is OK, use that value */
          if(*pPvStat == PV_OK) {
            puserPvt = precPvt->caLinkStruct[i+NUM_POS].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkGet(&precPvt->caLinkStruct[i+NUM_POS],
                       &pPosFields->r_cv, &nRequest, 0, 0, 0);
            }
            else {
                status = dbGet(puserPvt->pAddr,DBR_DOUBLE, &pPosFields->r_cv,
                           0,0,NULL);
            }

            precPvt->posBufPtr[i].pFill[pscan->cpt] = pPosFields->r_cv;

          }

          /* Does the readback PV = "time" ? */
          else if(((recDynLinkPvt *)
                  (precPvt->caLinkStruct[i+NUM_POS].puserPvt))->ts) {
            TSgetTimeStamp((int)precPvt->pscan->tse,
                           (struct timespec*)&currentTs);
            TsDiffAsDouble(&pPosFields->r_cv, &currentTs, &pscan->time);
            precPvt->posBufPtr[i].pFill[pscan->cpt] = pPosFields->r_cv;
          }
          
          /* Is the positioner PV valid ? */
          else if(*pPvStatPos == PV_OK) {
            /* stuff array with desired value */
            /* if onTheFly and cpt>0,add the step increment to the previous*/
            if((pPosFields->p_sm != REC_SCAN_MO_OTF) || (pscan->cpt == 0)) {
               pPosFields->r_cv = pPosFields->p_dv;
               precPvt->posBufPtr[i].pFill[pscan->cpt] = pPosFields->r_cv;
            } else {
               pPosFields->r_cv =
                 precPvt->posBufPtr[i].pFill[pscan->cpt-1] + pPosFields->p_si;
               precPvt->posBufPtr[i].pFill[pscan->cpt] = pPosFields->r_cv;
            }
          }
          else {
            /* Neither PV is valid, store a 0 */
            pPosFields->r_cv = 0;
            precPvt->posBufPtr[i].pFill[pscan->cpt] = pPosFields->r_cv;
          }
        }

        /* read each valid detector PV, place data in buffered array */
        status = 0;
        pPvStat = &pscan->d1nv;
        pDetFields = (detFields *)&pscan->d1hr;
        for(i=0; i < precPvt->valDetPvs; i++, pDetFields++, pPvStat++) {
          if(precPvt->acqDet[i] && (precPvt->detBufPtr[i].pFill != NULL)) {
            if(*pPvStat == PV_OK) {
              puserPvt = precPvt->caLinkStruct[i+D1_IN].puserPvt;
              if(puserPvt->dbAddrNv) {
                  status |= recDynLinkGet(&precPvt->caLinkStruct[i+D1_IN],
                         &pDetFields->d_cv, &nRequest, 0, 0, 0);
              }
              else {
                  status |= dbGet(puserPvt->pAddr, DBR_FLOAT, &pDetFields->d_cv,
                           0,0, NULL);
              }
            }
            else {
              pDetFields->d_cv = 0;
            }
            precPvt->detBufPtr[i].pFill[pscan->cpt] = pDetFields->d_cv;
          }
        }

        pscan->udf=0;

        pscan->cpt++;
        /* Has number of points been reached ? */
        if(pscan->cpt < (pscan->npts)) {
            /* determine next desired position for each positioner */
            pPosFields = (posFields *)&pscan->p1pp;
            pPvStat = &pscan->p1nv;

            /* figure out next position */
            for(i=0; i < precPvt->valPosPvs; i++, pPosFields++, pPvStat++) {
              if(*pPvStat == PV_OK) {
                  switch(pPosFields->p_sm) {
                    case REC_SCAN_MO_LIN:
                        pPosFields->p_dv = pPosFields->p_dv + pPosFields->p_si;
                      break;
                    case REC_SCAN_MO_TAB:
                        if(pPosFields->p_ar) {
                            pPosFields->p_dv = pPosFields->p_pp + 
                                               pPosFields->p_pa[pscan->cpt];
                        }
                        else {
                            pPosFields->p_dv = pPosFields->p_pa[pscan->cpt];
                        }
                      break;
                    case REC_SCAN_MO_OTF:
                        if(pPosFields->p_ar) {
                            pPosFields->p_dv = pPosFields->p_pp +
                                               pPosFields->p_ep;
                        }
                        else {
                            pPosFields->p_dv = pPosFields->p_ep;
                        }
                      break;
                  }
              }
            }

            /* request callback to move motors to new positions */
            precPvt->phase = MOVE_MOTORS; 
            /* For onTheFly, it will fall through to TRIG_DETCTRS */
            /* after MOVE_MOTORS */
            callbackRequest(&precPvt->doPutsCallback);
            return;
        }
        else {
            endScan(pscan);   /* scan is successfully complete */
            sprintf(pscan->smsg,"SCAN Complete");
            db_post_events(pscan,&pscan->smsg,DBE_VALUE);
            precPvt->scanErr = 0;
            return;
        }
   } 
}

static void endScan(pscan)
struct scanRecord *pscan;
{

  recPvtStruct    *precPvt = (recPvtStruct *)pscan->rpvt;
  int              counter;
  unsigned short  *pPvStat, i;
  detFields       *pDetFields;


  /* Done with scan. Do we want to fill the remainder of the
     array with 0 or something ? */
  /* It turns out that medm plots the whole array, so for it
     to look right the remainder of the arrays will be filled
     with the last values. This will cause medm to plot the
     same point over and over again, but it will look correct */

  counter = pscan->cpt;
  if(recScanDebug) {
      printf("endScan(): Fill Counter - %d\n",counter);
  }
  while (counter < pscan->mpts) {   
      /* Fill valid detector arrays with last value */
      pDetFields = (detFields *)&pscan->d1hr;
      pPvStat = &pscan->d1nv;
      for(i=0; i < precPvt->valDetPvs; i++, pDetFields++, pPvStat++) {
        if(precPvt->acqDet[i]) {
          precPvt->detBufPtr[i].pFill[counter] = pDetFields->d_cv;   
        }
      }
      /* Fill in the readback arrays with last values */
      precPvt->posBufPtr[0].pFill[counter] = 
              precPvt->posBufPtr[0].pFill[counter-1];

      precPvt->posBufPtr[1].pFill[counter] = 
              precPvt->posBufPtr[1].pFill[counter-1];

      precPvt->posBufPtr[2].pFill[counter] = 
              precPvt->posBufPtr[2].pFill[counter-1];

      precPvt->posBufPtr[3].pFill[counter] = 
              precPvt->posBufPtr[3].pFill[counter-1];

      counter++;
  }

  /* Switch validBuf flag and fill pointers */
  if(precPvt->validBuf == A_BUFFER) {
      precPvt->validBuf = B_BUFFER;
      precPvt->posBufPtr[0].pFill = precPvt->posBufPtr[0].pBufA;
      precPvt->posBufPtr[1].pFill = precPvt->posBufPtr[1].pBufA;
      precPvt->posBufPtr[2].pFill = precPvt->posBufPtr[2].pBufA;
      precPvt->posBufPtr[3].pFill = precPvt->posBufPtr[3].pBufA;
      for(i=0; i < NUM_DET; i++) {
          precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufA;
      }
  } else {
      precPvt->validBuf = A_BUFFER;
      precPvt->posBufPtr[0].pFill = precPvt->posBufPtr[0].pBufB;
      precPvt->posBufPtr[1].pFill = precPvt->posBufPtr[1].pBufB;
      precPvt->posBufPtr[2].pFill = precPvt->posBufPtr[2].pBufB;
      precPvt->posBufPtr[3].pFill = precPvt->posBufPtr[3].pBufB;
      for(i=0; i < NUM_DET; i++) {
          precPvt->detBufPtr[i].pFill = precPvt->detBufPtr[i].pBufB;
      }
  }
      
  pscan->exsc = 0;  /* done with scan */
  
  /* Check the "after scan flag to see what to do about the positioners */
  /* Also see if there is an After Scan Link to process */

  if(pscan->pasm || (pscan->asnv == PV_OK)) {
      precPvt->phase = AFTER_SCAN;
      /* request callback to do dbPutFields */
      callbackRequest(&precPvt->doPutsCallback);
  }
  else {
      precPvt->phase = IDLE;
  }
  return;

}


/****************************************************************************
 *
 * This is the code that is executed to initiate the next
 * step in the scan or trigger detectors (does the "puts" to the
 * reassignable links). Since the recDynLinkPut request is actually
 * done with a separate task, one need not worry about lock sets.
 * 
 * Note that the argument (which is the address of the CALLBACK structure),
 * is interpreted to be a pointer to the record private area. 
 *
 ***************************************************************************/
void doPuts(precPvt)
   recPvtStruct *precPvt;
{
  struct scanRecord *pscan = precPvt->pscan;
  recDynLinkPvt  *puserPvt;
  posFields      *pPosFields;
  unsigned short *pPvStat;
  int   i;
  long status;

  switch (precPvt->phase) {
    case MOVE_MOTORS:
        if(recScanDebug >=5) {
            printf("MOVE_MOTORS  - Point %d\n",precPvt->pscan->cpt);
        }
        /* Schedule the next phase as CHECK_MOTORS */
        precPvt->phase = CHECK_MOTORS;

        pPosFields = (posFields *)&precPvt->pscan->p1pp;
        pPvStat = &precPvt->pscan->p1nv;

        /* for each valid positioner, write the desired position */
        /* if positioner is "on-the-fly", only write if step 0 or 1 */
        for(i=0; i < precPvt->valPosPvs; i++, pPosFields++, pPvStat++) {
          if((*pPvStat == PV_OK) && 
              (!((pPosFields->p_sm == REC_SCAN_MO_OTF) &&
                 (precPvt->pscan->cpt > 1)) || precPvt->phase == AFTER_SCAN)) {
                puserPvt = precPvt->caLinkStruct[i].puserPvt;
                if(puserPvt->dbAddrNv) {
                  status = recDynLinkPut(&precPvt->caLinkStruct[i+P1_OUT],
                              &(pPosFields->p_dv), 1);
                } else {
                  status = dbPutField(puserPvt->pAddr,DBF_DOUBLE,
                              &(pPosFields->p_dv), 1);
                }
          }
        }

        if(!(precPvt->onTheFly && precPvt->pscan->cpt))
            break;
        /* Fall through to TRIG_DETCTRS if in onTheFly mode and flying */

    case TRIG_DETCTRS:
        if(recScanDebug >=5) {
            printf("TRIG_DETCTRS - Point %d\n",precPvt->pscan->cpt);
        }
        /* Schedule the next phase  */
        if(!precPvt->onTheFly || (precPvt->pscan->cpt == 0)) {
            precPvt->phase = READ_DETCTRS;
        }
        else {
            precPvt->phase = CHECK_MOTORS;
        }

        /* for each valid detector trigger, write the desired value */
        if(!precPvt->pscan->t1nv) {
            puserPvt = precPvt->caLinkStruct[T1_OUT].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkPut(&precPvt->caLinkStruct[T1_OUT],
                            &(precPvt->pscan->t1cd), 1);
            } else {
                status = dbPutField(puserPvt->pAddr,DBF_FLOAT,
                            &(precPvt->pscan->t1cd), 1);
            }
        }
        if(!precPvt->pscan->t2nv) {
            puserPvt = precPvt->caLinkStruct[T2_OUT].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkPut(&precPvt->caLinkStruct[T2_OUT],
                            &(precPvt->pscan->t2cd), 1);
            } else {
                status = dbPutField(puserPvt->pAddr,DBF_FLOAT,
                            &(precPvt->pscan->t2cd), 1);
            }
        }
        break;

    case BEFORE_SCAN:
        if(recScanDebug >=5)  printf("BEFORE_SCAN Fwd Lnk\n");
        if(precPvt->pscan->bsnv == OK) {
            puserPvt = precPvt->caLinkStruct[BS_OUT].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkPut(&precPvt->caLinkStruct[BS_OUT],
                            &(precPvt->pscan->bscd), 1);
            } else {
                status = dbPutField(puserPvt->pAddr,DBF_FLOAT,
                            &(precPvt->pscan->bscd), 1);
            }
        }
        /* Leave the phase at BEFORE_SCAN */
        break;
        
    case AFTER_SCAN:
        /* If Retrace indicates motors must move ... */
        if(precPvt->pscan->pasm) {
             if(recScanDebug >=5)  printf("RETRACE\n");
             pPosFields = (posFields *)&precPvt->pscan->p1pp;
             pPvStat = &precPvt->pscan->p1nv;
             for(i=0; i < precPvt->valPosPvs; i++, pPosFields++, pPvStat++) {
                 if(*pPvStat == PV_OK) {
                     /* Figure out where to go ... */
                     if(precPvt->pscan->pasm == REC_SCAN_AS_BS) {
                         pPosFields->p_dv = pPosFields->p_pp;
                     } else {
                         if(pPosFields->p_sm == REC_SCAN_MO_TAB) {
                           if(pPosFields->p_ar) {
                             pPosFields->p_dv =
                                       pPosFields->p_pp + pPosFields->p_pa[0];
                           }
                           else {
                             pPosFields->p_dv = pPosFields->p_pa[0];
                           }
                         }
                         else {
                           if(pPosFields->p_ar) {
                             pPosFields->p_dv =
                                       pPosFields->p_pp + pPosFields->p_sp;
                           }
                           else {
                             pPosFields->p_dv = pPosFields->p_sp;
                           }
                         }
                     }

                     /* Command motor */
                     puserPvt = precPvt->caLinkStruct[i].puserPvt;
                     if(puserPvt->dbAddrNv) {
                       status = recDynLinkPut(&precPvt->caLinkStruct[i+P1_OUT],
                                    &(pPosFields->p_dv), 1);
                     } else {
                       status = dbPutField(puserPvt->pAddr,DBF_DOUBLE,
                                    &(pPosFields->p_dv), 1);
                     }
                 }
             }
         }

        /* If an After Scan Link PV is valid, execute it */
        if(precPvt->pscan->asnv == PV_OK) {
            if(recScanDebug >=5)  printf("AFTER_SCAN Fwd Lnk\n");
            puserPvt = precPvt->caLinkStruct[AS_OUT].puserPvt;
            if(puserPvt->dbAddrNv) {
                status = recDynLinkPut(&precPvt->caLinkStruct[AS_OUT],
                            &(precPvt->pscan->ascd), 1);
            } else {
                status = dbPutField(puserPvt->pAddr,DBF_FLOAT,
                            &(precPvt->pscan->ascd), 1);
            }
        }

        precPvt->phase = IDLE;
        break;

    case IDLE:
    case PREVIEW:
        /* This must be a request to move a positioner interactivly ... */
        
        pPvStat = &precPvt->pscan->p1nv;
        pPosFields = (posFields *)&precPvt->pscan->p1pp;
        for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++) {
          if((*pPvStat == PV_OK) && precPvt->movPos[i]) {
              puserPvt = precPvt->caLinkStruct[i].puserPvt;
              status = dbPutField(puserPvt->pAddr,DBF_DOUBLE,
                              &(pPosFields->p_dv), 1);
              precPvt->movPos[i] = 0;
          }
        }
        break;

    case SCAN_PENDING:
        checkConnections(pscan);
        /* If .EXSC == 0, or no longer in SCAN_PENDING */
        if((!pscan->exsc) || (precPvt->phase != SCAN_PENDING)) {
            if(recScanDebug) printf("scanPending - no effect\n");
            return;
        }

        precPvt->phase = IDLE;

        /* Only scanOnce if no bad PV's */
        if(pscan->exsc && !precPvt->badOutputPv && !precPvt->badInputPv)  {
            pscan->alrt = 0;
            db_post_events(pscan,&pscan->alrt,DBE_VALUE);
            strcpy(pscan->smsg,"");
            db_post_events(pscan,&pscan->smsg,DBE_VALUE);
            scanOnce(pscan);
            if(recScanDebug) printf("scanPending - start scan\n");
        }
        else {
            pscan->exsc = 0;
            db_post_events(pscan,&pscan->exsc,DBE_VALUE);
            sprintf(pscan->smsg,"Scan aborted, PV(s) not connected");
            db_post_events(pscan,&pscan->smsg,DBE_VALUE);
            if(recScanDebug) printf("scanPending - end scan\n");
        }
        break;
    default:
	break;
  }
}




/* routine to resolve and adjust linear scan definition.
 * Might get complicated !!!
 */

static void adjLinParms(paddr)      
    struct dbAddr       *paddr;
{
    struct scanRecord   *pscan =   (struct scanRecord *)(paddr->precord);
    recPvtStruct        *precPvt = (recPvtStruct *)pscan->rpvt;
    struct posFields    *pParms =  (posFields *)&precPvt->pscan->p1pp;

    int                 special_type = paddr->special;
    int                 i;
    int                 foundOne = 0;

    /* determine which positioner */
    for(i=0; i<NUM_POS; i++, pParms++) {
        if ((paddr->pfield >= (void *)&pParms->p_pp) &&
            (paddr->pfield <= (void *)&pParms->p_pr)) {
            
            foundOne = 1;
            break;
        }
    }
    if(!foundOne) return;   /* couldn't determine positioner */

    if(pParms->p_sm == REC_SCAN_MO_TAB) {
        /* if positioner is in table mode, zero parms and return */
        zeroPosParms(pscan, (unsigned short) i);
        sprintf(pscan->smsg,"Positioner #%1d is in Table Mode !",i+1);
        pscan->alrt = 1;
        return;
    }

    if(recScanDebug) printf("Positioner %d\n",i);
    switch(special_type) {
      case(SPC_SC_S):              /* start position changed */
        /* if step increment/center/width are not frozen, change them  */
        if(!pParms->p_fi && !pParms->p_fc && !pParms->p_fw) {
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            return;
        /* if # of points/center/width are not frozen, change them  */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fw) {
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points!",i+1);
              /* adjust changed field to be consistent */
              pParms->p_sp = pParms->p_ep-(pParms->p_si * (pscan->npts - 1));
              db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if end/center are not frozen, change them  */
        } else if(!pParms->p_fe && !pParms->p_fc) {      
            pParms->p_ep = pParms->p_sp +
                               ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            return;
        /* if step increment/end/width are not frozen, change them  */
        } else if(!pParms->p_fi && !pParms->p_fe && !pParms->p_fw) { 
            pParms->p_wd = (pParms->p_cp - pParms->p_sp) * 2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_si = pParms->p_wd/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_ep = (pParms->p_sp + pParms->p_wd);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            return;
        /* if # of points/end/width are not frozen, change them  */
        } else if(!pscan->fpts && !pParms->p_fe && !pParms->p_fw) { 
            pscan->npts = ((pParms->p_cp - pParms->p_sp)*2/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points!",i+1);
              /* adjust changed field to be consistent */
              pParms->p_sp=pParms->p_cp-((pParms->p_si*(pscan->npts - 1))/2);
              db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        }
        break;

      case(SPC_SC_I):              /* step increment changed */
        /* if # of points is not frozen, change it  */
        if(!pscan->fpts) {
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
              db_post_events(pscan,&pParms->p_si,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if end/center/width are not frozen, change them */
        } else if(!pParms->p_fe && !pParms->p_fc && !pParms->p_fw) { 
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            return;
        /* if start/center/width are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fc && !pParms->p_fw) { 
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            return;
        /* if start/end/width are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fe && !pParms->p_fw) { 
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            return;
        } else {                   /* too constrained !! */
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i+1);
            pscan->alrt = 1;
            return;
        }
        break;

      case(SPC_SC_E):              /* end position changed   */
        /* if step increment/center/width are not frozen, change them */
        if(!pParms->p_fi && !pParms->p_fc && !pParms->p_fw) {
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            return;
        /* if # of points/center/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fw) {
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_ep=pParms->p_sp + (pParms->p_si * (pscan->npts - 1));
              db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if start/center are not frozen, change them  */
        } else if(!pParms->p_fs && !pParms->p_fc) {
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            return;
        /* if step start/width/increment are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fw && !pParms->p_fi) {
            pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - pParms->p_wd;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            return;
        /* if # of points/start/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fw) {
            pscan->npts=(((pParms->p_ep - pParms->p_cp)*2)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_ep=pParms->p_cp+(pParms->p_si * (pscan->npts - 1)/2);
              db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - pParms->p_wd; 
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        } else {                   /* too constrained !! */
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i+1);
            pscan->alrt = 1;
            return;
        }
        break;

      case(SPC_SC_C):              /* center position changed   */
        /* if start/end are not frozen, change them */
        if(!pParms->p_fs && !pParms->p_fe) { 
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            return;
        /* if end/inc/width are not frozen, change them */
        } else if(!pParms->p_fe && !pParms->p_fi && !pParms->p_fw) {
            pParms->p_wd = (pParms->p_cp - pParms->p_sp)*2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + pParms->p_wd;
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            return;
        /* if start/inc/width are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fi && !pParms->p_fw) { 
            pParms->p_wd = (pParms->p_ep - pParms->p_cp)*2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - pParms->p_wd;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            return;
        /* if # of points/end/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fe && !pParms->p_fw) {
            pscan->npts=(((pParms->p_cp - pParms->p_sp)*2)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_cp=pParms->p_sp+(pParms->p_si * (pscan->npts - 1)/2);
              db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_wd = (pParms->p_cp - pParms->p_sp) * 2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + pParms->p_wd;
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if # of points/start/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fw) {
            pscan->npts=(((pParms->p_ep - pParms->p_cp)*2)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_cp=pParms->p_ep-(pParms->p_si * (pscan->npts - 1)/2);
              db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_cp) * 2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - pParms->p_wd; 
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        } else {                   /* too constrained !! */
            pParms->p_cp = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i+1);
            pscan->alrt = 1;
            return;
        }
        break;

      case(SPC_SC_W):              /* width position changed   */
        /* if step start/inc/end are not frozen, change them */
        if(!pParms->p_fs && !pParms->p_fi && !pParms->p_fe) { 
            pParms->p_si = pParms->p_wd/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            return;
        /* if # of points/start/end are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fe) {
            pscan->npts = (pParms->p_wd/pParms->p_si) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_wd = (pParms->p_si * (pscan->npts - 1));
              db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if center/end/inc are not frozen, change them */
        } else if(!pParms->p_fc && !pParms->p_fe && !pParms->p_fi) { 
            pParms->p_si = pParms->p_wd/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            return;
        /* if start/center/inc are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fc && !pParms->p_fi) { 
            pParms->p_si = pParms->p_wd/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            return;
        /* if # of points/center/end are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fe) {
            pscan->npts = (pParms->p_wd/pParms->p_si) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_wd = (pParms->p_si * (pscan->npts - 1));
              db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        /* if # of points/start/center are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fc) {
            pscan->npts = (pParms->p_wd/pParms->p_si) + 1;
            if(pscan->npts > pscan->mpts) {
              pscan->npts = pscan->mpts;
              sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i+1);
              /* adjust changed field to be consistent */
              pParms->p_wd = (pParms->p_si * (pscan->npts - 1));
              db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
              pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        } else {                   /* too constrained !! */
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i+1);
            pscan->alrt = 1;
            return;
        }
        break;

      default:
        return;
    }
}
  


/* This routine attempts to bring all linear scan parameters into
 * "consistency". It is used at init and when the # of pts changes.
 * If the number of points changed because of a change in a Positioner's
 * scan parameters, that positioner is skipped to make sure that there is
 * no conflict with that logic. Any positioner in TABLE mode is also skipped.
 *
 * Any positioner that is in TABLE mode is checked to make sure the last
 * table loaded into p_pa had enough points for the new npts. If not, the
 * operator is warned
 *
 */

static void changedNpts(pscan)
    struct scanRecord   *pscan;
{

    recPvtStruct        *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields           *pParms = (posFields *)&pscan->p1pp;
    int                  i;
    unsigned short       freezeState = 0;

    /* for each positioner, calculate scan params as best as we can */
    /* if the positioner is in table mode, don't touch linear scan parms !*/
    for(i=0; i<NUM_POS; i++, pParms++) {
      /* Check if Positioner is in TABLE Mode */
      if(pParms->p_sm == REC_SCAN_MO_TAB) {
          if(precPvt->tablePts[i] < pscan->npts) {
              sprintf(pscan->smsg,"Pts in P%d Table < # of Steps",i+1);
              if(!pscan->alrt) {
                  pscan->alrt = 1;
              }
          } 
      }
      /* Skip the positioner that caused this */
      else if(i != precPvt->nptsCause) {
        /* Adjust linear scan params as best we can */

        /* develop freezeState switching word ... */
        /* START_FRZ | STEP_FRZ | END_FRZ | CENTER_FRZ | WIDTH_FRZ */

        freezeState = (pParms->p_fs << 4) |
                      (pParms->p_fi << 3) |   
                      (pParms->p_fe << 2) |   
                      (pParms->p_fc << 1) |   
                      (pParms->p_fw); 

        if(recScanDebug) {
            printf("Freeze State of P%1d = 0x%hx \n",i,freezeState);
        }
        
        /* a table describing what happens is at the end of the file */
        switch(freezeState) {
          case(0):
          case(1):  /* if center or width is frozen, but not inc , ... */
          case(2):
          case(3):
          case(4):
          case(5):  /* if end/center or end/width are frozen, but not inc, */
          case(6):
          case(7):
          case(16):
          case(17): /*if start/center or start/width are frozen, but not inc*/
          case(18):
          case(19):
          case(20):
          case(21):
          case(22):
          case(23):
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            break;

          case(8):  /* end/center/width unfrozen, change them */
          case(24):
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          case(12): /* start/center/width are not frozen, change them  */
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          case(10): /* if center is frozen, but not width/start/end , ...  */
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          /* The following freezeStates are known to be "Too Constrained" */
          /* 9,11,13,14,15,25,26,27,28,29,30,31 */
          default:                 /* too constrained !! */
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i+1);
            pscan->alrt = 1;
            break;
        }

      }
    }
}



static long checkScanLimits(pscan)
    struct scanRecord   *pscan;
{

  recDynLinkPvt   *puserPvt;
  recPvtStruct    *precPvt = (recPvtStruct *)pscan->rpvt;
  posFields       *pPosFields = (posFields *)&pscan->p1pp;
  unsigned short  *pPvStat = &pscan->p1nv;


  /* for each valid positioner, fetch control limits */
  long    status;
  size_t    nRequest = 1;
  int     i,j;
  double  value;

  if(recScanDebug) {
      if(!pscan->p1nv) 
          printf("P1 Control Limits : %.4f   %.4f\n",
              pscan->p1lr, pscan->p1hr);
      if(!pscan->p2nv) 
          printf("P2 Control Limits : %.4f   %.4f\n",
              pscan->p2lr, pscan->p2hr);
      if(!pscan->p3nv) 
          printf("P3 Control Limits : %.4f   %.4f\n",
              pscan->p3lr, pscan->p3hr);
      if(!pscan->p4nv) 
          printf("P4 Control Limits : %.4f   %.4f\n",
              pscan->p4lr, pscan->p4hr);
  }

  /* Update "previous position" of positioners to use in relative mode */
  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++) {
    if(*pPvStat == PV_OK) {
      puserPvt = precPvt->caLinkStruct[i].puserPvt;
      if(puserPvt->dbAddrNv) {
          status |= recDynLinkGet(&precPvt->caLinkStruct[i],
                 &pPosFields->p_pp, &nRequest, 0, 0, 0);
      }
      else {
          status |= dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPosFields->p_pp,
                     0,0, NULL);
      }
      db_post_events(pscan,&pPosFields->p_pp,DBE_VALUE);
    }
  }

  /* If recScanDontCheckLimits flag is set and EXSC = 1, don't proceed */
  if(recScanDontCheckLimits && pscan->exsc) return(OK);

  /* First check if any pos'rs are in Table mode with insufficient points*/
  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++) {
    if((*pPvStat == PV_OK) &&
       (pPosFields->p_sm == REC_SCAN_MO_TAB) &&
       (precPvt->tablePts[i] < pscan->npts)) {
           sprintf(pscan->smsg,"Pts in P%d Table < # of Steps",i+1);
           db_post_events(pscan,&pscan->smsg,DBE_VALUE);
           if(!pscan->alrt) {
               pscan->alrt = 1;
               db_post_events(pscan,&pscan->alrt,DBE_VALUE);
           }
           return(ERROR);
    }
  }

  /* check each point of scan against control limits. */
  /* Stop on first error */

  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++) {
    if(*pPvStat == PV_OK) {
      for(j=0; j<pscan->npts; j++) {
          /* determine next desired position for each positioner */
          switch(pPosFields->p_sm) {
            case REC_SCAN_MO_LIN:
              if(pPosFields->p_ar) {
                value = (pPosFields->p_pp + pPosFields->p_sp) + 
                        (j * pPosFields->p_si);
              } else {
                value = pPosFields->p_sp + (j * pPosFields->p_si);
              }
            break;

            case REC_SCAN_MO_TAB:    
              if(pPosFields->p_ar) {
                value = pPosFields->p_pp + pPosFields->p_pa[j]; 
              } else {
                value = pPosFields->p_pa[j];
              }
            break;

            case REC_SCAN_MO_OTF:    
              if(pPosFields->p_ar) {
                  if(j == 0) value = pPosFields->p_pp + pPosFields->p_sp;
                  else value = pPosFields->p_pp + pPosFields->p_ep;
              } else {
                  if(j == 0) value = pPosFields->p_sp;
                  else value = pPosFields->p_ep;
              }
            break;

            default:
              value = 0;
          }

          if((pPosFields->p_lr != 0) && (value < pPosFields->p_lr)) {
              sprintf(pscan->smsg,"P%-d Value < LO_Limit @ point %1d",i+1,j);
              pscan->alrt = 1;
              return(ERROR);
          }
          else if((pPosFields->p_hr != 0) && (value > pPosFields->p_hr)) {
              sprintf(pscan->smsg,"P%-d Value > HI_Limit @ point %1d",i+1,j);
              pscan->alrt = 1;
              return(ERROR);
          }
      }
    }
  }

  /* No errors if we made it here ... */
  sprintf(pscan->smsg,"SCAN Values within limits");
  return(OK);

}


/* This routine show a "preview" of the scan positions by
 * loading the detector arrays (d_da) with the scan positions at each
 * point and loading the readback arrays (p_ra) with the step #.
 * A plot of d_da vs p_ra will provide a graphical view of the scan 
 * ranges.
 *
 */

static void previewScan(pscan)
    struct scanRecord   *pscan;
{

  recPvtStruct     *precPvt = (recPvtStruct *)pscan->rpvt;
  recDynLinkPvt    *puserPvt;
  posFields        *pPosFields;
  detFields        *pDetFields;
  unsigned short   *pPvStat;
  double           *pPosBuf;
  float            *pDetBuf;
  float             value;
  int               i,j;
  long    status;
  size_t    nRequest = 1;

  /* Update "previous position" of positioners to use in relative mode */
  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++) {
    if(*pPvStat == PV_OK) {
      puserPvt = precPvt->caLinkStruct[i].puserPvt;
      if(puserPvt->dbAddrNv) {
          status |= recDynLinkGet(&precPvt->caLinkStruct[i],
                 &pPosFields->p_pp, &nRequest, 0, 0, 0);
      }
      else {
          status |= dbGet(puserPvt->pAddr, DBR_DOUBLE, &pPosFields->p_pp,
                     0,0, NULL);
      }
      db_post_events(pscan,&pPosFields->p_pp,DBE_VALUE);
    }
  }

  /* Run through entire scan for each valid positioner */
  pPvStat = &pscan->p1nv;
  pPosFields = (posFields *)&pscan->p1pp;
  pDetFields = (detFields *)&pscan->d1hr;
  for(i=0; i < NUM_POS; i++, pPosFields++, pPvStat++, pDetFields++) {
    if(*pPvStat == PV_OK) {
      /* must use the current buffer pointer */
      if(precPvt->validBuf) {
          pPosBuf = precPvt->posBufPtr[i].pBufB;
          pDetBuf = precPvt->detBufPtr[i].pBufB;
      }
      else {
          pPosBuf = precPvt->posBufPtr[i].pBufA;
          pDetBuf = precPvt->detBufPtr[i].pBufA;
      }
      for(j=0; j<pscan->npts; j++) {
          /* determine next desired position for each positioner */
          switch(pPosFields->p_sm) {
            case REC_SCAN_MO_LIN:
              if(pPosFields->p_ar) {
                value = (pPosFields->p_pp + pPosFields->p_sp) +
                        (j * pPosFields->p_si);
              } else {
                value = pPosFields->p_sp + (j * pPosFields->p_si);
              }
            break;

            case REC_SCAN_MO_TAB:
              if(pPosFields->p_ar) {
                value = pPosFields->p_pp + pPosFields->p_pa[j];
              } else {
                value = pPosFields->p_pa[j];
              }
            break;

            case REC_SCAN_MO_OTF:
              if(pPosFields->p_ar) {
                  if(j == 0) value = pPosFields->p_pp + pPosFields->p_sp;
                  else value = pPosFields->p_pp + pPosFields->p_ep;
              } else {
                  if(j == 0) value = pPosFields->p_sp;
                  else value = pPosFields->p_ep;
              }
            break;

            default:
              value = 0;
          }
          pPosBuf[j] = j;
          pDetBuf[j] = value;
      }
      /* now fill the rest of the array(s) with the last values */
      for(j=j; j<pscan->mpts;j++) {
          pPosBuf[j] = pPosBuf[j-1];
          pDetBuf[j] = pDetBuf[j-1];
      }
      db_post_events(pscan, precPvt->posBufPtr[i].pBufA, DBE_VALUE);
      db_post_events(pscan, precPvt->posBufPtr[i].pBufB, DBE_VALUE);
      db_post_events(pscan, precPvt->detBufPtr[i].pBufA, DBE_VALUE);
      db_post_events(pscan, precPvt->detBufPtr[i].pBufB, DBE_VALUE);
      /* I must also post a monitor on the NULL array, because some
         clients connected to D?PV's without valid PV's ! */
      db_post_events(pscan, precPvt->nullArray, DBE_VALUE);
    }
  }
}
        
static void saveFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields    *pPosFields = (posFields *)&pscan->p1pp;
    int           i;


    /* save state of each freeze flag */
    precPvt->fpts = pscan->fpts;
    for(i=0; i<NUM_POS; i++, pPosFields++) {
        precPvt->posParms[i].p_fs = pPosFields->p_fs;
        precPvt->posParms[i].p_fi = pPosFields->p_fi;
        precPvt->posParms[i].p_fc = pPosFields->p_fc;
        precPvt->posParms[i].p_fe = pPosFields->p_fe;
        precPvt->posParms[i].p_fw = pPosFields->p_fw;
    }
}

static void savePosParms(struct scanRecord *pscan, unsigned short i)
{

    recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields    *pPosFields = (posFields *)&pscan->p1pp + i;

    /* save state of linear scan parameters 0 */
    /* Do this when in table mode so operator is not confused */
        precPvt->posParms[i].p_sp = pPosFields->p_sp;
        precPvt->posParms[i].p_si = pPosFields->p_si;
        precPvt->posParms[i].p_ep = pPosFields->p_ep;
        precPvt->posParms[i].p_cp = pPosFields->p_cp;
        precPvt->posParms[i].p_wd = pPosFields->p_wd;
}

static void zeroPosParms(struct scanRecord *pscan, unsigned short i)
{

    posFields    *pPosFields = (posFields *)&pscan->p1pp + i;

    /* set them to 0 */
    /* Do this when in table mode so operator is not confused */
        pPosFields->p_sp = 0;
        db_post_events(pscan,&pPosFields->p_sp, DBE_VALUE);
        pPosFields->p_si = 0;
        db_post_events(pscan,&pPosFields->p_si, DBE_VALUE);
        pPosFields->p_ep = 0;
        db_post_events(pscan,&pPosFields->p_ep, DBE_VALUE);
        pPosFields->p_cp = 0;
        db_post_events(pscan,&pPosFields->p_cp, DBE_VALUE);
        pPosFields->p_wd = 0;
        db_post_events(pscan,&pPosFields->p_wd, DBE_VALUE);
}

static void resetFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    posFields    *pPosFields = (posFields *)&pscan->p1pp;
    int           i;

    /* reset each frzFlag, post monitor if changed */

    if(pscan->fpts) {
       pscan->fpts = 0;
       db_post_events(pscan,&pscan->fpts, DBE_VALUE);
    }

    for(i=0; i<NUM_POS; i++, pPosFields++) {
        if(pPosFields->p_fs) {
           pPosFields->p_fs = 0;
           db_post_events(pscan,&pPosFields->p_fs, DBE_VALUE);
        }

        if(pPosFields->p_fi) {
           pPosFields->p_fi = 0;
           db_post_events(pscan,&pPosFields->p_fi, DBE_VALUE);
        }

        if(pPosFields->p_fc) {
           pPosFields->p_fc = 0;
           db_post_events(pscan,&pPosFields->p_fc, DBE_VALUE);
        }

        if(pPosFields->p_fe) {
           pPosFields->p_fe = 0;
           db_post_events(pscan,&pPosFields->p_fe, DBE_VALUE);
        }

        if(pPosFields->p_fw) {
           pPosFields->p_fw = 0;
           db_post_events(pscan,&pPosFields->p_fw, DBE_VALUE);
        }
    }
}


/* Restores Freeze Flags to the state they were in */
static void restoreFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields    *pPosFields = (posFields *)&pscan->p1pp;
    int           i;


    /* restore state of each freeze flag, post if changed */
    pscan->fpts = precPvt->fpts;
    if(pscan->fpts) {
       db_post_events(pscan,&pscan->fpts, DBE_VALUE);
    }

    for(i=0; i<NUM_POS; i++, pPosFields++) {
        pPosFields->p_fs = precPvt->posParms[i].p_fs;
        if(pPosFields->p_fs) {
           db_post_events(pscan,&pPosFields->p_fs, DBE_VALUE);
        }

        pPosFields->p_fi = precPvt->posParms[i].p_fi;
        if(pPosFields->p_fi) {
           db_post_events(pscan,&pPosFields->p_fi, DBE_VALUE);
        }

        pPosFields->p_fc = precPvt->posParms[i].p_fc;
        if(pPosFields->p_fc) {
           db_post_events(pscan,&pPosFields->p_fc, DBE_VALUE);
        }

        pPosFields->p_fe = precPvt->posParms[i].p_fe;
        if(pPosFields->p_fe) {
           db_post_events(pscan,&pPosFields->p_fe, DBE_VALUE);
        }

        pPosFields->p_fw = precPvt->posParms[i].p_fw;
        if(pPosFields->p_fw) {
           db_post_events(pscan,&pPosFields->p_fw, DBE_VALUE);
        }

    }
}

/* Restores Position Parms after leaving table mode (for 1 positioner) */
static void restorePosParms(struct scanRecord *pscan, unsigned short i)
{

    recPvtStruct *precPvt = (recPvtStruct *)pscan->rpvt;
    posFields    *pPosFields = (posFields *)&pscan->p1pp + i;

    pPosFields->p_sp = precPvt->posParms[i].p_sp;
    db_post_events(pscan,&pPosFields->p_sp, DBE_VALUE);

    pPosFields->p_si = precPvt->posParms[i].p_si;
    db_post_events(pscan,&pPosFields->p_si, DBE_VALUE);

    pPosFields->p_ep = precPvt->posParms[i].p_ep;
    db_post_events(pscan,&pPosFields->p_ep, DBE_VALUE);

    pPosFields->p_cp = precPvt->posParms[i].p_cp;
    db_post_events(pscan,&pPosFields->p_cp, DBE_VALUE);

    pPosFields->p_wd = precPvt->posParms[i].p_wd;
    db_post_events(pscan,&pPosFields->p_wd, DBE_VALUE);

}







/*  Memo from Tim Mooney suggesting change in order of precedence */

/*

Ned,

Current effects of scan-parameter changes are summarized below, along
with the effects I think might be preferable.  The ideas are to make NPTS
nearly as easy to change as is INCR, to make END easier to change than
is START, and to prefer changing INCR in response to a change of NPTS
whenever permissible.

If an indirect change in NPTS would make it larger than MPTS, I think
NPTS ideally should be regarded as frozen, rather than being set to
MPTS.  I don't feel all that strongly about this, since I think it
might be more of a pain to code than it's worth.

Comments on any of this?

Tim

====================================================================
changing	now affects		should affect
--------------------------------------------------------------
START	->	INCR CENTER WIDTH	INCR CENTER WIDTH
	or	END CENTER		CENTER NPTS WIDTH
	or	INCR END WIDTH		END CENTER
	or	NPTS CENTER WIDTH	INCR END WIDTH
	or	NPTS END WIDTH		NPTS END WIDTH
	or	CENTER END		<punt>
	or	<punt>			
INCR	->	CENTER END WIDTH	NPTS
	or	START CENTER WIDTH	CENTER END WIDTH
	or	START END WIDTH		START CENTER WIDTH
	or	NPTS			START END WIDTH
	or	<punt>			<punt>
CENTER	->	START END		START END
	or	START INCR WIDTH	END INCR WIDTH
	or	END INCR WIDTH		START INCR WIDTH
	or	NPTS START WIDTH	NPTS END WIDTH
	or	NPTS END WIDTH		NPTS START WIDTH
	or	<punt>						
END	->	INCR CENTER WIDTH	INCR CENTER WIDTH
	or	START CENTER WIDTH	NPTS CENTER WIDTH
	or	START WIDTH INCR	START CENTER
	or	NPTS START WIDTH	START WIDTH INCR
	or	NPTS CENTER WIDTH	NPTS START WIDTH
	or	START CENTER		<punt>
	or	<punt>			
WIDTH	->	START END INCR		START END INCR
	or	CENTER END INCR		START END NPTS
	or	START CENTER INCR	CENTER END INCR
	or	NPTS START END		START CENTER INCR
	or	NPTS CENTER END		NPTS CENTER END
	or	NPTS START CENTER	NPTS START CENTER
	or	<punt>			<punt>

NPTS: given
SIECW (Start,Incr,End,Center,Width freeze-switch states; '1' = 'frozen')
---------------------------------------------------------------
00000	 (0)	END CENTER WIDTH	INCR
00001	 (1)	INCR			INCR
00010	 (2)	START END WIDTH		INCR
00011	 (3)	INCR			INCR
00100	 (4)	START CENTER WIDTH	INCR
00101	 (5)	INCR			INCR
00110	 (6)	INCR			INCR
00111	 (7)	INCR			INCR
01000	 (8)	END CENTER WIDTH	END CENTER WIDTH
01001	 (9)	<punt>			<punt>
01010	(10)	START END WIDTH		START END WIDTH
01011	(11)	<punt>			<punt>
01100	(12)	START CENTER WIDTH	START CENTER WIDTH
01101	(13)	<punt>			<punt>
01110	(14)	<punt>			<punt>
01111	(15)	<punt>			<punt>
10000	(16)	END CENTER WIDTH	INCR
10001	(17)	INCR			INCR
10010	(18)	INCR			INCR
10011	(19)	INCR			INCR
10100	(20)	INCR			INCR
10101	(21)	INCR			INCR
10110	(22)	INCR			INCR
10111	(23)	INCR			INCR
11000	(24)	END CENTER WIDTH	END CENTER WIDTH
11001	(25)	<punt>			<punt>
11010	(26)	<punt>			<punt>
11011	(27)	<punt>			<punt>
11100	(28)	<punt>			<punt>
11101	(29)	<punt>			<punt>
11110	(30)	<punt>			<punt>
11111	(31)	<punt>			<punt>


*/

