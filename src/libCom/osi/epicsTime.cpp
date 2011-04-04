/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsTime.cpp */
/* Author Jeffrey O. Hill */

// Notes:
// 1) The epicsTime::nSec field is not public and so it could be 
// changed to work more like the fractional seconds field in the NTP time 
// stamp. That would significantly improve the precision of epicsTime on 
// 64 bit architectures.
//

#include <stdexcept>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <string> // vxWorks 6.0 requires this include 

#define epicsExportSharedSymbols
#include "epicsStdioRedirect.h"
#include "locationException.h"
#include "epicsAssert.h"
#include "epicsVersion.h"
#include "envDefs.h"
#include "epicsTime.h"
#include "osiSock.h" /* pull in struct timeval */
#include "epicsStdio.h"

static const char pEpicsTimeVersion[] = 
    "@(#) " EPICS_VERSION_STRING ", Common Utilities Library " __DATE__;

//
// useful public constants
//
static const unsigned mSecPerSec = 1000u;
static const unsigned uSecPerMSec = 1000u;
static const unsigned uSecPerSec = uSecPerMSec * mSecPerSec;
static const unsigned nSecPerUSec = 1000u;
static const unsigned nSecPerSec = nSecPerUSec * uSecPerSec;
static const unsigned nSecFracDigits = 9u;


// Timescale conversion data

static const unsigned long NTP_TIME_AT_POSIX_EPOCH = 2208988800ul;
static const unsigned long NTP_TIME_AT_EPICS_EPOCH =
    NTP_TIME_AT_POSIX_EPOCH + POSIX_TIME_AT_EPICS_EPOCH;

//
// epicsTime (const unsigned long secIn, const unsigned long nSecIn)
//
inline epicsTime::epicsTime (const unsigned long secIn, const unsigned long nSecIn) :
    secPastEpoch ( nSecIn / nSecPerSec + secIn ), nSec ( nSecIn % nSecPerSec ) {}

//
// epicsTimeLoadTimeInit
//
class epicsTimeLoadTimeInit {
public:
    epicsTimeLoadTimeInit ();
    double epicsEpochOffset; // seconds
    double time_tSecPerTick; // seconds (both NTP and EPICS use int sec)
    unsigned long epicsEpochOffsetAsAnUnsignedLong;
    bool useDiffTimeOptimization;
};

//
// epicsTimeLoadTimeInit ()
//
epicsTimeLoadTimeInit::epicsTimeLoadTimeInit ()
{
    // All we know about time_t is that it is an arithmetic type.
    time_t t_zero = static_cast<time_t> (0);
    time_t t_one  = static_cast<time_t> (1);
    this->time_tSecPerTick = difftime (t_one, t_zero);

    /* The EPICS epoch (1/1/1990 00:00:00UTC) was 631152000 seconds after
     * the ANSI epoch (1/1/1970 00:00:00UTC)
     * Convert this offset into time_t units, however this must not be
     * calculated using local time (i.e. using mktime() or similar), since
     * in the UK the ANSI Epoch had daylight saving time in effect, and 
     * the value calculated would be 3600 seconds wrong.*/
    this->epicsEpochOffset =
        (double) POSIX_TIME_AT_EPICS_EPOCH / this->time_tSecPerTick;

    if (this->time_tSecPerTick == 1.0 &&
        this->epicsEpochOffset <= ULONG_MAX &&
        this->epicsEpochOffset >= 0) {
        // We can use simpler code on Posix-compliant systems
        this->useDiffTimeOptimization = true;
        this->epicsEpochOffsetAsAnUnsignedLong = 
            static_cast<unsigned long>(this->epicsEpochOffset);
    } else {
        // Forced to use the slower but correct code
        this->useDiffTimeOptimization = false;
        this->epicsEpochOffsetAsAnUnsignedLong = 0;
    }
}

//
// epicsTime::addNanoSec ()
//
// many of the UNIX timestamp formats have nano sec stored as a long
//
inline void epicsTime::addNanoSec (long nSecAdj)
{
    double secAdj = static_cast <double> (nSecAdj) / nSecPerSec;
    *this += secAdj;
}

