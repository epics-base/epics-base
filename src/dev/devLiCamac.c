/* devLiCamac.c */
/* devLiCamac.c - Device Support Routines for  Generic Camac Long input */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            17th November 1993
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1993, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
 *	4-28-94		jt
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<devCamac.h>
#include	<link.h>
#include	<module_types.h>
#include	<longinRecord.h>
#include        "camacLib.h"


/* Create the dset for devLiCamac */
static long init();
static long init_record();
static long read_longin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin;
	DEVSUPFUN	special_linconv;
}devLiCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	read_longin,
        NULL};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devLiCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(plongin)
struct longinRecord	*plongin;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devLiCamac (init_record) Called.\n");
#endif

    /* li.inp must be a CAMAC_IO */
    switch (plongin->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if ( CDEBUG)printf("devLiCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(plongin->inp.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devLiCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */
	sscanf((char *)pcamacio->parm, "%i",&fsd);

        
	if (fsd > 0) /* move up to 2^n if it's 2^n -1 */
	     if (!(fsd & (fsd+1))) fsd++;
        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        plongin->dpvt = (long *)pcio;
	break;

    default :
	recGblRecordError(S_db_badField,(void *)plongin,
		"devLiCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_longin(plongin)
struct longinRecord	*plongin;
{
register struct dinfo *pcio;
int q;

        pcio = (struct dinfo *)plongin->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devLiCamac (read_longin):  f=%d ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
        q=0;
        cfsa(pcio->f, pcio->ext, &(plongin->val), &q);
        plongin->val &= pcio->mask;

        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
