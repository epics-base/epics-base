/* recScan.c */   
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
 * .01  07-18-94  nda     significantly expanded functionality from prototype
 * .02  08-18-94  nda     Starting with R3.11.6, dbGetField locks the record
 *                        before fetching the data. This can cause deadlocks
 *                        within a database. Change all dbGetField() to dbGet()
 * .03  08-23-94  nda     added code for checking/adjusting linear scan
 *                        params (it gets a little messy !)
 * .04  08-25-94  nda     Added check of scan positions vs Positioner Control
 *                        Limits
 * .05  08-29-94  nda     Added "viewScanPos" that puts desired positions
 *                        in D1 array any time a scan parameter is changed
 * .06  10-03-94  nda     added code for enumerated string .CMND. Changed 
 *                        .EXSC to a SHORT in record definition. Added VERSION
 *                        for .VERS field (1.06) to keep track of version. 
 * .07  10-21-94  nda     changed linear scan parameter algorithms so changing
 *                        start/end modifies step/width unless frozen. This
 *                        seems more intuitive.
 * .08  12-06-94  nda     added support for .FFO .When set to 1, frzFlag values
 *                        are saved in recPvtStruct. Restored when FFO set to 0.
 * .09  02-02-95  nda     fixed order of posting monitors to be what people
 *                        expect (i.e. .cpt, .pxdv, .dxcv)
 * .10  02-10-95  nda     fixed on-the-fly so 1st step is to end position
 */

#define VERSION 1.10



#include	<vxWorks.h> 
#include	<types.h>
#include	<stdioLib.h>
#include	<lstLib.h>
#include	<string.h>
#include	<math.h>
#include	<tickLib.h>
#include	<semLib.h>
#include	<taskLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include        <dbDefs.h>
#include	<dbFldTypes.h>
#include	<devSup.h>
#include	<errMdef.h>
#include	<recSup.h>
#include	<special.h>
#include	<choiceScan.h>
#include	<scanRecord.h>
#include	<callback.h>
#include	<taskwd.h>

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
static long get_value();
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

struct p_limits {
    DBRctrlDouble
    float        value;
};


struct recPvtStruct {
    CALLBACK           doPutsCallback;/* do output links callback structure */
    struct scanRecord *pscan;         /* ptr to record which needs work done */
    short              phase;         /* what to do to the above record */
    short              validRbPvs;    /* at least 1 valid Readback PV name */
    short              validDtPvs;    /* at least 1 valid Detector Trig PV  */
    short              validDataBuf;  /* which data array buffer is valid */
    float             *pdet1Fill;     /* points to data array to be filled */
    float             *pdet1DataA;    /* points to data array A */
    float             *pdet1DataB;    /* points to data array B */
    float             *pdet2Fill;
    float             *pdet2DataA;
    float             *pdet2DataB;
    float             *pdet3Fill;
    float             *pdet3DataA;
    float             *pdet3DataB;
    float             *pdet4Fill;
    float             *pdet4DataA;
    float             *pdet4DataB;
    struct p_limits   *pP1Limits;
    struct p_limits   *pP2Limits;
    struct p_limits   *pP3Limits;
    struct p_limits   *pP4Limits;
    short              pffo;          /* previouss state of ffo */
    short              fpts;          /* backup copy of all freeze flags */
    short              p1fs;
    short              p1fi;
    short              p1fc;
    short              p1fe;
    short              p1fw;
    short              p2fs;
    short              p2fi;
    short              p2fc;
    short              p2fe;
    short              p2fw;
    short              p3fs;
    short              p3fi;
    short              p3fc;
    short              p3fe;
    short              p3fw;
    short              p4fs;
    short              p4fi;
    short              p4fc;
    short              p4fe;
    short              p4fw;
    unsigned long      tickStart;     /* used to time the scan */
    unsigned char      scanErr;
    unsigned char      nptsCause;     /* who caused the "# of points to change:
                                         0-operator; 1-4 Positioners*/
};


/* the following structure must match EXACTLY with the order and type of
   fields defined in scanRecord.h for each positioner (even including
   the "Created Pad"s  */

struct linScanParms {
    float           p_sp;           /* Start Position */
    unsigned short  p_fs;           /* Freeze Start Pos */
    char            p76 [2];           /* Created Pad  */
    float           p_si;           /* Step Increment */
    unsigned short  p_fi;           /* Freeze Step Inc */
    char            p77 [2];             /* Created Pad  */
    float           p_cp;           /* Center Position */
    unsigned short  p_fc;           /* Freeze Center Pos */
    char            p78 [2];             /* Created Pad  */
    float           p_ep;           /* End Position */
    unsigned short  p_fe;           /* Freeze End Pos */
    char            p79 [2];             /* Created Pad  */
    float           p_wd;           /* Scan Width */
    unsigned short  p_fw;           /*  Freeze Width */
    char            p80 [2];             /* Created Pad  */
};
        
 

/***************************
  Declare constants
***************************/
#define DEF_WF_SIZE	100
#define PVN_SIZE        40
#define NUM_DYN_PVS     14
#define MIN_MON         3 /* # of ticks between monitor postings. 3 = 50 ms */

#define A_BUFFER        0
#define B_BUFFER        1

#define IDLE            0
#define MOVE_MOTORS     1
#define CHCK_MOTORS     2
#define TRIG_DETCTRS    3
#define READ_DETCTRS    4

#define CLEAR_MSG       0
#define CHECK_LIMITS    1


/*  forward declarations */

static void checkMonitors();
static void lookupPVs();
static void initScan();
static void contScan();
static void endScan();
static void doPuts();
static void adjLinParms();
static void changedNpts();
static void checkScanLimits();
static void drawPos1Scan();
static void saveFrzFlags();
static void resetFrzFlags();
static void restoreFrzFlags();
/* variables ... */
long    scanRecDebug=0;
long    viewScanPos=0; 


static long init_record(pscan,pass)
    struct scanRecord	*pscan;
    int pass;
{
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    long    status = 0;

    if (pass==0) {
      pscan->vers = VERSION;
      if(pscan->mpts < 100) pscan->mpts = DEF_WF_SIZE;

      pscan->p1db = calloc(1, sizeof(struct dbAddr));
      pscan->p2db = calloc(1, sizeof(struct dbAddr));
      pscan->p3db = calloc(1, sizeof(struct dbAddr));
      pscan->p4db = calloc(1, sizeof(struct dbAddr));
      pscan->r1db = calloc(1, sizeof(struct dbAddr));
      pscan->r2db = calloc(1, sizeof(struct dbAddr));
      pscan->r3db = calloc(1, sizeof(struct dbAddr));
      pscan->r4db = calloc(1, sizeof(struct dbAddr));
      pscan->t1db = calloc(1, sizeof(struct dbAddr));
      pscan->t2db = calloc(1, sizeof(struct dbAddr));
      pscan->d1db = calloc(1, sizeof(struct dbAddr));
      pscan->d2db = calloc(1, sizeof(struct dbAddr));
      pscan->d3db = calloc(1, sizeof(struct dbAddr));
      pscan->d4db = calloc(1, sizeof(struct dbAddr));

      pscan->p1pa = (float *) calloc(pscan->mpts, sizeof(float));
      pscan->p2pa = (float *) calloc(pscan->mpts, sizeof(float));
      pscan->p3pa = (float *) calloc(pscan->mpts, sizeof(float));
      pscan->p4pa = (float *) calloc(pscan->mpts, sizeof(float));

      /* First time through, rpvt needs initialized */
      pscan->rpvt = calloc(1, sizeof(struct recPvtStruct));
      precPvt = (struct recPvtStruct *)pscan->rpvt;

      precPvt->pdet1DataA = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet1DataB = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet2DataA = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet2DataB = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet3DataA = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet3DataB = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet4DataA = (float *) calloc(pscan->mpts, sizeof(float));
      precPvt->pdet4DataB = (float *) calloc(pscan->mpts, sizeof(float));

      precPvt->pP1Limits = (struct p_limits *)calloc(1, sizeof(struct p_limits));
      precPvt->pP2Limits = (struct p_limits *)calloc(1, sizeof(struct p_limits));
      precPvt->pP3Limits = (struct p_limits *)calloc(1, sizeof(struct p_limits));
      precPvt->pP4Limits = (struct p_limits *)calloc(1, sizeof(struct p_limits));

      /* initialize array fill pointers to buffer A, consider B_BUFFER valid */
      precPvt->pdet1Fill = precPvt->pdet1DataA;
      precPvt->pdet2Fill = precPvt->pdet2DataA;
      precPvt->pdet3Fill = precPvt->pdet3DataA;
      precPvt->pdet4Fill = precPvt->pdet4DataA;
      precPvt->validDataBuf = B_BUFFER;
      pscan->d1da = precPvt->pdet1DataB;
      pscan->d2da = precPvt->pdet2DataB;
      pscan->d3da = precPvt->pdet3DataB;
      pscan->d4da = precPvt->pdet4DataB;
      return(0);
    }

    callbackSetCallback(doPuts, &precPvt->doPutsCallback);
    callbackSetPriority(pscan->prio, &precPvt->doPutsCallback);

    precPvt->pscan = pscan;

    lookupPVs(pscan);

    /* initialize all linear scan fields */
    precPvt->nptsCause = 0; /* resolve all positioner parameters */
    changedNpts(pscan);

    if(pscan->ffo) {
       saveFrzFlags(pscan);
       resetFrzFlags(pscan);
    }

    /* init field values */
    pscan->exsc = 0;
    pscan->pxsc = 0;
    return(0);
}

