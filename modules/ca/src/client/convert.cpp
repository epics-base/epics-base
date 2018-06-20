/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *  C O N V E R T . C
 *
 *  Author: D. Kersteins
 *
 *
 *  NOTES: 
 *
 *  1) All routines in this file have an encode argument which
 *  determines if we are converting from the standard format to
 *  the local format or vise versa. To date only float and double data 
 *  types must be converted differently depending on the encode
 *  argument - joh
 *
 */

#include <string.h>

#include "dbDefs.h"
#include "osiSock.h"
#include "osiWireFormat.h"

#define epicsExportSharedSymbols
#include "net_convert.h"
#include "iocinf.h"
#include "caProto.h"
#include "caerr.h"

/*
 * NOOP if this isnt required
 */
#ifdef EPICS_CONVERSION_REQUIRED

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 */
typedef void ( * CACVRTFUNCPTR ) ( 
    const void *pSrc, void *pDest, int hton, arrayElementCount count );

inline  void dbr_htond ( 
    const dbr_double_t * pHost, dbr_double_t * pNet )
{
    AlignedWireRef < epicsFloat64 > tmp ( *pNet );
    tmp = *pHost;
}
inline void dbr_ntohd ( 
    const dbr_double_t * pNet, dbr_double_t * pHost )
{
    *pHost = AlignedWireRef < const epicsFloat64 > ( *pNet );
}
inline void dbr_htonf ( 
    const dbr_float_t * pHost, dbr_float_t * pNet )
{
    AlignedWireRef < epicsFloat32 > tmp ( *pNet );
    tmp = *pHost;
}
inline void dbr_ntohf ( 
    const dbr_float_t * pNet, dbr_float_t * pHost )
{
    *pHost = AlignedWireRef < const epicsFloat32 > ( *pNet );
}

inline epicsUInt16 dbr_ntohs( const epicsUInt16 & net ) 
{
    return AlignedWireRef < const epicsUInt16 > ( net );
}

inline epicsUInt16 dbr_htons ( const epicsUInt16 & host ) 
{
    epicsUInt16 tmp;
    AlignedWireRef < epicsUInt16 > awr ( tmp );
    awr = host;
    return tmp;
}

inline epicsUInt32 dbr_ntohl( const epicsUInt32 & net )
{
    return AlignedWireRef < const epicsUInt32 > ( net );
}

inline epicsUInt32 dbr_htonl ( const epicsUInt32 & host )
{
    epicsUInt32 tmp;
    AlignedWireRef < epicsUInt32 > awr ( tmp );
    awr = host;
    return tmp;
}

/*
 * if hton is true then it is a host to network conversion
 * otherwise vise-versa
 *
 * net format: big endian and IEEE float
 * 
 */

/*
 *  CVRT_STRING()
 */
static void cvrt_string(
const void          *s,             /* source           */
void                *d,             /* destination          */
int                 /* encode */,   /* cvrt HOST to NET if T    */
arrayElementCount   num             /* number of values     */
)
{
    char        *pSrc = (char *) s;
    char        *pDest = (char *) d;

    /* convert "in place" -> nothing to do */
    if (s == d)
        return;
    memcpy ( pDest, pSrc, num*MAX_STRING_SIZE );  
}

/*
 *  CVRT_SHORT()
 */
static void cvrt_short(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_short_t         *pSrc = (dbr_short_t *) s;
    dbr_short_t         *pDest = (dbr_short_t *) d;

    if(encode){
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_htons( pSrc[i] );
        }
    }
    else {
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_ntohs( pSrc[i] );
        }
    }
}

/*
 *  CVRT_CHAR()
 *
 *
 *
 *
 */
static void cvrt_char(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_char_t          *pSrc = (dbr_char_t *) s;
    dbr_char_t          *pDest = (dbr_char_t *) d;

    /* convert "in place" -> nothing to do */
    if (s == d)
        return;
    for( arrayElementCount i=0; i<num; i++){
        pDest[i] = pSrc[i];
    }
}

/*
 *  CVRT_LONG()
 */
