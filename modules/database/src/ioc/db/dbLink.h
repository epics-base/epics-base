/*************************************************************************\
* Copyright (c) 2010 The UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbLink.h
 *
 *  Created on: Mar 21, 2010
 *      Author: Andrew Johnson
 */

#ifndef INC_dbLink_H
#define INC_dbLink_H

#include "link.h"
#include "shareLib.h"
#include "epicsTypes.h"
#include "epicsTime.h"
#include "dbAddr.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbLocker;

/** @file   dbLink.h
 * @brief   Link Support API
 *
 * Link support run-time API, all link types provide an lset which is used by
 * the IOC database to control and operate the link. This file also declares the
 * dbLink routines that IOC, record and device code can call to perform link
 * operations.
 */

/** @brief  callback routine for locked link operations
 *
 * Called by the lset::doLocked method to permit multiple link operations
 * while the link instance is locked.
 *
 * @param   plink   the link
 * @param   priv    context for the callback routine
 */
typedef long (*dbLinkUserCallback)(struct link *plink, void *priv);

/** @brief  Link Support Entry Table
 *
 * This structure provides information about and methods for an individual link
 * type. A pointer to this structure is included in every link's lset field, and
 * is used to perform operations on the link. For JSON links the pointer is
 * obtained by calling pjlink->pif->get_lset() at link initialization time,
 * immediately before calling dbLinkOpen() to activate the link.
 */
