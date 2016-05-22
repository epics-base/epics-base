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
 *           c  - epicsInt8
 *           uc - epicsUInt8
 *           s  - epicsInt16
 *           us - epicsUInt16
 *           l  - epicsInt32
 *           ul - epicsUInt32
 *           f  - epicsFloat32
 *           d  - epicsFloat64
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
     const dbAddr *paddr)
{
   size_t size;

   if (paddr && paddr->field_size < MAX_STRING_SIZE) {
      size =  paddr->field_size - 1;
   } else {
      size = MAX_STRING_SIZE - 1;
   }
   strncpy(to, from, size);
   to[size] = 0;
   return 0;
}

/* Convert String to Char */
static long cvt_st_c(
     char *from,
     epicsInt8 *to,
     const dbAddr *paddr)
{
   char *end;
   long value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   value = strtol(from, &end, 10);
   if (end > from) {
      *to = (epicsInt8) value;
      return 0;
   }
   return -1;
}

/* Convert String to Unsigned Char */
static long cvt_st_uc(
     char *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
{
   char *end;
   unsigned long value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   value = strtoul(from, &end, 10);
   if (end > from) {
      *to = (epicsUInt8) value;
      return 0;
   }
   return -1;
}

/* Convert String to Short */
static long cvt_st_s(
     char *from,
     epicsInt16 *to,
     const dbAddr *paddr)
{
   char *end;
   long value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   value = strtol(from, &end, 10);
   if (end > from) {
      *to = (epicsInt16) value;
      return 0;
   }
   return -1;
}

/* Convert String to Unsigned Short */
static long cvt_st_us(
     char *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
{
   char *end;
   unsigned long value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   value = strtoul(from, &end, 10);
   if (end > from) {
      *to = (epicsUInt16) value;
      return 0;
   }
   return -1;
}

/* Convert String to Long */
static long cvt_st_l(
     char *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 {
   char *end;
   long value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   value = strtol(from, &end, 10);
   if (end > from) {
      *to = (epicsInt32) value;
      return 0;
   }
   return -1;
 }

/* Convert String to Unsigned Long */
static long cvt_st_ul(
     char *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 {
   double value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   /*Convert via double so that numbers like 1.0e3 convert properly*/
   /*Problem was old database access said to get unsigned long as double*/
   if (epicsScanDouble(from, &value) == 1) {
      *to = (epicsUInt32)value;
      return 0;
   }
   return -1;
 }

/* Convert String to Float */
static long cvt_st_f(
     char *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 {
   float value;

   if (*from == 0) {
      *to = 0;
      return 0;
   }
   if (epicsScanFloat(from, &value) == 1) {
      *to = value;
      return 0;
   }
   return -1;
 }

/* Convert String to Double */
static long cvt_st_d(
     char *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 {
   double value;

   if (*from == 0) {
      *to = 0.0;
      return 0;
   }
   if (epicsScanDouble(from, &value) == 1) {
      *to = value;
      return 0;
   }
   return -1;
 }

/* Convert String to Enumerated */
static long cvt_st_e(
     char *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
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
		nargs = sscanf(from,"%u%n",&ind,&nchars);
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
     const dbAddr *paddr)
{
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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
	nargs = sscanf(from,"%u%n",&ind,&nchars);
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
     const dbAddr *paddr)
{
    dbFldDes		*pdbFldDes = paddr->pfldDes;
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
	nargs = sscanf(from,"%u%n",&ind,&nchars);
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
     epicsInt8 *from,
     char *to,
     const dbAddr *paddr)
{ cvtCharToString(*from, to); return(0); }

/* Convert Char to Char */
static long cvt_c_c(
     epicsInt8 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Char */
static long cvt_c_uc(
     epicsInt8 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Short */
static long cvt_c_s(
     epicsInt8 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Short */
static long cvt_c_us(
     epicsInt8 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Long */
static long cvt_c_l(
     epicsInt8 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Long */
static long cvt_c_ul(
     epicsInt8 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Float */
static long cvt_c_f(
     epicsInt8 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Double */
static long cvt_c_d(
     epicsInt8 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Enumerated */
static long cvt_c_e(
     epicsInt8 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to String */
static long cvt_uc_st(
     epicsUInt8 *from,
     char *to,
     const dbAddr *paddr)
{ cvtUcharToString(*from, to); return(0); }

/* Convert Unsigned Char to Char */
static long cvt_uc_c(
     epicsUInt8 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Char */
static long cvt_uc_uc(
     epicsUInt8 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Short */
static long cvt_uc_s(
     epicsUInt8 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Short */
static long cvt_uc_us(
     epicsUInt8 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Long */
static long cvt_uc_l(
     epicsUInt8 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Long */
static long cvt_uc_ul(
     epicsUInt8 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Float */
static long cvt_uc_f(
     epicsUInt8 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Double */
static long cvt_uc_d(
     epicsUInt8 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Enumerated */
static long cvt_uc_e(
     epicsUInt8 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to String */
static long cvt_s_st(
     epicsInt16 *from,
     char *to,
     const dbAddr *paddr)
{ cvtShortToString(*from, to); return(0); }

/* Convert Short to Char */
static long cvt_s_c(
     epicsInt16 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Char */
static long cvt_s_uc(
     epicsInt16 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Short */
static long cvt_s_s(
     epicsInt16 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Short */
static long cvt_s_us(
     epicsInt16 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Long */
static long cvt_s_l(
     epicsInt16 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Long */
static long cvt_s_ul(
     epicsInt16 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Float */
static long cvt_s_f(
     epicsInt16 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Double */
static long cvt_s_d(
     epicsInt16 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Enumerated */
static long cvt_s_e(
     epicsInt16 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to String */
static long cvt_us_st(
     epicsUInt16 *from,
     char *to,
     const dbAddr *paddr)
{ cvtUshortToString(*from, to); return(0); }

/* Convert Unsigned Short to Char */
static long cvt_us_c(
     epicsUInt16 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Char */
static long cvt_us_uc(
     epicsUInt16 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Short */
static long cvt_us_s(
     epicsUInt16 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Short */
static long cvt_us_us(
     epicsUInt16 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Long */
static long cvt_us_l(
     epicsUInt16 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Long */
static long cvt_us_ul(
     epicsUInt16 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Float */
static long cvt_us_f(
     epicsUInt16 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Double */
static long cvt_us_d(
     epicsUInt16 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Enumerated */
static long cvt_us_e(
     epicsUInt16 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to String */
static long cvt_l_st(
     epicsInt32 *from,
     char *to,
     const dbAddr *paddr)
{ cvtLongToString(*from, to); return(0); }

/* Convert Long to Char */
static long cvt_l_c(
     epicsInt32 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Char */
static long cvt_l_uc(
     epicsInt32 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Short */
static long cvt_l_s(
     epicsInt32 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Short */
static long cvt_l_us(
     epicsInt32 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Long */
static long cvt_l_l(
     epicsInt32 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Long */
static long cvt_l_ul(
     epicsInt32 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Float */
static long cvt_l_f(
     epicsInt32 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Double */
static long cvt_l_d(
     epicsInt32 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Enumerated */
static long cvt_l_e(
     epicsInt32 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to String */
static long cvt_ul_st(
     epicsUInt32 *from,
     char *to,
     const dbAddr *paddr)
{ cvtUlongToString(*from, to); return(0); }

/* Convert Unsigned Long to Char */
static long cvt_ul_c(
     epicsUInt32 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Char */
static long cvt_ul_uc(
     epicsUInt32 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Short */
static long cvt_ul_s(
     epicsUInt32 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Short */
static long cvt_ul_us(
     epicsUInt32 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Long */
static long cvt_ul_l(
     epicsUInt32 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Long */
static long cvt_ul_ul(
     epicsUInt32 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Float */
static long cvt_ul_f(
     epicsUInt32 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Double */
static long cvt_ul_d(
     epicsUInt32 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Enumerated */
static long cvt_ul_e(
     epicsUInt32 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to String */
static long cvt_f_st(
     epicsFloat32 *from,
     char *to,
     const dbAddr *paddr)
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
     epicsFloat32 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Char */
static long cvt_f_uc(
     epicsFloat32 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Short */
static long cvt_f_s(
     epicsFloat32 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Short */
static long cvt_f_us(
     epicsFloat32 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Long */
static long cvt_f_l(
     epicsFloat32 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Long */
static long cvt_f_ul(
     epicsFloat32 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Float */
static long cvt_f_f(
     epicsFloat32 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Double */
static long cvt_f_d(
     epicsFloat32 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Enumerated */
static long cvt_f_e(
     epicsFloat32 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to String */
static long cvt_d_st(
     epicsFloat64 *from,
     char *to,
     const dbAddr *paddr)
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
     epicsFloat64 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Char */
static long cvt_d_uc(
     epicsFloat64 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Short */
static long cvt_d_s(
     epicsFloat64 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Short */
static long cvt_d_us(
     epicsFloat64 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Long */
static long cvt_d_l(
     epicsFloat64 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=(epicsInt32)*from; return(0); }

/* Convert Double to Unsigned Long */
static long cvt_d_ul(
     epicsFloat64 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=(epicsUInt32)*from; return(0); }

/* Convert Double to Float */
static long cvt_d_f(
     epicsFloat64 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
{ *to = epicsConvertDoubleToFloat(*from); return 0;}

/* Convert Double to Double */
static long cvt_d_d(
     epicsFloat64 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Enumerated */
static long cvt_d_e(
     epicsFloat64 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Char */
static long cvt_e_c(
     epicsEnum16 *from,
     epicsInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Char */
static long cvt_e_uc(
     epicsEnum16 *from,
     epicsUInt8 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Short */
static long cvt_e_s(
     epicsEnum16 *from,
     epicsInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Short */
static long cvt_e_us(
     epicsEnum16 *from,
     epicsUInt16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Long */
static long cvt_e_l(
     epicsEnum16 *from,
     epicsInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Long */
static long cvt_e_ul(
     epicsEnum16 *from,
     epicsUInt32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Float */
static long cvt_e_f(
     epicsEnum16 *from,
     epicsFloat32 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Double */
static long cvt_e_d(
     epicsEnum16 *from,
     epicsFloat64 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Enumerated */
static long cvt_e_e(
     epicsEnum16 *from,
     epicsEnum16 *to,
     const dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Choices And Enumerated Types To String ... */

/* Get Enumerated to String */
static long cvt_e_st_get(
     epicsEnum16 *from,
     char *to,
     const dbAddr *paddr)
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
     const dbAddr *paddr)
 { cvtUshortToString(*from, to); return(0); }

/* Get Menu to String */
static long cvt_menu_st(
     epicsEnum16 *from,
     char *to,
     const dbAddr *paddr)
 { 
   dbFldDes		*pdbFldDes;
   dbMenu		*pdbMenu;
   char			**papChoiceValue;
   char			*pchoice;

    if(! paddr 
    || !(pdbFldDes = paddr->pfldDes)
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
     const dbAddr *paddr)
 { 
   dbFldDes		*pdbFldDes;
   dbDeviceMenu		*pdbDeviceMenu;
   char			**papChoice;
   char			*pchoice;

    if(!paddr 
    || !(pdbFldDes = paddr->pfldDes)
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

