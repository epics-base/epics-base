/*
 *      $Id$
 *
 *      Author  Jeffrey O. Hill
 *              johill@lanl.gov
 *              505 665 1831
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *
 * History
 * $Log$
 * Revision 1.1.1.1  1996/06/20 00:28:16  jhill
 * ca server installation
 *
 *
 */


#ifndef casClientIL_h
#define casClientIL_h

#include <caServerIIL.h> // caServerI inline func 
#include <casCtxIL.h> // caServerI inline func 

//
// find the channel associated with a resource id
//
inline casChannelI *casClient::resIdToChannel(const caResId &id)
{
        casChannelI *pChan;
 
        pChan = this->ctx.getServer()->resIdToChannel(id);
        this->ctx.setChannel(pChan);
        if (pChan) {
                this->ctx.setPV(&pChan->getPVI());
        }
        return pChan;
}

//
// casClient::getDebugLevel()
//
//inline unsigned casClient::getDebugLevel() const
//{
//	return this->ctx.getServer()->getDebugLevel();
//}

#endif // casClientIL_h

