/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/rec  $Id$ */

/* 
 *      spectrum analyzer record support routines
 *
 *      Author:     Jeff Hill
 *      Ancestry:   Based on the waveform record by Bob Dalesio and Marty Kraimer
 *      Date:       9-29-03
 */
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <complex>
#include <iostream>

#include "dbDefs.h"
#include "epicsPrint.h"
#include "alarm.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbFldTypes.h"
#include "dbScan.h"
#include "devSup.h"
#include "errMdef.h"
#include "recSup.h"
#include "recGbl.h"
#include "special.h"
#include "epicsDSP.h"

typedef double fft_t;

#define GEN_SIZE_OFFSET
#include "spectrumRecord.h"
#undef  GEN_SIZE_OFFSET

static const fft_t  one = 1.0;
static const fft_t  four = 4.0;
static const fft_t  PI = four * atan ( one );

static void changeFrequencies ( struct spectrumRecord * pwf )
{
    unsigned long L = 1ul << pwf->logl;

    // freq change is unsuccessful if improper input parameters
    if ( pwf->hfrq <= pwf->lfrq ) {
        pwf->frok = 0;
        return;
    }
    if ( pwf->hfrq > pwf->ifrq / 2.0 ) {
        pwf->frok = 0;
        return;
    }
    if ( pwf->ifrq <= 0.0 ) {
        pwf->frok = 0;
        return;
    }

    if ( ! ( pwf->g && pwf->h && pwf->ival && 
            pwf->mag && pwf->ang && pwf->hamw ) ) {
        pwf->frok = 0;
        return;
    }

    const fft_t phi0d2 = ( PI / 2.0 / pwf->nelm ) * 
        ( pwf->hfrq - pwf->lfrq ) / pwf->ifrq ;
    for ( unsigned n = 0; n < pwf->nelm; n++ )
    {
        const fft_t nd = n;
        const fft_t radians = nd * nd * phi0d2;
        pwf->h[n] = std::polar ( one, radians );
    }
    for ( unsigned n = pwf->nelm; n < L-pwf->nin; n++ )
    {
        pwf->h[n] = std::complex < fft_t > ( 0.0, 0.0 );
    }
	for ( unsigned n = L-pwf->nin; n < L; n++ )
	{
        const fft_t index = L - n;
        const fft_t radians =  index * index * phi0d2;
        pwf->h[n] = std::polar ( one, radians );
	}
	epicsOneDimFFT ( pwf->h, pwf->logl, false );
    pwf->frok = true;
}

static void changeResolution ( struct spectrumRecord * pwf )
{
    {
        long nElemInTmp;
        long status = dbGetNelements ( 
            & pwf->inpw, & nElemInTmp );
        if ( status ) {
            pwf->nin = 1;
        }
        else { 
            if ( nElemInTmp < 1 ) {
                pwf->nin = 1;
            }
            else {
                pwf->nin = static_cast < unsigned long > 
                                ( nElemInTmp );
            }
        }
    }

    if ( pwf->nelm <= 0 ) 
        pwf->nelm = 1;

    {
        double tot = pwf->nin + pwf->nelm;
        double dblL = 1.0 + log ( tot ) / log ( 2.0 );
        pwf->logl = static_cast < unsigned long > ( dblL + 0.5 );
    }

    unsigned long L = 1ul << pwf->logl;
    assert ( L >= pwf->nin + pwf->nelm );
    delete pwf->g;
    delete pwf->h;
    delete pwf->ival;
    delete pwf->hamw;
    delete pwf->mag;
    delete pwf->ang;
    try {
        pwf->g = new std::complex < fft_t > [L];
        pwf->h = new std::complex < fft_t > [L];
        pwf->ival = new fft_t [pwf->nin];
        pwf->hamw = new fft_t [pwf->nin];
        pwf->mag = new fft_t [pwf->nelm];
        pwf->ang = new fft_t [pwf->nelm];
        // initialize hamming window 
        // see Oppenhiem and Schafer figure 5.33
	    for ( unsigned long n = 0; n < pwf->nin; n++ )
	    {
            pwf->hamw[n] = 0.54 - 0.46 * 
                cos ( 2 * PI * n / ( pwf->nin - 1 ) );
	    }
    }
    catch (... ) {
        recGblSetSevr ( pwf, CALC_ALARM, INVALID_ALARM );
        pwf->frok = false;
        return;
    }
    changeFrequencies ( pwf );
}

static long init_record ( struct spectrumRecord * pwf, int pass )
{
    if ( pass == 1 ) {
        changeResolution ( pwf );
    }
    return 0;
}

