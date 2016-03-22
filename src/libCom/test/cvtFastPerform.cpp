// Original Author: Jeff Hill, LANL

#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <stdexcept>
#include <typeinfo>

#include "epicsStdio.h"
#include "cvtFast.h"
#include "epicsTime.h"
#include "testMain.h"

using namespace std;

class PerfConverter {
public:
    virtual const char *_name(void) const = 0;
    virtual int _maxPrecision(void) const = 0;
    virtual void _target (double srcD, float srcF, char *pDst, int prec) const = 0;
    virtual void _add(int prec, double elapsed) = 0;
    virtual double _total(int prec) const = 0;
    virtual ~PerfConverter () {};
};

class Perf {
public:
    Perf ( int maxConverters );
    virtual ~Perf ();
    void addConverter( PerfConverter * c );
    void execute ();
    void report (const char *title, int count);
protected:
    static unsigned const _nUnrolled = 10;
    static const unsigned _uSecPerSec = 1000000;
    static unsigned const _nIterations = 10000;

    const int _maxConverters;
    PerfConverter **_converters;
    int _nConverters;
    int _maxPrecision;

    void _measure ( double srcD, float srcF, int prec );

private:
    Perf ( const Perf & );
    Perf & operator = ( Perf & );
};

Perf :: Perf ( int maxConverters ) :
    _maxConverters ( maxConverters ),
    _converters ( new PerfConverter * [ maxConverters ] ),
    _nConverters ( 0 ),
    _maxPrecision ( 0 )
{
}

Perf :: ~Perf ()
{
    for ( int j = 0; j < _nConverters; j++ )
        delete _converters[ j ];

    delete [] _converters;
}

void Perf :: addConverter(PerfConverter *c)
{
    if ( _nConverters >= _maxConverters )
        throw std :: runtime_error ( "Too many converters" );

    _converters[ _nConverters++ ] = c;

    int prec = c->_maxPrecision();
    if ( prec > _maxPrecision )
        _maxPrecision = prec;
}

void Perf :: execute ()
{
    const int count = 10;

    for ( unsigned i = 0; i < count; i++ ) {
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

        for ( int prec = 0; prec <= _maxPrecision; prec++ ) {
            _measure (srcFlt, srcDbl, prec);
        }
    }
    report ( "Random Exponent", count );

    for ( unsigned i = 0; i < count; i++ ) {
        double srcDbl = rand ();
        srcDbl /= (RAND_MAX + 1.0);
        srcDbl *= 10.0;
        srcDbl -= 5.0;
        float srcFlt = (float) srcDbl;

        for ( int prec = 0; prec <= _maxPrecision; prec++ ) {
            _measure (srcFlt, srcDbl, prec);
        }
    }
    report ( "-5..+5", count );
}

void Perf :: report (const char *title, int count)
{
    printf( "\n%s\n\nprec\t", title );
    for ( int j = 0; j < _nConverters; j++ )
        printf( "%-17s  ", _converters[j]->_name() );

    for (int prec = 0; prec < _maxPrecision; prec++ ) {
	printf( "\n %2d\t", prec );
        for (int j = 0; j < _nConverters; j++ ) {
            PerfConverter *c = _converters[j];
	    if (prec > c->_maxPrecision())
	        printf( "%11s        ", "-" );
	    else {
		double total = c->_total(prec);
	        printf( "%11.9f sec    ", c->_total(prec) / count );
		c->_add(prec, -total);   // Reset counter
	    }
	}
    }
    printf( "\n\n" );
}

void Perf :: _measure (double srcD, float srcF, int prec)
{
    char pDst[40];

    for ( int j = 0; j < _nConverters; j++ ) {
        PerfConverter *c = _converters[j];

        if (prec > c->_maxPrecision())
            continue;

        epicsTime beg = epicsTime :: getCurrent ();
        for ( unsigned i = 0; i < _nIterations; i++ ) {
            c->_target (srcD, srcF, pDst, prec);
        }
        epicsTime end = epicsTime :: getCurrent ();

        double elapsed = end - beg;
        elapsed /= _nIterations * _nUnrolled;
	c->_add( prec, elapsed );
        // printf ( "%17s: %11.9f sec, prec=%2i '%s'\n",
        //    c->_name (), elapsed, prec, pDst );
    }
}

class PerfCvtFastFloat : public PerfConverter {
public:
    const char *_name (void) const { return "cvtFloatToString"; }
    int _maxPrecision (void) const { return 12; }
    void _target (double srcD, float srcF, char *pDst, int prec) const
    {
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );

        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
        cvtFloatToString ( srcF, pDst, prec );
    }
    void _add(int prec, double elapsed) { _measured[prec] += elapsed; }
    double _total (int prec) const { return _measured[prec]; }
private:
    double _measured[13];
};


class PerfCvtFastDouble : public PerfConverter {
public:
    const char *_name (void) const { return "cvtDoubleToString"; }
    int _maxPrecision (void) const { return 17; }
    void _target (double srcD, float srcF, char *pDst, int prec) const
    {
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );

        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
        cvtDoubleToString ( srcD, pDst, prec );
    }
    void _add(int prec, double elapsed) { _measured[prec] += elapsed; }
    double _total (int prec) const { return _measured[prec]; }
private:
    double _measured[18];
};


class PerfSNPrintf : public PerfConverter {
public:
    const char *_name (void) const { return "epicsSnprintf"; }
    int _maxPrecision (void) const { return 17; }
    void _target (double srcD, float srcF, char *pDst, int prec) const
    {
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );

        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
        epicsSnprintf ( pDst, 39, "%.*g", prec, srcD );
    }
    void _add(int prec, double elapsed) { _measured[prec] += elapsed; }
    double _total (int prec) const { return _measured[prec]; }
private:
    double _measured[18];
};


MAIN(cvtFastPerform)
{
    Perf t(3);

    t.addConverter( new PerfCvtFastFloat );
    t.addConverter( new PerfCvtFastDouble );
    t.addConverter( new PerfSNPrintf );

    t.execute ();

    return 0;
}
