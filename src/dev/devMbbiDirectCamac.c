/* devMbbiDirectCamac.c */
/* devMbbiDirectCamac.c - Device Support Routines for Camac Multi-bit binary direct input
 * Camac 32 bit Multibit binary input							
 *					
 *      Qriginal Author: 	Johnny Tang
 *      Current Author: 	Johnny Tang
 *      Date:            	March 23, 1994
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
 *      4-28-94		jt
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
#include	<module_types.h>
#include	<mbbiDirectRecord.h>

#include "camacLib.h"

static long init();
static long init_record();
static long read_mbbidirect();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbidirect;
}devMbbiDirectCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	read_mbbidirect};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if(CDEBUG)printf("devMbbiDirectCamac (init) Called. pass = %d\n", after);
#endif
    return(0);
}

static long init_record(pmbbidirect)
struct mbbiDirectRecord	*pmbbidirect;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if(CDEBUG)printf("devMbbiDirectCamac (init_record) Called.\n");
#endif

    /* mbbidirect.inp must be a CAMAC_IO */
    switch (pmbbidirect->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if(CDEBUG)printf("devMbbiDirectCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pmbbidirect->inp.value);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devMbbiDirectCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

        fsd = atoi((char *)pcamacio->parm);

/*
        if (fsd > 0) {
          if (!(fsd & (fsd+1))) fsd++; 
          pmbbidirect->eslo = (pmbbidirect->eguf - pmbbidirect->egul)/fsd;
        }
 */

        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        pmbbidirect->dpvt = (long *)pcio;
        break;

    default :
	recGblRecordError(S_db_badField,(void *)pmbbidirect,
		"devMbbiDirectCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(CONVERT);
}

static long read_mbbidirect(pmbbidirect)
struct mbbiDirectRecord	*pmbbidirect;
{
register struct dinfo *pcio;
int q;

        pcio = (struct dinfo *)pmbbidirect->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devMbbiDirectCamac (read_mbbidirect):  f=%d ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
        q=0;
        cfsa(pcio->f, pcio->ext, &(pmbbidirect->rval), &q);
        pmbbidirect->rval &= pcio->mask;

        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
