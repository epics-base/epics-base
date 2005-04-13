/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
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
#include <string> // vxWorks 6.0 requires this include 

#define epicsExportSharedSymbols
#include "epicsStdioRedirect.h"
#include "locationException.h"
#include "epicsAssert.h"
#include "epicsVersion.h"
#include "envDefs.h"
#include "epicsTime.h"
#include "osiSock.h" /* pull in struct timeval */

static const char *pEpicsTimeVersion = 
    "@(#) " EPICS_VERSION_STRING ", Common Utilities Library " __DATE__;

//
// useful public constants
//
static const unsigned mSecPerSec = 1000u;
static const unsigned uSecPerSec = 1000u * mSecPerSec;
static const unsigned nSecPerSec = 1000u * uSecPerSec;
static const unsigned nSecPerUSec = 1000u;
static const unsigned secPerMin = 60u;

//
// force this module to include code that can convert
// to GDD's aitTimeStamp, but dont require that it must
// link with gdd. Therefore, gdd.h is not included here.
//
class aitTimeStamp {
public:
    unsigned long tv_sec;
    unsigned long tv_nsec;
};

static const unsigned tmStructEpochYear = 1900;

static const unsigned epicsEpochYear = 1990;
static const unsigned epicsEpocMonth = 0; // January
static const unsigned epicsEpocDayOfTheMonth = 1; // the 1st day of the month

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

static const epicsTimeLoadTimeInit lti;

//
// epicsTimeLoadTimeInit ()
//
epicsTimeLoadTimeInit::epicsTimeLoadTimeInit ()
{
    static const time_t ansiEpoch = 0;
    double secWest;

    {
        time_t current = time ( NULL );
        time_t error;
        struct tm date; // vxWorks 6.0 requires "struct" here 

        int status = epicsTime_gmtime ( &current, &date );
        assert ( status == epicsTimeOK );
        error = mktime ( &date );
        assert ( error != (time_t) - 1 );
        secWest =  difftime ( error, current );
    }
    
    {
        time_t first = static_cast<time_t> (0);
        time_t last = static_cast<time_t> (1);
        this->time_tSecPerTick = difftime (last, first);
    }

    {
        struct tm tmEpicsEpoch;
        time_t epicsEpoch;

        tmEpicsEpoch.tm_sec = 0;
        tmEpicsEpoch.tm_min = 0;
        tmEpicsEpoch.tm_hour = 0;
        tmEpicsEpoch.tm_mday = epicsEpocDayOfTheMonth;
        tmEpicsEpoch.tm_mon = epicsEpocMonth;
        tmEpicsEpoch.tm_year = epicsEpochYear - tmStructEpochYear;
        // must not correct for DST because secWest does 
        // not include a DST offset
        tmEpicsEpoch.tm_isdst = 0; 

        epicsEpoch = mktime (&tmEpicsEpoch);
        assert (epicsEpoch!=(time_t)-1);
        this->epicsEpochOffset = difftime ( epicsEpoch, ansiEpoch ) - secWest;
    }

    if ( this->time_tSecPerTick == 1.0 && this->epicsEpochOffset <= ULONG_MAX &&
           this->epicsEpochOffset >= 0 ) {
        this->useDiffTimeOptimization = true;
        this->epicsEpochOffsetAsAnUnsignedLong = 
            static_cast < unsigned long > ( this->epicsEpochOffset );
    }
    else {
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
    this->secPastEpoch = ts.secPastEpoch;
    this->nSec = ts.nsec;
}

epicsTime::epicsTime () : secPastEpoch(0u), nSec(0u) {}	

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
    int status = epicsTimeGetEvent (&current, event.eventNumber);
    if (status) {
        throwWithLocation ( unableToFetchCurrentTime () );
    }
    return epicsTime ( current );
}

void epicsTime::synchronize () {} // depricated

//
// operator time_t_wrapper ()
//
epicsTime::operator time_t_wrapper () const
{
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
        throw std::logic_error ( "epicsTime_gmtime failed" );
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
    ts.tv_sec = ansiTimeTicks.ts;
    ts.tv_usec = static_cast<long> (this->nSec / nSecPerUSec);
    return ts;
}

//
// epicsTime (const struct timeval &ts)
//
epicsTime::epicsTime (const struct timeval &ts)
{
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = epicsTime (ansiTimeTicks);
    this->addNanoSec (ts.tv_usec * nSecPerUSec);
}

//
// operator aitTimeStamp ()
//
epicsTime::operator aitTimeStamp () const
{
    aitTimeStamp ts;
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks = *this;
    ts.tv_sec = ansiTimeTicks.ts;
    ts.tv_nsec = this->nSec;
    return ts;
}

//
// epicsTime (const aitTimeStamp &ts)
//
epicsTime::epicsTime (const aitTimeStamp &ts) 
{
    time_t_wrapper ansiTimeTicks;

    ansiTimeTicks.ts = ts.tv_sec;
    *this = epicsTime (ansiTimeTicks);

    unsigned long secAdj = ts.tv_nsec / nSecPerSec;
    unsigned long nSecAdj = ts.tv_nsec % nSecPerSec;
    *this = epicsTime (this->secPastEpoch+secAdj, this->nSec+nSecAdj);
}

