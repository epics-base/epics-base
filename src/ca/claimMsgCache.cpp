
/*  $Id$
 *
 *                    L O S  A L A M O S
 *              Los Alamos National Laboratory
 *               Los Alamos, New Mexico 87545
 *
 *  Copyright, 1986, The Regents of the University of California.
 *
 *  Author: Jeff Hill
 */

#include "iocinf.h"

bool claimMsgCache::set ( nciu & chan )
{
    this->clientId = chan.id;
    this->serverId = chan.sid;
    if ( this->v44 ) {
        unsigned len = strlen ( chan.pNameStr ) + 1u;
        if ( this->bufLen < len ) {
            unsigned newBufLen = 2 * len;
            char *pNewStr = new char [ newBufLen ];
            if ( pNewStr ) {
                delete [] this->pStr;
                this->pStr = pNewStr;
                this->bufLen = newBufLen;
            }
            else {
                return false;
            }
        }
        strcpy ( this->pStr, chan.pNameStr );
        this->currentStrLen = len;
    }
    else {
        this->currentStrLen = 0u;
    }
    return true;
}

