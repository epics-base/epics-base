/* devAaoCamac.c */
/* devAaoCamac.c - Device Support Routines for Camac array analog output */
/*
 *      Author:          Johnny Tang
 *      Date:            1st April 1994.
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
#include	<aaoRecord.h>

#include "camacLib.h"

/* Create the dset for devAaoCamac */
static long init();
static long init_record();
static long write_aao();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_aao;
	DEVSUPFUN	special_linconv;
}devAaoCamac={
	6,
	NULL,
	init,
	init_record,
	NULL,
	write_aao,
        NULL};


static long init(after)
int after;
{
#ifdef DEBUG_ON
    if(CDEBUG)printf("devAaoCamac (init) Called, pass = %d\n", after);
#endif
    return(0);
}

static long init_record(paao)
struct aaoRecord	*paao;
{
struct camacio *pcamacio;
struct dinfo *pcio;
int fsd; 

#ifdef DEBUG_ON
	if(CDEBUG)printf("devAaoCamac (init_record) called.\n");
#endif 
    /* aao.out must be a CAMAC_IO */
    switch (paao->out.type) {
    case (CAMAC_IO) :
        pcio = (struct dinfo *)malloc(sizeof(struct dinfo));
        if (pcio == NULL) {
#ifdef DEBUG_ON
            if(CDEBUG)printf("devAaoCamac (init_record): malloc failed.\n");
#endif
            return(DO_NOT_CONVERT);
        }
        pcio->ext = 0;
	pcamacio = (struct camacio *)&(paao->out.value);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devAaiCamac (init_record): B=%d C=%d N=%d A=%d F=%d\n",
                 pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a, pcamacio->f);
#endif
        cdreg(&(pcio->ext), pcamacio->b, pcamacio->c, pcamacio->n, pcamacio->a);

        if(!(pcio->ext)) return(DO_NOT_CONVERT); /* cdreg failed if ext is zero */

        fsd = atoi((char *)pcamacio->parm);

        for (pcio->mask=1; pcio->mask<fsd; pcio->mask=pcio->mask<<1);
        pcio->mask--;

        pcio->f = pcamacio->f;

        paao->dpvt = (long *)pcio;

	break;
    default :
	recGblRecordError(S_db_badField,(void *)paao,
		"devAaoCamac (init_record) Illegal INP field");
	return(S_db_badField);
    }
    
    if (paao->nelm<=0) paao->nelm=1;
    if (paao->ftvl == 0) || (paao->ftvl > DBF_ENUM) {
	paao->bptr = (char *)calloc(paao->nelm,MAX_STRING_SIZE);
    }
    else {
        paao->bptr = (char *)calloc(paao->nelm,sizeofTypes[paao->ftvl]);
    }

    return(0);
}

static long write_aao(paao)
struct aaoRecord	*paao;
{
register struct dinfo *pcio;
int    cb[4] = {0, 0, 0, 0};

	pcio = (struct dinfo *)paao->dpvt;
        if(!(pcio->ext))return(DO_NOT_CONVERT);
#ifdef DEBUG_ON
        if(CDEBUG)printf("devAaoCamac (read_aai): F=%ld ext=%ld mask=%ld\n",
                pcio->f, pcio->ext, pcio->mask);
#endif
        cb[0] = paao->nelm;
        /* Execute CAMAC function 
           if aao.ftvl is SHORT/USHORT use csubc (16 bits)
           else use cfubc (24 bits)  
         */
	/* BLOCK TRANSFER mode */
#ifdef DEBUG_ON
	if(CDEBUG)printf("devAaoCamac (write_aao) : Block Transfer mode\n");
#endif
       	switch (paao->ftvl) {
        	case (DBF_USHORT) :
        	case (DBF_SHORT) :
	      		csubc(pcio->f, pcio->ext, paao->bptr, cb);
    	      	break;	
        	case (DBF_ULONG) :
        	case (DBF_LONG) :
	      		cfubc(pcio->f, pcio->ext, paao->bptr, cb);
    	      	break;	
		default:
#ifdef DEBUG_ON
   			if(CDEBUG)printf("devAaoCamac (write_aao): unexpected dbFldType: %d\n",
			  		paao->ftvl);
#endif
	      		cfubc(pcio->f, pcio->ext, paao->bptr, cb);
	}
	paao->nord = cb[1];
#ifdef DEBUG_ON
        if (CDEBUG) printf("paao->ftvl:%d cb[0]:%d cb[1]:%d cb[2]:%d cb[3]:%d\n", 
				paao->ftvl,cb[0],cb[1],cb[2],cb[3]);
#endif
        if(cb[0] == cb[1]) {
#ifdef DEBUG_ON
	   if(CDEBUG) 
		for(ii=0;ii<paao->nelm;ii++)printf("bptr[%d]=%d\n",ii,(int)paao->bptr+ii);
#endif
           return(CONVERT);
        }
        else return(DO_NOT_CONVERT);
}
