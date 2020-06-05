/*************************************************************************\
* Copyright (c) 2019 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* This test includes all public headers from libCom and database modules
 * to ensure they are all syntaxtically correct in both C and C++
 */

#include <aaiRecord.h>
#include <aaoRecord.h>
#include <addrList.h>
#include <adjustment.h>
#include <aiRecord.h>
#include <alarm.h>
#include <alarmString.h>
#include <aoRecord.h>
#include <asCa.h>
#include <asDbLib.h>
#include <asIocRegister.h>
#include <asLib.h>
#include <asTrapWrite.h>
#include <aSubRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <bucketLib.h>
#include <calcoutRecord.h>
#include <calcRecord.h>
#include <callback.h>
#include <cantProceed.h>
#include <caProto.h>
#include <caVersion.h>
#include <chfPlugin.h>
#include <compilerDependencies.h>
#include <compressRecord.h>
#include <cvtFast.h>
#include <cvtTable.h>
#include <databaseVersion.h>
#include <dbAccessDefs.h>
#include <dbAccess.h>
#include <dbAddr.h>
#include <dbBase.h>
#include <dbBkpt.h>
#include <dbCa.h>
#include <dbCaTest.h>
#include <dbChannel.h>
#include <dbCommon.h>
#include <dbConstLink.h>
#include <dbConvertFast.h>
#include <dbConvert.h>
#include <dbConvertJSON.h>
#include <dbDbLink.h>
#include <dbDefs.h>
#include <dbEvent.h>
#include <dbExtractArray.h>
#include <db_field_log.h>
#include <dbFldTypes.h>
#include <dbIocRegister.h>
#include <dbJLink.h>
#include <dbLink.h>
#include <dbLoadTemplate.h>
#include <dbLock.h>
#include <dbmf.h>
#include <dbNotify.h>
#include <dbScan.h>
#include <dbServer.h>
#include <dbState.h>
#include <dbStaticIocRegister.h>
#include <dbStaticLib.h>
#include <dbStaticPvt.h>
#include <db_test.h>
#include <dbTest.h>
#include <dbtoolsIocRegister.h>
#include <dbUnitTest.h>
#include <devLib.h>
#include <devLibVME.h>
#include <devLibVMEImpl.h>
#include <devSup.h>
#include <dfanoutRecord.h>
#include <drvSup.h>
#include <ellLib.h>
#include <envDefs.h>
#ifdef __cplusplus
#  include <epicsAlgorithm.h>
#endif
#include <epicsAssert.h>
#include <epicsAtomic.h>
#include <epicsConvert.h>
#include <epicsEndian.h>
#include <epicsEvent.h>
#include <epicsExit.h>
#include <epicsFindSymbol.h>
#include <epicsGeneralTime.h>
#ifdef __cplusplus
#  include <epicsGuard.h>
#endif
#include <epicsInterrupt.h>
#include <epicsMemFs.h>
#include <epicsMessageQueue.h>
#include <epicsMutex.h>
#include <epicsPrint.h>
#include <epicsReadline.h>
#include <epicsRelease.h>
#include <epicsRingBytes.h>
#include <epicsRingPointer.h>
#ifdef __rtems__
#  include <epicsRtemsInitHooks.h>
#endif
#include <epicsSignal.h>
#ifdef __cplusplus
#  include <epicsSingleton.h>
#endif
#include <epicsSpin.h>
#include <epicsStackTrace.h>
#include <epicsStdio.h>
#include <epicsStdioRedirect.h>
#include <epicsStdlib.h>
#include <epicsString.h>
#include <epicsTempFile.h>
#include <epicsThread.h>
#include <epicsThreadPool.h>
#include <epicsTime.h>
#include <epicsTimer.h>
#include <epicsTypes.h>
#include <epicsUnitTest.h>
#include <epicsVersion.h>
#include <errlog.h>
#include <errMdef.h>
#include <errSymTbl.h>
#include <eventRecord.h>
#include <fanoutRecord.h>
#ifdef __cplusplus
#  include <fdManager.h>
#endif
#include <fdmgr.h>
#include <freeList.h>
#include <generalTimeSup.h>
#include <gpHash.h>
#include <histogramRecord.h>
#include <initHooks.h>
#include <int64inRecord.h>
#include <int64outRecord.h>
#include <iocInit.h>
#include <iocLog.h>
#include <iocsh.h>
#include <iocshRegisterCommon.h>
#ifdef __cplusplus
#  include <ipAddrToAsciiAsynchronous.h>
#endif
#include <libComRegister.h>
#include <libComVersion.h>
#include <link.h>
#ifdef __cplusplus
#  include <locationException.h>
#endif
#include <logClient.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <lsiRecord.h>
#include <lsoRecord.h>
#include <macLib.h>
#include <mbbiDirectRecord.h>
#include <mbbiRecord.h>
#include <mbboDirectRecord.h>
#include <mbboRecord.h>
#include <menuAlarmSevr.h>
#include <menuAlarmStat.h>
#include <menuConvert.h>
#include <menuFtype.h>
#include <menuIvoa.h>
#include <menuOmsl.h>
#include <menuPini.h>
#include <menuPost.h>
#include <menuPriority.h>
#include <menuScan.h>
#include <menuSimm.h>
#include <menuYesNo.h>
#include <miscIocRegister.h>
#include <osiClockTime.h>
#include <osiPoolStatus.h>
#include <osiProcess.h>
#include <osiSock.h>
#ifdef __cplusplus
#  include <osiWireFormat.h>
#endif
#include <epicsGetopt.h>
#include <epicsMath.h>
#include <epicsMMIO.h>
#include <osiFileName.h>
#include <osiUnistd.h>
#include <permissiveRecord.h>
#include <postfix.h>
#include <printfRecord.h>
#include <recGbl.h>
#include <recSup.h>
#include <registryCommon.h>
#include <registryDeviceSupport.h>
#include <registryDriverSupport.h>
#include <registryFunction.h>
#include <registry.h>
#include <registryIocRegister.h>
#include <registryJLinks.h>
#include <registryRecordType.h>
#ifdef __cplusplus
#  include <resourceLib.h>
#endif
#include <rsrv.h>
#include <selRecord.h>
#include <seqRecord.h>
#include <shareLib.h>
#include <special.h>
#include <stateRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <subArrayRecord.h>
#include <subRecord.h>
#include <taskwd.h>
#include <testMain.h>
#ifdef __cplusplus
#  include <tsDLList.h>
#  include <tsFreeList.h>
#  include <tsMinMax.h>
#  include <tsSLList.h>
#endif
#include <valgrind/valgrind.h>
#include <waveformRecord.h>
#include <yajl_alloc.h>
#include <yajl_common.h>
#include <yajl_gen.h>
#include <yajl_parse.h>

/* must be last */
#include <epicsExport.h>

MAIN(dbHeaderTest)
{
    testPlan(1);
    testPass("Compiled successfully");
    return testDone();
}