static void cvrt_long(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_long_t          *pSrc = (dbr_long_t *) s;
    dbr_long_t          *pDest = (dbr_long_t *) d;

    if(encode){
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_htonl( pSrc[i] );
        }
    }
    else {
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_ntohl( pSrc[i] );
        }
    }
}

/*
 *  CVRT_ENUM()
 *
 *
 *
 *
 */
static void cvrt_enum(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_enum_t          *pSrc = (dbr_enum_t *) s;
    dbr_enum_t          *pDest = (dbr_enum_t *) d;

    if(encode){
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_htons ( pSrc[i] );
        }
    }
    else {
        for(arrayElementCount i=0; i<num; i++){
            pDest[i] = dbr_ntohs ( pSrc[i] );
        }
    }
}

/*
 *  CVRT_FLOAT()
 *
 *
 *  NOTES:
 *  placing encode outside the loop results in more 
 *  code but better performance.
 *
 */
static void cvrt_float(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    const dbr_float_t   *pSrc = (const dbr_float_t *) s;
    dbr_float_t         *pDest = (dbr_float_t *) d;

    if(encode){
        for(arrayElementCount i=0; i<num; i++){
            dbr_htonf ( &pSrc[i], &pDest[i] );
        }
    }
    else{
        for(arrayElementCount i=0; i<num; i++){
            dbr_ntohf ( &pSrc[i], &pDest[i] );
        }
    }
}

/*
 *  CVRT_DOUBLE()
 */
static void cvrt_double(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_double_t        *pSrc = (dbr_double_t *) s;
    dbr_double_t        *pDest = (dbr_double_t *) d;

    if(encode){
        for(arrayElementCount i=0; i<num; i++){
            dbr_htond ( &pSrc[i], &pDest[i] );
        }
    }
    else{
        for(arrayElementCount i=0; i<num; i++){
            dbr_ntohd( &pSrc[i], &pDest[i] );
        }
    }
}

/****************************************************************************
**  cvrt_sts_string(s,d)
**      struct dbr_sts_string *s    pointer to source struct
**      struct dbr_sts_string *d    pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**        
**  converts fields of struct in HOST format to NET format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_sts_string(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_string   *pSrc = (struct dbr_sts_string *) s;
    struct dbr_sts_string   *pDest = (struct dbr_sts_string *) d;
            
    /* convert ieee to vax format or vax to ieee */
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    /* convert "in place" -> nothing else to do */
    if (s == d)
        return;

    memcpy ( pDest->value, pSrc->value, (MAX_STRING_SIZE * num) );

}

