/* dbFastLinkConv.c */
/* base/src/db  $Id$ */
/*
 *      Author:            Matthew Needes
 *      Date:              12-9-93
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 */
#include        <vxWorks.h>
#include        <lstLib.h>
#include        <stdlib.h>
#include        <stdarg.h>
#include        <stdio.h>
#include        <string.h>
#include        <taskLib.h>
#include        <vxLib.h>
#include        <tickLib.h>
 
#include        <fast_lock.h>
#include        <cvtFast.h>
#include        <alarm.h>
#include        <choice.h>
#include        <dbDefs.h>
#include        <dbBase.h>
#include        <dbAccess.h>
#include        <dbStaticLib.h>
#include        <dbScan.h>
#include        <dbCommon.h>
#include        <dbFldTypes.h>
#include        <dbRecDes.h>
#include        <dbRecType.h>
#include        <db_field_log.h>
#include        <errMdef.h>
#include        <recSup.h>
#include        <special.h>
 
extern struct dbBase *pdbBase;

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

/* Uninitialized Conversion */
long cvt_uninit(
     void *from,
     void *to,
     struct dbAddr *paddr)
{
  printf("Error in record support, uninitialized link.\n");
  return(-1);
}

/*
 *  Dummy Conversion
 *    In the case of an unsupported conversion, run this
 *    dummy function instead to avoid a bus error.
 */
long cvt_dummy(
     void *from,
     void *to,
     struct dbAddr *paddr)
{ return(-1); }

/* Convert String to String */
static long cvt_st_st(
     char *from,
     char *to,
     struct dbAddr *paddr)
 {
   char size = paddr->field_size;

   if (size >= MAX_STRING_SIZE)
      size = MAX_STRING_SIZE - 1;

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

   return(-1);       /* Change to SYMBOL */
 }