//
// epicsTime (const time_t_wrapper &tv)
//
epicsTime::epicsTime ( const time_t_wrapper & ansiTimeTicks )
{
    // avoid c++ static initialization order issues
    static epicsTimeLoadTimeInit & lti = * new epicsTimeLoadTimeInit ();

    //
    // try to directly map time_t into an unsigned long integer because this is 
    // faster on systems w/o hardware floating point and a simple integer type time_t.
    //
    if ( lti.useDiffTimeOptimization ) {
        // LONG_MAX is used here and not ULONG_MAX because some systems (linux) 
        // still store time_t as a long.
        if ( ansiTimeTicks.ts > 0 && ansiTimeTicks.ts <= LONG_MAX ) {
            unsigned long ticks = static_cast < unsigned long > ( ansiTimeTicks.ts );
            if ( ticks >= lti.epicsEpochOffsetAsAnUnsignedLong ) {
                this->secPastEpoch = ticks - lti.epicsEpochOffsetAsAnUnsignedLong;
            }
            else {
                this->secPastEpoch = ( ULONG_MAX - lti.epicsEpochOffsetAsAnUnsignedLong ) + ticks;
            }
            this->nSec = 0;
            return;
        }
    }

    //
    // otherwise map time_t, which ANSI C and POSIX define as any arithmetic type, 
    // into type double 
    //
    double sec = ansiTimeTicks.ts * lti.time_tSecPerTick - lti.epicsEpochOffset;

    //
    // map into the the EPICS time stamp range (which allows rollover)
    //
    static double uLongMax = static_cast<double> (ULONG_MAX);
    if ( sec < 0.0 ) {
        if ( sec < -uLongMax ) {
            sec = sec + static_cast<unsigned long> ( -sec / uLongMax ) * uLongMax;
        }
        sec += uLongMax;
    }
    else if ( sec > uLongMax ) {
        sec = sec - static_cast<unsigned long> ( sec / uLongMax ) * uLongMax;
    }

    this->secPastEpoch = static_cast <unsigned long> ( sec );
    this->nSec = static_cast <unsigned long> ( ( sec-this->secPastEpoch ) * nSecPerSec );
}

epicsTime::epicsTime (const epicsTimeStamp &ts) 
{
    if ( ts.nsec < nSecPerSec ) {
        this->secPastEpoch = ts.secPastEpoch;
        this->nSec = ts.nsec;
    }
    else {
        throw std::logic_error ( 
            "epicsTimeStamp has overflow in nano-seconds field" );
    }
}

epicsTime::epicsTime () :
    secPastEpoch(0u), nSec(0u) {}

epicsTime::epicsTime (const epicsTime &t) :
    secPastEpoch (t.secPastEpoch), nSec (t.nSec) {}

epicsTime epicsTime::getCurrent ()
{
    epicsTimeStamp current;
    int status = epicsTimeGetCurrent (&current);
    if (status) {
        throwWithLocation ( unableToFetchCurrentTime () );
    }
    return epicsTime ( current );
}

epicsTime epicsTime::getEvent (const epicsTimeEvent &event)
{
    epicsTimeStamp current;
    int status = epicsTimeGetEvent (&current, event);
    if (status) {
        throwWithLocation ( unableToFetchCurrentTime () );
    }
    return epicsTime ( current );
}

//
// operator time_t_wrapper ()
//
epicsTime::operator time_t_wrapper () const
{
    // avoid c++ static initialization order issues
    static epicsTimeLoadTimeInit & lti = * new epicsTimeLoadTimeInit ();
    time_t_wrapper wrap;

    if ( lti.useDiffTimeOptimization ) {
        if ( this->secPastEpoch < ULONG_MAX - lti.epicsEpochOffsetAsAnUnsignedLong ) {
           wrap.ts = static_cast <time_t> ( this->secPastEpoch + lti.epicsEpochOffsetAsAnUnsignedLong );
           return wrap;
       }
    }
    
    //
    // map type double into time_t which ansi C defines as some arithmetic type
    //
    double tmp = (this->secPastEpoch + lti.epicsEpochOffset) / lti.time_tSecPerTick;
    tmp += (this->nSec / lti.time_tSecPerTick) / nSecPerSec;

    wrap.ts = static_cast <time_t> ( tmp );

    return wrap;
}

