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
INC += taskwd.h
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
USER_DBDFLAGS += -I ..

DBDINSTALL +=  dbCommonRecord.dbd 
DBDINSTALL +=  menuGlobal.dbd

SRCS += dbLock.c
SRCS += dbAccess.c
SRCS += dbBkpt.c
SRCS += dbConvert.c
SRCS += dbFastLinkConv.c
SRCS += dbNotify.c 
SRCS += iocInit.c
SRCS += dbScan.c
SRCS += dbEvent.c
SRCS += dbTest.c
SRCS += db_access.c
SRCS += db_test.c
SRCS += recGbl.c
SRCS += callback.c
SRCS += taskwd.c
SRCS += dbCa.c 
SRCS += initHooks.c 
SRCS += dbcar.c 


LIBRARY_IOC = db
DLL_LIBS = dbStaticIoc Com

include $(TOP)/configure/RULES

dbCommon.h: ../dbCommonRecord.dbd ../dbCommon.dbd
	$(RM) $@
	$(EPICS_BASE_HOST_BIN)/dbToRecordtypeH$(HOSTEXE) \
		$(USER_DBDFLAGS) $<
