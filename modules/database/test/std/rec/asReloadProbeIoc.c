/*************************************************************************\
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Keep the database and CA DBR type definitions in separate translation units. */
#include "dbAccess.h"

int recTestIoc_registerRecordDeviceDriver(struct dbBase *);

int asReloadProbeLoadDatabase(const char *dbd)
{
    return dbLoadDatabase(dbd, NULL, NULL) ||
           recTestIoc_registerRecordDeviceDriver(pdbbase);
}
