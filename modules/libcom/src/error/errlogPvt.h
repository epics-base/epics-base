/*************************************************************************\
* Copyright (c) 2026 European Spallation Source ERIC
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_errlogPvt_H
#define INC_errlogPvt_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * errlogShow is used to display the contents of the error log buffers
 *
 * \param level How much detail to print out
 */
void errlogShow(int level);

#ifdef __cplusplus
}
#endif

#endif /*INC_errlogPvt_H*/
