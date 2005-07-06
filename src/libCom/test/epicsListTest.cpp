/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
// $Id$
//	Author: Andrew Johnson
//	Date:   December 2000

// Test code for the epics::List class

#include <iostream>
#include <stdio.h>

#include "epicsList.h"

using std::cout;
using std::endl;

#if defined(vxWorks) || defined(__rtems__)
  #define MAIN epicsListTest
  extern "C" int MAIN(int /*argc*/, char* /*argv[]*/);
#else
  #define MAIN main
#endif

class fred {
public:
    fred(const char* const _name) : name(_name) {}
    void show () const {cout << name << ' ';}
private:
    const char* const name;
};

static int tests = 0;
static int nak = 0;

static void check(bool res, const char* str, int line) {
    tests++;
    if (!res) {
	printf("Test %d failed, line %d: %s\n", tests, line, str);
	nak++;
    }
}

#define test(expr) check(expr, #expr, __LINE__)

int MAIN(int /*argc*/, char* /*argv[]*/) {
    
    fred* apf[10];
    const char* const names[] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"
    };
    int i;

    for (i=0; i<10; i++)
	apf[i] = new fred(names[i]);

    epicsList<fred*> Fred;
    test(Fred.empty());
    test(Fred.size() == 0);
    test(Fred.begin() == Fred.end());
    
    Fred.push_back(apf[0]);
    test(!Fred.empty());
    test(Fred.size() == 1);
    test(Fred.begin() != Fred.end());
    test(Fred.front() == apf[0]);
    test(Fred.back() == apf[0]);
    
    epicsList<fred*>::iterator Fi = Fred.begin();
    test(Fi != Fred.end());
    test(*Fi == apf[0]);
    
    epicsList<fred*>::const_iterator Fci = Fred.begin();
    test(Fci != Fred.end());
    test(Fci == Fi);
    test(Fi == Fci);
    test(*Fci == apf[0]);
    
    ++Fi;
    test(Fi == Fred.end());
    test(Fci != Fi);
    test(Fi != Fci);
    
    ++Fci;
    test(Fci == Fred.end());
    
    Fci = --Fi;
    test(Fi == Fred.begin());
    test(Fci == Fred.begin());
    
    Fred.push_front(apf[1]);
    test(Fred.size() == 2);
    test(Fred.front() == apf[1]);
    test(Fred.back() == apf[0]);
    test(*Fi == apf[0]);
    
    Fi--;
    test(*Fi == apf[1]);
    
    Fci--;
    test(*Fci == apf[1]);
    
    Fi = Fred.insert(++Fi, apf[2]);
    test(Fred.size() == 3);
    test(Fred.front() == apf[1]);
    test(Fred.back() == apf[0]);
    test(Fi != Fci);
    test(*Fi == apf[2]);
    
    Fred.pop_back();
    test(Fred.size() == 2);
    
    Fred.push_front(apf[0]);
    test(Fred.size() == 3);
    
    Fi = Fred.begin();
    Fred.erase(Fi);
    test(Fred.size() == 2);
    
    Fred.push_front(apf[0]);
    test(Fred.size() == 3);
    
    Fi = Fred.begin();
    for (i=0; Fi != Fred.end(); i++, ++Fi)
	test(*Fi == apf[i]);
    
    Fci = Fred.begin();
    for (i=0; Fci != Fred.end(); i++, ++Fci)
	test(*Fci == apf[i]);
    
    for (i=0; i<10; i++) {
	test(Fred.size() == 3);
	
	epicsList<fred*> Freda;
	test(Freda.empty());
	
	epicsListSwap(Fred, Freda);
	test(Fred.empty());
	test(Freda.size() == 3);
	test(Freda.front() == apf[0]);
	
	for (Fi = Freda.begin(); Fi != Freda.end(); Fi++)
	    Fred.push_back(*Fi);
	test(Fred.size() == 3);
	
	Fi = Freda.begin();
	for (Fci = Fred.begin(); Fci != Fred.end(); ++Fci, ++Fi) {
	    test(Fi != Freda.end());
	    test(*Fci == *Fi);
	}
	// Freda's destructor returns her nodes to global store,
	// from where they are allocated again during the next loop.
    }
    
    Fred.erase(Fred.begin(), --Fred.end());
    test(Fred.size() == 1);
    
    Fred.clear();
    test(Fred.empty());
    
    // Some more complicated stuff, just for the fun of it.
    // Try doing this in tsDLList!
    
    epicsList<epicsList<fred*>*> llf;
    for (i=0; i<10; i++) {
	llf.push_front(new epicsList<fred*>);
	llf.front()->push_front(apf[i]);
    }
    test(llf.size() == 10);
    
    epicsList<epicsList<fred*>*>::iterator llfi;
    for (llfi = llf.begin(); llfi != llf.end(); ++llfi)
	test(llfi->size() == 1);
    
    for (llfi = llf.begin(); llfi != llf.end(); ++llfi) {
	llfi->clear();
	delete *llfi;
    }
    llf.clear();
    
    for (i=0; i<10; i++)
	delete apf[i];
    
    cout << tests << " tests completed, " << nak << " failed." << endl;
    return 0;
}
