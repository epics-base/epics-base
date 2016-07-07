/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

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