static long special ( struct dbAddr * paddr, int after )
{
    struct spectrumRecord * pwf = 
        reinterpret_cast < struct spectrumRecord * > ( paddr->precord );
    long status = 0;
    if ( after ) {
        switch ( paddr->special ) {
        case ( SPC_MOD ):
            if ( paddr->pfield == (void *) & pwf->nelm ||
                 paddr->pfield == (void *) & pwf->inpw ) {
                changeResolution ( pwf );
            }
            else {
                changeFrequencies ( pwf );
            }
            break;
        default:
            recGblDbaddrError ( S_db_badChoice, paddr, "spectrum: special" );
            status = S_db_badChoice;
            break;
        }
    }
    return status;
}

static void monitor ( struct spectrumRecord *pwf )
{
    unsigned short  monitor_mask;

    monitor_mask = recGblResetAlarms(pwf);
    monitor_mask |= ( DBE_LOG | DBE_VALUE );
    if ( monitor_mask ) {
        db_post_events ( pwf, pwf->mag, monitor_mask );
        db_post_events ( pwf, pwf->ang, monitor_mask );
    }
    return;
}

static long process ( struct spectrumRecord * pwf )
{
    if ( ! pwf->frok ) {
		recGblSetSevr ( pwf, CALC_ALARM, INVALID_ALARM );
        return 0;
    }
    long nRequest = static_cast < long > ( pwf->nin );
    long status = dbGetLink ( & pwf->inpw, DBR_DOUBLE, 
        pwf->ival, 0, & nRequest );
    if ( status ) {
		recGblSetSevr ( pwf, CALC_ALARM, INVALID_ALARM );
        return 0;
    }

    unsigned long nActual;
    if ( nRequest < 0 ) {
		recGblSetSevr ( pwf, CALC_ALARM, INVALID_ALARM );
        return 0;
    }
    else {
        nActual = static_cast < unsigned long > ( nRequest );
    }

    //epicsTime begin = epicsTime::getCurrent();

    const unsigned L = 1 << pwf->logl;

    const fft_t phi0d2 = ( PI / 2.0 / pwf->nelm ) * 
        ( pwf->hfrq - pwf->lfrq  ) / pwf->ifrq  ;
    const fft_t theta0 = PI * pwf->lfrq  / pwf->ifrq;

	for ( unsigned long n = 0; n < nActual; n++ )
	{
        const fft_t nd = n;
		const fft_t psi = nd * theta0 + 
            nd * nd * phi0d2;
		pwf->g[n] = pwf->ival[n] * pwf->hamw[n] *
            std::polar ( one, - psi );
	}
	for( unsigned long n = nActual ; n < L; n++ )
	{
		pwf->g[n] = 0.0;
	}

	epicsOneDimFFT ( pwf->g, pwf->logl, false );

	for( unsigned n = 0; n < L; n++ )
	{
		pwf->g[n] = pwf->g[n] * pwf->h[n];
        // epicsOneDimFFT does not scale 
        pwf->g[n] /= L;
	}

	epicsOneDimFFT ( pwf->g, pwf->logl, true );

	for ( unsigned k = 0; k < pwf->nelm; k++ )
	{
        const fft_t kd = k;
        const fft_t psi = kd * kd * phi0d2;
        pwf->g[k] = pwf->g[k] * std::polar ( one, - psi );
        pwf->mag[k] = abs ( pwf->g[k] );
        pwf->ang[k] = arg ( pwf->g[k] );
	}

    //epicsTime end = epicsTime::getCurrent();

    //cout << "delay per spectrum point " << 
    //    ((end - begin) / pwf->nelm ) * 1e6 << " uS " << endl;
	
    //for ( unsigned i = 0; i < pwf->nelm; i++ ) {
    //  cout << i << g[i] << endl;
    //}

    pwf->udf = FALSE;
    recGblGetTimeStamp ( pwf) ;

    monitor ( pwf );
    // process the forward scan link record
    recGblFwdLink ( pwf );

    pwf->pact=FALSE;
    return 0;
}

static long cvt_dbaddr( struct dbAddr * paddr )
{
    struct spectrumRecord * pwf = 
        reinterpret_cast < struct spectrumRecord * > 
            ( paddr->precord );

    if ( paddr->pfield == (void *) & pwf->val ) {
        paddr->pfield = pwf->mag;
        paddr->no_elements = pwf->nelm;
        paddr->field_type = DBF_DOUBLE;
        paddr->field_size = 8;
        paddr->dbr_field_type = DBF_DOUBLE;
    }
    else if ( paddr->pfield == (void *) & pwf->mag ) {
        paddr->pfield = pwf->mag;
        paddr->no_elements = pwf->nelm;
        paddr->field_type = DBF_DOUBLE;
        paddr->field_size = 8;
        paddr->dbr_field_type = DBF_DOUBLE;
    }
    else if ( paddr->pfield == (void *) & pwf->ang ) {
        paddr->pfield = pwf->ang;
        paddr->no_elements = pwf->nelm;
        paddr->field_type = DBF_DOUBLE;
        paddr->field_size = 8;
        paddr->dbr_field_type = DBF_DOUBLE;
    }
    return(0);
}