static long process(pscan)
	struct scanRecord	*pscan;
{
        struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
        long	status = 0;

	/* Begin Scan cycle or continue scan cycle */
	if ((pscan->pxsc == 0) && (pscan->exsc == 1)) {
          if(scanRecDebug) {
              precPvt->tickStart = tickGet();
          }
	  initScan(pscan);
             
        } 
        else if ((pscan->pxsc == 1) && (pscan->exsc == 0)) {
          sprintf(pscan->smsg,"Scan aborted by operator");
          db_post_events(pscan,&pscan->smsg,DBE_VALUE);
          endScan(pscan);
        }
        else if (pscan->exsc == 1) {
          contScan(pscan);
        }

        checkMonitors(pscan);

        /* do forward link on last scan aquisition */
        if ((pscan->pxsc == 1) && (pscan->exsc == 0)) {
	recGblFwdLink(pscan);	/* process the forward scan link record */
        if(scanRecDebug) {
            printf("Scan Time = %.2f ms\n",
                    (float)((tickGet()-(precPvt->tickStart))*16.67)); 
          }

	}

        pscan->pxsc = pscan->exsc;
        recGblResetAlarms(pscan);
        recGblGetTimeStamp(pscan);

	pscan->pact = FALSE;
	return(status);
}

static long special(paddr,after)
    struct dbAddr *paddr;
    int	   	  after;
{
    struct scanRecord  	*pscan = (struct scanRecord *)(paddr->precord);
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    int           	special_type = paddr->special;
    long status;
    unsigned char       prevAlrt;

    if(!after) {
        precPvt->pffo = pscan->ffo; /* save previous ffo flag */
        return(0);
    }
    switch(special_type) {
    case(SPC_MOD):   
        if(paddr->pfield==(void *)&pscan->exsc) {
            pscan->alrt = 0;
            db_post_events(pscan,&pscan->alrt, DBE_VALUE);
            scanOnce(pscan);
            return(0);
        } else if(paddr->pfield==(void *)&pscan->cmnd) { 
            if(pscan->cmnd == CLEAR_MSG) {
                sprintf(pscan->smsg,"");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
            }
            else if(pscan->cmnd == CHECK_LIMITS) {
                prevAlrt = pscan->alrt;
                pscan->alrt = 0;
                checkScanLimits(pscan);
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                if(pscan->alrt != prevAlrt) {
                    db_post_events(pscan,&pscan->alrt,DBE_VALUE);
                } 
            }
            return(0);
        } else if((paddr->pfield >= (void *)pscan->p1pv) &&
                  (paddr->pfield <= (void *)pscan->d4pv)) {
            lookupPVs(pscan);
            return(0);
        } else if(paddr->pfield==(void *)&pscan->prio) {
            callbackSetPriority(pscan->prio, &precPvt->doPutsCallback);
            return(0);
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
        sprintf(pscan->smsg,"");
        adjLinParms(paddr);       
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(pscan->alrt != prevAlrt) {
            db_post_events(pscan,&pscan->alrt,DBE_VALUE);
        }
        if(viewScanPos) {
            drawPos1Scan(pscan);
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
        sprintf(pscan->smsg,"");
        precPvt->nptsCause = 0; /* resolve all positioner parameters */
        changedNpts(pscan);       
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(pscan->alrt != prevAlrt) {
            db_post_events(pscan,&pscan->alrt,DBE_VALUE);
        } 
        if(viewScanPos) {
            drawPos1Scan(pscan);
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
/*      recGblDbaddrError(S_db_badChoice,paddr,"scan: special");
	return(S_db_badChoice);
*/
    }
return(0);
}

static long cvt_dbaddr(paddr)
    struct dbAddr *paddr;
{
    struct scanRecord *pscan=(struct scanRecord *)paddr->precord;
    
    /* all arrays are <currently> floats */

    if (paddr->pfield == &(pscan->p1pa)) {
      paddr->pfield = (void *)(pscan->p1pa);
    } else if (paddr->pfield == &(pscan->p2pa)) {
      paddr->pfield = (void *)(pscan->p2pa);
    } else if (paddr->pfield == &(pscan->p3pa)) {
      paddr->pfield = (void *)(pscan->p3pa);
    } else if (paddr->pfield == &(pscan->p4pa)) {
      paddr->pfield = (void *)(pscan->p4pa);
    } else if (paddr->pfield == &(pscan->d1da)) {
      paddr->pfield = (void *)(pscan->d1da);
    } else if (paddr->pfield == &(pscan->d2da)) {
      paddr->pfield = (void *)(pscan->d2da);
    } else if (paddr->pfield == &(pscan->d3da)) {
      paddr->pfield = (void *)(pscan->d3da);
    } else if (paddr->pfield == &(pscan->d4da)) {
      paddr->pfield = (void *)(pscan->d4da);
    }

    paddr->no_elements = pscan->mpts;
    paddr->field_type = DBF_FLOAT;
    paddr->field_size = sizeof(float);
    paddr->dbr_field_type = DBF_FLOAT;
    return(0);
}

static long get_array_info(paddr,no_elements,offset)
    struct dbAddr *paddr;
    long          *no_elements;
    long          *offset;
{
    struct scanRecord       *pscan=(struct scanRecord *)paddr->precord;
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;

    /* if the paddr has a double buffer pointer, update it now */
    if ((paddr->pfield == precPvt->pdet1DataA) ||
        (paddr->pfield == precPvt->pdet1DataB)) {
        paddr->pfield = (void *)(pscan->d1da);
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if ((paddr->pfield == precPvt->pdet2DataA) ||
               (paddr->pfield == precPvt->pdet2DataB)) {
        paddr->pfield = (void *)(pscan->d2da);
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if ((paddr->pfield == precPvt->pdet3DataA) ||
               (paddr->pfield == precPvt->pdet3DataB)) {
        paddr->pfield = (void *)(pscan->d3da);
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if ((paddr->pfield == precPvt->pdet4DataA) ||
               (paddr->pfield == precPvt->pdet4DataB)) {
        paddr->pfield = (void *)(pscan->d4da);
        *no_elements = pscan->mpts;
        *offset = 0;
    } else if (paddr->pfield == pscan->p1pa) {
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
    struct scanRecord       *pscan=(struct scanRecord *)paddr->precord;
    
    if(nNew < pscan->npts) {
        sprintf(pscan->smsg,"Pts in Table < # of Steps");
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(!pscan->alrt) {
           pscan->alrt = 1;
           db_post_events(pscan,&pscan->alrt,DBE_VALUE);
        }
    }else {
        sprintf(pscan->smsg,"");
        db_post_events(pscan,&pscan->smsg,DBE_VALUE);
        if(pscan->alrt) {
           pscan->alrt = 0;
           db_post_events(pscan,&pscan->alrt,DBE_VALUE);
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
        if(sscanf(pstring,"%i",&pscan->cmnd)<=0)
            return(S_db_badChoice);
    }
    else
    {
        return(S_db_badChoice);
    }

    return(0);
}


static long get_value(pscan,pvdes)
    struct scanRecord		*pscan;
    struct valueDes	*pvdes;
{
    pvdes->field_type = DBF_DOUBLE;
    pvdes->no_elements=1;
    (double *)(pvdes->pvalue) = &pscan->p1dv;
    return(0);
}

static long get_units(paddr,units)
    struct dbAddr *paddr;
    char	  *units;
{
    struct scanRecord	*pscan=(struct scanRecord *)paddr->precord;
    return(0);
}

static long get_precision(paddr,precision)
    struct dbAddr *paddr;
    long	  *precision;
{
    struct scanRecord	*pscan=(struct scanRecord *)paddr->precord;

    if ((paddr->pfield >= (void *)&pscan->p1sm) &&
        (paddr->pfield <= (void *)&pscan->p1pa)) {
        *precision = pscan->p1pr; 
    }
    else if((paddr->pfield >= (void *)&pscan->p2sm) &&
        (paddr->pfield <= (void *)&pscan->p2pa)) {
        *precision = pscan->p2pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->p3sm) &&
        (paddr->pfield <= (void *)&pscan->p3pa)) {
        *precision = pscan->p3pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->p4sm) &&
        (paddr->pfield <= (void *)&pscan->p4pa)) {
        *precision = pscan->p4pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->d1cv) &&
        (paddr->pfield <= (void *)&pscan->d1da)) {
        *precision = pscan->d1pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->d2cv) &&
        (paddr->pfield <= (void *)&pscan->d2da)) {
        *precision = pscan->d2pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->d3cv) &&
        (paddr->pfield <= (void *)&pscan->d3da)) {
        *precision = pscan->d3pr;      
    }
    else if((paddr->pfield >= (void *)&pscan->d4cv) &&
        (paddr->pfield <= (void *)&pscan->d4da)) {
        *precision = pscan->d4pr;      
    }
    else {
        *precision = 2;
    }

    return(0);
}

static long get_graphic_double(paddr,pgd)
    struct dbAddr *paddr;
    struct dbr_grDouble	*pgd;
{
    struct scanRecord	*pscan=(struct scanRecord *)paddr->precord;

    if (((paddr->pfield >= (void *)&pscan->p1sm) &&
         (paddr->pfield <= (void *)&pscan->p1pa)) ||
         (paddr->pfield == pscan->p1pa)) {
          pgd->upper_disp_limit = pscan->p1hr;
          pgd->lower_disp_limit = pscan->p1lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->p2sm) &&
             (paddr->pfield <= (void *)&pscan->p2pa)) ||
             (paddr->pfield == pscan->p2pa)) {
          pgd->upper_disp_limit = pscan->p2hr;
          pgd->lower_disp_limit = pscan->p2lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->p3sm) &&
             (paddr->pfield <= (void *)&pscan->p3pa)) ||
             (paddr->pfield == pscan->p3pa)) {
          pgd->upper_disp_limit = pscan->p3hr;
          pgd->lower_disp_limit = pscan->p3lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->p4sm) &&
             (paddr->pfield <= (void *)&pscan->p4pa)) ||
             (paddr->pfield == pscan->p4pa)) {
          pgd->upper_disp_limit = pscan->p4hr;
          pgd->lower_disp_limit = pscan->p4lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->d1cv) &&
             (paddr->pfield <= (void *)&pscan->d1da)) ||
             (paddr->pfield == pscan->d1da)) {
          pgd->upper_disp_limit = pscan->d1hr;
          pgd->lower_disp_limit = pscan->d1lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->d2cv) &&
             (paddr->pfield <= (void *)&pscan->d2da)) ||
             (paddr->pfield == pscan->d2da)) {
          pgd->upper_disp_limit = pscan->d2hr;
          pgd->lower_disp_limit = pscan->d2lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->d3cv) &&
             (paddr->pfield <= (void *)&pscan->d3da)) ||
             (paddr->pfield == pscan->d3da)) {
          pgd->upper_disp_limit = pscan->d3hr;
          pgd->lower_disp_limit = pscan->d3lr;
    }
    else if(((paddr->pfield >= (void *)&pscan->d4cv) &&
             (paddr->pfield <= (void *)&pscan->d4da)) ||
             (paddr->pfield == pscan->d4da)) {
          pgd->upper_disp_limit = pscan->d4hr;
          pgd->lower_disp_limit = pscan->d4lr;
    }
    else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}

