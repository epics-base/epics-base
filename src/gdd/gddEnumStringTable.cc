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
// gddEnumStringTable.cc
// Author: Jeff Hill
//

#include <new>

#include <string.h>

#define epicsExportSharedSymbols
#include "gddEnumStringTable.h"

gddEnumStringTable::~gddEnumStringTable ()
{
    for ( unsigned i = 0u; i < this->nStringSlots; i++ ) {
        delete [] this->pStringTable[i].pString;
    }
    delete [] this->pStringTable;
}

bool gddEnumStringTable::expand ( unsigned nStringsRequired )
{
    stringEntry * pNextTable = new ( std::nothrow ) stringEntry [nStringsRequired];
    if ( pNextTable ) {
        for ( unsigned i = 0u; i < this->nStringSlots; i++ ) {
            pNextTable[i] = this->pStringTable[i];
        }
        for ( unsigned j = this->nStringSlots; j < nStringsRequired; j++ ) {
            pNextTable[j].pString = 0;
            pNextTable[j].length = 0;
        }
        delete [] this->pStringTable;
        this->pStringTable = pNextTable;
        this->nStringSlots = nStringsRequired;
        return true;
    }
    else {
        return false;
    }
}

void gddEnumStringTable::reserve ( unsigned nStringsIn )
{
    if ( nStringsIn > this->nStringSlots ) {
        this->expand ( nStringsIn );
    }
}

void gddEnumStringTable::clear ()
{
    for ( unsigned i = 0u; i < this->nStringSlots; i++ ) {
        delete [] this->pStringTable[i].pString;
    }
    delete [] this->pStringTable;
    this->pStringTable = 0;
    this->nStringSlots = 0;
    this->nStrings = 0;
}

bool gddEnumStringTable::setString ( unsigned index, const char *pString )
{
    if ( index >= this->nStringSlots ) {
        unsigned nStringsNext;
        if ( this->nStringSlots < 16 ) {
            nStringsNext = 16;
        }
        else {
            nStringsNext = this->nStringSlots;
        }
        while ( index >= nStringsNext ) {
            nStringsNext += nStringsNext;
        }
        if ( ! this->expand ( nStringsNext ) ) {
            return false;
        }
    }
    unsigned nChar = strlen ( pString );
    char *pNewString = new ( std::nothrow ) char [ nChar + 1 ];
    if ( ! pNewString ) {
        return false;
    }
    delete [] this->pStringTable[index].pString;
    this->pStringTable[index].pString = pNewString;
    strcpy ( this->pStringTable[index].pString, pString );
    this->pStringTable[index].length = nChar;
    if ( this->nStrings <= index ) {
        this->nStrings = index + 1;
    }
    return true;
}

void gddEnumStringTable::getString ( unsigned index, char *pBuf, unsigned size ) const
{
    if ( index < this->nStrings && size ) {
        if ( this->pStringTable[index].pString ) {
            strncpy ( pBuf, this->pStringTable[index].pString, size );
            pBuf[ size - 1 ] = '\0';
        }
        else {
            pBuf[ 0 ] = '\0';
        }
    }
    else {
        pBuf[ 0 ] = '\0';
    }
}

const char * gddEnumStringTable::getString ( unsigned index ) const
{
    if ( index < this->nStrings ) {
        if ( this->pStringTable[index].pString ) {
            return this->pStringTable[index].pString;
        }
        else {
            return "";
        }
    }
    else {
        return "";
    }
}

unsigned gddEnumStringTable::getStringLength ( unsigned index ) const
{
    if ( index < this->nStrings ) {
        return this->pStringTable[index].length;
    }
    else {
        return 0;
    }
}

bool gddEnumStringTable::getIndex ( const char * pString, unsigned & indexOut ) const
{
    for ( unsigned index = 0u; index < this->nStrings; index++ ) {
        if ( ! strcmp ( pString, this->pStringTable[index].pString ) ) {
            indexOut = index;
            return true;
        }
    }
    return false;
}

