
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
class locationAndType : public T {
public:
    locationAndType (T &);
    unsigned lineNumber () const;
    const char *fileName () const;
    const type_info & typeInfo () const;
    void show (unsigned level) const;
private:
    unsigned lineNumberCopy;
    const char *pFileName;
};

template <class T>
inline locationAndType::locationAndType (T &tIn, const char *fileName, unsigned lineNumber) :
    T (tIn) , lineNumberCopy(lineNumber), pFileName (fileName) {}

template <class T>
inline unsigned locationAndType::lineNumber () const
{
    return this->lineNumberCopy;
}

template <class T>
inline const char * locationAndType::typeInfo () const
{
    return typeid (T);
}

template <class T>
inline const char * locationAndType::lineNumber () const
{
    return this->lineNumberCopy;
}

template <class T>
inline const char * locationAndType::show (unsigned level) const
{
    cerr    << "C++ exception=" << typeid(T).name()
            << " in file=" << this->pFileName
            << " at line=" << this->lineNumberCopy;
}

#define throwWithLocation (parm) throwExceptionWithLocation (parm, __FILE__, __LINE__);

template <class T>
inline void throwExceptionWithLocation (T &parm, const char *pFileName, unsigned lineNo)
{
    locationAndType<T> lat (parm, pFileName, lineNo);
#   ifdef noExceptionsFromCXX
        lat.show (UINT_MAX);
        cantProceed ("No compiler support for C++ exception");
#   else            
        throw lat;
#   endif
}

#endif // ifdef locationException_h