static long get_array_info ( struct dbAddr * paddr, long * no_elements, long * offset )
{
    struct spectrumRecord * pwf = ( struct spectrumRecord * ) paddr->precord;
    *no_elements = pwf->nelm;
    *offset = 0;
    return 0;
}

static long put_array_info ( struct dbAddr * paddr, long nNew )
{
    return 0;
}

static long get_units ( struct dbAddr * paddr, char * units )
{
    struct spectrumRecord * pwf = ( struct spectrumRecord * ) paddr->precord;
    strncpy ( units,pwf->egu, DB_UNITS_SIZE );
    return 0;
}

static long get_precision ( struct dbAddr * paddr, long * precision )
{
    struct spectrumRecord * pwf = ( struct spectrumRecord * ) paddr->precord;

    if ( paddr->pfield == (void *) pwf->mag ) {
        *precision = pwf->prec;
    }
    else if ( paddr->pfield == (void *) pwf->ang ) {
        *precision = pwf->prec;
    }
    else {
        recGblGetPrec ( paddr, precision );
    }
    return 0;
}

static long get_graphic_double ( struct dbAddr * paddr, struct dbr_grDouble *pgd )
{
    struct spectrumRecord * pwf = (struct spectrumRecord *) paddr->precord;
    if ( paddr->pfield == (void *) pwf->mag ) {
        pgd->upper_disp_limit = pwf->hopr;
        pgd->lower_disp_limit = pwf->lopr;
    } 
    else if ( paddr->pfield == (void *) pwf->ang ) {
        pgd->upper_disp_limit = +PI;
        pgd->lower_disp_limit = -PI;
    }
    else if ( paddr->pfield == (void *) & pwf->hfrq ) {
        pgd->upper_disp_limit = pwf->ifrq / 2.0;
        pgd->lower_disp_limit = 0;
    }
    else if ( paddr->pfield == (void *) & pwf->lfrq ) {
        pgd->upper_disp_limit = pwf->ifrq / 2.0;
        pgd->lower_disp_limit = 0;
    }
    else {
        recGblGetGraphicDouble ( paddr, pgd ) ;
    }
    return 0;
}

static long get_control_double ( struct dbAddr * paddr, struct dbr_ctrlDouble * pcd )
{
    struct spectrumRecord * pwf = ( struct spectrumRecord * ) paddr->precord;

    if ( paddr->pfield == (void *) pwf->mag ) {
        pcd->upper_ctrl_limit = pwf->hopr;
        pcd->lower_ctrl_limit = pwf->lopr;
    } 
    else if ( paddr->pfield == (void *) pwf->ang ) {
        pcd->upper_ctrl_limit = +PI;
        pcd->lower_ctrl_limit = -PI;
    }
    else if ( paddr->pfield == (void *) & pwf->hfrq ) {
        pcd->upper_ctrl_limit = pwf->ifrq / 2.0;
        pcd->lower_ctrl_limit = 0;
    }
    else if ( paddr->pfield == (void *) & pwf->lfrq ) {
        pcd->upper_ctrl_limit = pwf->ifrq / 2.0;
        pcd->lower_ctrl_limit = 0;
    }
    else {
        recGblGetControlDouble ( paddr, pcd );
    }

    return 0;
}

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
#define get_value NULL
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
#define get_alarm_double NULL
rset spectrumRSET={
    RSETNUMBER,
    (RECSUPFUN) report,
    (RECSUPFUN) initialize,
    (RECSUPFUN) init_record,
    (RECSUPFUN) process,
    (RECSUPFUN) special,
    (RECSUPFUN) get_value,
    (RECSUPFUN) cvt_dbaddr,
    (RECSUPFUN) get_array_info,
    (RECSUPFUN) put_array_info,
    (RECSUPFUN) get_units,
    (RECSUPFUN) get_precision,
    (RECSUPFUN) get_enum_str,
    (RECSUPFUN) get_enum_strs,
    (RECSUPFUN) put_enum_str,
    (RECSUPFUN) get_graphic_double,
    (RECSUPFUN) get_control_double,
    (RECSUPFUN) get_alarm_double
};

extern "C" {
    epicsExportAddress ( rset, spectrumRSET );
} 


