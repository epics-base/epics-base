/*************************************************************************\
* Copyright (c) 2011 LANS LLC, as Operator of
*     Los Alamos National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* epicsTime.cpp */
/* Author Jeffrey O. Hill */

#include <stdexcept>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <string> // vxWorks 6.0 requires this include

#include "errSymTbl.h"
#include "epicsAssert.h"
#include "epicsVersion.h"
#include "epicsStdlib.h"
#include "epicsMath.h"
#include "envDefs.h"
#include "epicsTime.h"
#include "osiSock.h" /* pull in struct timeval */
#include "epicsStdio.h"

static const epicsUInt32 nSecPerSec = 1000000000u;
static const unsigned nSecFracDigits = 9u;


void epicsTime::throwError(int code)
{
    if(code==epicsTimeOK)
        return;
    throw std::logic_error(errSymMsg(code));
}


epicsTime::epicsTime ( const epicsTimeStamp & replace )
    :ts(replace)
{
    if(ts.nsec >= nSecPerSec)
        throw std::logic_error("epicsTimeStamp has overflow in nano-seconds field");
}

epicsTime epicsTime::getCurrent ()
{
    epicsTimeStamp current;
    int status = epicsTimeGetCurrent (&current);
    if (status) {
        throw unableToFetchCurrentTime ("Unable to fetch Current Time");
    }
    return epicsTime ( current );
}

epicsTime epicsTime::getEvent (const epicsTimeEvent &event)
{
    epicsTimeStamp current;
    int status = epicsTimeGetEvent (&current, event);
    if (status) {
        throw unableToFetchCurrentTime ("Unable to fetch Event Time");
    }
    return epicsTime ( current );
}

epicsTime::operator struct timeval () const {
    timeval ret;
    epicsTimeToTimeval(&ret, &ts);
    return ret;
}

epicsTime::epicsTime ( const struct timeval & replace) {
    throwError(epicsTimeFromTimeval(&ts, &replace));
}

epicsTime & epicsTime::operator = ( const struct timeval & replace) {
    throwError(epicsTimeFromTimeval(&ts, &replace));
    return *this;
}

std::ostream& operator<<(std::ostream& strm, const epicsTime& ts)
{
    char temp[64];

    (void)ts.strftime(temp, sizeof(temp), "%Y-%m-%d %H:%M:%S.%09f");
    temp[sizeof(temp)-1u] = '\0';
    return strm<<temp;
}

// Break up a format string into "<strftime prefix>%0<nnn>f<postfix>"
// (where <nnn> in an unsigned integer)
// Result:
// A) Copies a prefix which is valid for ANSI strftime into the supplied
// buffer setting the buffer to an empty string if no prefix is present.
// B) Indicates whether a valid "%0<n>f]" is present or not and if so
// specifying its nnnn
// C) returning a pointer to the postfix (which might be passed again
// to fracFormatFind.
static const char * fracFormatFind (
    const char * const pFormat,
    char * const pPrefixBuf,
    const size_t prefixBufLen,
    bool & fracFmtFound,
    unsigned long & fracFmtWidth )
{
    assert ( prefixBufLen > 1 );
    unsigned long width = 0xffffffff;
    bool fracFound = false;
    const char * pAfter = pFormat;
    const char * pFmt = pFormat;
    while ( *pFmt != '\0' ) {
        if ( *pFmt == '%' ) {
            if ( pFmt[1] == '%' ) {
                pFmt++;
            }
            else if ( pFmt[1] == 'f' ) {
                fracFound = true;
                pAfter = & pFmt[2];
                break;
            }
            else {
                errno = 0;
                char * pAfterTmp;
                unsigned long result = strtoul ( pFmt + 1, & pAfterTmp, 10 );
                if ( errno == 0 && *pAfterTmp == 'f' && result > 0 ) {
                    width = result;
                    fracFound = true;
                    pAfter = pAfterTmp + 1;
                    break;
                }
            }
        }
        pFmt++;
        pAfter = pFmt;
    }
    size_t len = pFmt - pFormat;
    if ( len < prefixBufLen ) {
        strncpy ( pPrefixBuf, pFormat, len );
        pPrefixBuf [ len ] = '\0';
        if ( fracFound ) {
            fracFmtFound = true;
            fracFmtWidth = width;
        }
        else {
            fracFmtFound = false;
        }
    }
    else {
        strncpy ( pPrefixBuf, "<invalid format>", prefixBufLen - 1 );
        pPrefixBuf [ prefixBufLen - 1 ] = '\0';
        fracFmtFound = false;
        pAfter = "";
    }

    return pAfter;
}

