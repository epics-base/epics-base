/* devAiCamac.c */
/* devAiCamac.c - Device Support Routines for  Generic Camac Analogue input */
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
 * 4/14/94	jt	modify cebaf_camacio to camacio data struct
 */

#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<dbDefs.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<devCamac.h>
#include	<link.h>
#include	<aiRecord.h>
#include "camacLib.h"
#define DEBUG_ON 1
/* Create the dset for devAiCamac */
static long init();
static long init_record();
static long read_ai();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	read_ai,
        NULL};


static long init(after)
int after;
{

#ifdef DEBUG_ON
    if ( CDEBUG) printf("devAiCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(pai)
struct aiRecord	*pai;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devAiCamac (init_record) Called.\n");
#endif

    /* ai.inp must be a CAMAC_IO */
    switch (pai->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if ( CDEBUG)printf("devAiCamac (init_record): malloc failed.\n");
#endif
	    return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
	pcamacio = (struct camacio *)&(pai->inp.value);


#ifdef DEBUG_ON
        if ( CDEBUG) printf("devAiCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
        	 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);
	
	if(!(pcio->ext)) 
        {
		pai->dpvt = (long *)pcio;
		return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */
	}

        sscanf((char *)pcamacio->parm, "%i",&fsd);
        if (fsd > 0) {
	  if (!(fsd & (fsd+1))) fsd++; /* camac card count should be 2's power */ 
          pai->eslo = (pai->eguf - pai->egul)/fsd;
	}

	for (pcio->mask=1; pcio->mask < fsd; pcio->mask=pcio->mask<<1);
	pcio->mask--;

	pcio->f = pcamacio->f;
  
	pai->dpvt = (long *)pcio;
	break;

    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
struct aiRecord	*pai;
{
register struct dinfo *pcio;
int q;

	pcio = (struct dinfo *)pai->dpvt;

	if(!(pcio->ext)) return(DO_NOT_CONVERT); 
#ifdef DEBUG_ON
        if ( CDEBUG) printf("devAiCamac (read_ai):  f=%d ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
	q=0;
        cfsa(pcio->f, pcio->ext, &(pai->rval), &q);
	pai->rval &= pcio->mask;

        
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
