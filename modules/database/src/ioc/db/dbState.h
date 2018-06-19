/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#ifndef INCdbStateH
#define INCdbStateH

#include "shareLib.h"

/** @file dbState.h
 * @brief Generic IOC state facility
 *
 * This library provides a simple global flag facility that can be used to
 * synchronize e.g. plugins with IOC-wide states, that may be derived from
 * events (either soft events or hard events coming from specialized timing
 * and event hardware).
 *
 * A subset of this API is provided as IOC Shell commands to allow
 * command line debugging.
 *
 */

typedef struct dbState *dbStateId;

/** @brief Create db state.
 *
 * Creates a new db state with the specified 'name', returning the new id.
 * If state with that name already exists, the existing state's id is returned.
 *
 * <em>Also provided as an IOC Shell command.</em>
 *
 * @param name Db state name.
 * @return Id of db state, NULL for failure.
 */
epicsShareFunc dbStateId dbStateCreate(const char *name);

/** @brief Find db state.
 *
 * @param name Db state name.
 * @return Id of db state, NULL if not found.
 */
epicsShareFunc dbStateId dbStateFind(const char *name);

/** @brief Set db state to TRUE.
 *
 * <em>Also provided as an IOC Shell command.</em>
 *
 * @param id Db state id.
 */
epicsShareFunc void dbStateSet(dbStateId id);

/** @brief Set db state to FALSE.
 *
 * <em>Also provided as an IOC Shell command.</em>
 *
 * @param id Db state id.
 */
epicsShareFunc void dbStateClear(dbStateId id);

/** @brief Get db state.
 *
 * @param id Db state id.
 * @return Current db state (0|1).
 */
epicsShareFunc int dbStateGet(dbStateId id);

/** @brief Print info about db state.
 *
 * <em>Also provided as an IOC Shell command.</em>
 *
 * @param id Db state id.
 * @param level Interest level.
 */
epicsShareFunc void dbStateShow(dbStateId id, unsigned int level);

/** @brief Print info about all db states.
 *
 * <em>Also provided as an IOC Shell command.</em>
 *
 * @param level Interest level.
 */
epicsShareFunc void dbStateShowAll(unsigned int level);

#endif // INCdbStateH
