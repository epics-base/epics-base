/*
 *      $Id$
 * This file contains the old R3.13 
 * time stamp definitions and routines
 * needed by epics extensions. They exist 
 * for compatibility purposes only and 
 * should not be used by new code.
 *
 */
#ifndef tsDefsh
#define tsDefsh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define epicsExportSharedSymbols
#include "tsStamp.h"

/*----------------------------------------------------------------------------
* TS_TEXT_xxx text type codes for converting between text and time stamp
*
*    TS_TEXT_MONDDYYYY  Mon dd, yyyy hh:mm:ss.nano-secs
*    TS_TEXT_MMDDYY     mm/dd/yy hh:mm:ss.nano-secs
*                       123456789012345678901234567890123456789
*                       0        1         2         3
*----------------------------------------------------------------------------*/
enum tsTextType{
    TS_TEXT_MONDDYYYY,
    TS_TEXT_MMDDYY
};

epicsShareFunc char * epicsShareAPI tsStampToText(const TS_STAMP *pTS,enum tsTextType textType,char *textBuffer);
epicsShareFunc long epicsShareAPI tsLocalTime(TS_STAMP *pStamp);

#ifdef __cplusplus
}
#endif
#endif /* tsDefsh  */