/****************************************************************************
**  cvrt_sts_short(s,d)
**      struct dbr_sts_int *s   pointer to source struct
**      struct dbr_sts_int *d   pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_sts_short(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_int  *pSrc = (struct dbr_sts_int *) s;
    struct dbr_sts_int  *pDest = (struct dbr_sts_int *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohs(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_short(&pSrc->value, &pDest->value, encode, num);
    }
}
/****************************************************************************
**  cvrt_sts_float(s,d)
**      struct dbr_sts_float *s     pointer to source struct
**      struct dbr_sts_float *d     pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_sts_float(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_float    *pSrc = (struct dbr_sts_float *) s;
    struct dbr_sts_float    *pDest = (struct dbr_sts_float *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    cvrt_float(&pSrc->value, &pDest->value, encode, num);
}

/****************************************************************************
**  cvrt_sts_double(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_sts_double(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_double   *pSrc = (struct dbr_sts_double *) s;
    struct dbr_sts_double   *pDest = (struct dbr_sts_double *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    cvrt_double(&pSrc->value, &pDest->value, encode, num);
}

/****************************************************************************
**  cvrt_sts_enum(s,d)
**      struct dbr_sts_enum *s      pointer to source struct
**      struct dbr_sts_enum *d      pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_sts_enum(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_enum *pSrc = (struct dbr_sts_enum *) s;
    struct dbr_sts_enum *pDest = (struct dbr_sts_enum *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    if (num == 1)
        pDest->value    = dbr_ntohs(pSrc->value);
    else {
        cvrt_enum(&pSrc->value,&pDest->value,encode,num);
    }
}

/****************************************************************************
**  cvrt_gr_short()
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_gr_short(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_int   *pSrc = (struct dbr_gr_int *) s;
    struct dbr_gr_int   *pDest = (struct dbr_gr_int *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    pDest->upper_disp_limit     = dbr_ntohs(pSrc->upper_disp_limit);
    pDest->lower_disp_limit     = dbr_ntohs(pSrc->lower_disp_limit);
    pDest->upper_alarm_limit    = dbr_ntohs(pSrc->upper_alarm_limit);
    pDest->upper_warning_limit  = dbr_ntohs(pSrc->upper_warning_limit);
    pDest->lower_alarm_limit    = dbr_ntohs(pSrc->lower_alarm_limit);
    pDest->lower_warning_limit  = dbr_ntohs(pSrc->lower_warning_limit);

    if (num == 1)
        pDest->value = dbr_ntohs(pSrc->value);
    else {
        cvrt_short(&pSrc->value, &pDest->value, encode,num);
    }
}

/****************************************************************************
**  cvrt_gr_char()
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_gr_char(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_char  *pSrc = (struct dbr_gr_char *) s;
    struct dbr_gr_char  *pDest = (struct dbr_gr_char *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    if (s == d) /* source == dest -> no more conversions */
        return;

    memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));

    pDest->upper_disp_limit     = pSrc->upper_disp_limit;
    pDest->lower_disp_limit     = pSrc->lower_disp_limit;
    pDest->upper_alarm_limit    = pSrc->upper_alarm_limit;
    pDest->upper_warning_limit  = pSrc->upper_warning_limit;
    pDest->lower_alarm_limit    = pSrc->lower_alarm_limit;
    pDest->lower_warning_limit  = pSrc->lower_warning_limit;

    if (num == 1)
        pDest->value = pSrc->value;
    else {
        memcpy((char *)&pDest->value, (char *)&pSrc->value, num);
    }
}

