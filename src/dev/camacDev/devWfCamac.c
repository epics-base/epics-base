/* devWfCamac.c */
/* devWfCamac.c - Device Support Routines for  Camac waveform block read */
/*
 *      Author:          Johnny Tang
 *      Date:            16th March 1994.
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1994, SURA CEBAF.
 *
 *
 * Modification Log:
 * -----------------
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
#include	<waveformRecord.h>

#include "camacLib.h"

/* Create the dset for devWfCamac */
static long init();
static long init_record();
static long read_wf();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
	DEVSUPFUN	special_linconv;
}devWfCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	read_wf,
        NULL};

static long init(after)
int after;
{
#ifdef DEBUG_ON
    if(CDEBUG)printf("devWfCamac (init) called, pass = %d\n", after);
#endif
    return(0);
}

static long init_record(pwf)
struct waveformRecord	*pwf;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd;

#ifdef DEBUG_ON
    if(CDEBUG)printf("devWfCamac (init_record) called.\n");
#endif

    /* wf.inp must be a CAMAC_IO */
    switch (pwf->inp.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if(CDEBUG)printf("devWfCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
        pcamacio = (struct camacio *)&(pwf->inp.value);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devWfCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

        fsd = atoi((char *)pcamacio->parm);
/*
        if (fsd > 0) {
          if (!(fsd & (fsd+1))) fsd++; 
          pwf->eslo = (pwf->eguf - pwf->egul)/fsd;
        }
*/
        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        pwf->dpvt = (long *)pcio;

	break;

    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    
    return(0);
}

static long read_wf(pwf)
struct waveformRecord	*pwf;
{
register struct dinfo *pcio;
int    cb[4] = {0, 0, 0, 0};

        pcio = (struct dinfo *)pwf->dpvt;
        if(!(pcio->ext))return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devWfCamac (read_aai): F=%ld ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
        cb[0] = pwf->nelm;
        /* Execute CAMAC function
           if wf.ftvl is SHORT/USHORT use csubc (16 bits)
           else use cfubc (24 bits)
         */
        /* BLOCK TRANSFER mode */
#ifdef DEBUG_ON
        if(CDEBUG)printf("devWfCamac (read_aai) : Block Transfer mode\n");
#endif
        switch (pwf->ftvl) {
        case (DBF_USHORT) :
        case (DBF_SHORT) :
                csubc(pcio->f, pcio->ext, pwf->bptr, cb);
                break;
        case (DBF_ULONG) :
        case (DBF_LONG) :
                cfubc(pcio->f, pcio->ext, pwf->bptr, cb);
                break;
        default:
                cfubc(pcio->f, pcio->ext, pwf->bptr, cb);
        }
        pwf->nord = cb[1];
#ifdef DEBUG_ON
        if (CDEBUG) printf("pwf->ftvl:%d cb[0]:%d cb[1]:%d cb[2]:%d cb[3]:%d\n",
                                pwf->ftvl,cb[0],cb[1],cb[2],cb[3]);
#endif
        if(cb[0] == cb[1]) return(CONVERT);
        else return(DO_NOT_CONVERT);
}
