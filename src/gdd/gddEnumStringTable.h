/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
//
// gddEnumStringTable.h
// Author: Jeff Hill
//

#ifndef gddEnumStringTableh
#define gddEnumStringTableh

#include "shareLib.h"

class epicsShareClass gddEnumStringTable {
public:
    gddEnumStringTable ();
    ~gddEnumStringTable ();
    void clear ();
    void reserve ( unsigned nStrings );
    bool setString ( unsigned index, const char *pString );
    void getString ( unsigned index, char *pBuf, unsigned size ) const;
    const char * getString ( unsigned index ) const;
    unsigned getStringLength ( unsigned index ) const;
    bool getIndex ( const char * pString, unsigned & index ) const;
    unsigned numberOfStrings () const;
private:
    unsigned nStrings;
    unsigned nStringSlots;
    struct stringEntry {
        char * pString;
        unsigned length;
    } * pStringTable;
    bool expand ( unsigned nStringsRequired );
};

inline gddEnumStringTable::gddEnumStringTable () :
    nStrings ( 0 ), nStringSlots ( 0 ), pStringTable ( 0 ) {}

inline unsigned gddEnumStringTable::numberOfStrings () const
{
    return this->nStrings;
}

#endif // ifndef gddEnumStringTableh