static long get_control_double(paddr,pcd)
    struct dbAddr *paddr;
    struct dbr_ctrlDouble *pcd;
{
    struct scanRecord	*pscan=(struct scanRecord *)paddr->precord;
    recGblGetControlDouble(paddr,pcd);
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
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    unsigned long        now;

    /* If last posting time is > MIN_MON, check to see if any dynamic fields 
       have changed (also post monitors on end of scan)*/

    now=tickGet();
    if(((now - pscan->tolp) > MIN_MON) || 
       ((pscan->pxsc == 1) && (pscan->exsc == 0))) {
        pscan->tolp = now;
        if(fabs(pscan->d1lv - pscan->d1cv) > 0) {
            db_post_events(pscan,&pscan->d1cv, DBE_VALUE);
            pscan->d1lv = pscan->d1cv;
        }
        if(fabs(pscan->d2lv - pscan->d2cv) > 0) {
            db_post_events(pscan,&pscan->d2cv, DBE_VALUE);
            pscan->d2lv = pscan->d2cv;
        }
        if(fabs(pscan->d3lv - pscan->d3cv) > 0) {
            db_post_events(pscan,&pscan->d3cv, DBE_VALUE);
            pscan->d3lv = pscan->d3cv;
        }
        if(fabs(pscan->d4lv - pscan->d4cv) > 0) {
            db_post_events(pscan,&pscan->d4cv, DBE_VALUE);
            pscan->d4lv = pscan->d4cv;
        }
        if(pscan->pcpt != pscan->cpt) {
            db_post_events(pscan,&pscan->cpt, DBE_VALUE);
            pscan->pcpt = pscan->cpt;
        }
        if(fabs(pscan->p1lv - pscan->p1dv) > 0) {
            db_post_events(pscan,&pscan->p1dv, DBE_VALUE);
            pscan->p1lv = pscan->p1dv;
        }
        if(fabs(pscan->r1lv - pscan->r1cv) > 0) {
            db_post_events(pscan,&pscan->r1cv, DBE_VALUE);
            pscan->r1lv = pscan->r1cv;
        }
        if(fabs(pscan->p2lv - pscan->p2dv) > 0) {
            db_post_events(pscan,&pscan->p2dv, DBE_VALUE);
            pscan->p2lv = pscan->p2dv;
        }
        if(fabs(pscan->r2lv - pscan->r2cv) > 0) {
            db_post_events(pscan,&pscan->r2cv, DBE_VALUE);
            pscan->r2lv = pscan->r2cv;
        }
        if(fabs(pscan->p3lv - pscan->p3dv) > 0) {
            db_post_events(pscan,&pscan->p3dv, DBE_VALUE);
            pscan->p3lv = pscan->p3dv;
        }
        if(fabs(pscan->r3lv - pscan->r3cv) > 0) {
            db_post_events(pscan,&pscan->r3cv, DBE_VALUE);
            pscan->r3lv = pscan->r3cv;
        }
        if(fabs(pscan->p4lv - pscan->p4dv) > 0) {
            db_post_events(pscan,&pscan->p4dv, DBE_VALUE);
            pscan->p4lv = pscan->p4dv;
        }
        if(fabs(pscan->r4lv - pscan->r4cv) > 0) {
            db_post_events(pscan,&pscan->r4cv, DBE_VALUE);
            pscan->r4lv = pscan->r4cv;
        }
    }