/* Convert String to Unsigned Char */
static long cvt_st_uc(
     char *from,
     unsigned char *to,
     struct dbAddr *paddr)
 {
   short value;

   if (sscanf(from, "%hu", &value) == 1) {
      *to = (unsigned char) value;
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
  
   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Unsigned Short */
static long cvt_st_us(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   short value;

   if (sscanf(from, "%hu", &value) == 1) {
     *to = (unsigned short) value;
     return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Long */
static long cvt_st_l(
     char *from,
     long *to,
     struct dbAddr *paddr)
 {
   long value;

   if (sscanf(from, "%ld", &value) == 1) {
      *to = value;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Unsigned Long */
static long cvt_st_ul(
     char *from,
     unsigned long *to,
     struct dbAddr *paddr)
 {
   unsigned long value;

   if (sscanf(from, "%lu", &value) == 1) {
      *to = value;
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

   if (sscanf(from, "%f", &value) == 1) {
      *to = value;
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

   if (sscanf(from, "%lf", &value) == 1) {
      *to = value;
      return(0);
   }

   return(-1);      /* Change to SYMBOL */
 }

/* Convert String to Enumerated */
static long cvt_st_e(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   struct rset *prset;
   long status;

   prset = GET_PRSET(pdbBase->precSup, paddr->record_type);

   if (prset && prset->put_enum_str)
      return (*prset->put_enum_str)(paddr, from);

   status = S_db_noRSET;
   recGblRecSupError(status, paddr, "dbPutField", "put_enum_str"); 
   return(S_db_badDbrtype);
 }

/* Convert String to Global Choice */
static long cvt_st_gbl(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   char *pchoice;
   struct choiceSet *pchoiceSet;
   unsigned short i;

   pchoiceSet = GET_PCHOICE_SET(pdbBase->pchoiceGbl, paddr->choice_set);

   if (pchoiceSet) {
      for (i=0; i < pchoiceSet->number; i++) {
          pchoice = pchoiceSet->papChoice[i];
          if (!pchoice)
              continue;
          if (strcmp(pchoice, from) == 0) {
              *to = i;
              return(0);
          }
      }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringGchoice)");
   return(S_db_badChoice);
 }

/* Convert String to Cvt Choice */
static long cvt_st_cvt(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   char *pchoice;
   struct choiceSet *pchoiceSet;
   unsigned short i;

   pchoiceSet = pdbBase->pchoiceCvt;

   if (pchoiceSet) {
      for (i=0; i < pchoiceSet->number; i++) {
          pchoice = pchoiceSet->papChoice[i];
          if (!pchoice)
              continue;
          if (strcmp(pchoice, from) == 0) {
              *to = i;
              return(0);
          }
      }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringCchoice)");
   return(S_db_badChoice);
 }

/* Convert String to Rec Choice */
static long cvt_st_rec(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   char *pchoice;
   struct choiceSet *pchoiceSet;
   struct arrChoiceSet *parrChoiceSet;
   unsigned short i;

   parrChoiceSet = GET_PARR_CHOICE_SET(pdbBase->pchoiceRec, paddr->record_type);

   if (parrChoiceSet) {
      pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, paddr->choice_set);

      if (pchoiceSet) {
         for (i=0; i < pchoiceSet->number; i++) {
             pchoice = pchoiceSet->papChoice[i];
             if (!pchoice)
                 continue;
             if (strcmp(pchoice, from) == 0) {
                 *to = i;
                 return(0);
             }
         }
      }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringRchoice)");
   return(S_db_badChoice);
 }

/* Convert String to Dev Choice */
static long cvt_st_dev(
     char *from,
     unsigned short *to,
     struct dbAddr *paddr)
 {
   char *pchoice;
   struct devChoiceSet *pdevChoiceSet;
   unsigned short i;

   pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev, paddr->record_type);

   if (pdevChoiceSet) {
      for (i=0; i < pdevChoiceSet->number; i++) {
             pchoice = pdevChoiceSet->papDevChoice[i]->pchoice;
             if (!pchoice)
                 continue;
             if (strcmp(pchoice, from) == 0) {
                 *to = i;
                 return(0);
             }
      }
   }
   recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringDchoice)");
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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Char to Unsigned Long */
static long cvt_c_ul(
     char *from,
     unsigned long *to,
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
     unsigned short *to,
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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Char to Unsigned Long */
static long cvt_uc_ul(
     unsigned char *from,
     unsigned long *to,
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
     unsigned short *to,
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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Short to Unsigned Long */
static long cvt_s_ul(
     short *from,
     unsigned long *to,
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
     unsigned short *to,
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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Short to Unsigned Long */
static long cvt_us_ul(
     unsigned short *from,
     unsigned long *to,
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
     long *from,
     char *to,
     struct dbAddr *paddr)
{ cvtLongToString(*from, to); return(0); }

/* Convert Long to Char */
static long cvt_l_c(
     long *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Char */
static long cvt_l_uc(
     long *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Short */
static long cvt_l_s(
     long *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Short */
static long cvt_l_us(
     long *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Long */
static long cvt_l_l(
     long *from,
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Unsigned Long */
static long cvt_l_ul(
     long *from,
     unsigned long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Float */
static long cvt_l_f(
     long *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Double */
static long cvt_l_d(
     long *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Long to Enumerated */
static long cvt_l_e(
     long *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to String */
static long cvt_ul_st(
     unsigned long *from,
     char *to,
     struct dbAddr *paddr)
{ cvtUlongToString(*from, to); return(0); }

/* Convert Unsigned Long to Char */
static long cvt_ul_c(
     unsigned long *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Char */
static long cvt_ul_uc(
     unsigned long *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Short */
static long cvt_ul_s(
     unsigned long *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Short */
static long cvt_ul_us(
     unsigned long *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Long */
static long cvt_ul_l(
     unsigned long *from,
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Unsigned Long */
static long cvt_ul_ul(
     unsigned long *from,
     unsigned long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Float */
static long cvt_ul_f(
     unsigned long *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Double */
static long cvt_ul_d(
     unsigned long *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Unsigned Long to Enumerated */
static long cvt_ul_e(
     unsigned long *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to String */
static long cvt_f_st(
     float *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset;
   long status = 0;
   long precision = 2;

   prset = GET_PRSET(pdbBase->precSup, paddr->record_type);
 
   if (prset && prset->get_precision)
     (*prset->get_precision)(paddr, &precision);
   else
     status = S_db_precision;

   if (status) {
      recGblRecSupError(status, paddr, "dbGetField", "get_precision");
      return(status);
   }

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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Float to Unsigned Long */
static long cvt_f_ul(
     float *from,
     unsigned long *to,
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
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to String */
static long cvt_d_st(
     double *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset;
   long status = 0;
   long precision = 2;

   prset = GET_PRSET(pdbBase->precSup, paddr->record_type);
 
   if (prset && prset->get_precision)
     (*prset->get_precision)(paddr, &precision);
   else
     status = S_db_precision;

   if (status) {
      recGblRecSupError(status, paddr, "dbGetField", "get_precision");
      return(status);
   }

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
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Unsigned Long */
static long cvt_d_ul(
     double *from,
     unsigned long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Float */
static long cvt_d_f(
     double *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Double */
static long cvt_d_d(
     double *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Double to Enumerated */
static long cvt_d_e(
     double *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Char */
static long cvt_e_c(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Char */
static long cvt_e_uc(
     unsigned short *from,
     unsigned char *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Short */
static long cvt_e_s(
     unsigned short *from,
     short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Short */
static long cvt_e_us(
     unsigned short *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Long */
static long cvt_e_l(
     unsigned short *from,
     long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Unsigned Long */
static long cvt_e_ul(
     unsigned short *from,
     unsigned long *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Float */
static long cvt_e_f(
     unsigned short *from,
     float *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Double */
static long cvt_e_d(
     unsigned short *from,
     double *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Enumerated to Enumerated */
static long cvt_e_e(
     unsigned short *from,
     unsigned short *to,
     struct dbAddr *paddr)
 { *to=*from; return(0); }

/* Convert Choices And Enumerated Types To String ... */

/* Get Enumerated to String */
static long cvt_e_st_get(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct rset *prset;
   long status;

   prset = GET_PRSET(pdbBase->precSup, paddr->record_type);
 
   if (prset && prset->get_enum_str)
       return (*prset->get_enum_str)(paddr, to);

   status = S_db_noRSET;
   recGblRecSupError(status, paddr, "dbGetField", "get_enum_str");

   return(S_db_badDbrtype);
 }

/* Put Enumerated to String */
static long cvt_e_st_put(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { cvtUshortToString(*from, to); return(0); }

/* Get Gbl Choice to String */
static long cvt_gbl_st(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { 
   struct choiceSet *pchoiceSet;
   char *pchoice;

   pchoiceSet = GET_PCHOICE_SET(pdbBase->pchoiceGbl, paddr->choice_set);

   if (pchoiceSet) {
      pchoice = GET_CHOICE(pchoiceSet, *from);

      if (pchoice) {
           strncpy(to, pchoice, MAX_STRING_SIZE);
           return(0);
      }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbGetField(getGchoiceString)");
   return(S_db_badChoice);
 }

/* Get Cvt Choice to String */
static long cvt_cvt_st(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { 
   char *pchoice;

   pchoice = GET_CHOICE(pdbBase->pchoiceCvt, *from);

   if (pchoice) {
       strncpy(to, pchoice, MAX_STRING_SIZE);
       return(0);
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbGetField(getCchoiceString)");
   return(S_db_badChoice);
 }

/* Get Record Choice to String */
static long cvt_rec_st(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 {
   struct choiceSet *pchoiceSet;
   struct arrChoiceSet *parrChoiceSet;
   char *pchoice;

   parrChoiceSet = GET_PARR_CHOICE_SET(pdbBase->pchoiceRec, paddr->record_type);

   if (parrChoiceSet) {
       pchoiceSet = GET_PCHOICE_SET(parrChoiceSet, paddr->choice_set);

       if (pchoiceSet) {
             pchoice = GET_CHOICE(pchoiceSet, *from);

             if (pchoice) {
                 strncpy(to, pchoice, MAX_STRING_SIZE);
                 return(0);
             }
       }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbGetField(getRchoiceString)");
   return(S_db_badChoice);
}

/* Get Dev Choice to String */
static long cvt_dev_st(
     unsigned short *from,
     char *to,
     struct dbAddr *paddr)
 { 
   struct devChoiceSet *pdevChoiceSet;
   struct devChoice *pdevChoice;

   pdevChoiceSet = GET_PDEV_CHOICE_SET(pdbBase->pchoiceDev, paddr->record_type);

   if (pdevChoiceSet) {
       pdevChoice = GET_DEV_CHOICE(pdevChoiceSet, *from);

       if (pdevChoice) {
           strncpy(to, pdevChoice->pchoice, MAX_STRING_SIZE);
           return(0);
       }
   }

   recGblDbaddrError(S_db_badChoice, paddr, "dbGetField(getRchoiceString)");
   return(S_db_badChoice);
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

long (*get_cvt_table[DBF_DEVCHOICE+1][DBR_ENUM+1])() = {

 /* Convert DBF_STRING to ... */
{ cvt_st_st, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL },

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

 /* Convert DBF_GBLCHOICE to ... */
{ cvt_gbl_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_CVTCHOICE to ... */
{ cvt_cvt_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_RECCHOICE to ... */
{ cvt_rec_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_DEVCHOICE to ... */
{ cvt_dev_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e } };

/*
 *  Put conversion routine lookup table
 *
 *  Converts type X to ...
 *
 *  DBF_STRING     DBF_CHAR       DBF_UCHAR      DBF_SHORT      DBF_USHORT
 *  DBF_LONG       DBF_ULONG      DBF_FLOAT      DBF_DOUBLE     DBF_ENUM
 *  DBF_GBLCHOICE  DBF_CVTCHOICE  DBF_RECCHOICE  DBF_DEVCHOICE
 *
 *  NULL implies the conversion is not supported.
 */

long (*put_cvt_table[DBR_ENUM+1][DBF_DEVCHOICE+1])() = {

 /* Convert DBR_STRING to ... */
{ cvt_st_st, cvt_st_c, cvt_st_uc, cvt_st_s, cvt_st_us, cvt_st_l, cvt_st_ul, cvt_st_f, cvt_st_d, cvt_st_e, cvt_st_gbl, cvt_st_cvt, cvt_st_rec, cvt_st_dev },

 /* Convert DBR_CHAR to ... */
{ cvt_c_st, cvt_c_c, cvt_c_uc, cvt_c_s, cvt_c_us, cvt_c_l, cvt_c_ul, cvt_c_f, cvt_c_d, cvt_c_e, cvt_c_e, cvt_c_e, cvt_c_e, cvt_c_e },

 /* Convert DBR_UCHAR to ... */
{ cvt_uc_st, cvt_uc_c, cvt_uc_uc, cvt_uc_s, cvt_uc_us, cvt_uc_l, cvt_uc_ul, cvt_uc_f, cvt_uc_d, cvt_uc_e, cvt_uc_e, cvt_uc_e, cvt_uc_e, cvt_uc_e },

 /* Convert DBR_SHORT to ... */
{ cvt_s_st, cvt_s_c, cvt_s_uc, cvt_s_s, cvt_s_us, cvt_s_l, cvt_s_ul, cvt_s_f, cvt_s_d, cvt_s_e, cvt_s_e, cvt_s_e, cvt_s_e, cvt_s_e },

 /* Convert DBR_USHORT to ... */
{ cvt_us_st, cvt_us_c, cvt_us_uc, cvt_us_s, cvt_us_us, cvt_us_l, cvt_us_ul, cvt_us_f, cvt_us_d, cvt_us_e, cvt_us_e, cvt_us_e, cvt_us_e, cvt_us_e },

 /* Convert DBR_LONG to ... */
{ cvt_l_st, cvt_l_c, cvt_l_uc, cvt_l_s, cvt_l_us, cvt_l_l, cvt_l_ul, cvt_l_f, cvt_l_d, cvt_l_e, cvt_l_e, cvt_l_e, cvt_l_e, cvt_l_e },

 /* Convert DBR_ULONG to ... */
{ cvt_ul_st, cvt_ul_c, cvt_ul_uc, cvt_ul_s, cvt_ul_us, cvt_ul_l, cvt_ul_ul, cvt_ul_f, cvt_ul_d, cvt_ul_e, cvt_ul_e, cvt_ul_e, cvt_ul_e, cvt_ul_e },

 /* Convert DBR_FLOAT to ... */
{ cvt_f_st, cvt_f_c, cvt_f_uc, cvt_f_s, cvt_f_us, cvt_f_l, cvt_f_ul, cvt_f_f, cvt_f_d, cvt_f_e, cvt_f_e, cvt_f_e, cvt_f_e, cvt_f_e },

 /* Convert DBR_DOUBLE to ... */
{ cvt_d_st, cvt_d_c, cvt_d_uc, cvt_d_s, cvt_d_us, cvt_d_l, cvt_d_ul, cvt_d_f, cvt_d_d, cvt_d_e, cvt_d_e, cvt_d_e, cvt_d_e, cvt_d_e },

 /* Convert DBR_ENUM to ... */
{ cvt_e_st_put, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_f, cvt_e_d, cvt_e_e, cvt_e_e, cvt_e_e, cvt_e_e, cvt_e_e } };