//
// convert to ANSI C struct tm (with nano seconds) adjusted for the local time zone
//
epicsTime::operator local_tm_nano_sec () const
{
    time_t_wrapper ansiTimeTicks = *this;

    local_tm_nano_sec tm;

    int status = epicsTime_localtime ( &ansiTimeTicks.ts, &tm.ansi_tm );
    if ( status != epicsTimeOK ) {
        throw std::logic_error ( "epicsTime_localtime failed" );
    }

    tm.nSec = this->nSec;

    return tm;
}

//
// convert to ANSI C struct tm (with nano seconds) adjusted for UTC
//
epicsTime::operator gm_tm_nano_sec () const
{
    time_t_wrapper ansiTimeTicks = *this;

    gm_tm_nano_sec tm;

    int status = epicsTime_gmtime ( &ansiTimeTicks.ts, &tm.ansi_tm );
    if ( status != epicsTimeOK ) {
        throw std::logic_error ( "epicsTime_gmtime failed" );
    }

    tm.nSec = this->nSec;

    return tm;
}

//
// epicsTime (const local_tm_nano_sec &tm)
//
epicsTime::epicsTime (const local_tm_nano_sec &tm)
{
    static const time_t mktimeFailure = static_cast <time_t> (-1);
    time_t_wrapper ansiTimeTicks;
    struct tm tmp = tm.ansi_tm;

    ansiTimeTicks.ts = mktime (&tmp);
    if (ansiTimeTicks.ts ==  mktimeFailure) {
        throwWithLocation ( formatProblemWithStructTM () );
    }

    *this = epicsTime (ansiTimeTicks);

    unsigned long nSecAdj = tm.nSec % nSecPerSec;
    unsigned long secAdj = tm.nSec / nSecPerSec;
    *this = epicsTime ( this->secPastEpoch+secAdj, this->nSec+nSecAdj );
}

//
// operator struct timespec ()
//
epicsTime::operator struct timespec () const
{
    struct timespec ts;
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks = *this;
    ts.tv_sec = ansiTimeTicks.ts;
    ts.tv_nsec = static_cast<long> (this->nSec);
    return ts;
}

//
// epicsTime (const struct timespec &ts)
//
epicsTime::epicsTime (const struct timespec &ts)
{
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = epicsTime (ansiTimeTicks);
    this->addNanoSec (ts.tv_nsec);
}

//
// operator struct timeval ()
//
epicsTime::operator struct timeval () const
{
    struct timeval ts;
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks = *this;
    // On Posix systems timeval :: tv_sec is a time_t so this can be 
    // a direct assignement. On other systems I dont know that we can
    // guarantee that time_t and timeval :: tv_sec will have the
    // same epoch or have the same scaling factor to discrete seconds.
    // For example, on windows time_t changed recently to a 64 bit 
    // quantity but timeval is still a long. That can cause problems
    // on 32 bit systems. So technically, we should have an os 
    // dependent conversion between time_t and timeval :: tv_sec?
    ts.tv_sec = ansiTimeTicks.ts;
    ts.tv_usec = static_cast < long > ( this->nSec / nSecPerUSec );
    return ts;
}

//
// epicsTime (const struct timeval &ts)
//
epicsTime::epicsTime (const struct timeval &ts)
{
    time_t_wrapper ansiTimeTicks;
    // On Posix systems timeval :: tv_sec is a time_t so this can be 
    // a direct assignement. On other systems I dont know that we can
    // guarantee that time_t and timeval :: tv_sec will have the
    // same epoch or have the same scaling factor to discrete seconds.
    // For example, on windows time_t changed recently to a 64 bit 
    // quantity but timeval is still a long. That can cause problems
    // on 32 bit systems. So technically, we should have an os 
    // dependent conversion between time_t and timeval :: tv_sec?
    ansiTimeTicks.ts = ts.tv_sec;
    *this = epicsTime (ansiTimeTicks);
    this->addNanoSec (ts.tv_usec * nSecPerUSec);
}


static const double NTP_FRACTION_DENOMINATOR = 1.0 + 0xffffffff;

struct l_fp { /* NTP time stamp */
    epicsUInt32 l_ui; /* sec past NTP epoch */
    epicsUInt32 l_uf; /* fractional seconds */
};

//
// epicsTime::l_fp ()
//
epicsTime::operator l_fp () const
{
    l_fp ts;
    ts.l_ui = this->secPastEpoch + NTP_TIME_AT_EPICS_EPOCH;
    ts.l_uf = static_cast < unsigned long > 
        ( ( this->nSec * NTP_FRACTION_DENOMINATOR ) / nSecPerSec );
    return ts;
}

