
//
// $Id$
//
// Author: Jeff Hill
//

#ifndef locationException_h
#define locationException_h

#include <stdio.h>

#include "cantProceed.h"
#include "errlog.h"

template <class T>
class sourceFileLocation : public T {
public:
    sourceFileLocation (const T &parm, const char *fileName, unsigned lineNumber);
    sourceFileLocation ( const sourceFileLocation & );
    sourceFileLocation & operator = ( const sourceFileLocation & );
    const char *fileName () const;
    unsigned lineNumber () const;
private:
    const char *pFileName;
    unsigned lineNumberCopy;
};

template <class T>
inline sourceFileLocation<T>::sourceFileLocation (const T &parm, const char *fileName, unsigned lineNumber) :
    T ( parm ), pFileName ( fileName ) , lineNumberCopy ( lineNumber ) {}

template <class T>
inline sourceFileLocation<T>::sourceFileLocation ( const sourceFileLocation<T> &in ) :
    T ( in ), pFileName ( in.pFileName ), lineNumberCopy ( in.lineNumberCopy )
{
}

template < class T >
inline sourceFileLocation<T> & sourceFileLocation<T>::operator = ( const sourceFileLocation<T> &in )
{
    this->pFileName = in.pFileName;
    this->lineNumberCopy = in.lineNumberCopy;
    return *this;
}

template < class T >
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
inline void throwExceptionWithLocation (const T &parm, const char *pFileName, unsigned lineNo)
{
    throw sourceFileLocation<T> (parm, pFileName, lineNo);
}

#endif // ifdef locationException_h

