/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//	Author: Andrew Johnson & Jeff Hill
//	Date:   December 2000

#ifndef __EPICS_EXCEPT_H__
#define __EPICS_EXCEPT_H__

#define epicsThrowHere(exc) \
    throw locationException(exc, __FILE__, __LINE__)

class sourceLocation {
public: // Functions
    sourceLocation(const char *fileName, int lineNumber);
//  sourceLocation(const sourceLocation&);		Copy constructable
//  sourceLocation& operator=(const sourceLocation&);	Assignable

    const char *fileName() const;
    int lineNumber() const;

private: // Hide compiler-generated member functions
    sourceLocation();		// default constructor

private: // Data
    const char *file;
    int line;
};

template <class T>
class locationException : public T, public sourceLocation {
public:
    locationException(const T& exc, const char *fileName, int lineNumber);
};


/* Example:
 *	if (status) epicsThrowHere(std::logic_error("operation failed!"));
 *	try { ... } catch(sourceLocation& where) { ... }
 */

// END OF DECLARATIONS

// INLINE FUNCTIONS

// sourceFileLocation
inline sourceLocation::sourceLocation (const char *fileName, int lineNumber) :
    file(fileName), line(lineNumber) {}

inline const char* sourceLocation::fileName () const {
    return this->file;
}

inline int sourceLocation::lineNumber () const {
    return this->line;
}

// locationException<T>
template <class T>
inline locationException<T>::locationException
    (const char *fileName, int lineNumber, const E& exc) :
    T(exc), sourceLocation(fileName, lineNumber) {}


#endif // __EPICS_EXCEPT_H__