//
// size_t epicsTime::strftime ()
//
size_t epicsStdCall epicsTimeToStrftime (char *pBuff, size_t bufLength, const char *pFormat, const epicsTimeStamp *pTS)
{
    if ( bufLength == 0u ) {
        return 0u;
    }

    // presume that EPOCH date is an uninitialized time stamp
    if ( pTS->secPastEpoch == 0 && pTS->nsec == 0u ) {
        strncpy ( pBuff, "<undefined>", bufLength );
        pBuff[bufLength-1] = '\0';
        return strlen ( pBuff );
    }

    char * pBufCur = pBuff;
    const char * pFmt = pFormat;
    size_t bufLenLeft = bufLength;
    while ( *pFmt != '\0' && bufLenLeft > 1 ) {
        // look for "%0<n>f" at the end (used for fractional second formatting)
        char strftimePrefixBuf [256];
        bool fracFmtFound;
        unsigned long fracWid = 0;
        pFmt = fracFormatFind (
            pFmt,
            strftimePrefixBuf, sizeof ( strftimePrefixBuf ),
            fracFmtFound, fracWid );

        // nothing more in the string, then quit
        if ( ! ( strftimePrefixBuf[0] != '\0' || fracFmtFound ) ) {
            break;
        }
        // all but fractional seconds use strftime formatting
        if ( strftimePrefixBuf[0] != '\0' ) {
            tm tm;
            (void)epicsTimeToTM(&tm, 0, pTS);
            size_t strftimeNumChar = :: strftime (
                pBufCur, bufLenLeft, strftimePrefixBuf, &tm );
            pBufCur [ strftimeNumChar ] = '\0';
            pBufCur += strftimeNumChar;
            bufLenLeft -= strftimeNumChar;
        }

        // fractional seconds formatting
        if ( fracFmtFound && bufLenLeft > 1 ) {
            if ( fracWid > nSecFracDigits ) {
                fracWid = nSecFracDigits;
            }
            // verify that there are enough chars left for the fractional seconds
            if ( fracWid < bufLenLeft )
            {
                tm tm;
                (void)epicsTimeToTM(&tm, 0, pTS);
                if ( pTS->nsec < nSecPerSec ) {
                    // divisors for fraction (see below)
                    static const unsigned long div[] = {
                        static_cast < unsigned long > ( 1e9 ),
                        static_cast < unsigned long > ( 1e8 ),
                        static_cast < unsigned long > ( 1e7 ),
                        static_cast < unsigned long > ( 1e6 ),
                        static_cast < unsigned long > ( 1e5 ),
                        static_cast < unsigned long > ( 1e4 ),
                        static_cast < unsigned long > ( 1e3 ),
                        static_cast < unsigned long > ( 1e2 ),
                        static_cast < unsigned long > ( 1e1 ),
                        static_cast < unsigned long > ( 1e0 )
                    };
                    // round without overflowing into whole seconds
                    unsigned long frac = pTS->nsec + div[fracWid] / 2;
                    if (frac >= nSecPerSec)
                        frac = nSecPerSec - 1;
                    // convert nanosecs to integer of correct range
                    frac /= div[fracWid];
                    char fracFormat[32];
                    sprintf ( fracFormat, "%%0%lulu", fracWid );
                    int status = epicsSnprintf ( pBufCur, bufLenLeft, fracFormat, frac );
                    if ( status > 0 ) {
                        unsigned long nChar = static_cast < unsigned long > ( status );
                        if  ( nChar >= bufLenLeft ) {
                            nChar = bufLenLeft - 1;
                        }
                        pBufCur[nChar] = '\0';
                        pBufCur += nChar;
                        bufLenLeft -= nChar;
                    }
                }
                else {
                    static const char pOVF [] = "OVF";
                    size_t tmpLen = sizeof ( pOVF ) - 1;
                    if ( tmpLen >= bufLenLeft ) {
                        tmpLen = bufLenLeft - 1;
                    }
                    strncpy ( pBufCur, pOVF, tmpLen );
                    pBufCur[tmpLen] = '\0';
                    pBufCur += tmpLen;
                    bufLenLeft -= tmpLen;
                }
            }
            else {
                static const char pDoesntFit [] = "************";
                size_t tmpLen = sizeof ( pDoesntFit ) - 1;
                if ( tmpLen >= bufLenLeft ) {
                    tmpLen = bufLenLeft - 1;
                }
                strncpy ( pBufCur, pDoesntFit, tmpLen );
                pBufCur[tmpLen] = '\0';
                pBufCur += tmpLen;
                bufLenLeft -= tmpLen;
                break;
            }
        }
    }
    return pBufCur - pBuff;
}

