TOP=../..
include $(TOP)/configure/CONFIG

#       includes to install from this sub-project
#
INC += callback.h
INC += dbLock.h
INC += dbAccess.h
INC += dbConvert.h
INC += dbEvent.h
INC += dbScan.h
INC += db_access.h
INC += dbAddr.h
INC += db_field_log.h
INC += initHooks.h
INC += recSup.h
INC += recGbl.h
INC += dbBkpt.h

MENUS += menuAlarmSevr.h
MENUS += menuAlarmStat.h
MENUS += menuArrType.h
MENUS += menuCompress.h
MENUS += menuFtype.h
MENUS += menuIvoa.h
MENUS += menuLinr.h
MENUS += menuOmsl.h
MENUS += menuPriority.h
MENUS += menuScan.h
MENUS += menuYesNo.h

RECTYPES += dbCommon.h

DBDINSTALL +=  dbCommonRecord.dbd 
DBDINSTALL +=  menuGlobal.dbd

LIBSRCS += dbLock.c
LIBSRCS += dbAccess.c
LIBSRCS += dbBkpt.c
LIBSRCS += dbConvert.c
LIBSRCS += dbFastLinkConv.c
LIBSRCS += dbNotify.c 
LIBSRCS += dbScan.c
LIBSRCS += dbEvent.c
LIBSRCS += dbTest.c
LIBSRCS += db_access.c
LIBSRCS += db_test.c
LIBSRCS += recGbl.c
LIBSRCS += callback.c
LIBSRCS += dbCa.c 
LIBSRCS += initHooks.c 
LIBSRCS += dbcar.c 

OBJS_IOC += iocInit

LIBRARY_IOC = dbIoc
DLL_LIBS = dbStaticIoc Com

include $(TOP)/configure/RULES

dbCommon.h: ../dbCommonRecord.dbd ../dbCommon.dbd
	$(RM) $@
	$(EPICS_BASE_HOST_BIN)/dbToRecordtypeH$(HOSTEXE) \
		$(DBDFLAGS) $<
