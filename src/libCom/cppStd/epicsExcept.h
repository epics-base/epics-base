// $Id$
//	Author: Andrew Johnson & Jeff Hill
//	Date:   December 2000

#ifndef __EPICS_EXCEPT_H__
#define __EPICS_EXCEPT_H__

#include "errlog.h"
#include "cantProceed.h"

#ifdef noExceptionsFromCXX

// In the absence of exception support, the epicsThrow macros map
// to functions that output all the info and call cantProceed()

#define epicsThrow(exc, msg) \
    cantThrow(#exc, msg)
#define epicsThrowHere(exc, msg) \
    cantThrow(#exc, msg, __FILE__, __LINE__)

#else // noExceptionsFromCXX

// With exception support, one base class holds location information
// and the first macro argument names the standard exception class

#define epicsThrow(exc, msg) \
    throw exc(msg)
#define epicsThrowHere(exc, msg) \
    throw locationException<exc>(msg, __FILE__, __LINE__)

#endif // noExceptionsFromCXX

class sourceLocation {
public:
    sourceLocation (const char *fileName, int lineNumber);
    const char *fileName () const;
    int lineNumber () const;

private:
    const char *pFileName;
    int lineNumberCopy;
};

template <class T>
class locationException : public T, public sourceLocation {
public:
    locationException(const char *msg, const char *fileName, int lineNumber);
    // NB: In standard exception classes the msg argument is a string&
    //     I use char* here to avoid having to include <string>
    // If <T> will always be derived from std::exception then
    // locationException could provide a version of what() as well:
    // virtual const char* what() const;
    // NB: Constructing that string is tricky, as it needs storage in
    //     the locationException class to hold it.
};


// cantThrow functions
inline void cantThrow(const char *exc, const char *msg) {
    errlogPrintf("cantThrow: %s exception - %s\n", exc, msg);
    cantProceed("C++ exceptions not supported on this platform");
}

inline void cantThrow(const char *exc, const char *msg,
		      const char *file, int line) {
    errlogPrintf("cantThrow: %s exception at %s:%d - %s\n", exc, file, line, msg);
    cantProceed("C++ exceptions not supported on this platform");
}


/* Examples:
 *	if (x<0) epicsThrow(STD_ invalid_argument, "myfunc: x<0");
 *	if (status) epicsThrowHere(STD_ logic_error, "failed!");
 *	try { ... } catch(sourceLocation& where) { ... }
 */

// END OF DECLARATIONS

// INLINE FUNCTIONS

// sourceFileLocation
inline sourceLocation::sourceLocation (const char *fileName, int lineNumber) :
    pFileName (fileName) , lineNumberCopy(lineNumber) {}

inline const char* sourceLocation::fileName () const {
    return this->pFileName;
}

inline int sourceLocation::lineNumber () const {
    return this->lineNumberCopy;
}

// locationException<T>
template <class T>
inline locationException<T>::locationException
    (const char* msg, const char *fileName, int lineNumber) :
    T(msg), sourceLocation(fileName, lineNumber) {}


#endif // __EPICS_EXCEPT_H__