// 631152000 (at posix epic) + 2272060800 (btw posix and epics epoch) +
// 15 ( leap seconds )
static const unsigned long epicsEpocSecsPastEpochNTP = 2903212815u;
static const double NTP_FRACTION_DENOMINATOR = (static_cast<double>(0xffffffff) + 1.0);

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
    ts.l_ui = this->secPastEpoch + epicsEpocSecsPastEpochNTP;
    ts.l_uf = static_cast < unsigned long > 
        ( ( this->nSec * NTP_FRACTION_DENOMINATOR ) / nSecPerSec );
    return ts;
}

//
// epicsTime::epicsTime ( const l_fp & ts )
//
epicsTime::epicsTime ( const l_fp & ts )
{
    this->secPastEpoch = ts.l_ui - epicsEpocSecsPastEpochNTP;
    this->nSec = static_cast < unsigned long > 
        ( ( ts.l_uf / NTP_FRACTION_DENOMINATOR ) * nSecPerSec );
}

// return pointer to trailing "%0<n>f" (<n> is a number) in a format string,
// NULL if none; also return <n> and a pointer to the "f"
static char *fracFormat (const char *pFormat, unsigned long *width)
{
    // initialize returned field width
    *width = 0;

    // point at char past format string
    const char *ptr = pFormat + strlen (pFormat);

    // allow trailing ws
    while (isspace (*(--ptr))); 

    // if (last) char not 'f', no match
    if (*ptr != 'f') return NULL;

    // look for %f
    if ( *(ptr-1) == '%' ) {
        *width = 9;
        return (char *) (ptr-1);
    }

    // skip digits, extracting <n> (must start with 0)
    while (isdigit (*(--ptr))); // NB, empty loop body
    if (*ptr != '%') {
        return NULL;
    }
    ++ptr; // points to first digit, if any
    if (sscanf (ptr, "%lu", width) != 1) return NULL;

    // if (prev) char not '%', no match
    if (*(--ptr) != '%') return NULL;

    // return pointer to '%'
    return (char *) ptr;
}

//
// size_t epicsTime::strftime (char *pBuff, size_t bufLength, const char *pFormat)
//
size_t epicsTime::strftime ( char *pBuff, size_t bufLength, const char *pFormat ) const
{
    if ( bufLength == 0u ) {
        return 0u;
    }

    // dont report EPOCH date if its an uninitialized time stamp
    if ( this->secPastEpoch == 0 && this->nSec == 0u ) {
        strncpy ( pBuff, "<undefined>", bufLength );
        pBuff[bufLength-1] = '\0';
        return strlen ( pBuff );
    }

    // copy format (needs to be writable)
    char format[256];
    strcpy (format, pFormat);

    // look for "%0<n>f" at the end (used for fractional second formatting)
    unsigned long fracWid;
    char *fracPtr = fracFormat (format, &fracWid);

    // if found, hide from strftime
    if (fracPtr != NULL) *fracPtr = '\0';

    // format all but fractional seconds
    local_tm_nano_sec tmns = *this;
    size_t numChar = ::strftime (pBuff, bufLength, format, &tmns.ansi_tm);
    if (numChar == 0 || fracPtr == NULL) return numChar;

    // check there are enough chars for the fractional seconds
    if (numChar + fracWid < bufLength)
    {
        // divisors for fraction (see below)
        const int div[9+1] = {(int)1e9,(int)1e8,(int)1e7,(int)1e6,(int)1e5,
                      (int)1e4,(int)1e3,(int)1e2,(int)1e1,(int)1e0};

        // round and convert nanosecs to integer of correct range
        int ndp = fracWid <= 9 ? fracWid : 9;
        int frac = ((tmns.nSec + div[ndp]/2) % ((int)1e9)) / div[ndp];

        // restore fractional format and format fraction
        *fracPtr = '%';
        *(format + strlen (format) - 1) = 'u';
        sprintf (pBuff+numChar, fracPtr, frac);
    }
    return numChar + fracWid;
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
        return epicsTime (*pLeft) - epicsTime (*pRight);
    }
    epicsShareFunc void epicsShareAPI epicsTimeAddSeconds (epicsTimeStamp *pDest, double seconds)
    {
        *pDest = epicsTime (*pDest) + seconds;
    }
    epicsShareFunc int epicsShareAPI epicsTimeEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) == epicsTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI epicsTimeNotEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) != epicsTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI epicsTimeLessThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) < epicsTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI epicsTimeLessThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) <= epicsTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI epicsTimeGreaterThan (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) > epicsTime (*pRight);
    }
    epicsShareFunc int epicsShareAPI epicsTimeGreaterThanEqual (const epicsTimeStamp *pLeft, const epicsTimeStamp *pRight)
    {
        return epicsTime (*pLeft) >= epicsTime (*pRight);
    }
    epicsShareFunc size_t epicsShareAPI epicsTimeToStrftime (char *pBuff, size_t bufLength, const char *pFormat, const epicsTimeStamp *pTS)
    {
        return epicsTime(*pTS).strftime (pBuff, bufLength, pFormat);
    }
    epicsShareFunc void epicsShareAPI epicsTimeShow (const epicsTimeStamp *pTS, unsigned interestLevel)
    {
        epicsTime(*pTS).show (interestLevel);
    }
}

