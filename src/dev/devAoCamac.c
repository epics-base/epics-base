/* devAoCamac.c */

/* devAoCamac.c - Device Support Routines for Generic Camac Analogue output */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            27th July 1993.
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1993, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
 * 4/14/94	jt	use camacio data struct 
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
#include	<aoRecord.h>

#include "camacLib.h"

/* Create the dset for devAoCamac */
static long init();
static long init_record();
static long write_ao();
static long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	write_ao,
        special_linconv};

static long init(after)
int after;
{
#ifdef DEBUG_ON 
    if ( CDEBUG)printf("devAoCamac (init) called, pass=%d\n", after);
#endif
    return(0);
}

static long init_record(pao)
struct aoRecord	*pao;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devAoCamac (init_record) called.\n");
#endif

    /* ao.out must be a CAMAC_IO */
    switch (pao->out.type) {
    case (CAMAC_IO) :
	  pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
	  if (pcio == NULL) {
#ifdef DEBUG_ON
    	     if ( CDEBUG)printf("devAoCamac (init_record): malloc failed.\n");
#endif
	     return(DO_NOT_CONVERT);
	  }
	  pcio->ext = 0;
          pcamacio = (struct camacio *)&(pao->out.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devAioamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
          cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

	  if(!(pcio->ext)) return(DO_NOT_CONVERT);
          sscanf((char *)pcamacio->parm, "%i",&fsd);

 /*         fsd = (int)atoi((char *)pcamacio->parm); */
          if (fsd > 0) {
	     if (!(fsd & (fsd+1))) fsd++;  
             pao->eslo=(pao->eguf - pao->egul)/fsd;
	  }

	  for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
	  pcio->mask--;

	  pcio->f = pcamacio->f;

	  pao->dpvt = (long *)pcio;
	  break;

    default :
	  recGblRecordError(S_db_badField,(void *)pao, 
			"devAoCamac (init_record) Illegal OUT field");
	  return(S_db_badField);
    }

    return(0);
}

static long write_ao(pao)
struct aoRecord	*pao;
{
register struct dinfo *pcio;
int   q;

	pcio = (struct dinfo *)pao->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
	
        pao->rval &= pcio->mask;
	q = 0;
        cfsa(pcio->f, pcio->ext, &(pao->rval), &q);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devAoCamac (write_ao):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pcio->mask, pao->rval);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}

static long special_linconv(pao,after)
struct aoRecord *pao;
int after;
{
    /* Stub routine, added for consistency. */
    return(0);
}
