// $Id$
//	Author: Andrew Johnson & Jeff Hill
//	Date:   December 2000

#ifndef __EPICS_EXCEPT_H__
#define __EPICS_EXCEPT_H__

#define epicsThrowHere(exc, msg) \
    throw locationException<exc>(msg, __FILE__, __LINE__)

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
    //     I've used const char* here to avoid having to include <string>
};


/* Examples:
 *	if (status) epicsThrowHere(std::logic_error, "failed!");
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