/****************************************************************************
**  cvrt_gr_long()
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_gr_long(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_long  *pSrc = (struct dbr_gr_long *) s;
    struct dbr_gr_long  *pDest = (struct dbr_gr_long *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    pDest->upper_disp_limit     = dbr_ntohl(pSrc->upper_disp_limit);
    pDest->lower_disp_limit     = dbr_ntohl(pSrc->lower_disp_limit);
    pDest->upper_alarm_limit    = dbr_ntohl(pSrc->upper_alarm_limit);
    pDest->upper_warning_limit  = dbr_ntohl(pSrc->upper_warning_limit);
    pDest->lower_alarm_limit    = dbr_ntohl(pSrc->lower_alarm_limit);
    pDest->lower_warning_limit  = dbr_ntohl(pSrc->lower_warning_limit);

    if (num == 1)
        pDest->value = dbr_ntohl(pSrc->value);
    else {
        cvrt_long(&pSrc->value, &pDest->value, encode, num);
    }
}

/****************************************************************************
**  cvrt_gr_enum(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_gr_enum(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_enum  *pSrc = (struct dbr_gr_enum *) s;
    struct dbr_gr_enum  *pDest = (struct dbr_gr_enum *) d;

    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    pDest->no_str           = dbr_ntohs(pSrc->no_str);
    if ( s != d ) {
        memcpy((void *)pDest->strs,(void *)pSrc->strs,sizeof(pSrc->strs));
    }

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohs(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
    }
}

/****************************************************************************
**  cvrt_gr_double(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_gr_double(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_double    *pSrc = (struct dbr_gr_double *) s;
    struct dbr_gr_double    *pDest = (struct dbr_gr_double *) d;

    /* these are same for vax to ieee or ieee to vax */
    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    pDest->precision        = dbr_ntohs(pSrc->precision);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    if (encode)         /* vax to ieee convert      */
    {
        if (num == 1){
            dbr_htond(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_double(&pSrc->value, &pDest->value, encode,num);
        }
        dbr_htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    }
    else            /* ieee to vax convert      */
    {
        if (num == 1){
            dbr_ntohd(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_double(&pSrc->value, &pDest->value, encode,num);
        }
        dbr_ntohd(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    }
}


/****************************************************************************
**  cvrt_gr_float(s,d)
**      struct dbr_gr_float *d      pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_gr_float(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_gr_float     *pSrc = (struct dbr_gr_float *) s;
    struct dbr_gr_float     *pDest = (struct dbr_gr_float *) d;

    /* these are same for vax to ieee or ieee to vax */
    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    pDest->precision        = dbr_ntohs(pSrc->precision);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    if (encode)         /* vax to ieee convert      */
    {
        if (num == 1){
            dbr_htonf(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_float(&pSrc->value, &pDest->value, encode,num);
        }
        dbr_htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    }
    else            /* ieee to vax convert      */
    {
        if (num == 1){
            dbr_ntohf(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_float(&pSrc->value, &pDest->value, encode,num);
        }
        dbr_ntohf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
    }
}



/****************************************************************************
**  cvrt_ctrl_short(s,d)
**      struct dbr_gr_int *s    pointer to source struct
**      struct dbr_gr_int *d    pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_ctrl_short(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_int *pSrc = (struct dbr_ctrl_int *) s;
    struct dbr_ctrl_int *pDest = (struct dbr_ctrl_int *) d;

    /* vax to ieee or ieee to vax -- same code */
    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    pDest->upper_disp_limit     = dbr_ntohs(pSrc->upper_disp_limit);
    pDest->lower_disp_limit     = dbr_ntohs(pSrc->lower_disp_limit);
    pDest->upper_alarm_limit    = dbr_ntohs(pSrc->upper_alarm_limit);
    pDest->upper_warning_limit  = dbr_ntohs(pSrc->upper_warning_limit);
    pDest->lower_alarm_limit    = dbr_ntohs(pSrc->lower_alarm_limit);
    pDest->lower_warning_limit  = dbr_ntohs(pSrc->lower_warning_limit);
    pDest->lower_ctrl_limit     = dbr_ntohs(pSrc->lower_ctrl_limit);
    pDest->upper_ctrl_limit     = dbr_ntohs(pSrc->upper_ctrl_limit);

    if (num == 1)
        pDest->value = dbr_ntohs(pSrc->value);
    else {
        cvrt_short(&pSrc->value, &pDest->value, encode, num);
    }
}

/****************************************************************************
**  cvrt_ctrl_long(s,d)
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_ctrl_long(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_long    *pSrc = (struct dbr_ctrl_long*) s;
    struct dbr_ctrl_long    *pDest = (struct dbr_ctrl_long *) d;

    /* vax to ieee or ieee to vax -- same code */
    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }

    pDest->upper_disp_limit     = dbr_ntohl(pSrc->upper_disp_limit);
    pDest->lower_disp_limit     = dbr_ntohl(pSrc->lower_disp_limit);
    pDest->upper_alarm_limit    = dbr_ntohl(pSrc->upper_alarm_limit);
    pDest->upper_warning_limit  = dbr_ntohl(pSrc->upper_warning_limit);
    pDest->lower_alarm_limit    = dbr_ntohl(pSrc->lower_alarm_limit);
    pDest->lower_warning_limit  = dbr_ntohl(pSrc->lower_warning_limit);
    pDest->lower_ctrl_limit     = dbr_ntohl(pSrc->lower_ctrl_limit);
    pDest->upper_ctrl_limit     = dbr_ntohl(pSrc->upper_ctrl_limit);

    if (num == 1)
        pDest->value = dbr_ntohl(pSrc->value);
    else {
        cvrt_long(&pSrc->value, &pDest->value, encode, num);
    }
}

/****************************************************************************
**  cvrt_ctrl_short(s,d)
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_ctrl_char(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_char    *pSrc = (struct dbr_ctrl_char *) s;
    struct dbr_ctrl_char    *pDest = (struct dbr_ctrl_char *) d;

    /* vax to ieee or ieee to vax -- same code */
    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);

    if ( s == d ) 
        return;

    pDest->upper_disp_limit     = pSrc->upper_disp_limit;
    pDest->lower_disp_limit     = pSrc->lower_disp_limit;
    pDest->upper_alarm_limit    = pSrc->upper_alarm_limit;
    pDest->upper_warning_limit  = pSrc->upper_warning_limit;
    pDest->lower_ctrl_limit     = pSrc->lower_ctrl_limit;
    pDest->upper_ctrl_limit     = pSrc->upper_ctrl_limit;

    if (num == 1)
        pDest->value = pSrc->value;
    else {
        memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    }
}

