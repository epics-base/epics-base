/* devMbboDirectCamac.c */
/* devMbboDirectCamac.c - Device Support Routines for Camac Multi-bit binary direct output
 * -- 32 bit binary output						 	 	
 *
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            6th Aug 1993
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, SURA CEBAF.
 *
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
#include	<mbboDirectRecord.h>

#include "camacLib.h"

/* Create the dset for devMbboDirectCamac */
static long init();
static long init_record();
static long write_mbbodirect();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbodirect;
}devMbboDirectCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	write_mbbodirect};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if(CDEBUG)printf("devMbboDirectCamac (init) called, pass=%d\n", after);
#endif
    return(0);
}

static long init_record(pmbbodirect)
struct mbboDirectRecord	*pmbbodirect;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if(CDEBUG)printf("devMbboDirectCamac (init_record) called.\n");
#endif

    /* mbbodirect.out must be a CAMAC_IO */
    switch (pmbbodirect->out.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
             if(CDEBUG)printf("devMbboDirectCamac (init_record): malloc failed.\n")
#endif
             return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pmbbodirect->out.value);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devMbboDirectCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        fsd = (int)atoi((char *)pcamacio->parm);
/*
        if (fsd > 0) {
             if (!(fsd & (fsd+1))) fsd++;
             pmbbodirect->eslo=(pmbbodirect->eguf - pmbbodirect->egul)/fsd;
        }
*/
        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        pmbbodirect->dpvt = (long *)pcio;
        break;

    default :
	recGblRecordError(S_db_badField,(void *)pmbbodirect,
		"devMbboDirectCamac (init_record) Illegal OUT field");
        return(S_db_badField);
    }
    return(CONVERT);
}

static long write_mbbodirect(pmbbodirect)
struct mbboDirectRecord	*pmbbodirect;
{
register struct dinfo *pcio;
int   q;

        pcio = (struct dinfo *)pmbbodirect->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        q = 0;
        cfsa(pcio->f, pcio->ext, &(pmbbodirect->val), &q);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devMbboDirectCamac (write_mbbodirect):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pcio->mask, pmbbodirect->val);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