//
// epicsTime::epicsTime ( const l_fp & ts )
//
epicsTime::epicsTime ( const l_fp & ts )
{
    this->secPastEpoch = ts.l_ui - NTP_TIME_AT_EPICS_EPOCH;
    this->nSec = static_cast < unsigned long > 
        ( ( ts.l_uf / NTP_FRACTION_DENOMINATOR ) * nSecPerSec );
}

epicsTime::operator epicsTimeStamp () const
{
    if ( this->nSec >= nSecPerSec ) {
        throw std::logic_error ( 
            "epicsTimeStamp has overflow in nano-seconds field?" );
    }
    epicsTimeStamp ts;
    //
    // trucation by design
    // -------------------
    // epicsTime::secPastEpoch is based on ulong and has much greater range 
    // on 64 bit hosts than the orginal epicsTimeStamp::secPastEpoch. The 
    // epicsTimeStamp::secPastEpoch is based on epicsUInt32 so that it will 
    // match the original network protocol. Of course one can anticipate
    // that eventually, a epicsUInt64 based network time stamp will be 
    // introduced when 64 bit architectures are more ubiquitous.
    //
    // Truncation usually works fine here because the routines in this code
    // that compute time stamp differences and compare time stamps produce
    // good results when the operands are on either side of a time stamp
    // rollover as long as the difference between the operands does not exceed
    // 1/2 of full range.
    //
    ts.secPastEpoch = static_cast < epicsUInt32 > ( this->secPastEpoch );
    ts.nsec = static_cast < epicsUInt32 > ( this->nSec );
    return ts;
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
    unsigned long width = ULONG_MAX;
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
size_t epicsTime::strftime ( 
    char * pBuff, size_t bufLength, const char * pFormat ) const 
{
    if ( bufLength == 0u ) {
        return 0u;
    }

    // presume that EPOCH date is an uninitialized time stamp
    if ( this->secPastEpoch == 0 && this->nSec == 0u ) {
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
            local_tm_nano_sec tmns = *this;
            size_t strftimeNumChar = :: strftime (
                pBufCur, bufLenLeft, strftimePrefixBuf, & tmns.ansi_tm );
            pBufCur [ strftimeNumChar ] = '\0';
            pBufCur += strftimeNumChar;
            bufLenLeft -= strftimeNumChar;
        }

        // fractional seconds formating
        if ( fracFmtFound && bufLenLeft > 1 ) {
            if ( fracWid > nSecFracDigits ) {
                fracWid = nSecFracDigits;
            }
            // verify that there are enough chars left for the fractional seconds
            if ( fracWid < bufLenLeft )
            {
                local_tm_nano_sec tmns = *this;
                if ( tmns.nSec < nSecPerSec ) {
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
                    // round and convert nanosecs to integer of correct range
                    unsigned long frac = tmns.nSec + div[fracWid] / 2;
                    frac %= static_cast < unsigned long > ( 1e9 );
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
void epicsTime::show ( unsigned level ) const
{
    char bigBuffer[256];

    size_t numChar = this->strftime ( bigBuffer, sizeof ( bigBuffer ),
                    "%a %b %d %Y %H:%M:%S.%09f" );
    if ( numChar > 0 ) {
        printf ( "epicsTime: %s\n", bigBuffer );
    }

    if ( level > 1 ) {
        // this also supresses the "defined, but not used" 
        // warning message
        printf ( "epicsTime: revision \"%s\"\n", 
            pEpicsTimeVersion );
    }

}

//
// epicsTime::operator + (const double &rhs)
//
// rhs has units seconds
//
epicsTime epicsTime::operator + (const double &rhs) const
{
    unsigned long newSec, newNSec, secOffset, nSecOffset;
    double fnsec;

    if (rhs >= 0) {
        secOffset = static_cast <unsigned long> (rhs);
        fnsec = rhs - secOffset;
        nSecOffset = static_cast <unsigned long> ( (fnsec * nSecPerSec) + 0.5 );

        newSec = this->secPastEpoch + secOffset; // overflow expected
        newNSec = this->nSec + nSecOffset;
        if (newNSec >= nSecPerSec) {
            newSec++; // overflow expected
            newNSec -= nSecPerSec;
        }
    }
    else {
        secOffset = static_cast <unsigned long> (-rhs);
        fnsec = rhs + secOffset;
        nSecOffset = static_cast <unsigned long> ( (-fnsec * nSecPerSec) + 0.5 );

        newSec = this->secPastEpoch - secOffset; // underflow expected
        if (this->nSec>=nSecOffset) {
            newNSec = this->nSec - nSecOffset;
        }
        else {
            // borrow
            newSec--; // underflow expected
            newNSec = this->nSec + (nSecPerSec - nSecOffset);
        }
    }
    return epicsTime (newSec, newNSec);
}

//
// operator - 
//
// To make this code robust during timestamp rollover events
// time stamp differences greater than one half full scale are 
// interpreted as rollover situations:
//
// when RHS is greater than THIS:
// RHS-THIS > one half full scale => return THIS + (ULONG_MAX-RHS)
// RHS-THIS <= one half full scale => return -(RHS-THIS)
//
// when THIS is greater than or equal to RHS
// THIS-RHS > one half full scale => return -(RHS + (ULONG_MAX-THIS))
// THIS-RHS <= one half full scale => return THIS-RHS
//
double epicsTime::operator - (const epicsTime &rhs) const
{
    double nSecRes, secRes;

    //
    // first compute the difference between the nano-seconds members
    //
    // nano sec member is not allowed to be greater that 1/2 full scale
    // so the unsigned to signed conversion is ok
    //
    if (this->nSec>=rhs.nSec) {
        nSecRes = this->nSec - rhs.nSec;    
    }
    else {
        nSecRes = rhs.nSec - this->nSec;
        nSecRes = -nSecRes;
    }

    //
    // next compute the difference between the seconds members
    // and invert the sign of the nano seconds result if there
    // is a range violation
    //
    if (this->secPastEpoch<rhs.secPastEpoch) {
        secRes = rhs.secPastEpoch - this->secPastEpoch;
        if (secRes > ULONG_MAX/2) {
            //
            // In this situation where the difference is more than
            // 68 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" difference
            //
            secRes = 1 + (ULONG_MAX-secRes);
            nSecRes = -nSecRes;
        }
        else {
            secRes = -secRes;
        }
    }
    else {
        secRes = this->secPastEpoch - rhs.secPastEpoch;
        if (secRes > ULONG_MAX/2) {
            //
            // In this situation where the difference is more than
            // 68 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" difference
            //
            secRes = 1 + (ULONG_MAX-secRes);
            secRes = -secRes;
            nSecRes = -nSecRes;
        }
    }

    return secRes + nSecRes/nSecPerSec; 
}

//
// operator <=
//
bool epicsTime::operator <= (const epicsTime &rhs) const
{
    bool rc;

    if (this->secPastEpoch<rhs.secPastEpoch) {
        if (rhs.secPastEpoch-this->secPastEpoch < ULONG_MAX/2) {
            //
            // In this situation where the difference is less than
            // 69 years compute the expected result
            //
            rc = true;
        }
        else {
            //
            // In this situation where the difference is more than
            // 69 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" result
            //
            rc = false;
        }
    }
    else if (this->secPastEpoch>rhs.secPastEpoch) {
        if (this->secPastEpoch-rhs.secPastEpoch < ULONG_MAX/2) {
            //
            // In this situation where the difference is less than
            // 69 years compute the expected result
            //
            rc = false;
        }
        else {
            //
            // In this situation where the difference is more than
            // 69 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" result
            //
            rc = true;
        }
    }
    else {
        if (this->nSec<=rhs.nSec) {
            rc = true;
        }
        else {
            rc = false;
        }
    }
    return rc;
}

//
// operator <
//
bool epicsTime::operator < (const epicsTime &rhs) const
{
    bool    rc;

    if (this->secPastEpoch<rhs.secPastEpoch) {
        if (rhs.secPastEpoch-this->secPastEpoch < ULONG_MAX/2) {
            //
            // In this situation where the difference is less than
            // 69 years compute the expected result
            //
            rc = true;
        }
        else {
            //
            // In this situation where the difference is more than
            // 69 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" result
            //
            rc = false;
        }
    }
    else if (this->secPastEpoch>rhs.secPastEpoch) {
        if (this->secPastEpoch-rhs.secPastEpoch < ULONG_MAX/2) {
            //
            // In this situation where the difference is less than
            // 69 years compute the expected result
            //
            rc = false;
        }
        else {
            //
            // In this situation where the difference is more than
            // 69 years assume that the seconds counter has rolled 
            // over and compute the "wrap around" result
            //
            rc = true;
        }
    }
    else {
        if (this->nSec<rhs.nSec) {
            rc = true;
        }
        else {
            rc = false;
        }
    }
    return rc;
}

extern "C" {
    //
    // ANSI C interface
    //
    // its too bad that these cant be implemented with inline functions 
    // at least when running the GNU compiler
    //
    epicsShareFunc int epicsShareAPI epicsTimeToTime_t (time_t *pDest, const epicsTimeStamp *pSrc)
    {
        try {
            time_t_wrapper dst = epicsTime (*pSrc);
            *pDest = dst.ts;
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeFromTime_t (epicsTimeStamp *pDest, time_t src)
    {
        try {
            time_t_wrapper dst;
            dst.ts = src;
            *pDest = epicsTime ( dst );
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeToTM (struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc)
    {
        try {
            local_tm_nano_sec tmns = epicsTime (*pSrc);
            *pDest = tmns.ansi_tm;
            *pNSecDest = tmns.nSec;
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeToGMTM (struct tm *pDest, unsigned long *pNSecDest, const epicsTimeStamp *pSrc)
    {
        try {
            gm_tm_nano_sec gmtmns = epicsTime (*pSrc);
            *pDest = gmtmns.ansi_tm;
            *pNSecDest = gmtmns.nSec;
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeFromTM (epicsTimeStamp *pDest, const struct tm *pSrc, unsigned long nSecSrc)
    {
        try {
            local_tm_nano_sec tmns;
            tmns.ansi_tm = *pSrc;
            tmns.nSec = nSecSrc;
            *pDest = epicsTime (tmns);
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeToTimespec (struct timespec *pDest, const epicsTimeStamp *pSrc)
    {
        try {
            *pDest = epicsTime (*pSrc);
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeFromTimespec (epicsTimeStamp *pDest, const struct timespec *pSrc)
    {
        try {
            *pDest = epicsTime (*pSrc);
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeToTimeval (struct timeval *pDest, const epicsTimeStamp *pSrc)
    {
        try {
            *pDest = epicsTime (*pSrc);
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc int epicsShareAPI epicsTimeFromTimeval (epicsTimeStamp *pDest, const struct timeval *pSrc)
    {
        try {
            *pDest = epicsTime (*pSrc);
        }
        catch (...) {
            return epicsTimeERROR;
        }
        return epicsTimeOK;
    }
    epicsShareFunc double epicsShareAPI epicsTimeDiffInSeconds (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) - epicsTime (*pRight);
        }
        catch (...) {
            return - DBL_MAX;
        }
    }
    epicsShareFunc void epicsShareAPI epicsTimeAddSeconds (epicsTimeStamp *pDest, double seconds)
    {
        try {
            *pDest = epicsTime (*pDest) + seconds;
        }
        catch ( ... ) {
            *pDest = epicsTime ();
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) == epicsTime (*pRight);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeNotEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) != epicsTime (*pRight);
        }
        catch ( ... ) {
            return 1;
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeLessThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) < epicsTime (*pRight);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeLessThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) <= epicsTime (*pRight);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeGreaterThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) > epicsTime (*pRight);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc int epicsShareAPI epicsTimeGreaterThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        try {
            return epicsTime (*pLeft) >= epicsTime (*pRight);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc size_t epicsShareAPI epicsTimeToStrftime (char *pBuff, size_t bufLength, const char *pFormat, const epicsTimeStamp *pTS)
    {
        try {
            return epicsTime(*pTS).strftime (pBuff, bufLength, pFormat);
        }
        catch ( ... ) {
            return 0;
        }
    }
    epicsShareFunc void epicsShareAPI epicsTimeShow (const epicsTimeStamp *pTS, unsigned interestLevel)
    {
        try {
            epicsTime(*pTS).show (interestLevel);
        }
        catch ( ... ) {
            printf ( "Invalid epicsTimeStamp\n" );
        }
    }
}

