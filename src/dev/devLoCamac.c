/* devLoCamac.c */
/* devLoCamac.c - Device Support Routines for  Generic Camac Long output */
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
 *      4-28-94		jt
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
#include	<longoutRecord.h>
#include        "camacLib.h"

/* Create the dset for devLoCamac */
static long init();
static long init_record();
static long write_longout();
static long special_linconv();


struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;
	DEVSUPFUN	special_linconv;
}devLoCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	write_longout,
        special_linconv};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devLoCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(plongout)
struct longoutRecord	*plongout;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devAoCamac (init_record) called.\n");
#endif

    /* lo.out must be a CAMAC_IO */
    switch (plongout->out.type) {
    case (CAMAC_IO) :
          pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
          if (pcio == NULL) {
#ifdef DEBUG_ON
             if ( CDEBUG)printf("devLoCamac (init_record): malloc failed.\n");
#endif
             return(DO_NOT_CONVERT);
          }
          pcio->ext = 0;
          pcamacio = (struct camacio *)&(plongout->out.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devLoCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
          cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

          if(!(pcio->ext)) return(DO_NOT_CONVERT);

	sscanf((char *)pcamacio->parm, "%i",&fsd);
        
	if (fsd > 0) /* move up to 2^n if it's 2^n -1 */
	     if (!(fsd & (fsd+1))) fsd++;
          for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
          pcio->mask--;

          pcio->f = pcamacio->f;

          plongout->dpvt = (long *)pcio;
          break;

    default :
          recGblRecordError(S_db_badField,(void *)plongout,
                        "devLoCamac (init_record) Illegal OUT field");
          return(S_db_badField);
    }

    return(0);
}

static long write_longout(plongout)
struct longoutRecord	*plongout;
{
register struct dinfo *pcio;
int   q;

        pcio = (struct dinfo *)plongout->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        plongout->val &= pcio->mask;
        q = 0;
        cfsa(pcio->f, pcio->ext, &(plongout->val), &q);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devLoCamac (write_longout):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pcio->mask, plongout->val);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}

static long special_linconv(plongout,after)
struct longoutRecord *plongout;
int after;
{
    /* Stub routine, added for consistency. */
    return(0);
}