typedef struct lset {
    /* Characteristics of the link type */

    /** @brief  link constancy
     *
     * 1 means this is a constant link type whose value doesn't change.
     * The link's value will be obtained using one of the methods loadScalar,
     * loadLS or loadArray.
     */
    const unsigned isConstant:1;

    /** @brief  link volatility
     *
     * 0 means the link is always connected.
     */
    const unsigned isVolatile:1;

    /** @brief  activate link
     *
     * Optional, called whenever a JSON link is initialized or added at runtime.
     *
     * @param   plink   the link
     */
    void (*openLink)(struct link *plink);

    /** @brief  deactivate link
     *
     * Optional, called whenever a link address is changed at runtime, or the
     * IOC is shutting down.
     *
     * @param   locker
     * @param   plink   the link
     */
    void (*removeLink)(struct dbLocker *locker, struct link *plink);

    /* Constant link initialization and data type hinting */

    /** @brief  load constant scalar from link type
     *
     * Usually called during IOC initialization, constant link types must copy a
     * scalar value of the indicated data type to the buffer provided and return
     * 0. A non-constant link type can use this method call as an early hint
     * that subsequent calls to dbGetLink() will request scalar data of the
     * indicated type, although the type might change.
     *
     * @param   plink   the link
     * @param   dbrType data type code
     * @param   pbuffer where to put the value
     * @returns 0 if a value was loaded, non-zero otherwise
     */
    long (*loadScalar)(struct link *plink, short dbrType, void *pbuffer);

    /** @brief  load constant long string from link type
     *
     * Usually called during IOC initialization, constant link types must copy a
     * nil-terminated string up to size characters long to the buffer provided,
     * and write the length of that string to the plen location. A non-constant
     * link type can use this as an early hint that subsequent calls to
     * dbGetLink() will request long string data, although this might change.
     *
     * @param   plink   the link
     * @param   pbuffer where to put the string
     * @param   size    length of pbuffer in chars
     * @param   plen    set to number of chars written
     * @returns status value
     */
    long (*loadLS)(struct link *plink, char *pbuffer, epicsUInt32 size,
            epicsUInt32 *plen);

    /** @brief  load constant array from link type
     *
     * Usually called during IOC initialization, constant link types must copy
     * an array value of the indicated data type to the buffer provided, update
     * the pnRequest location to indicate how many elements were loaded, and
     * return 0. A non-constant link type can use this method call as an early
     * hint that subsequent calls to dbGetLink() will request array data of the
     * indicated type and max size, although the request might change.
     *
     * @param   plink       the link
     * @param   dbrType     data type code
     * @param   pbuffer     where to put the value
     * @param   pnRequest   Max elements on entry, actual on exit
     * @returns 0 if elements were loaded, non-zero otherwise
     */
    long (*loadArray)(struct link *plink, short dbrType, void *pbuffer,
            long *pnRequest);

    /* Metadata */

    /** @brief  return link connection status
     *
     * Return an indication whether this link is connected or not. This routine
     * is polled by the calcout and some external record types. Not required for
     * non-volatile link types, which are by definition always connected.
     *
     * @param   plink   the link
     * @returns 1 if connected, 0 if disconnected
     */
    int (*isConnected)(const struct link *plink);

    /** @brief  get data type of link destination
     *
     * Called on both input and output links by long string support code to
     * decide whether to use DBR_CHAR/DBR_UCHAR or DBR_STRING for a subsequent
     * dbPutLink() or dbGetLink() call. Optional, but if not provided long
     * strings cannot be transported over this link type, and no warning or
     * error will appear to explain why. Not required for constant link types.
     *
     * @param   plink   the link
     * @returns DBF_* type code, or -1 on error/disconnected link
     */
    int (*getDBFtype)(const struct link *plink);

    /* Get data */

    /** @brief  get array size of an input link
     *
     * Called on input links by the compress record type for memory allocation
     * purposes, before using the dbGetLink() routine to fetch the actual
     * array data.
     *
     * @param   plink       the link
     * @param   pnElements  where to put the answer
     * @returns status value
     */
    long (*getElements)(const struct link *plink, long *pnElements);

    /** @brief  get value from an input link
     *
     * Called to fetch data from the link, which must be converted into the
     * given data type and placed in the buffer indicated. The actual number of
     * elements retrieved should be updated in the pnRequest location. If this
     * method returns an error status value, the link's record will be placed
     * into an Invalid severity / Link Alarm state by the dbGetLink() routine
     * that calls this method.
     *
     * @param   plink       the link
     * @param   dbrType     data type code
     * @param   pbuffer     where to put the value
     * @param   pnRequest   max elements on entry, actual on exit
     * @returns status value
     */
    long (*getValue)(struct link *plink, short dbrType, void *pbuffer,
            long *pnRequest);

    /** @brief  get the control range for an output link
     *
     * Called to fetch the control range for the link target, as a pair of
     * double values for the lowest and highest values that the target will
     * accept. This method is not used at all by the IOC or built-in record
     * types, although external record types may require it.
     *
     * @param   plink   the link
     * @param   lo      lowest accepted value
     * @param   hi      highest accepted value
     * @returns status value
     */
    long (*getControlLimits)(const struct link *plink, double *lo, double *hi);

    /** @brief  get the display range from an input link
     *
     * Called to fetch the display range for an input link target, as a pair of
     * double values for the lowest and highest values that the PV expects to
     * return. This method is used by several built-in record types to obtain
     * the display range for their generic input links.
     *
     * @param   plink   the link
     * @param   lo      lowest accepted value
     * @param   hi      highest accepted value
     * @returns status value
     */
    long (*getGraphicLimits)(const struct link *plink, double *lo, double *hi);

    /** @brief  get the alarm limits from an input link
     *
     * Called to fetch the alarm limits for an input link target, as four
     * double values for the warning and alarm levels that the PV checks its
     * value against. This method is used by several built-in record types to
     * obtain the alarm limits for their generic input links.
     *
     * @param   plink   the link
     * @param   lolo    low alarm value
     * @param   lo      low warning value
     * @param   hi      high warning value
     * @param   hihi    high alarm value
     * @returns status value
     */
    long (*getAlarmLimits)(const struct link *plink, double *lolo, double *lo,
            double *hi, double *hihi);

    /** @brief  get the precision from an input link
     *
     * Called to fetch the precision for an input link target. This method is
     * used by several built-in record types to obtain the precision for their
     * generic input links.
     *
     * @param   plink       the link
     * @param   precision   where to put the answer
     * @returns status value
     */
    long (*getPrecision)(const struct link *plink, short *precision);

    /** @brief  get the units string from an input link
     *
     * Called to fetch the units string for an input link target. This method is
     * used by several built-in record types to obtain the units string for
     * their generic input links.
     *
     * @param   plink       the link
     * @param   units       where to put the answer
     * @param   unitsSize   buffer size for the answer
     * @returns status value
     */
    long (*getUnits)(const struct link *plink, char *units, int unitsSize);

    /** @brief  get the alarm condition from an input link
     *
     * Called to fetch the alarm status and severity for an input link target.
     * Either status or severity pointers may be NULL when that value is not
     * needed by the calling code. This method is used by several built-in
     * record types to obtain the alarm condition for their generic input links.
     *
     * @param   plink       the link
     * @param   status      where to put the alarm status (or NULL)
     * @param   severity    where to put the severity (or NULL)
     * @returns status value
     */
    long (*getAlarm)(const struct link *plink, epicsEnum16 *status,
            epicsEnum16 *severity);

    /** @brief  get the time-stamp from an input link
     *
     * Called to fetch the time-stamp for an input link target. This method is
     * used by many built-in device supports to obtain the precision for their
     * generic input links.
     *
     * @param   plink   the link
     * @param   pstamp  where to put the answer
     * @returns status value
     */
    long (*getTimeStamp)(const struct link *plink, epicsTimeStamp *pstamp);

    /* Put data */

    /** @brief  put a value to an output link
     *
     * Called to send nRequest elements of type dbrType found at pbuffer to an
     * output link target.
     *
     * @param   plink       the link
     * @param   dbrType     data type code
     * @param   pbuffer     where to put the value
     * @param   nRequest    number of elements to send
     * @returns status value
     */
    long (*putValue)(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest);

    /** @brief  put a value to an output link with asynchronous completion
     *
     * Called to send nRequest elements of type dbrType found at pbuffer to an
     * output link target. If the return status is zero, the link type will
     * later indicate the put has completed by calling dbLinkAsyncComplete()
     * from a background thread, which will be used to continue the record
     * process operation from where it left off.
     *
     * @param   plink       the link
     * @param   dbrType     data type code
     * @param   pbuffer     where to put the value
     * @param   nRequest    number of elements to send
     * @returns status value
     */
    long (*putAsync)(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest);

    /* Process */

    /** @brief  trigger processing of a forward link
     *
     * Called to trigger processing of the record pointed to by a forward link.
     * This routine is optional, but if not provided no warning message will be
     * shown when called by dbScanFwdLink(). JSON link types that do not support
     * this operation should return NULL from their jlif::alloc_jlink() method
     * if it gets called with a dbfType of DBF_FWDLINK.
     *
     * @param   plink       the link
     */
    void (*scanForward)(struct link *plink);

    /* Atomicity */

    /** @brief  execute a callback routine with link locked
     *
     * Called on an input link when multiple link attributes need to be fetched
     * in an atomic fashion. The link type must call the callback routine and
     * prevent any background I/O from updating any cached link data until that
     * routine returns. This method is used by most input device support to
     * fetch the timestamp along with the value when the record's TSE field is
     * set to epicsTimeEventDeviceTime.
     *
     * @param   plink   the link
     * @param   rtn     routine to execute
     * @returns status value
     */
    long (*doLocked)(struct link *plink, dbLinkUserCallback rtn, void *priv);
} lset;

