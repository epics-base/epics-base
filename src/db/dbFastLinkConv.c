/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbFastLinkConv.c */
/* base/src/db  $Id$ */
/*
 *      Author:            Matthew Needes
 *      Date:              12-9-93
 */
#include <stddef.h>
#include <epicsStdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "epicsConvert.h"
#include "dbDefs.h"
#include "errlog.h"
#include "cvtFast.h"
#include "alarm.h"
#include "dbBase.h"
#include "link.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "dbCommon.h"
#include "dbFldTypes.h"
#include "errMdef.h"
#include "recSup.h"
#include "special.h"
#define epicsExportSharedSymbols
#include "dbAddr.h"
#include "dbAccessDefs.h"
#include "recGbl.h"
#include "dbConvertFast.h"
 

/*
 *  In the following functions:
 *
 *      cvt_y_z(ARGS)
 *
 *    converts from type y to type z.  If
 *     type y and z are the same, it merely copies.
 *
 *     where st - string
 *           c  - char
 *           uc - unsigned char
 *           s  - short
 *           us - unsigned short
 *           l  - long
 *           ul - unsigned long
 *           f  - float
 *           d  - double
 *           e  - enum
 *
 *  These functions are _single_ value functions,
 *       i.e.: do not deal with array types.
 */

/*
 *  A DB_LINK that is not initialized with recGblInitFastXXXLink()
 *     will have this conversion.
 */

/* Convert String to String */
static long cvt_st_st(
     char *from,
     char *to,
     struct dbAddr *paddr)
 {
   char size;

   if(paddr && paddr->field_size<MAX_STRING_SIZE) {
	size =  paddr->field_size - 1;
   } else {
	size = MAX_STRING_SIZE - 1;
   }
   strncpy(to, from, size);
   *(to+size) = '\000';
   return(0);
 }

/* Convert String to Char */
static long cvt_st_c(
     char *from,
     char *to,
     struct dbAddr *paddr)
 {
   short value;

   if (sscanf(from, "%hd", &value) == 1) {
      *to = (char) value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = '0';
      return(0);
   }
   return(-1);       /* Change to SYMBOL */
 }

