
//
// $Id$
//
// Author: Jeff Hill
//

#ifndef locationException_h
#define locationException_h

#include <limits.h>
#include <typeinfo>
#include <iostream>

#include "cantProceed.h"

template <class T>
class sourceFileLocation : public T {
public:
    sourceFileLocation (T &parm, const char *fileName, unsigned lineNumber);
    const char *fileName () const;
    unsigned lineNumber () const;
private:
    const char *pFileName;
    unsigned lineNumberCopy;
};

template <class T>
inline sourceFileLocation<T>::sourceFileLocation (const T &parm, const char *fileName, unsigned lineNumber) :
    T (parm), pFileName (fileName) , lineNumberCopy(lineNumber) {}

template <class T>
inline unsigned sourceFileLocation<T>::lineNumber () const
{
    return this->lineNumberCopy;
}

template <class T>
inline const char * sourceFileLocation<T>::fileName () const
{
    return this->pFileName;
}

#define throwWithLocation(parm) throwExceptionWithLocation (parm, __FILE__, __LINE__);

inline void throwExceptionWithLocation (T &parm, const char *pFileName, unsigned lineNo)
{
#   ifdef noExceptionsFromCXX
        cerr << "C++ exception in file=" << pFileName
            << " at line=" << lineNo
            << " - cant continue";
        cantProceed ("No compiler support for C++ exception");
#   else
        throw sourceFileLocation<T> (parm, pFileName, lineNo);
#   endif
}

#endif // ifdef locationException_h