/****************************************************************************
**  cvrt_ctrl_double(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_ctrl_double(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_double  *pSrc = (struct dbr_ctrl_double *) s;
    struct dbr_ctrl_double  *pDest = (struct dbr_ctrl_double *) d;

    /* these are the same for ieee to vax or vax to ieee */
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->precision    = dbr_ntohs(pSrc->precision);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }
    if (encode)         /* vax to ieee convert      */
    {
        if (num == 1){
            dbr_htond(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_double(&pSrc->value, &pDest->value, encode, num);
        }
        dbr_htond(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_htond(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_htond(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_htond(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_htond(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_htond(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
        dbr_htond(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
        dbr_htond(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    }
    else            /* ieee to vax convert      */
    {
        if (num == 1){
                dbr_ntohd(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_double(&pSrc->value, &pDest->value, encode, num);
        }
        dbr_ntohd(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_ntohd(&pSrc->upper_disp_limit, &pDest->upper_disp_limit);
        dbr_ntohd(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_ntohd(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_ntohd(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_ntohd(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
        dbr_ntohd(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
        dbr_ntohd(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    }

}



/****************************************************************************
**  cvrt_ctrl_float(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_ctrl_float(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_float   *pSrc = (struct dbr_ctrl_float *) s;
    struct dbr_ctrl_float   *pDest = (struct dbr_ctrl_float *) d;

    /* these are the same for ieee to vaax or vax to ieee */
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->precision        = dbr_ntohs(pSrc->precision);
    if ( s != d ) {
        memcpy(pDest->units,pSrc->units,sizeof(pSrc->units));
    }
    if (encode)         /* vax to ieee convert      */
    {
        if (num == 1){
                dbr_htonf(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_float(&pSrc->value, &pDest->value, encode, num);
        }
        dbr_htonf(&pSrc->upper_disp_limit,&pDest->upper_disp_limit);
        dbr_htonf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_htonf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_htonf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_htonf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_htonf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
        dbr_htonf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
        dbr_htonf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    }
    else            /* ieee to vax convert      */
    {
        if (num == 1){
            dbr_ntohf(&pSrc->value, &pDest->value);
        }
        else {
            cvrt_float(&pSrc->value, &pDest->value, encode, num);
        }
        dbr_ntohf(&pSrc->lower_disp_limit, &pDest->lower_disp_limit);
        dbr_ntohf(&pSrc->upper_disp_limit, &pDest->upper_disp_limit);
        dbr_ntohf(&pSrc->upper_alarm_limit, &pDest->upper_alarm_limit);
        dbr_ntohf(&pSrc->upper_warning_limit, &pDest->upper_warning_limit);
        dbr_ntohf(&pSrc->lower_alarm_limit, &pDest->lower_alarm_limit);
        dbr_ntohf(&pSrc->lower_warning_limit, &pDest->lower_warning_limit);
        dbr_ntohf(&pSrc->lower_ctrl_limit, &pDest->lower_ctrl_limit);
        dbr_ntohf(&pSrc->upper_ctrl_limit, &pDest->upper_ctrl_limit);
    }

}


/****************************************************************************
**  cvrt_ctrl_enum(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_ctrl_enum(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_ctrl_enum    *pSrc = (struct dbr_ctrl_enum *) s;
    struct dbr_ctrl_enum    *pDest = (struct dbr_ctrl_enum *) d;

    pDest->status           = dbr_ntohs(pSrc->status);
    pDest->severity         = dbr_ntohs(pSrc->severity);
    pDest->no_str           = dbr_ntohs(pSrc->no_str); 
    if ( s != d ) {
        memcpy((void *)pDest->strs,(void *)pSrc->strs,sizeof(pSrc->strs));
    }

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohs(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_enum(&(pSrc->value), &(pDest->value), encode, num);
    }
}

/****************************************************************************
**  cvrt_sts_char(s,d)
**      struct dbr_sts_int *s   pointer to source struct
**      struct dbr_sts_int *d   pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_sts_char(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_char *pSrc = (struct dbr_sts_char *) s;
    struct dbr_sts_char *pDest = (struct dbr_sts_char *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    if ( s == d ) 
        return;

    if (num == 1)   /* single value */
        pDest->value = pSrc->value;
    else        /* array chan-- multiple pts */
    {
        memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    }
}

/****************************************************************************
**  cvrt_sts_long(s,d)
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_sts_long(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_sts_long *pSrc = (struct dbr_sts_long *) s;
    struct dbr_sts_long *pDest = (struct dbr_sts_long *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohl(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_long(&pDest->value, &pSrc->value, encode, num);
    }
}


/****************************************************************************
**  cvrt_time_string(s,d)
**        
**  converts fields of struct in HOST format to NET format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_time_string(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_string  *pSrc = (struct dbr_time_string *) s;
    struct dbr_time_string  *pDest = (struct dbr_time_string *) d;
            
    /* convert ieee to vax format or vax to ieee */
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    if ( s != d ) {
        memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));
    }
}

/****************************************************************************
**  cvrt_time_short(s,d)
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_time_short(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_short   *pSrc = (struct dbr_time_short *) s;
    struct dbr_time_short   *pDest = (struct dbr_time_short *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohs(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_short(&pSrc->value, &pDest->value, encode, num);
    }
}

/****************************************************************************
**  cvrt_time_float(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_time_float(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_float   *pSrc = (struct dbr_time_float *) s;
    struct dbr_time_float   *pDest = (struct dbr_time_float *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    cvrt_float(&pSrc->value, &pDest->value, encode, num);
}

/****************************************************************************
**  cvrt_time_double(s,d)
**
**     if encode 
**      converts struct in HOST format to ieee format
**     else 
**      converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_time_double(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_double  *pSrc = (struct dbr_time_double *) s;
    struct dbr_time_double  *pDest = (struct dbr_time_double *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    cvrt_double(&pSrc->value, &pDest->value, encode, num);
}



/****************************************************************************
**  cvrt_time_enum(s,d)
**
**  converts fields of struct in NET format to fields with HOST format
**       or  
**  converts fields of struct in HOST format to fields with NET format
**   
****************************************************************************/

static void cvrt_time_enum(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_enum    *pSrc = (struct dbr_time_enum *) s;
    struct dbr_time_enum    *pDest = (struct dbr_time_enum *) d;

    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);
    if (num == 1)
        pDest->value    = dbr_ntohs(pSrc->value);
    else {
        cvrt_enum(&pSrc->value,&pDest->value,encode,num);
    }
}

/****************************************************************************
**  cvrt_sts_char(s,d)
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_time_char(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_char    *pSrc = (struct dbr_time_char *) s;
    struct dbr_time_char    *pDest = (struct dbr_time_char *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    if ( s == d ) 
        return;

    if (num == 1)   /* single value */
        pDest->value = pSrc->value;
    else        /* array chan-- multiple pts */
    {
        memcpy((void *)&pDest->value, (void *)&pSrc->value, num);
    }
}
/****************************************************************************
**  cvrt_time_long(s,d)
**
**  converts fields ofstruct in HOST format to ieee format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format
****************************************************************************/

static void cvrt_time_long(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 encode,     /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_time_long    *pSrc = (struct dbr_time_long *) s;
    struct dbr_time_long    *pDest = (struct dbr_time_long *) d;

    /* convert vax to ieee or ieee to vax format -- same code*/
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->stamp.secPastEpoch = dbr_ntohl(pSrc->stamp.secPastEpoch);
    pDest->stamp.nsec   = dbr_ntohl(pSrc->stamp.nsec);

    if (num == 1)   /* single value */
        pDest->value = dbr_ntohl(pSrc->value);
    else        /* array chan-- multiple pts */
    {
        cvrt_long(&pDest->value, &pSrc->value, encode, num);
    }
}

/*
 *  cvrt_put_ackt()
 *
 *
 *
 *
 */
static void cvrt_put_ackt(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    dbr_put_ackt_t      *pSrc = (dbr_put_ackt_t *) s;
    dbr_put_ackt_t      *pDest = (dbr_put_ackt_t *) d;
    arrayElementCount   i;

    for(i=0; i<num; i++){
        *pDest = dbr_ntohs( *pSrc );
        /*
         * dont increment these inside the MACRO 
         */
        pDest++;
        pSrc++;
    }
}

/****************************************************************************
**  cvrt_stsack_string(s,d)
**      struct dbr_stsack_string *s     pointer to source struct
**      struct dbr_stsack_string *d pointer to destination struct
**      int  encode;            boolean, if true vax to ieee
**                           else ieee to vax
**        
**  converts fields of struct in HOST format to NET format
**     or 
**  converts fields of struct in NET format to fields with HOST 
**      format;
****************************************************************************/

static void cvrt_stsack_string(
const void          *s,         /* source           */
void                *d,         /* destination          */
int                 /*encode*/, /* cvrt HOST to NET if T    */
arrayElementCount   num         /* number of values     */
)
{
    struct dbr_stsack_string    *pSrc = (struct dbr_stsack_string *) s;
    struct dbr_stsack_string    *pDest = (struct dbr_stsack_string *) d;
            
    /* convert ieee to vax format or vax to ieee */
    pDest->status       = dbr_ntohs(pSrc->status);
    pDest->severity     = dbr_ntohs(pSrc->severity);
    pDest->ackt         = dbr_ntohs(pSrc->ackt);
    pDest->acks         = dbr_ntohs(pSrc->acks);

    /* convert "in place" -> nothing else to do */
    if (s == d)
        return;

    memcpy(pDest->value, pSrc->value, (MAX_STRING_SIZE * num));
}

/*  cvrt is (array of) (pointer to) (function returning) int */
static CACVRTFUNCPTR cac_dbr_cvrt[] = {
    cvrt_string,
    cvrt_short,
    cvrt_float,
    cvrt_enum,
    cvrt_char,
    cvrt_long,
    cvrt_double,

    cvrt_sts_string,
    cvrt_sts_short,
    cvrt_sts_float,
    cvrt_sts_enum,
    cvrt_sts_char,
    cvrt_sts_long,
    cvrt_sts_double,

    cvrt_time_string,
    cvrt_time_short,
    cvrt_time_float,
    cvrt_time_enum,
    cvrt_time_char,
    cvrt_time_long,
    cvrt_time_double,

    cvrt_sts_string, /* DBR_GR_STRING identical to dbr_sts_string */
    cvrt_gr_short,
    cvrt_gr_float,
    cvrt_gr_enum,
    cvrt_gr_char,
    cvrt_gr_long,
    cvrt_gr_double,

    cvrt_sts_string, /* DBR_CTRL_STRING identical to dbr_sts_string */
    cvrt_ctrl_short,
    cvrt_ctrl_float,
    cvrt_ctrl_enum,
    cvrt_ctrl_char,
    cvrt_ctrl_long,
    cvrt_ctrl_double,

    cvrt_put_ackt,  
    cvrt_put_ackt, /* DBR_PUT_ACKS identical to DBR_PUT_ACKT */
    cvrt_stsack_string,
    cvrt_string
};

#endif /* EPICS_CONVERSION_REQUIRED */

int caNetConvert ( unsigned type, const void *pSrc, void *pDest, 
                  int hton, arrayElementCount count )
{
#   ifdef EPICS_CONVERSION_REQUIRED
        if ( type >= NELEMENTS ( cac_dbr_cvrt ) ) {
            return ECA_BADTYPE;
        }        
        ( * cac_dbr_cvrt [ type ] ) ( pSrc, pDest, hton, count );
#   else
        if ( INVALID_DB_REQ ( type ) ) {
            return ECA_BADTYPE;
        }        
        if ( pSrc != pDest ) {
            memcpy ( pDest, pSrc, dbr_size_n ( type, count ) );
        }
#   endif
    return ECA_NORMAL;
}

