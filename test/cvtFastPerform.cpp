// Original Author: Jeff Hill, LANL

#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <typeinfo>
#include <iostream>

#include "epicsStdio.h"
#include "cvtFast.h"
#include "epicsTime.h"
#include "testMain.h"

using namespace std;

class PerfConverter {
public:
    virtual const char * name(void) const = 0;
    virtual int maxPrecision(void) const = 0;
    virtual void target (double srcD, float srcF, char *dst, size_t len, int prec) const = 0;
    virtual void add(int prec, double elapsed) = 0;
    virtual double total(int prec) = 0;
    virtual ~PerfConverter () {};
};

class Perf {
public:
    Perf ( int maxConverters );
    virtual ~Perf ();
    void addConverter( PerfConverter * c );
    void execute (int count, bool verbose);
    void report (const char *title, int count);
protected:
    static unsigned const nUnrolled = 10;
    static const unsigned uSecPerSec = 1000000;
    static unsigned const nIterations = 10000;

    const int maxConverters;
    PerfConverter **converters;
    int nConverters;
    int maxPrecision;
    bool verbose;

    void measure ( double srcD, float srcF, int prec );

private:
    Perf ( const Perf & );
    Perf & operator = ( Perf & );
};

Perf :: Perf ( int maxConverters_ ) :
    maxConverters ( maxConverters_ ),
    converters ( new PerfConverter * [ maxConverters_ ] ),
    nConverters ( 0 ),
    maxPrecision ( 0 )
{
}

Perf :: ~Perf ()
{
    for ( int j = 0; j < nConverters; j++ )
        delete converters[ j ];

    delete [] converters;
}

void Perf :: addConverter(PerfConverter *c)
{
    if ( nConverters >= maxConverters )
        throw std :: runtime_error ( "Too many converters" );

    converters[ nConverters++ ] = c;

    int prec = c->maxPrecision();
    if ( prec > maxPrecision )
        maxPrecision = prec;
}

void Perf :: execute (const int count, bool verbose_)
{
    verbose = verbose_;

    for ( int i = 0; i < count; i++ ) {
        double srcDbl = rand ();
        srcDbl /= (RAND_MAX + 1.0);
        srcDbl *= 20.0;
        srcDbl -= 10.0;
        float srcFlt = (float) srcDbl;

        for ( int prec = 0; prec <= maxPrecision; prec++ ) {
            measure (srcFlt, srcDbl, prec);
        }
    }
    report ( "Small numbers, -10..+10", count );

    for ( int i = 0; i < count; i++ ) {
        double mVal = rand ();
        mVal /= (RAND_MAX + 1.0);
        double eVal = rand ();
        eVal /= (RAND_MAX + 1.0);

        double dVal = eVal;
        dVal *= FLT_MAX_EXP - FLT_MIN_EXP;
        dVal += FLT_MIN_EXP;
        int dEVal = static_cast < int > ( dVal + 0.5 );
        double srcDbl = ldexp ( mVal, dEVal );
        float srcFlt = (float) srcDbl;

        for ( int prec = 0; prec <= maxPrecision; prec++ ) {
            measure (srcFlt, srcDbl, prec);
        }
    }
    report ( "Random mantissa+exponent", count );
}

void Perf :: report (const char *title, const int count)
{
    printf( "\n%s\n\nprec\t", title );
    for ( int j = 0; j < nConverters; j++ )
        printf( "%-16s ", converters[j]->name() );

    for (int prec = 0; prec <= maxPrecision; prec++ ) {
        printf( "\n %2d\t", prec );
        for (int j = 0; j < nConverters; j++ ) {
            PerfConverter *c = converters[j];
            if (prec > c->maxPrecision())
                printf( "%11s      ", "-" );
            else {
                printf( "%11.9f sec  ", c->total(prec) / count );
            }
        }
    }
    printf( "\n\n" );
}

void Perf :: measure (double srcD, float srcF, int prec)
{
    char buf[40];

    for ( int j = 0; j < nConverters; j++ ) {
        PerfConverter *c = converters[j];

        if (prec > c->maxPrecision())
            continue;

        std::memset(buf, 0, sizeof(buf));

        epicsTime beg = epicsTime :: getCurrent ();
        for ( unsigned i = 0; i < nIterations; i++ ) {
            c->target (srcD, srcF, buf, sizeof(buf) - 1, prec);
        }
        epicsTime end = epicsTime :: getCurrent ();

        double elapsed = end - beg;
        elapsed /= nIterations * nUnrolled;
        c->add( prec, elapsed );

        if (verbose)
            printf ( "%17s: %11.9f sec, prec=%2i '%s'\n",
                c->name (), elapsed, prec, buf );
    }
}