    if (pscan->pxsc != pscan->exsc) 
        db_post_events(pscan,&pscan->exsc, DBE_VALUE);

    /* if this is the end of a scan, post data arrays */
    if ((pscan->pxsc == 1) && (pscan->exsc == 0)) { 
        db_post_events(pscan,pscan->p1pa, DBE_VALUE);
        db_post_events(pscan,pscan->p2pa, DBE_VALUE);
        db_post_events(pscan,pscan->p3pa, DBE_VALUE);
        db_post_events(pscan,pscan->p4pa, DBE_VALUE);

        /* For the following arrays that are "double buffered", it is
           currently necessary to post_events on both buffers because
           of the way channel access determines "monitors" . 
           These arrays are posted even if the PV names are not valid,
           because they might have changed from valid to non-valid and
           the data needs updates. (remember, nothing will happen if
           no one has a monitor set */

        db_post_events(pscan,precPvt->pdet1DataA, DBE_VALUE);
        db_post_events(pscan,precPvt->pdet1DataB, DBE_VALUE);
      
        db_post_events(pscan,precPvt->pdet2DataA, DBE_VALUE);
        db_post_events(pscan,precPvt->pdet2DataB, DBE_VALUE);
        
        db_post_events(pscan,precPvt->pdet3DataA, DBE_VALUE);
        db_post_events(pscan,precPvt->pdet3DataB, DBE_VALUE);
       
        db_post_events(pscan,precPvt->pdet4DataA, DBE_VALUE);
        db_post_events(pscan,precPvt->pdet4DataB, DBE_VALUE);

        /* post alert if changed */
        if(precPvt->scanErr) {
            pscan->alrt = precPvt->scanErr;
            db_post_events(pscan,&pscan->alrt, DBE_VALUE);
        }
    }
}


static void lookupPVs(pscan)
    struct scanRecord	*pscan;
{
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    char          *ppvn[PVN_SIZE];
    struct dbAddr **ppdbAddr;   /* ptr to a ptr to dbAddr */
    long          *paddrValid;
    long          prevValid;
    short         i;

    /* Look up all reassignable PV Names */

    *ppvn = &pscan->p1pv[0];
    ppdbAddr = (struct dbAddr **)&pscan->p1db;
    paddrValid = &pscan->p1nv;

    for(i=0;i<NUM_DYN_PVS; i++, *ppvn += PVN_SIZE, ppdbAddr++, paddrValid++) {
        prevValid = *paddrValid;
        *paddrValid = dbNameToAddr(*ppvn, *ppdbAddr);
        if(*paddrValid != prevValid)  {
            db_post_events(pscan, paddrValid, DBE_VALUE);
        }
    }

    /* determine if any valid readbacks or det trigs to save time later */
    if(!pscan->r1nv || !pscan->r2nv || !pscan->r3nv || !pscan->r4nv)  {
        precPvt->validRbPvs = 1; 
    }
    else {
        precPvt->validRbPvs = 0;
    }
    if(!pscan->t1nv || !pscan->t2nv)  {
        precPvt->validDtPvs = 1; 
    }
    else {
        precPvt->validDtPvs = 0;
    }

}



static void initScan(pscan)
struct scanRecord *pscan;
{
  struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
  long  status;
  long  nRequest = 1;
  long  options = 0;

  pscan->cpt = 0;

  /* Figure out starting positions for each positioner */
  if(pscan->p1sm == REC_SCAN_MO_TAB) {
      pscan->p1dv = pscan->p1pa[0];
  }
  else {
      pscan->p1dv = pscan->p1sp;
      pscan->p1pa[pscan->cpt] = pscan->p1dv;
  }
  if(pscan->p2sm == REC_SCAN_MO_TAB) {
      pscan->p2dv = pscan->p2pa[0];
  }
  else {
      pscan->p2dv = pscan->p2sp;
      pscan->p2pa[pscan->cpt] = pscan->p2dv;
  }
  if(pscan->p3sm == REC_SCAN_MO_TAB) {
      pscan->p3dv = pscan->p3pa[0];
  }
  else {
      pscan->p3dv = pscan->p3sp;
      pscan->p3pa[pscan->cpt] = pscan->p3dv;
  }
  if(pscan->p4sm == REC_SCAN_MO_TAB) {
      pscan->p4dv = pscan->p4pa[0];
  }
  else {
      pscan->p4dv = pscan->p4sp;
      pscan->p4pa[pscan->cpt] = pscan->p4dv;
  }

  precPvt->phase = MOVE_MOTORS;
  /* request callback to do dbPutFields */
  callbackRequest(&precPvt->doPutsCallback);
  sprintf(pscan->smsg,"Scanning ...");
  db_post_events(pscan,&pscan->smsg,DBE_VALUE);
  return;
}

static void contScan(pscan)
struct scanRecord *pscan;
{
  struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
  long  status;
  long  nRequest = 1;
  long  options = 0;
  long  abortScan = 0;

