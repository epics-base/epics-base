// $Id$
//	Author: Andrew Johnson
//	Date:   December 2000

// Uncomment the following to check non-exception code on an
// exception target - NB, only runs one test...
//#define noExceptionsFromCXX

#include "epicsExcept.h"
#include "epicsCppStd.h"
#include <exception>
#include <stdexcept>
#include <iostream.h>

#ifdef vxWorks
  #define MAIN epicsExceptTest
#else
  #define MAIN main
#endif

int MAIN(int /*argc*/, char* /*argv[]*/) {
    bool caught = false;
    
    #define EXCEPTION_MSG "test string"
    try {
	epicsThrow(STD_ logic_error, EXCEPTION_MSG);
    }
    catch (STD_ logic_error& it) {
	if (strcmp(it.what(), EXCEPTION_MSG))
	    cout << "what() returned \"" << it.what()
		 << "\", expected \"" << EXCEPTION_MSG << "\"!\n";
	caught = true;
    }
    catch (...) {
	cout << "exception not caught by own class!\n";
	caught = true;
    }
    if (!caught)
	cout << "exception not caught!\n";
    
    caught = false;
    try {
	epicsThrow(STD_ domain_error, EXCEPTION_MSG);
    }
    catch (STD_ logic_error& /* dummy */) {
	caught = true;
    }
    catch (STD_ exception& /* dummy */) {
	cout << "exception caught by grandparent but not parent class!\n";
	caught = true;
    }
    catch (...) {
	cout << "exception not caught by parent or grand-parent class!\n";
	caught = true;
    }
    if (!caught)
	cout << "exception not caught!\n";
    
    caught = false;
    int throwLine = 0;
    try {
	throwLine = __LINE__; epicsThrowHere(STD_ domain_error, EXCEPTION_MSG);
    }
    catch (STD_ exception& it) {
	sourceLocation* here = dynamic_cast<sourceLocation*>(&it);
	if (here) {
	    if (strcmp(here->fileName(), __FILE__))
		cout << "fileName() returned \"" << here->fileName()
		     << "\", expected \"" << __FILE__ << "\"!\n";
	    if (throwLine != here->lineNumber())
		cout << "lineNumber() returned " << here->lineNumber()
		     << ", expected " << __LINE__ << "!\n";
	} else
	    cout << "dynamic cast failed!\n";
	caught = true;
    }
    if (!caught)
	cout << "exception not caught!\n";
    
    // And finally, demonstrate an uncaught exception
    cout << "Behaviour tests completed\n"
	<< "This is what an uncaught exception does:\n";
    
    epicsThrow(STD_ runtime_error, "bye!");
    
    return 0;	/* stop a warning from g++ */
}
