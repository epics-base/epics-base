
// Author: Jeff Hill, LANL

#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <typeinfo>

#include "epicsStdio.h"
#include "cvtFast.h"
#include "epicsTime.h"
#include "testMain.h"

using namespace std;

typedef void ( * PTestFunc ) ( const double &, char * pBug, size_t bufSize );

class Test {
public:
    Test ();
    virtual ~Test ();
    void execute ();
protected:
    char _pDst[128];
    double _srcVal;
    unsigned short _prec;
    static unsigned const _nUnrolled = 10;
    static const unsigned _uSecPerSec = 1000000;
    static unsigned const _nIterations = 10000;
    virtual void _target () = 0;
    void _measure ();
    Test ( const Test & );
    Test & operator = ( Test & );
};

class TestCvtFastDouble : public Test {
protected:
    void _target ();
};

class TestSNPrintf : public Test {
protected:
    void _target ();
};

Test ::
    Test () :
    _srcVal ( 0.0 ), _prec ( 0 )
{
}

Test :: ~Test () 
{
}

void Test :: execute ()
{
    static const unsigned lowPrecision = 2;
    static const unsigned highPrecision = DBL_MANT_DIG;
    
    for ( unsigned i = 0; i < 3; i++ ) {
        double mVal = rand ();
        mVal /= (RAND_MAX + 1.0);
        double fEVal = rand ();
        fEVal /= (RAND_MAX + 1.0);
        fEVal *= DBL_MAX_EXP - DBL_MIN_EXP;
        fEVal += DBL_MIN_EXP;
        int eVal = static_cast < int > ( fEVal + 0.5 );
        _srcVal = ldexp ( mVal, eVal );
        for ( _prec = lowPrecision; 
                        _prec <= highPrecision; _prec += 4u ) {
            _measure ();
        }
        _srcVal = rand ();
        _srcVal /= (RAND_MAX + 1.0);
        _srcVal *= 10.0;
        _srcVal -= 5.0;
        for ( _prec = lowPrecision; 
                        _prec <= highPrecision; _prec += 4u ) {
            _measure ();
        }
    }
}

void Test :: _measure ()
{
    epicsTime beg = epicsTime :: getCurrent ();
    for ( unsigned i = 0; i < _nIterations; i++ ) {
        _target ();
    }
    epicsTime end = epicsTime :: getCurrent ();
    double elapsed = end - beg;
    elapsed /= _nIterations * _nUnrolled;
    elapsed *= _uSecPerSec;
    printf ( " %4.4f usec, prec=%i, val=%4.4g, for %s\n",
            elapsed, _prec, _srcVal, typeid ( *this ).name () );
}
    
void TestCvtFastDouble :: _target ()
{
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );

    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
    cvtDoubleToString ( _srcVal, _pDst, _prec );
}

void TestSNPrintf :: _target ()
{
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
                
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcVal );
}

MAIN(cvtFastPerform)
{
    TestCvtFastDouble testCvtFastDouble;
    TestSNPrintf testSNPrintf;
    
    testCvtFastDouble.execute ();
    testSNPrintf.execute ();

    return 0;
}
