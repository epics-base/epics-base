
TOP=../..

include $(TOP)/configure/CONFIG

#       includes to install from this sub-project
#
#INC += drvSup.h moved to include dir
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
#INC += devSup.h moved to include dir
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

USR_CFLAGS += -ansi

dbLib_SRCS = \
	dbLock.c\
	dbAccess.c\
	dbBkpt.c\
	dbConvert.c\
	dbFastLinkConv.c\
	dbNotify.c \
	iocInit.c\
	dbScan.c\
	dbEvent.c\
	dbTest.c\
	db_access.c\
	db_test.c\
	recGbl.c\
	callback.c\
	taskwd.c\
	dbCa.c \
        initHooks.c \
	dbcar.c 


LIBRARY_vxWorks = dbLib

include $(TOP)/configure/RULES_BUILD

dbCommon.h: ../dbCommonRecord.dbd ../dbCommon.dbd
	$(RM) $@
	$(EPICS_BASE)/bin/$(HOST_ARCH)/dbToRecordtypeH$(EXE) \
		$(USER_DBDFLAGS) $<