/* Convert String to Unsigned Char */
static long cvt_st_uc(
     char *from,
     unsigned char *to,
     struct dbAddr *paddr)
 {
   unsigned short value;

   if (sscanf(from, "%hu", &value) == 1) {
      *to = value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = '0';
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Short */
static long cvt_st_s(
     char *from,
     short *to,
     struct dbAddr *paddr)
 {
   short value;

   if (sscanf(from, "%hd", &value) == 1) {
      *to = value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = 0;
      return(0);
   }
  
   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Unsigned Short */
static long cvt_st_us(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   unsigned short value;

   if (sscanf(from, "%hu", &value) == 1) {
     *to = value;
     return(0);
   }
   if(strlen(from) == 0) {
      *to = 0;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Long */
static long cvt_st_l(
     char *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 {
   epicsInt32 value;

   if (sscanf(from, "%d", &value) == 1) {
      *to = value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = 0;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Unsigned Long */
static long cvt_st_ul(
     char *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 {
   double value;

   /*Convert to double first so that numbers like 1.0e3 convert properly*/
   /*Problem was old database access said to get unsigned long as double*/
   if (epicsScanDouble(from, &value) == 1) {
      *to = (epicsUInt32)value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = 0;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Float */
static long cvt_st_f(
     char *from,
     float *to,
     struct dbAddr *paddr)
 {
   float value;

   if (epicsScanFloat(from, &value) == 1) {
      *to = value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = 0.0;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Double */
static long cvt_st_d(
     char *from,
     double *to,
     struct dbAddr *paddr)
 {
   double value;

   if (epicsScanDouble(from, &value) == 1) {
      *to = value;
      return(0);
   }
   if(strlen(from) == 0) {
      *to = 0.0;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Enumerated */
static long cvt_st_e(
     char *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 {
   struct rset 		*prset = 0;
   long 		status;
   epicsEnum16	*pfield= (epicsEnum16*)(paddr->pfield);
   unsigned int		nchoices,ind;
   int			nargs,nchars;
   struct dbr_enumStrs	enumStrs;

    if(paddr && (prset=dbGetRset(paddr))
    && (prset->put_enum_str)) {
	status = (*prset->put_enum_str)(paddr,from);
	if(!status) return(0);
	if(prset->get_enum_strs) {
	    status = (*prset->get_enum_strs)(paddr,&enumStrs);
	    if(!status) {
		nchoices = enumStrs.no_str;
		nargs = sscanf(from," %u %n",&ind,&nchars);
		if(nargs==1 && nchars==strlen(from) && ind<nchoices) {
		    *pfield = ind;
		    return(0);
		}
		status = S_db_badChoice;
	    }
	}else {
	    status=S_db_noRSET;
	}
    } else {
	status=S_db_noRSET;
    }
    if(status == S_db_noRSET) {
	recGblRecSupError(status,paddr,"dbPutField","put_enum_str");
    } else {
	recGblRecordError(status,(void *)paddr->precord,from);
    }
    return(status);
 }

/* Convert String to Menu */
static long cvt_st_menu(
     char *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
{
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbMenu		*pdbMenu = (dbMenu *)pdbFldDes->ftPvt;
    char		**papChoiceValue;
    char		*pchoice;
    unsigned int	nChoice,ind;
    int			nargs,nchars;

    if( pdbMenu  && (papChoiceValue = pdbMenu->papChoiceValue)) {
	nChoice = pdbMenu->nChoice;
	for(ind=0; ind<nChoice; ind++) {
	    if(!(pchoice=papChoiceValue[ind])) continue;
	    if(strcmp(pchoice,from)==0) {
		*to = ind;
		return(0);
	    }
	}
	nargs = sscanf(from," %u %n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(from) && ind<nChoice) {
	    *to = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbFastLinkConv(cvt_st_menu)");
    return(S_db_badChoice);
}

/* Convert String to Device */
static long cvt_st_device(
     char *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
{
    dbFldDes		*pdbFldDes = (dbFldDes *)paddr->pfldDes;
    dbDeviceMenu	*pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt;
    char		**papChoice;
    char		*pchoice;
    unsigned int	nChoice,ind;
    int			nargs,nchars;

    if( pdbDeviceMenu  && (papChoice = pdbDeviceMenu->papChoice)) {
	nChoice = pdbDeviceMenu->nChoice;
	for(ind=0; ind<nChoice; ind++) {
	    if(!(pchoice=papChoice[ind])) continue;
	    if(strcmp(pchoice,from)==0) {
		*to = ind;
		return(0);
	    }
	}
	nargs = sscanf(from," %u %n",&ind,&nchars);
	if(nargs==1 && nchars==strlen(from) && ind<nChoice) {
	    *to = ind;
	    return(0);
	}
    }
    recGblDbaddrError(S_db_badChoice,paddr,"dbFastLinkConv(cvt_st_device)");
    return(S_db_badChoice);
}

/* Convert Char to String */
static long cvt_c_st(
     char *from,
     char *to,
     struct dbAddr *paddr)
{ cvtCharToString(*from, to); return(0); }

/* Convert Char to Char */
static long cvt_c_c(
     char *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Char */
static long cvt_c_uc(
     char *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Short */
static long cvt_c_s(
     char *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Short */
static long cvt_c_us(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Long */
static long cvt_c_l(
     char *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Long */
static long cvt_c_ul(
     char *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Float */
static long cvt_c_f(
     char *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Double */
static long cvt_c_d(
     char *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Enumerated */
static long cvt_c_e(
     char *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to String */
static long cvt_uc_st(
     unsigned char *from,
     char *to,
     struct dbAddr *paddr)
{ cvtUcharToString(*from, to); return(0); }

/* Convert Unsigned Char to Char */
static long cvt_uc_c(
     unsigned char *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Char */
static long cvt_uc_uc(
     unsigned char *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Short */
static long cvt_uc_s(
     unsigned char *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Short */
static long cvt_uc_us(
     unsigned char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Long */
static long cvt_uc_l(
     unsigned char *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Long */
static long cvt_uc_ul(
     unsigned char *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Float */
static long cvt_uc_f(
     unsigned char *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Double */
static long cvt_uc_d(
     unsigned char *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Enumerated */
static long cvt_uc_e(
     unsigned char *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to String */
static long cvt_s_st(
     short *from,
     char *to,
     struct dbAddr *paddr)
{ cvtShortToString(*from, to); return(0); }

/* Convert Short to Char */
static long cvt_s_c(
     short *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Char */
static long cvt_s_uc(
     short *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Short */
static long cvt_s_s(
     short *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Short */
static long cvt_s_us(
     short *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Long */
static long cvt_s_l(
     short *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Long */
static long cvt_s_ul(
     short *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Float */
static long cvt_s_f(
     short *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Double */
static long cvt_s_d(
     short *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Enumerated */
static long cvt_s_e(
     short *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to String */
static long cvt_us_st(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
{ cvtUshortToString(*from, to); return(0); }

/* Convert Unsigned Short to Char */
static long cvt_us_c(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Char */
static long cvt_us_uc(
     unsigned short *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Short */
static long cvt_us_s(
     unsigned short *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Short */
static long cvt_us_us(
     unsigned short *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Long */
static long cvt_us_l(
     unsigned short *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Long */
static long cvt_us_ul(
     unsigned short *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Float */
static long cvt_us_f(
     unsigned short *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Double */
static long cvt_us_d(
     unsigned short *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Enumerated */
static long cvt_us_e(
     unsigned short *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to String */
static long cvt_l_st(
     epicsInt32 *from,
     char *to,
     struct dbAddr *paddr)
{ cvtLongToString(*from, to); return(0); }

/* Convert Long to Char */
static long cvt_l_c(
     epicsInt32 *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Char */
static long cvt_l_uc(
     epicsInt32 *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Short */
static long cvt_l_s(
     epicsInt32 *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Short */
static long cvt_l_us(
     epicsInt32 *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Long */
static long cvt_l_l(
     epicsInt32 *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Long */
static long cvt_l_ul(
     epicsInt32 *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Float */
static long cvt_l_f(
     epicsInt32 *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Double */
static long cvt_l_d(
     epicsInt32 *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Enumerated */
static long cvt_l_e(
     epicsInt32 *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to String */
static long cvt_ul_st(
     epicsUInt32 *from,
     char *to,
     struct dbAddr *paddr)
{ cvtUlongToString(*from, to); return(0); }

/* Convert Unsigned Long to Char */
static long cvt_ul_c(
     epicsUInt32 *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Char */
static long cvt_ul_uc(
     epicsUInt32 *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Short */
static long cvt_ul_s(
     epicsUInt32 *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Short */
static long cvt_ul_us(
     epicsUInt32 *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Long */
static long cvt_ul_l(
     epicsUInt32 *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Long */
static long cvt_ul_ul(
     epicsUInt32 *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Float */
static long cvt_ul_f(
     epicsUInt32 *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Double */
static long cvt_ul_d(
     epicsUInt32 *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Enumerated */
static long cvt_ul_e(
     epicsUInt32 *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to String */
static long cvt_f_st(
     float *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset = 0;
   long status = 0;
   long precision = 6;

   if(paddr) prset = dbGetRset(paddr);
 
   if (prset && prset->get_precision)
     status = (*prset->get_precision)(paddr, &precision);
   cvtFloatToString(*from, to, precision);
   return(status);
 }

/* Convert Float to Char */
static long cvt_f_c(
     float *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Char */
static long cvt_f_uc(
     float *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Short */
static long cvt_f_s(
     float *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Short */
static long cvt_f_us(
     float *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Long */
static long cvt_f_l(
     float *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Long */
static long cvt_f_ul(
     float *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Float */
static long cvt_f_f(
     float *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Double */
static long cvt_f_d(
     float *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Enumerated */
static long cvt_f_e(
     float *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to String */
static long cvt_d_st(
     double *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset = 0;
   long status = 0;
   long precision = 6;

   if(paddr) prset = dbGetRset(paddr);
 
   if (prset && prset->get_precision)
     status = (*prset->get_precision)(paddr, &precision);
   cvtDoubleToString(*from, to, precision);
   return(status);
 }

/* Convert Double to Char */
static long cvt_d_c(
     double *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Char */
static long cvt_d_uc(
     double *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Short */
static long cvt_d_s(
     double *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Short */
static long cvt_d_us(
     double *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Long */
static long cvt_d_l(
     double *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Long */
static long cvt_d_ul(
     double *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Float */
static long cvt_d_f(
     double *from,
     float *to,
     struct dbAddr *paddr)
{ *to = epicsConvertDoubleToFloat(*from); return 0;}

/* Convert Double to Double */
static long cvt_d_d(
     double *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Enumerated */
static long cvt_d_e(
     double *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Char */
static long cvt_e_c(
     epicsEnum16 *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Char */
static long cvt_e_uc(
     epicsEnum16 *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Short */
static long cvt_e_s(
     epicsEnum16 *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Short */
static long cvt_e_us(
     epicsEnum16 *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Long */
static long cvt_e_l(
     epicsEnum16 *from,
     epicsInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Long */
static long cvt_e_ul(
     epicsEnum16 *from,
     epicsUInt32 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Float */
static long cvt_e_f(
     epicsEnum16 *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Double */
static long cvt_e_d(
     epicsEnum16 *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Enumerated */
static long cvt_e_e(
     epicsEnum16 *from,
     epicsEnum16 *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Choices And Enumerated Types To String ... */

/* Get Enumerated to String */
static long cvt_e_st_get(
     epicsEnum16 *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset = 0;
   long status;

   if(paddr) prset = dbGetRset(paddr);
 
   if (prset && prset->get_enum_str)
       return (*prset->get_enum_str)(paddr, to);

   status = S_db_noRSET;
   recGblRecSupError(status, paddr, "dbGetField", "get_enum_str");

   return(S_db_badDbrtype);
 }

/* Put Enumerated to String */
static long cvt_e_st_put(
     epicsEnum16 *from,
     char *to,
     struct dbAddr *paddr)
 { cvtUshortToString(*from, to); return(0); }

/* Get Menu to String */
static long cvt_menu_st(
     epicsEnum16 *from,
     char *to,
     struct dbAddr *paddr)
 { 
   dbFldDes		*pdbFldDes;
   dbMenu		*pdbMenu;
   char			**papChoiceValue;
   char			*pchoice;

    if(! paddr 
    || !(pdbFldDes = (dbFldDes *)paddr->pfldDes)
    || !(pdbMenu = (dbMenu *)pdbFldDes->ftPvt)
    || *from>=pdbMenu->nChoice
    || !(papChoiceValue = pdbMenu->papChoiceValue)
    || !(pchoice=papChoiceValue[*from])) {
	recGblDbaddrError(S_db_badChoice,paddr,"dbFastLinkConv(cvt_menu_st)");
	return(S_db_badChoice);
    }
    strncpy(to,pchoice,MAX_STRING_SIZE);
    return(0);
 }


/* Get Device to String */
static long cvt_device_st(
     epicsEnum16 *from,
     char *to,
     struct dbAddr *paddr)
 { 
   dbFldDes		*pdbFldDes;
   dbDeviceMenu		*pdbDeviceMenu;
   char			**papChoice;
   char			*pchoice;

    if(!paddr 
    || !(pdbFldDes = (dbFldDes *)paddr->pfldDes)
    || !(pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt)
    || *from>=pdbDeviceMenu->nChoice
    || !(papChoice= pdbDeviceMenu->papChoice)
    || !(pchoice=papChoice[*from])) {
	recGblDbaddrError(S_db_badChoice,paddr,"dbFastLinkConv(cvt_device_st)");
	return(S_db_badChoice);
    }
    strncpy(to,pchoice,MAX_STRING_SIZE);
    return(0);
 }

/*
 *  Get conversion routine lookup table
 *
 *  Converts type X to ...
 *
 *  DBR_STRING,    DBR_CHR,       DBR_UCHAR,     DBR_SHORT,     DBR_USHORT,
 *  DBR_LONG,      DBR_ULONG,     DBR_FLOAT,     DBR_DOUBLE,    DBR_ENUM
 *
 *  NULL implies the conversion is not supported.
 */

epicsShareDef long (*dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1])() = {

 /* Convert DBF_STRING to ... */
{ cvt_st_st, cvt_st_c, cvt_st_uc, cvt_st_s, cvt_st_us, cvt_st_l, cvt_st_ul, cvt_st_f, cvt_st_d, cvt_st_e },

 /* Convert DBF_CHAR to ... */
{ cvt_c_st, cvt_c_c, cvt_c_uc, cvt_c_s, cvt_c_us, cvt_c_l, cvt_c_ul, cvt_c_f, cvt_c_d, cvt_c_e },

 /* Convert DBF_UCHAR to ... */
{ cvt_uc_st, cvt_uc_c, cvt_uc_uc, cvt_uc_s, cvt_uc_us, cvt_uc_l, cvt_uc_ul, cvt_uc_f, cvt_uc_d, cvt_uc_e },

 /* Convert DBF_SHORT to ... */
{ cvt_s_st, cvt_s_c, cvt_s_uc, cvt_s_s, cvt_s_us, cvt_s_l, cvt_s_ul, cvt_s_f, cvt_s_d, cvt_s_e },

 /* Convert DBF_USHORT to ... */
{ cvt_us_st, cvt_us_c, cvt_us_uc, cvt_us_s, cvt_us_us, cvt_us_l, cvt_us_ul, cvt_us_f, cvt_us_d, cvt_us_e },

 /* Convert DBF_LONG to ... */
{ cvt_l_st, cvt_l_c, cvt_l_uc, cvt_l_s, cvt_l_us, cvt_l_l, cvt_l_ul, cvt_l_f, cvt_l_d, cvt_l_e },

 /* Convert DBF_ULONG to ... */
{ cvt_ul_st, cvt_ul_c, cvt_ul_uc, cvt_ul_s, cvt_ul_us, cvt_ul_l, cvt_ul_ul, cvt_ul_f, cvt_ul_d, cvt_ul_e },

 /* Convert DBF_FLOAT to ... */
{ cvt_f_st, cvt_f_c, cvt_f_uc, cvt_f_s, cvt_f_us, cvt_f_l, cvt_f_ul, cvt_f_f, cvt_f_d, cvt_f_e },

 /* Convert DBF_DOUBLE to ... */
{ cvt_d_st, cvt_d_c, cvt_d_uc, cvt_d_s, cvt_d_us, cvt_d_l, cvt_d_ul, cvt_d_f, cvt_d_d, cvt_d_e },

 /* Convert DBF_ENUM to ... */
{ cvt_e_st_get, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_MENU to ... */
{ cvt_menu_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_DEVICE to ... */
{ cvt_device_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e } };

/*
 *  Put conversion routine lookup table
 *
 *  Converts type X to ...
 *
 *  DBF_STRING     DBF_CHAR       DBF_UCHAR      DBF_SHORT      DBF_USHORT
 *  DBF_LONG       DBF_ULONG      DBF_FLOAT      DBF_DOUBLE     DBF_ENUM
 *  DBF_MENU	   DBF_DEVICE 
 *
 *  NULL implies the conversion is not supported.
 */

epicsShareDef long (*dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1])() = {

 /* Convert DBR_STRING to ... */
{ cvt_st_st, cvt_st_c, cvt_st_uc, cvt_st_s, cvt_st_us, cvt_st_l, cvt_st_ul, cvt_st_f, cvt_st_d, cvt_st_e, cvt_st_menu, cvt_st_device},

 /* Convert DBR_CHAR to ... */
{ cvt_c_st, cvt_c_c, cvt_c_uc, cvt_c_s, cvt_c_us, cvt_c_l, cvt_c_ul, cvt_c_f, cvt_c_d, cvt_c_e, cvt_c_e, cvt_c_e},

 /* Convert DBR_UCHAR to ... */
{ cvt_uc_st, cvt_uc_c, cvt_uc_uc, cvt_uc_s, cvt_uc_us, cvt_uc_l, cvt_uc_ul, cvt_uc_f, cvt_uc_d, cvt_uc_e, cvt_uc_e, cvt_uc_e},

 /* Convert DBR_SHORT to ... */
{ cvt_s_st, cvt_s_c, cvt_s_uc, cvt_s_s, cvt_s_us, cvt_s_l, cvt_s_ul, cvt_s_f, cvt_s_d, cvt_s_e, cvt_s_e, cvt_s_e},

 /* Convert DBR_USHORT to ... */
{ cvt_us_st, cvt_us_c, cvt_us_uc, cvt_us_s, cvt_us_us, cvt_us_l, cvt_us_ul, cvt_us_f, cvt_us_d, cvt_us_e, cvt_us_e, cvt_us_e},

 /* Convert DBR_LONG to ... */
{ cvt_l_st, cvt_l_c, cvt_l_uc, cvt_l_s, cvt_l_us, cvt_l_l, cvt_l_ul, cvt_l_f, cvt_l_d, cvt_l_e, cvt_l_e, cvt_l_e},

 /* Convert DBR_ULONG to ... */
{ cvt_ul_st, cvt_ul_c, cvt_ul_uc, cvt_ul_s, cvt_ul_us, cvt_ul_l, cvt_ul_ul, cvt_ul_f, cvt_ul_d, cvt_ul_e, cvt_ul_e, cvt_ul_e},

 /* Convert DBR_FLOAT to ... */
{ cvt_f_st, cvt_f_c, cvt_f_uc, cvt_f_s, cvt_f_us, cvt_f_l, cvt_f_ul, cvt_f_f, cvt_f_d, cvt_f_e, cvt_f_e, cvt_f_e},

 /* Convert DBR_DOUBLE to ... */
{ cvt_d_st, cvt_d_c, cvt_d_uc, cvt_d_s, cvt_d_us, cvt_d_l, cvt_d_ul, cvt_d_f, cvt_d_d, cvt_d_e, cvt_d_e, cvt_d_e},

 /* Convert DBR_ENUM to ... */
{ cvt_e_st_put, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e, cvt_e_e, cvt_e_e} };

