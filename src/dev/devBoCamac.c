/* devBoCamac.c */
/* devBoCamac.c : Generic device support for Camac binary output */
/*
 *      Original Author: Dave Barker
 *      Current Author:  Johnny Tang
 *      Date:            5th Aug 1993
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1993, SURA CEBAF.
 *
 *      Initial development by:
 *              MCC epics prototype team.
 *
 *      Co-developed with
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
#include	<module_types.h>
#include	<boRecord.h>
#include 	"camacLib.h"

/* Create the dset for devBoCamac */
static long init();
static long init_record();
static long write_bo();


struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoCamac={
	5,
	NULL,
	init,
	init_record,
	NULL,
	write_bo};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if ( CDEBUG)printf("devBoCamac (init) called, pass=%d\n", after);
#endif
    return(0);
}

static long init_record(pbo)
struct boRecord	*pbo;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if ( CDEBUG)printf("devBoCamac (init_record) called.\n");
#endif

    /* bo.out must be a CAMAC_IO */
    switch (pbo->out.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
             if ( CDEBUG)printf("devBoCamac (init_record): malloc failed.\n");
#endif
             return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pbo->out.value);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devBoCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        fsd = atoi((char *)pcamacio->parm);

        pbo->mask = 1;
        pbo->mask <<= fsd;

        pcio->f = pcamacio->f;

        pbo->dpvt = (long *)pcio;
	break;

    default :
        recGblRecordError(S_db_badField,(void *)pbo,
                       "devBoCamac (init_record) Illegal OUT field");
        return(S_db_badField);
    }
    return(CONVERT);
}

static long write_bo(pbo)
struct boRecord	*pbo;
{
register struct dinfo *pcio;
int   q;

        pcio = (struct dinfo *)pbo->dpvt;
        if(!(pcio->ext)) return(DO_NOT_CONVERT);

        pbo->rval &= pbo->mask;
        q = 0;
        cfsa(pcio->f, pcio->ext, &(pbo->rval), &q);
#ifdef DEBUG_ON
        if ( CDEBUG)printf("devBoCamac (write_bo):  f=%d ext=%ld mask=%ld value=%d\n",
                pcio->f, pcio->ext, pbo->mask, pbo->rval);
#endif
        if(q) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