//
// epicsTime::show (unsigned)
//
void epicsStdCall epicsTimeShow (const epicsTimeStamp *pTS, unsigned level)
{
    char bigBuffer[256];

    size_t numChar = epicsTimeToStrftime( bigBuffer, sizeof ( bigBuffer ),
                    "%a %b %d %Y %H:%M:%S.%09f", pTS );
    if ( numChar > 0 ) {
        printf ( "epicsTime: %s\n", bigBuffer );
    }
}

int epicsStdCall epicsTimeToTime_t (time_t *pDest, const epicsTimeStamp *pSrc)
{
    STATIC_ASSERT(sizeof(*pDest) >= sizeof(pSrc->secPastEpoch));

    // widen to 64-bit to (eventually) accommodate 64-bit time_t
    *pDest = epicsUInt64(pSrc->secPastEpoch) + POSIX_TIME_AT_EPICS_EPOCH;
    return epicsTimeOK;
}

int epicsStdCall epicsTimeFromTime_t (epicsTimeStamp *pDest, time_t src)
{
    pDest->secPastEpoch = epicsInt64(src) - POSIX_TIME_AT_EPICS_EPOCH;
    pDest->nsec = 0;
    return epicsTimeOK;
}

int epicsStdCall epicsTimeToTM (struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc)
{
    time_t temp;
    int err;
    err = epicsTimeToTime_t(&temp, pSrc);
    if(!err)
        err = epicsTime_localtime(&temp, pDest);
    if(!err && pNSecDest)
        *pNSecDest = pSrc->nsec;
    return err;
}

int epicsStdCall epicsTimeToGMTM (struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc)
{
    time_t temp;
    int err;
    err = epicsTimeToTime_t(&temp, pSrc);
    if(!err)
        err = epicsTime_gmtime(&temp, pDest);
    if(!err && pNSecDest)
        *pNSecDest = pSrc->nsec;
    return err;
}

int epicsStdCall epicsTimeFromTM (epicsTimeStamp *pDest, const struct tm *pSrc, unsigned long nSecSrc)
{
    tm temp = *pSrc; // mktime() modifies (at least) tm_wday and tm_yday
    time_t tsrc = mktime(&temp);
    int err = epicsTimeFromTime_t(pDest, tsrc);
    if(!err)
        pDest->nsec = nSecSrc;
    return err;
}

#ifdef _WIN32
#  define timegm _mkgmtime

#elif defined(__rtems__) || defined(vxWorks)

static
time_t timegm(tm* gtm)
{
    // ugly hack for targets without timegm(tm*), but which have mktime(tm*).
    // probably has issues near start/end of DST

    // translate to seconds as if a local time.  off by TZ offset
    time_t fakelocal = mktime(gtm);
    // now use gmtime() which applies the TZ offset again, but with the wrong sign
    tm wrongtm;
    epicsTime_gmtime(&fakelocal, &wrongtm);
    // translate this to seconds
    time_t fakex2 = mktime(&wrongtm);

    // tzoffset = fakelocal - fakex2;

    return epicsInt64(fakelocal)*2 - fakex2;
}

