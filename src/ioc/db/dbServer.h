/*************************************************************************\
* Copyright (c) 2014 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * @file dbServer.h
 * @author Andrew Johnson <anj@aps.anl.gov>
 *
 * @brief The IOC's interface to the server layers that publish its PVs.
 *
 * All server layers which publish IOC record data should initialize a
 * dbServer structure and register it with the IOC. The methods that
 * the dbServer interface provides allow the IOC to start, pause and stop
 * the servers together, and to provide status and debugging information
 * to the IOC user/developer through a common set of commands.
 *
 * @todo No API is provided yet for calling stats() methods.
 * Nothing in the IOC calls dbStopServers(), not sure where it should go.
 */

#ifndef INC_dbServer_H
#define INC_dbServer_H

#include <stddef.h>

#include "ellLib.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Server information structure.
 *
 * Every server layer should initialize and register an instance of this
 * structure with the IOC by passing it to the dbRegisterServer() routine.
 *
 * All methods in this struct are optional; use @c NULL if a server is
 * unable to support a particular operation (or if it hasn't been
 * implemented yet).
 */

typedef struct dbServer {
    /** @brief Linked list node; initialize to @c ELLNODE_INIT */
    ELLNODE node;

    /** @brief A short server identifier; printable, with no spaces */
    const char *name;

    /** @brief Print level-dependent status report to stdout.
     *
     * @param level Interest level, specifies how much detail to print.
     */
    void (* report) (unsigned level);

    /** @brief Get number of channels and clients currently connected.
     *
     * @param channels NULL or pointer for returning channel count.
     * @param clients NULL or pointer for returning client count.
     */
    void (* stats) (unsigned *channels, unsigned *clients);

    /** @brief Get identity of client initiating the calling thread.
     *
     * Must fill in the buffer with the client's identity when called from a
     *  thread that belongs to this server layer. For other threads, the
     *  method should do nothing, just return -1.
     * @param pBuf Buffer for client identity string.
     * @param bufSize Number of chars available in pBuf.
     * @return -1 means calling thread is not owned by this server.
     *  0 means the thread was recognized and pBuf has been filled in.
     */
    int (* client) (char *pBuf, size_t bufSize);

    /** @name Control Methods
     * These control methods for the server will be called by routines
     * related to iocInit for all registered servers in turn when the IOC
     * is being initialized, run, paused and stopped respectively.
     *
     * @{
     */

    /** @brief Server init method.
     *
     *  Called for all registered servers by dbInitServers().
     */
    void (* init) (void);

    /** @brief Server run method.
     *
     *  Called for all registered servers by dbRunServers().
     */
    void (* run) (void);

    /** @brief Server pause method.
     *
     *  Called for all registered servers by dbPauseServers().
     */
    void (* pause) (void);

    /** @brief Server stop method.
     *
     *  Called for all registered servers by dbStopServers().
     */
    void (* stop) (void);

    /** @}
     */
} dbServer;


/** @brief Register a server layer with the IOC
 *
 * This should only be called once for each server layer.
 * @param psrv Server information structure for the server
 */
epicsShareFunc int dbRegisterServer(dbServer *psrv);

/** @brief Unregister a server layer
 *
 * This should only be called when the servers are inactive.
 * @param psrv Server information structure for the server
 */
epicsShareFunc int dbUnregisterServer(dbServer *psrv);

/** @brief Print dbServer Reports.
*
 * Calls the report methods of all registered servers.
 * This routine is provided as an IOC Shell command.
 * @param level Interest level, specifies how much detail to print.
 */
epicsShareFunc void dbsr(unsigned level);

/** @brief Query servers for client's identity.
 *
 * This routine is called by code that wants to identify who (or what)
 *  is responsible for the thread which is currently running. Setting
 *  the @c TPRO field of a record is one way to trigger this; the identity
 *  of the calling thread is printed along with the record name whenever
 *  the record is subsequently processed.
 */
epicsShareFunc int dbServerClient(char *pBuf, size_t bufSize);

/** @brief Initialize all registered servers.
 *
 * Calls all dbServer::init() methods.
 */
epicsShareFunc void dbInitServers(void);

/** @brief Run all registered servers.
 *
 * Calls all dbServer::run() methods.
 */
epicsShareFunc void dbRunServers(void);

/** @brief Pause all registered servers.
 *
 * Calls all dbServer::pause() methods.
 */
epicsShareFunc void dbPauseServers(void);

/** @brief Stop all registered servers.
 *
 * Calls all dbServer::stop() methods.
 */
epicsShareFunc void dbStopServers(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbServer_H */
