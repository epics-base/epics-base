/*
 *      $Id$
 */

#define epicsExportSharedSymbols
#include "tsDefs.h"

epicsShareFunc char * epicsShareAPI tsStampToText(const TS_STAMP *pTS,enum tsTextType textType,char *textBuffer)
{
    switch (textType) {
        case TS_TEXT_MMDDYY:
            epicsTimeToStrftime(textBuffer,28,"%m/%d/%y %H:%M:%S.%09f",pTS);
            break;
        case TS_TEXT_MONDDYYYY:
            epicsTimeToStrftime(textBuffer,32,"%b %d, %Y %H:%M:%S.%09f",pTS);
            break;
        default:
            return NULL;
    }
    return textBuffer;
}

epicsShareFunc long epicsShareAPI tsLocalTime(TS_STAMP *pStamp)
{
    return epicsTimeGetCurrent(pStamp);
}