  if(scanRecDebug > 4) {
      printf("contScan; phase = %d\n", (int)precPvt->phase); 
  }
  switch (precPvt->phase) {
    case TRIG_DETCTRS:
        /* request callback to do dbPutFields to detector trigger fields */
        callbackRequest(&precPvt->doPutsCallback);
        return;

    case CHCK_MOTORS:
        /* motors have stopped. Check readbacks and deadbands */
        if(!pscan->r1nv) {
            status = dbGet(pscan->r1db, DBR_FLOAT, &(pscan->r1cv),
                           &options, &nRequest, NULL);
            if((pscan->r1dl > 0) &&
               (fabs(pscan->p1dv - pscan->r1cv) > pscan->r1dl)) {
                sprintf(pscan->smsg,"SCAN Aborted: P1 Readback > delta");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                precPvt->scanErr = 1;
                abortScan = 1;
            }
        }
        if(!pscan->r2nv) {
            nRequest = 1;
            status = dbGet(pscan->r2db, DBR_FLOAT, &(pscan->r2cv),
                           &options, &nRequest, NULL);
            if((pscan->r2dl > 0) &&
               (fabs(pscan->p2dv - pscan->r2cv) > pscan->r2dl)) {
                sprintf(pscan->smsg,"SCAN Aborted: P2 Readback > delta");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                precPvt->scanErr = 1;
                abortScan = 1;
            }
        }
        if(!pscan->r3nv) {
            nRequest = 1;
            status = dbGet(pscan->r3db, DBR_FLOAT, &(pscan->r3cv),
                           &options, &nRequest, NULL);
            if((pscan->r3dl > 0) &&
               (fabs(pscan->p3dv - pscan->r3cv) > pscan->r3dl)) {
                sprintf(pscan->smsg,"SCAN Aborted: P3 Readback > delta");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                precPvt->scanErr = 1;
                abortScan = 1;
            }
        }
        if(!pscan->r4nv) {
            nRequest = 1;
            status = dbGet(pscan->r4db, DBR_FLOAT, &(pscan->r4cv),
                           &options, &nRequest, NULL);
            if((pscan->r4dl > 0) &&
               (fabs(pscan->p4dv - pscan->r4cv) > pscan->r4dl)) {
                sprintf(pscan->smsg,"SCAN Aborted: P4 Readback > delta");
                db_post_events(pscan,&pscan->smsg,DBE_VALUE);
                precPvt->scanErr = 1;
                abortScan = 1;
            }
        }

        if(abortScan) { 
            endScan(pscan);
            return;
        } 

        if(precPvt->validDtPvs) {
            /* request callback to do dbPutFields to detector trigger fields */
            precPvt->phase = TRIG_DETCTRS;
            callbackRequest(&precPvt->doPutsCallback);
            return;
        }

        /* if no validDtPvs, fall through to READ_DETCTRS */
        
    case READ_DETCTRS:
        /* read each valid detector PV, place data in buffered array */
        status = 0;
        if(!pscan->d1nv) {
            /* dbAddr is valid */
            nRequest = 1;
            status |= dbGet(pscan->d1db, DBR_FLOAT, &(pscan->d1cv),
                            &options, &nRequest, NULL);
        } else pscan->d1cv = 0;
        if(!pscan->d2nv) {
            nRequest = 1;
            status |= dbGet(pscan->d2db, DBR_FLOAT,
                            &(pscan->d2cv), &options, &nRequest, NULL);
        } else pscan->d2cv = 0;
        if(!pscan->d3nv) {
            nRequest = 1;
            status |= dbGet(pscan->d3db, DBR_FLOAT, &(pscan->d3cv),
                            &options, &nRequest, NULL);
        } else pscan->d3cv = 0;
        if(!pscan->d4nv) {
            nRequest = 1;
            status |= dbGet(pscan->d4db, DBR_FLOAT, &(pscan->d4cv),
                            &options, &nRequest, NULL);
        } else pscan->d4cv = 0;

        /* store the data using the Fill pointer */
        precPvt->pdet1Fill[pscan->cpt] = pscan->d1cv;
        precPvt->pdet2Fill[pscan->cpt] = pscan->d2cv;
        precPvt->pdet3Fill[pscan->cpt] = pscan->d3cv;
        precPvt->pdet4Fill[pscan->cpt] = pscan->d4cv;

        pscan->udf=0;

        pscan->cpt++;
        /* Has number of points been reached ? */
        if(pscan->cpt < (pscan->npts)) {
            /* determine next desired position for each positioner */
            if(pscan->p1sm == REC_SCAN_MO_LIN) {
                pscan->p1dv = pscan->p1dv + pscan->p1si;
                pscan->p1pa[pscan->cpt] = pscan->p1dv;
            }
            else if(pscan->p1sm == REC_SCAN_MO_TAB) {
                pscan->p1dv = pscan->p1pa[pscan->cpt];
            } 
            else {
                /* this is on-the-fly, so set to end position */
                pscan->p1dv = pscan->p1ep;
                pscan->p1pa[pscan->cpt] = pscan->p1dv;
            }
            if(pscan->p2sm == REC_SCAN_MO_LIN) {
                pscan->p2dv = pscan->p2dv + pscan->p2si;
                pscan->p2pa[pscan->cpt] = pscan->p2dv;
            }
            else if(pscan->p2sm == REC_SCAN_MO_TAB) {
                pscan->p2dv = pscan->p2pa[pscan->cpt];
            } 
            else {
                /* this is on-the-fly, so set to end position */
                pscan->p2dv = pscan->p2ep;
                pscan->p2pa[pscan->cpt] = pscan->p2dv;
            }
            if(pscan->p3sm == REC_SCAN_MO_LIN) {
                pscan->p3dv = pscan->p3dv + pscan->p3si;
                pscan->p3pa[pscan->cpt] = pscan->p3dv;
            }
            else if(pscan->p3sm == REC_SCAN_MO_TAB) {
                pscan->p3dv = pscan->p3pa[pscan->cpt];
            } 
            else {
                /* this is on-the-fly, so set to end position */
                pscan->p3dv = pscan->p3ep;
                pscan->p3pa[pscan->cpt] = pscan->p3dv;
            }
            if(pscan->p4sm == REC_SCAN_MO_LIN) {
                pscan->p4dv = pscan->p4dv + pscan->p4si;
                pscan->p4pa[pscan->cpt] = pscan->p4dv;
            }
            else if(pscan->p4sm == REC_SCAN_MO_TAB) {
                pscan->p4dv = pscan->p4pa[pscan->cpt];
            } 
            else {
                /* this is on-the-fly, so set to end position */
                pscan->p4dv = pscan->p4ep;
                pscan->p4pa[pscan->cpt] = pscan->p4dv;
            }

            /* request callback to move motors to new positions */
            precPvt->phase = MOVE_MOTORS; 
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

  struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
  int   counter;


  /* Done with scan. Do we want to fill the remainder of the
     array with 0 or something ? */
  /* It turns out that medm plots the whole array, so for it
     to look right the remainder of the arrays will be filled
     with the last values. This will cause medm to plot the
     same point over and over again, but it will look correct */

  counter = pscan->cpt;
  if(scanRecDebug) {
      printf("endScan(): Fill Counter - %d\n",counter);
  }
  while (counter < pscan->mpts) {
      precPvt->pdet1Fill[counter] = pscan->d1cv;
      precPvt->pdet2Fill[counter] = pscan->d2cv;
      precPvt->pdet3Fill[counter] = pscan->d3cv;
      precPvt->pdet4Fill[counter] = pscan->d4cv;

      if(pscan->p1sm != REC_SCAN_MO_TAB) {
          pscan->p1pa[counter] = pscan->p1dv;
      }
      if(pscan->p2sm != REC_SCAN_MO_TAB) {
          pscan->p2pa[counter] = pscan->p2dv;
      }
      if(pscan->p3sm != REC_SCAN_MO_TAB) {
          pscan->p3pa[counter] = pscan->p3dv;
      }
      if(pscan->p4sm != REC_SCAN_MO_TAB) {
          pscan->p4pa[counter] = pscan->p4dv;
      }

      counter++;
  }

  /* Switch the array pointers to the valid data */

  if(precPvt->validDataBuf == A_BUFFER) {
      pscan->d1da = precPvt->pdet1DataB;
      pscan->d2da = precPvt->pdet2DataB;
      pscan->d3da = precPvt->pdet3DataB;
      pscan->d4da = precPvt->pdet4DataB;
      /* Switch the Fill pointers for the next scan  */
      precPvt->pdet1Fill = precPvt->pdet1DataA;
      precPvt->pdet2Fill = precPvt->pdet2DataA;
      precPvt->pdet3Fill = precPvt->pdet3DataA;
      precPvt->pdet4Fill = precPvt->pdet4DataA;
      precPvt->validDataBuf = B_BUFFER;
  }
  else {
      pscan->d1da = precPvt->pdet1DataA;
      pscan->d2da = precPvt->pdet2DataA;
      pscan->d3da = precPvt->pdet3DataA;
      pscan->d4da = precPvt->pdet4DataA;
      /* Switch the Fill pointers for the next scan  */
      precPvt->pdet1Fill = precPvt->pdet1DataB;
      precPvt->pdet2Fill = precPvt->pdet2DataB;
      precPvt->pdet3Fill = precPvt->pdet3DataB;
      precPvt->pdet4Fill = precPvt->pdet4DataB;
      precPvt->validDataBuf = A_BUFFER;
  }

  pscan->exsc = 0;  /* done with scan */
  return;
}




/******************************************************************************
 *
 * This is the code that is executed by the callback task to initiate the next
 * step in the scan or trigger detectors (does dbPutFields for the reassignable
 * links). It is done with a separate task so one need not worry about lock
 *  sets.
 *
 *****************************************************************************/
void doPuts(precPvt)
   struct recPvtStruct *precPvt;
{
static long status;
static long nRequest = 1;

  if(scanRecDebug > 4) {
      printf("DoPuts; phase = %d\n", (int)precPvt->phase); 
  }
  switch (precPvt->phase) {
    case MOVE_MOTORS:
        /* next processing of records means motors have stopped */
        /* Schedule the next phase depending on readback PV validity */
        /* This needs to come before moving motors */

        if(precPvt->validRbPvs) {
            precPvt->phase = CHCK_MOTORS; 
        }
        else if(precPvt->validDtPvs) {
            precPvt->phase = TRIG_DETCTRS;
        }
        else {
            precPvt->phase = READ_DETCTRS;
        }

        /* for each valid positioner, write the desired position */
        if(!precPvt->pscan->p1nv) {
          status = dbPutField(precPvt->pscan->p1db,DBR_FLOAT,
                              &(precPvt->pscan->p1dv), 1);
        }
        if(!precPvt->pscan->p2nv) {
          status = dbPutField(precPvt->pscan->p2db,DBR_FLOAT,
                              &(precPvt->pscan->p2dv), 1);
        }
        if(!precPvt->pscan->p3nv) {
          status = dbPutField(precPvt->pscan->p3db,DBR_FLOAT,
                              &(precPvt->pscan->p3dv), 1);
        }
        if(!precPvt->pscan->p4nv) {
          status = dbPutField(precPvt->pscan->p4db,DBR_FLOAT,
                              &(precPvt->pscan->p4dv), 1);
        }

        break;
    case TRIG_DETCTRS:
        /* Schedule the next phase  */
        precPvt->phase = READ_DETCTRS;

        /* for each valid detector trigger, write the desired value */
        if(!precPvt->pscan->t1nv) {
          status = dbPutField(precPvt->pscan->t1db,DBR_FLOAT,
                              &(precPvt->pscan->t1cd), 1);
        }
        if(!precPvt->pscan->t2nv) {
          status = dbPutField(precPvt->pscan->t2db,DBR_FLOAT,
                              &(precPvt->pscan->t2cd), 1);
        }
        
        break;
    default:
  }
}




/* routine to resolve and adjust linear scan definition.
 * Might get complicated !!!
 */

static void adjLinParms(paddr)      
    struct dbAddr       *paddr;
{
    struct scanRecord   *pscan = (struct scanRecord *)(paddr->precord);
    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    struct linScanParms *pParms;

    int                 special_type = paddr->special;
    int                 i;

    /* determine which positioner */
    if ((paddr->pfield >= (void *)&pscan->p1sp) &&
        (paddr->pfield <= (void *)&pscan->p1fw)) {
        pParms = (struct linScanParms *)&pscan->p1sp;
        i=1;      
    }
    else if((paddr->pfield >= (void *)&pscan->p2sp) &&
            (paddr->pfield <= (void *)&pscan->p2fw)) {
        pParms = (struct linScanParms *)&pscan->p2sp;
        i=2;      
    }
    else if((paddr->pfield >= (void *)&pscan->p3sp) &&
            (paddr->pfield <= (void *)&pscan->p3fw)) {
        pParms = (struct linScanParms *)&pscan->p3sp;
        i=3;      
    }
    else if((paddr->pfield >= (void *)&pscan->p4sp) &&
            (paddr->pfield <= (void *)&pscan->p4fw)) {
        pParms = (struct linScanParms *)&pscan->p4sp;
        i=4;      
    }
    else {
        return;
    }

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
        /* if # of points/center/width are not frozen, change them  */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fw) { 
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        /* if # of points/end/width are not frozen, change them  */
        } else if(!pscan->fpts && !pParms->p_fe && !pParms->p_fw) { 
            pscan->npts = ((pParms->p_cp - pParms->p_sp)*2/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        /* if center/end are not frozen, change them  */
        } else if(!pParms->p_fc && !pParms->p_fe) { 
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            return;
        } else {                   /* too constrained !! */
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
            pscan->alrt = 1;
            return;
        }
        break;

      case(SPC_SC_I):              /* step increment changed */
        /* if end/center/width are not frozen, change them */
        if(!pParms->p_fe && !pParms->p_fc && !pParms->p_fw) { 
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
        /* if # of points is not frozen, change it  */
        } else if(!pscan->fpts) { 
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
                /* adjust changed field to be consistent */
                pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
                db_post_events(pscan,&pParms->p_si,DBE_VALUE);
                pscan->alrt = 1;
            }
            db_post_events(pscan,&pscan->npts,DBE_VALUE);
            precPvt->nptsCause = i; /* indicate cause of # of points changed */
            changedNpts(pscan);
            return;
        } else {                   /* too constrained !! */
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
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
        /* if start/center/width are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fc && !pParms->p_fw) { 
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
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
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        /* if # of points/center/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fw) {
            pscan->npts = ((pParms->p_ep - pParms->p_sp)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        } else {                   /* too constrained !! */
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
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
        /* if start/inc/width are not frozen, change them */
        } else if(!pParms->p_fs && !pParms->p_fi && !pParms->p_fw) { 
            pParms->p_wd = (pParms->p_ep - pParms->p_cp)*2;
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            pParms->p_sp = pParms->p_ep - pParms->p_wd;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
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
        /* if # of points/start/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fw) {
            pscan->npts=(((pParms->p_ep - pParms->p_cp)*2)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        /* if # of points/end/width are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fe && !pParms->p_fw) {
            pscan->npts=(((pParms->p_cp - pParms->p_sp)*2)/(pParms->p_si)) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        } else {                   /* too constrained !! */
            pParms->p_cp = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
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
        /* if # of points/start/end are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fs && !pParms->p_fe) {
            pscan->npts = (pParms->p_wd/pParms->p_si) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
        /* if # of points/center/end are not frozen, change them */
        } else if(!pscan->fpts && !pParms->p_fc && !pParms->p_fe) {
            pscan->npts = (pParms->p_wd/pParms->p_si) + 1;
            if(pscan->npts > pscan->mpts) {
                pscan->npts = pscan->mpts;
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
                sprintf(pscan->smsg,"P%1d Request Exceeded Maximum Points !",i);
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
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
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
 * no conflict with that logic.
 *
 */

static void changedNpts(pscan)
    struct scanRecord   *pscan;
{

    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    struct linScanParms *pParms;
    int                  i;
    unsigned short       freezeState = 0;

    /* for each positioner, calculate scan params as best as we can */
    for(i=1; i <= 4; i++) {
        /* Skip the positioner that caused this */
        if(i == precPvt->nptsCause) i++;

        if(i==1)
            pParms = (struct linScanParms *)&pscan->p1sp;
        else if(i==2)
            pParms = (struct linScanParms *)&pscan->p2sp;
        else if(i==3)
            pParms = (struct linScanParms *)&pscan->p3sp;
        else if(i==4)
            pParms = (struct linScanParms *)&pscan->p4sp;
        else
            return;

        /* develop freezeState switching word ... */
        /* START_FRZ | STEP_FRZ | END_FRZ | CENTER_FRZ | WIDTH_FRZ */

        freezeState = (pParms->p_fs << 4) |
                      (pParms->p_fi << 3) |   
                      (pParms->p_fe << 2) |   
                      (pParms->p_fc << 1) |   
                      (pParms->p_fw); 

        if(scanRecDebug) {
            printf("Freeze State of P%1d = 0x%hx \n",i,freezeState);
        }
        
        switch(freezeState) {
          case(0):  /* end/center/width unfrozen, change them */
          case(8):
          case(16):  
          case(24):
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          case(4):  /* start/center/width are not frozen, change them  */
          case(12):
            pParms->p_sp = pParms->p_ep - ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_cp = (pParms->p_ep + pParms->p_sp)/2;
            db_post_events(pscan,&pParms->p_cp,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          case(2):  /* if center is frozen, but not width/start/end , ...  */
          case(10):
            pParms->p_sp = pParms->p_cp - ((pscan->npts - 1) * pParms->p_si)/2;
            db_post_events(pscan,&pParms->p_sp,DBE_VALUE);
            pParms->p_ep = pParms->p_sp + ((pscan->npts - 1) * pParms->p_si);
            db_post_events(pscan,&pParms->p_ep,DBE_VALUE);
            pParms->p_wd = (pParms->p_ep - pParms->p_sp);
            db_post_events(pscan,&pParms->p_wd,DBE_VALUE);
            break;

          case(1):  /* if center or width is frozen, but not inc , ... */
          case(3):
          case(5):  /* if end/center or end/width are frozen, but not inc, */
          case(6): 
          case(7):
          case(17): /* if start/center or start/width are frozen, but not inc*/
          case(18):
          case(19):
          case(20):
          case(21):
          case(22):
          case(23):
            pParms->p_si = (pParms->p_ep - pParms->p_sp)/(pscan->npts - 1);
            db_post_events(pscan,&pParms->p_si,DBE_VALUE);
            break;

          /* The following freezeStates are known to be "Too Constrained" */
          /* 9,11,13,14,15,25,26,27,28,29,30,31 */
          default:                 /* too constrained !! */
            sprintf(pscan->smsg,"P%1d SCAN Parameters Too Constrained !",i);
            pscan->alrt = 1;
            break;
        }

    }
}



static void checkScanLimits(pscan)
    struct scanRecord   *pscan;
{

  struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;


  /* for each valid positioner, fetch control limits */
  long    status;
  long    nRequest = 1;
  long    options = DBR_CTRL_DOUBLE;
  long    abortScan = 0;
  int     i;
  double  value;

  /* for each valid positioner, fetch control limits */
  if(!pscan->p1nv) {
      status = dbGet(pscan->p1db, DBR_FLOAT, precPvt->pP1Limits,
                     &options, &nRequest, NULL);
  }
  if(!pscan->p2nv) {
      nRequest = 1;
      status = dbGet(pscan->p2db, DBR_FLOAT, precPvt->pP2Limits,
                     &options, &nRequest, NULL);
  }
  if(!pscan->p3nv) {
      nRequest = 1;
      status = dbGet(pscan->p3db, DBR_FLOAT, precPvt->pP3Limits,
                     &options, &nRequest, NULL);
  }
  if(!pscan->p4nv) {
      nRequest = 1;
      status = dbGet(pscan->p4db, DBR_FLOAT, precPvt->pP4Limits,
                     &options, &nRequest, NULL);
  }

  if(scanRecDebug) {
      if(!pscan->p1nv) 
          printf("P1 Control Limits : %.4f   %.4f\n",
              precPvt->pP1Limits->lower_ctrl_limit,
              precPvt->pP1Limits->upper_ctrl_limit);
      if(!pscan->p2nv) 
          printf("P2 Control Limits : %.4f   %.4f\n",
              precPvt->pP2Limits->lower_ctrl_limit,
              precPvt->pP2Limits->upper_ctrl_limit);
      if(!pscan->p3nv) 
          printf("P3 Control Limits : %.4f   %.4f\n",
              precPvt->pP3Limits->lower_ctrl_limit,
              precPvt->pP3Limits->upper_ctrl_limit);
      if(!pscan->p4nv) 
          printf("P4 Control Limits : %.4f   %.4f\n",
              precPvt->pP4Limits->lower_ctrl_limit,
              precPvt->pP4Limits->upper_ctrl_limit);
  }

  /* check each point of scan against control limits. */
  /* Stop on first error */

  for(i=0; i<pscan->npts; i++) {
      if(!pscan->p1nv) {
          /* determine next desired position for each positioner */
          if(pscan->p1sm == REC_SCAN_MO_LIN) {
              value = pscan->p1sp + (i * pscan->p1si);
          }
          else if(pscan->p1sm == REC_SCAN_MO_TAB) {
              value = pscan->p1pa[i];
          } 
          else {
          /* this is on-the-fly, so set to end position */
              if(i == 0) value = pscan->p1sp;
              else value = pscan->p1sp + pscan->p1si;
          }

          if(value < precPvt->pP1Limits->lower_ctrl_limit) {
              sprintf(pscan->smsg,"P1 Value < LO_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
          else if(value > precPvt->pP1Limits->upper_ctrl_limit) {
              sprintf(pscan->smsg,"P1 Value > HI_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
      }

      if(!pscan->p2nv) {
          /* determine next desired position for each positioner */
          if(pscan->p2sm == REC_SCAN_MO_LIN) {
              value = pscan->p2sp + (i * pscan->p2si);
          }
          else if(pscan->p2sm == REC_SCAN_MO_TAB) {
              value = pscan->p2pa[i];
          } 
          else {
          /* this is on-the-fly, so set to end position */
              if(i == 0) value = pscan->p2sp;
              else value = pscan->p2sp + pscan->p2si;
          }

          if(value < precPvt->pP2Limits->lower_ctrl_limit) {
              sprintf(pscan->smsg,"P2 Value < LO_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
          else if(value > precPvt->pP2Limits->upper_ctrl_limit) {
              sprintf(pscan->smsg,"P2 Value > HI_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
      }

      if(!pscan->p3nv) {
          /* determine next desired position for each positioner */
          if(pscan->p3sm == REC_SCAN_MO_LIN) {
              value = pscan->p3sp + (i * pscan->p3si);
          }
          else if(pscan->p3sm == REC_SCAN_MO_TAB) {
              value = pscan->p3pa[i];
          } 
          else {
          /* this is on-the-fly, so set to end position */
              if(i == 0) value = pscan->p3sp;
              else value = pscan->p3sp + pscan->p3si;
          }

          if(value < precPvt->pP3Limits->lower_ctrl_limit) {
              sprintf(pscan->smsg,"P3 Value < LO_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
          else if(value > precPvt->pP3Limits->upper_ctrl_limit) {
              sprintf(pscan->smsg,"P3 Value > HI_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
      }

      if(!pscan->p4nv) {
          /* determine next desired position for each positioner */
          if(pscan->p4sm == REC_SCAN_MO_LIN) {
              value = pscan->p4sp + (i * pscan->p4si);
          }
          else if(pscan->p4sm == REC_SCAN_MO_TAB) {
              value = pscan->p4pa[i];
          } 
          else {
          /* this is on-the-fly, so set to end position */
              if(i == 0) value = pscan->p4sp;
              else value = pscan->p4sp + pscan->p4si;
          }

          if(value < precPvt->pP4Limits->lower_ctrl_limit) {
              sprintf(pscan->smsg,"P4 Value < LO_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
          else if(value > precPvt->pP4Limits->upper_ctrl_limit) {
              sprintf(pscan->smsg,"P4 Value > HI_Limit @ point %1d",i);
              pscan->alrt = 1;
              return;
          }
      }

  }

  /* No errors if we made it here ... */
  sprintf(pscan->smsg,"SCAN Values within limits");

}


/* This routine fills the position 1 array with the scan positions
 * and fills the detector 1 array with 1's. If the detector 1 array
 * is plotted against the position 1 array, a plot is generated 
 * showing the scan range specified. This provides a graphical     
 * view of the scan range while changing scan paramters.
 *
 */

static void drawPos1Scan(pscan)
    struct scanRecord   *pscan;
{

    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;
    struct linScanParms *pParms;
    int                  i;
    unsigned short       freezeState = 0;

    float  desValue;

    desValue = pscan->p1sp;

    for(i=0; i<pscan->npts; i++) {
        pscan->p1pa[i] = desValue + (i * pscan->p1si);
        pscan->d1da[i] = 1;
    }

    /* fill array with last position */
    for(i=i; i<pscan->mpts;i++) {
        pscan->p1pa[i] = pscan->p1pa[i-1];
        pscan->d1da[i] = 1;
    }

    db_post_events(pscan,pscan->p1pa, DBE_VALUE);
    db_post_events(pscan,precPvt->pdet1DataA, DBE_VALUE);
    db_post_events(pscan,precPvt->pdet1DataB, DBE_VALUE);
    
}
        
 
static void saveFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;

    /* save state of each freeze flag */
    precPvt->fpts = pscan->fpts;
    precPvt->p1fs = pscan->p1fs;
    precPvt->p1fi = pscan->p1fi;
    precPvt->p1fc = pscan->p1fc;
    precPvt->p1fe = pscan->p1fe;
    precPvt->p1fw = pscan->p1fw;
    precPvt->p2fs = pscan->p2fs;
    precPvt->p2fi = pscan->p2fi;
    precPvt->p2fc = pscan->p2fc;
    precPvt->p2fe = pscan->p2fe;
    precPvt->p2fw = pscan->p2fw;
    precPvt->p3fs = pscan->p3fs;
    precPvt->p3fi = pscan->p3fi;
    precPvt->p3fc = pscan->p3fc;
    precPvt->p3fe = pscan->p3fe;
    precPvt->p3fw = pscan->p3fw;
    precPvt->p4fs = pscan->p4fs;
    precPvt->p4fi = pscan->p4fi;
    precPvt->p4fc = pscan->p4fc;
    precPvt->p4fe = pscan->p4fe;
    precPvt->p4fw = pscan->p4fw;
}

static void resetFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;

    /* reset each frzFlag, post monitor if changed */

    if(pscan->fpts) {
       pscan->fpts = 0;
       db_post_events(pscan,&pscan->fpts, DBE_VALUE);
    }

    if(pscan->p1fs) {
       pscan->p1fs = 0;
       db_post_events(pscan,&pscan->p1fs, DBE_VALUE);
    }

    if(pscan->p1fi) {
       pscan->p1fi = 0;
       db_post_events(pscan,&pscan->p1fi, DBE_VALUE);
    }

    if(pscan->p1fc) {
       pscan->p1fc = 0;
       db_post_events(pscan,&pscan->p1fc, DBE_VALUE);
    }

    if(pscan->p1fe) {
       pscan->p1fe = 0;
       db_post_events(pscan,&pscan->p1fe, DBE_VALUE);
    }

    if(pscan->p1fw) {
       pscan->p1fw = 0;
       db_post_events(pscan,&pscan->p1fw, DBE_VALUE);
    }

    if(pscan->p2fs) {
       pscan->p2fs = 0;
       db_post_events(pscan,&pscan->p2fs, DBE_VALUE);
    }

    if(pscan->p2fi) {
       pscan->p2fi = 0;
       db_post_events(pscan,&pscan->p2fi, DBE_VALUE);
    }

    if(pscan->p2fc) {
       pscan->p2fc = 0;
       db_post_events(pscan,&pscan->p2fc, DBE_VALUE);
    }

    if(pscan->p2fe) {
       pscan->p2fe = 0;
       db_post_events(pscan,&pscan->p2fe, DBE_VALUE);
    }

    if(pscan->p2fw) {
       pscan->p2fw = 0;
       db_post_events(pscan,&pscan->p2fw, DBE_VALUE);
    }

    if(pscan->p3fs) {
       pscan->p3fs = 0;
       db_post_events(pscan,&pscan->p3fs, DBE_VALUE);
    }

    if(pscan->p3fi) {
       pscan->p3fi = 0;
       db_post_events(pscan,&pscan->p3fi, DBE_VALUE);
    }

    if(pscan->p3fc) {
       pscan->p3fc = 0;
       db_post_events(pscan,&pscan->p3fc, DBE_VALUE);
    }

    if(pscan->p3fe) {
       pscan->p3fe = 0;
       db_post_events(pscan,&pscan->p3fe, DBE_VALUE);
    }

    if(pscan->p3fw) {
       pscan->p3fw = 0;
       db_post_events(pscan,&pscan->p3fw, DBE_VALUE);
    }

    if(pscan->p4fs) {
       pscan->p4fs = 0;
       db_post_events(pscan,&pscan->p4fs, DBE_VALUE);
    }

    if(pscan->p4fi) {
       pscan->p4fi = 0;
       db_post_events(pscan,&pscan->p4fi, DBE_VALUE);
    }

    if(pscan->p4fc) {
       pscan->p4fc = 0;
       db_post_events(pscan,&pscan->p4fc, DBE_VALUE);
    }

    if(pscan->p4fe) {
       pscan->p4fe = 0;
       db_post_events(pscan,&pscan->p4fe, DBE_VALUE);
    }

    if(pscan->p4fw) {
       pscan->p4fw = 0;
       db_post_events(pscan,&pscan->p4fw, DBE_VALUE);
    }
}





/* Restores Freeze Flags to the state they were in */
static void restoreFrzFlags(pscan)
    struct scanRecord   *pscan;
{

    struct recPvtStruct *precPvt = (struct recPvtStruct *)pscan->rpvt;

    /* restore state of each freeze flag, post if changed */
    pscan->fpts = precPvt->fpts;
    if(pscan->fpts) {
       db_post_events(pscan,&pscan->fpts, DBE_VALUE);
    }

    pscan->p1fs = precPvt->p1fs;
    if(pscan->p1fs) {
       db_post_events(pscan,&pscan->p1fs, DBE_VALUE);
    }

    pscan->p1fi = precPvt->p1fi;
    if(pscan->p1fi) {
       db_post_events(pscan,&pscan->p1fi, DBE_VALUE);
    }

    pscan->p1fc = precPvt->p1fc;
    if(pscan->p1fc) {
       db_post_events(pscan,&pscan->p1fc, DBE_VALUE);
    }

    pscan->p1fe = precPvt->p1fe;
    if(pscan->p1fe) {
       db_post_events(pscan,&pscan->p1fe, DBE_VALUE);
    }

    pscan->p1fw = precPvt->p1fw;
    if(pscan->p1fw) {
       db_post_events(pscan,&pscan->p1fw, DBE_VALUE);
    }

    pscan->p2fs = precPvt->p2fs;
    if(pscan->p2fs) {
       db_post_events(pscan,&pscan->p2fs, DBE_VALUE);
    }

    pscan->p2fi = precPvt->p2fi;
    if(pscan->p2fi) {
       db_post_events(pscan,&pscan->p2fi, DBE_VALUE);
    }

    pscan->p2fc = precPvt->p2fc;
    if(pscan->p2fc) {
       db_post_events(pscan,&pscan->p2fc, DBE_VALUE);
    }

    pscan->p2fe = precPvt->p2fe;
    if(pscan->p2fe) {
       db_post_events(pscan,&pscan->p2fe, DBE_VALUE);
    }

    pscan->p2fw = precPvt->p2fw;
    if(pscan->p2fw) {
       db_post_events(pscan,&pscan->p2fw, DBE_VALUE);
    }

    pscan->p3fs = precPvt->p3fs;
    if(pscan->p3fs) {
       db_post_events(pscan,&pscan->p3fs, DBE_VALUE);
    }

    pscan->p3fi = precPvt->p3fi;
    if(pscan->p3fi) {
       db_post_events(pscan,&pscan->p3fi, DBE_VALUE);
    }

    pscan->p3fc = precPvt->p3fc;
    if(pscan->p3fc) {
       db_post_events(pscan,&pscan->p3fc, DBE_VALUE);
    }

    pscan->p3fe = precPvt->p3fe;
    if(pscan->p3fe) {
       db_post_events(pscan,&pscan->p3fe, DBE_VALUE);
    }

    pscan->p3fw = precPvt->p3fw;
    if(pscan->p3fw) {
       db_post_events(pscan,&pscan->p3fw, DBE_VALUE);
    }

    pscan->p4fs = precPvt->p4fs;
    if(pscan->p4fs) {
       db_post_events(pscan,&pscan->p4fs, DBE_VALUE);
    }

    pscan->p4fi = precPvt->p4fi;
    if(pscan->p4fi) {
       db_post_events(pscan,&pscan->p4fi, DBE_VALUE);
    }

    pscan->p4fc = precPvt->p4fc;
    if(pscan->p4fc) {
       db_post_events(pscan,&pscan->p4fc, DBE_VALUE);
    }

    pscan->p4fe = precPvt->p4fe;
    if(pscan->p4fe) {
       db_post_events(pscan,&pscan->p4fe, DBE_VALUE);
    }

    pscan->p4fw = precPvt->p4fw;
    if(pscan->p4fw) {
       db_post_events(pscan,&pscan->p4fw, DBE_VALUE);
    }

}


