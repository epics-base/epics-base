
//
// $Id$
//
// Author: Jeff Hill
//

#ifndef locationException_h
#define locationException_h

#include <limits.h>
#include <stdio.h>

#include "cantProceed.h"

template <class T>
class sourceFileLocation : public T {
public:
    sourceFileLocation (const T &parm, const char *fileName, unsigned lineNumber);
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

template <class T>
inline void throwExceptionWithLocation (T &parm, const char *pFileName, unsigned lineNo)
{
#   ifdef noExceptionsFromCXX
        fprintf (stderr, "C++ exception in file=%s" at line=%u, and no compiler support - cant proceed.\n",
            pFileName, lineNo);
        cantProceed ("No compiler support for C++ exception");
#   else
        throw sourceFileLocation<T> (parm, pFileName, lineNo);
#   endif
}

#endif // ifdef locationException_h