#define dbGetSevr(link, sevr) \
    dbGetAlarm(link, NULL, sevr)

epicsShareFunc const char * dbLinkFieldName(const struct link *plink);

epicsShareFunc void dbInitLink(struct link *plink, short dbfType);
epicsShareFunc void dbAddLink(struct dbLocker *locker, struct link *plink,
        short dbfType, DBADDR *ptarget);

epicsShareFunc void dbLinkOpen(struct link *plink);
epicsShareFunc void dbRemoveLink(struct dbLocker *locker, struct link *plink);

epicsShareFunc int dbLinkIsDefined(const struct link *plink);  /* 0 or 1 */
epicsShareFunc int dbLinkIsConstant(const struct link *plink); /* 0 or 1 */
epicsShareFunc int dbLinkIsVolatile(const struct link *plink); /* 0 or 1 */

epicsShareFunc long dbLoadLink(struct link *plink, short dbrType,
        void *pbuffer);
epicsShareFunc long dbLoadLinkArray(struct link *, short dbrType, void *pbuffer,
        long *pnRequest);

epicsShareFunc long dbGetNelements(const struct link *plink, long *pnElements);
epicsShareFunc int dbIsLinkConnected(const struct link *plink); /* 0 or 1 */
epicsShareFunc int dbGetLinkDBFtype(const struct link *plink);
epicsShareFunc long dbTryGetLink(struct link *, short dbrType, void *pbuffer,
        long *nRequest);
epicsShareFunc long dbGetLink(struct link *, short dbrType, void *pbuffer,
        long *options, long *nRequest);
epicsShareFunc long dbGetControlLimits(const struct link *plink, double *low,
        double *high);
epicsShareFunc long dbGetGraphicLimits(const struct link *plink, double *low,
        double *high);
epicsShareFunc long dbGetAlarmLimits(const struct link *plink, double *lolo,
        double *low, double *high, double *hihi);
epicsShareFunc long dbGetPrecision(const struct link *plink, short *precision);
epicsShareFunc long dbGetUnits(const struct link *plink, char *units,
        int unitsSize);
epicsShareFunc long dbGetAlarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity);
epicsShareFunc long dbGetTimeStamp(const struct link *plink,
        epicsTimeStamp *pstamp);
epicsShareFunc long dbPutLink(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbLinkAsyncComplete(struct link *plink);
epicsShareFunc long dbPutLinkAsync(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest);
epicsShareFunc void dbScanFwdLink(struct link *plink);

epicsShareFunc long dbLinkDoLocked(struct link *plink, dbLinkUserCallback rtn,
        void *priv);

epicsShareFunc long dbLoadLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 size, epicsUInt32 *plen);
epicsShareFunc long dbGetLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 buffer_size, epicsUInt32 *plen);
epicsShareFunc long dbPutLinkLS(struct link *plink, char *pbuffer,
        epicsUInt32 len);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbLink_H */
