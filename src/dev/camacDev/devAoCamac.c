/*************************************************************************\
* Copyright (c) 2002 Southeastern Universities Research Association, as
*     Operator of Thomas Jefferson National Accelerator Facility.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAoCamac.c */

/* devAoCamac.c - Device Support Routines for Generic Camac Analogue output */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            27th July 1993.
 *
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
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

static long init(int after)
{
#ifdef DEBUG_ON 
    if ( CDEBUG)printf("devAoCamac (init) called, pass=%d\n", after);
#endif
    return(0);
}

static long init_record(struct aoRecord *pao)
{
struct camacio *pcamacio;
struct dinfo *pcio;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devAoCamac (init_record) called.\n");
#endif

    /* ao.out must be a CAMAC_IO */
    switch (pao->out.type) {
    case (CAMAC_IO) :
	  pcio = (struct dinfo *)calloc(1,sizeof(struct dinfo));
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
          sscanf((char *)pcamacio->parm, "%i",&pcio->fsd);

 /*         pcio->fsd = (int)atoi((char *)pcamacio->parm); */
          if (pcio->fsd > 0) {
	     if (!(pcio->fsd & (pcio->fsd+1))) pcio->fsd++;  
             pao->eslo=(pao->eguf - pao->egul)/pcio->fsd;
	  }

	  for (pcio->mask=1; pcio->mask<pcio->fsd; pcio->mask=pcio->mask<<1);
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

static long write_ao(struct aoRecord *pao)
{
struct dinfo *pcio;
int   q;

	pcio = (struct dinfo *)pao->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
	
        pao->rval &= pcio->mask;
	q = 0;
        cfsa(pcio->f, pcio->ext, (int *)&(pao->rval), &q);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devAoCamac (write_ao):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pcio->mask, pao->rval);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}

static long special_linconv(struct aoRecord *pao, int after)
{
    struct dinfo *pcio = (struct dinfo *)pao->dpvt;

    if(!after) return(0);
    /* set linear conversion slope*/
    if(pcio->fsd!=0) pao->eslo = (pao->eguf - pao->egul)/pcio->fsd;
    return(0);
}