#endif

int epicsStdCall epicsTimeFromGMTM (epicsTimeStamp *pDest, const struct tm *pSrc, unsigned long nSecSrc)
{
    tm temp = *pSrc; // timegm() may modify (at least) tm_wday and tm_yday
    time_t tsrc = timegm(&temp);
    int err = epicsTimeFromTime_t(pDest, tsrc);
    if(!err)
        pDest->nsec = nSecSrc;
    return err;
}

int epicsStdCall epicsTimeToTimespec (struct timespec *pDest, const epicsTimeStamp *pSrc)
{
    int err = epicsTimeToTime_t(&pDest->tv_sec, pSrc);
    if(!err)
        pDest->tv_nsec = pSrc->nsec;
    return err;
}

int epicsStdCall epicsTimeFromTimespec (epicsTimeStamp *pDest, const struct timespec *pSrc)
{
    int err = epicsTimeFromTime_t(pDest, pSrc->tv_sec);
    if(!err)
        pDest->nsec = pSrc->tv_nsec;
    return err;
}

int epicsStdCall epicsTimeToTimeval (struct timeval *pDest, const epicsTimeStamp *pSrc)
{
    time_t temp;
    int err = epicsTimeToTime_t(&temp, pSrc);
    if(!err) {
        pDest->tv_sec = temp; // tv_sec is not time_t on windows
        pDest->tv_usec = pSrc->nsec/1000u;
    }
    return err;
}

int epicsStdCall epicsTimeFromTimeval (epicsTimeStamp *pDest, const struct timeval *pSrc)
{
    int err = epicsTimeFromTime_t(pDest, pSrc->tv_sec);
    if(!err)
        pDest->nsec = pSrc->tv_usec*1000u;
    return err;
}

double epicsStdCall epicsTimeDiffInSeconds (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    /* double(*pLeft - *pRight)
     *
     * 0xffffffff*1000000000 < 2**62
     * 0x200000000*1000000002 < 2**63
     * so there is (just barely) space to add 2 TSs as signed 64-bit integers without overflow
     */

    // handle over/underflow as u32 when subtracting
    epicsInt64 nsec = epicsInt32(pLeft->secPastEpoch - pRight->secPastEpoch);
    nsec *= nSecPerSec;
    nsec += epicsInt32(pLeft->nsec) - epicsInt32(pRight->nsec);

    return double(nsec)*1e-9;
}

void epicsStdCall epicsTimeAddSeconds (epicsTimeStamp *pDest, double seconds)
{
    epicsInt64 nsec = pDest->secPastEpoch;
    nsec *= nSecPerSec;
    nsec += epicsInt64(pDest->nsec);
    nsec += epicsInt64(seconds*1e9 + (seconds>=0.0 ? 0.5 : -0.5));
    pDest->secPastEpoch = nsec/nSecPerSec;
    pDest->nsec         = (nsec>=0 ? nsec : -nsec)%nSecPerSec;
}

int epicsStdCall epicsTimeEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return pLeft->secPastEpoch == pRight->secPastEpoch && pLeft->nsec == pRight->nsec;
}

int epicsStdCall epicsTimeNotEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return !epicsTimeEqual(pLeft, pRight);
}

epicsInt64 epicsStdCall epicsTimeDiffInNS (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    epicsInt64 delta = epicsInt64(pLeft->secPastEpoch) - pRight->secPastEpoch;
    delta *= nSecPerSec;
    delta += epicsInt64(pLeft->nsec) - pRight->nsec;
    return delta;
}

int epicsStdCall epicsTimeLessThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return epicsTimeDiffInNS(pLeft, pRight) < 0;
}

int epicsStdCall epicsTimeLessThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return epicsTimeDiffInNS(pLeft, pRight) <= 0;
}

int epicsStdCall epicsTimeGreaterThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return epicsTimeDiffInNS(pLeft, pRight) > 0;
}

int epicsStdCall epicsTimeGreaterThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
{
    return epicsTimeDiffInNS(pLeft, pRight) >= 0;
}
