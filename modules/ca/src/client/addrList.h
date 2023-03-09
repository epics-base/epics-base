/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file addrList.h
 * \brief API for configuring and managing a list of addresses.
 * 
 * The functions provided include configuring the address list, adding addresses to the list,
 * printing the list, and removing duplicate addresses.
 */


#ifndef INC_addrList_H
#define INC_addrList_H

#include "envDefs.h"
#include "osiSock.h"

#include "libCaAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Configures a list of addresses for use with Channel Access.
 * 
 * This function takes a pointer to an ELLLIST structure, a socket, and a port number as arguments.
 */
LIBCA_API void epicsStdCall configureChannelAccessAddressList
    ( struct ELLLIST *pList, SOCKET sock, unsigned short port );
/** \brief Adds an address to the Channel Access address list.
 * 
 * This function takes a pointer to an ELLLIST structure, a pointer to an ENV_PARAM structure,
 * a port number, and a flag that specifies whether to ignore non-default ports as arguments. 
 */
LIBCA_API int epicsStdCall addAddrToChannelAccessAddressList
    ( struct ELLLIST *pList, const ENV_PARAM *pEnv,
    unsigned short port, int ignoreNonDefaultPort );
/** \brief Prints the Channel Access address list to standard output.
 * 
 * This function takes a pointer to an ELLLIST structure as an argument.
 */
LIBCA_API void epicsStdCall printChannelAccessAddressList
    ( const struct ELLLIST *pList );
/** \brief Removes duplicate addresses from a source list and adds them to a destination list.
 * 
 * This function takes a pointer to the destination list, a pointer to the source list, and a flag
 * that specifies whether to print a warning message for each duplicate address encountered as arguments.
 */
LIBCA_API void epicsStdCall removeDuplicateAddresses
    ( struct ELLLIST *pDestList, ELLLIST *pSrcList, int silent);

#ifdef __cplusplus
}
#endif

#endif /* ifndef INC_addrList_H */