// Conversions to be measured

class PerfCvtFastFloat : public PerfConverter {
    static const int digits = 12;
public:
    PerfCvtFastFloat ()
    {
        for (int i = 0; i <= digits; i++)
            measured[i] = 0;    // Some targets seem to need this
    }
    int maxPrecision (void) const { return digits; }
    const char *name (void) const { return "cvtFloatToString"; }
    void target (double srcD, float srcF, char *dst, size_t len, int prec) const
    {
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );

        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
        cvtFloatToString ( srcF, dst, prec );
    }
    void add (int prec, double elapsed) { measured[prec] += elapsed; }
    double total (int prec) {
        double total = measured[prec];
        measured[prec] = 0;
        return total;
    }
private:
    double measured[digits+1];
};


class PerfCvtFastDouble : public PerfConverter {
    static const int digits = 17;
public:
    PerfCvtFastDouble ()
    {
        for (int i = 0; i <= digits; i++)
            measured[i] = 0;    // Some targets seem to need this
    }
    int maxPrecision (void) const { return digits; }
    const char *name (void) const { return "cvtDoubleToString"; }
    void target (double srcD, float srcF, char *dst, size_t len, int prec) const
    {
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );

        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
        cvtDoubleToString ( srcD, dst, prec );
    }
    void add(int prec, double elapsed) { measured[prec] += elapsed; }
    double total (int prec) {
        double total = measured[prec];
        measured[prec] = 0;
        return total;
    }
private:
    double measured[digits+1];
};


class PerfSNPrintf : public PerfConverter {
    static const int digits = 17;
public:
    PerfSNPrintf ()
    {
        for (int i = 0; i <= digits; i++)
            measured[i] = 0;    // Some targets seem to need this
    }
    int maxPrecision (void) const { return digits; }
    const char *name (void) const { return "epicsSnprintf"; }
    void target (double srcD, float srcF, char *dst, size_t len, int prec) const
    {
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );

        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
        epicsSnprintf ( dst, len, "%.*g", prec, srcD );
    }
    void add(int prec, double elapsed) { measured[prec] += elapsed; }
    double total (int prec) {
        double total = measured[prec];
        measured[prec] = 0;
        return total;
    }
private:
    double measured[digits+1];
};


// This is a quick-and-dirty std::streambuf converter that writes directly
// into the output buffer. Performance is slower than epicsSnprintf().

struct membuf: public std::streambuf {
    membuf(char *array, size_t size) {
        this->setp(array, array + size - 1);
    }
};

struct omemstream: virtual membuf, std::ostream {
    omemstream(char *array, size_t size):
        membuf(array, size),
        std::ostream(this) {
    }
};

static void ossConvertD(char *dst, size_t len, int prec, double src) {
    omemstream oss(dst, len);
    oss.precision(prec);
    oss << src << ends;
}

class PerfStreamBuf : public PerfConverter {
    static const int digits = 17;
public:
    PerfStreamBuf ()
    {
        for (int i = 0; i <= digits; i++)
            measured[i] = 0;    // Some targets seem to need this
    }
    int maxPrecision (void) const { return digits; }
    const char *name (void) const { return "std::streambuf"; }
    void target (double srcD, float srcF, char *dst, size_t len, int prec) const
    {
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );

        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
        ossConvertD ( dst, len, prec, srcD );
    }
    void add(int prec, double elapsed) { measured[prec] += elapsed; }
    double total (int prec) {
        double total = measured[prec];
        measured[prec] = 0;
        return total;
    }
private:
    double measured[digits+1];
};


MAIN(cvtFastPerform)
{
    Perf t(4);

    t.addConverter( new PerfCvtFastFloat );
    t.addConverter( new PerfCvtFastDouble );
    t.addConverter( new PerfSNPrintf );
    t.addConverter( new PerfStreamBuf );

    // The parameter to execute() below are:
    //    count = number of different random numbers to measure
    //    verbose = whether to display individual measurements

#ifdef vxWorks
    t.execute (3, true);    // Slow...
#else
    t.execute (5, false);
#endif

    return 0;
}
