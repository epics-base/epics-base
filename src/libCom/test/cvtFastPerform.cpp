
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
    double _srcDbl;
    float _srcFlt;
    unsigned short _prec;
    static unsigned const _nUnrolled = 10;
    static const unsigned _uSecPerSec = 1000000;
    static unsigned const _nIterations = 10000;
    virtual const char *_name(void) = 0;
    virtual int _maxPrecision(void) = 0;
    virtual void _target () = 0;
    void _measure ();
    Test ( const Test & );
    Test & operator = ( Test & );
};

class TestCvtFastFloat : public Test {
protected:
    void _target ();
    const char *_name (void) { return "cvtFloatToString"; }
    int _maxPrecision(void) { return 12; }
};

class TestCvtFastDouble : public Test {
protected:
    void _target (void);
    const char *_name (void) { return "cvtDoubleToString"; }
    int _maxPrecision(void) { return 17; }
};

class TestSNPrintf : public Test {
protected:
    void _target ();
    const char *_name (void) { return "epicsSnprintf"; }
    int _maxPrecision(void) { return 17; }
};

Test ::
    Test () :
    _srcDbl ( 0.0 ), _prec ( 0 )
{
}

Test :: ~Test () 
{
}

void Test :: execute ()
{
    
    for ( unsigned i = 0; i < 3; i++ ) {
        double mVal = rand ();
        mVal /= (RAND_MAX + 1.0);
        double eVal = rand ();
        eVal /= (RAND_MAX + 1.0);
        double dVal = eVal;
        dVal *= DBL_MAX_EXP - DBL_MIN_EXP;
        dVal += DBL_MIN_EXP;
        int dEVal = static_cast < int > ( dVal + 0.5 );
	float fVal = eVal;
	fVal *= FLT_MAX_EXP - FLT_MIN_EXP;
	fVal += FLT_MIN_EXP;
	int fEVal = static_cast < int > ( fVal + 0.5 );
        _srcDbl = ldexp ( mVal, dEVal );
	_srcFlt = ldexpf ( mVal, fEVal );
        for ( _prec = 0; _prec <= _maxPrecision(); _prec++ ) {
            _measure ();
        }
        _srcDbl = rand ();
        _srcDbl /= (RAND_MAX + 1.0);
        _srcDbl *= 10.0;
        _srcDbl -= 5.0;
	_srcFlt = (float) _srcDbl;
        for ( _prec = 0; _prec <= _maxPrecision(); _prec++ ) {
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
    printf ( "%17s: %12.9f sec, prec=%2i, out=%s\n",
            this->_name (), elapsed, _prec, _pDst );
}
    
void TestCvtFastFloat :: _target ()
{
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );

    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
    cvtFloatToString ( _srcFlt, _pDst, _prec );
}

void TestCvtFastDouble :: _target ()
{
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );

    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
    cvtDoubleToString ( _srcDbl, _pDst, _prec );
}

void TestSNPrintf :: _target ()
{
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
                
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
    epicsSnprintf ( _pDst, sizeof ( _pDst ), "%.*g", 
                static_cast < int > ( _prec ), _srcDbl );
}

MAIN(cvtFastPerform)
{
    TestCvtFastFloat testCvtFastFloat;
    TestCvtFastDouble testCvtFastDouble;
    TestSNPrintf testSNPrintf;
    
    testCvtFastFloat.execute ();
    testCvtFastDouble.execute ();
    testSNPrintf.execute ();

    return 0;
}
