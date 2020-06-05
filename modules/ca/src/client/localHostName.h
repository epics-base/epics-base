/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#ifndef INC_localHostName_H
#define INC_localHostName_H

#include <string.h>

#include "epicsSingleton.h"

class localHostName {
public:
    localHostName ();
    ~localHostName ();
    const char * pointer () const;
    unsigned getName ( char * pBuf, unsigned bufLength ) const;
    unsigned nameLength () const;
private:
    bool attachedToSockLib;
    unsigned length;
    char cache [128];
};

extern epicsSingleton < localHostName > localHostNameCache;

inline unsigned localHostName::nameLength () const
{
    return this->length;
}

inline const char * localHostName::pointer () const
{
    return this->cache;
}

#endif // ifndef INC_localHostName_H


