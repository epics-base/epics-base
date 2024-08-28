TOP = ..
include $(TOP)/configure/CONFIG

EXPAND = Doxyfile@
EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS
EXPAND_ME += RTD_SRC

DOXYGEN ?= doxygen

libcom_HEADERS += adjustment
libcom_HEADERS += alarm
libcom_HEADERS += alarmString
libcom_HEADERS += asTrapWrite
libcom_HEADERS += bucketLib
libcom_HEADERS += cantProceed
libcom_HEADERS += compilerDependencies
libcom_HEADERS += cvtFast
libcom_HEADERS += dbmf
libcom_HEADERS += devLibVME
libcom_HEADERS += devLibVMEImpl
libcom_HEADERS += ellLib
libcom_HEADERS += envDefs
libcom_HEADERS += epicsAlgorithm
libcom_HEADERS += epicsAssert
libcom_HEADERS += epicsAtomic
libcom_HEADERS += epicsEvent
libcom_HEADERS += epicsExit
libcom_HEADERS += epicsExport
libcom_HEADERS += epicsGeneralTime
libcom_HEADERS += epicsGuard
libcom_HEADERS += epicsMessageQueue
libcom_HEADERS += epicsMutex
libcom_HEADERS += epicsReadline
libcom_HEADERS += epicsRingBytes
libcom_HEADERS += epicsRingPointer
libcom_HEADERS += epicsSignal
libcom_HEADERS += epicsSpin
libcom_HEADERS += epicsStackTrace
libcom_HEADERS += epicsStdio
libcom_HEADERS += epicsStdlib
libcom_HEADERS += epicsString
libcom_HEADERS += epicsTempFile
libcom_HEADERS += epicsThread
libcom_HEADERS += epicsTime
libcom_HEADERS += epicsTypes
libcom_HEADERS += epicsUnitTest
libcom_HEADERS += errlog
libcom_HEADERS += freeList
libcom_HEADERS += iocsh
libcom_HEADERS += ipAddrToAsciiAsynchronous
libcom_HEADERS += logClient
libcom_HEADERS += macLib
libcom_HEADERS += osiPoolStatus
libcom_HEADERS += osiProcess
libcom_HEADERS += osiSock
libcom_HEADERS += postfix
libcom_HEADERS += shareLib
libcom_HEADERS += testMain
libcom_HEADERS += yajl_alloc
libcom_HEADERS += yajl_common
libcom_HEADERS += yajl_gen
libcom_HEADERS += yajl_parse

database_HEADERS += chfPlugin
database_HEADERS += dbChannel
database_HEADERS += dbCommon
database_HEADERS += dbDefs
database_HEADERS += dbExtractArray
database_HEADERS += dbLink
database_HEADERS += dbServer
database_HEADERS += dbState
database_HEADERS += dbStaticLib
database_HEADERS += dbUnitTest
database_HEADERS += devLib
database_HEADERS += devSup
database_HEADERS += drvSup
database_HEADERS += initHooks

record_HEADERS += aaiRecord
record_HEADERS += aaoRecord
record_HEADERS += aiRecord
record_HEADERS += aoRecord
record_HEADERS += aSubRecord
record_HEADERS += biRecord
record_HEADERS += boRecord
record_HEADERS += calcRecord
record_HEADERS += calcoutRecord
record_HEADERS += compressRecord
record_HEADERS += dfanoutRecord
record_HEADERS += eventRecord
record_HEADERS += fanoutRecord
record_HEADERS += histogramRecord
record_HEADERS += int64inRecord
record_HEADERS += int64outRecord
record_HEADERS += longinRecord
record_HEADERS += longoutRecord
record_HEADERS += lsiRecord
record_HEADERS += lsoRecord
record_HEADERS += mbbiRecord
record_HEADERS += mbbiDirectRecord
record_HEADERS += mbboRecord
record_HEADERS += mbboDirectRecord
record_HEADERS += permissiveRecord
record_HEADERS += printfRecord
record_HEADERS += selRecord
record_HEADERS += seqRecord
record_HEADERS += stateRecord
record_HEADERS += stringinRecord
record_HEADERS += stringoutRecord
record_HEADERS += subRecord
record_HEADERS += subArrayRecord
record_HEADERS += waveformRecord

menu_HEADERS += menuAlarmSevr
menu_HEADERS += menuAlarmStat
menu_HEADERS += menuConvert
menu_HEADERS += menuFtype
menu_HEADERS += menuIvoa
menu_HEADERS += menuOmsl
menu_HEADERS += menuPini
menu_HEADERS += menuPost
menu_HEADERS += menuPriority
menu_HEADERS += menuScan
menu_HEADERS += menuSimm
menu_HEADERS += menuYesNo

HEADER_TYPES = libcom database record menu

HEADER_MD_FILES = $(foreach t, $(HEADER_TYPES), \
    $(addsuffix _h.md, $($t_HEADERS)))
API_RST_FILES = $(addsuffix -api.rst, $(HEADER_TYPES))

RTD_SRC = $(COMMON_DIR)/rtd-src

DOCS += README.md
DOCS += RELEASE_NOTES.md

include $(TOP)/configure/RULES

$(HEADER_MD_FILES): %_h.md: ../HEADER_h.md
	$(EXPAND_TOOL) -t $(INSTALL_LOCATION) -DHEADER=$* $< $@

$(API_RST_FILES): %-api.rst: ../%-API.rst
	@$(RM) $@
	@$(ECHO) Creating $@
	@$(CP) $< $@
	@$(foreach h, $($*_HEADERS), \
	    echo "   $h_h.rst" >> $@;)

ifndef T_A
doxygen rtd sphinx: inc
	$(MAKE) -C O.$(EPICS_HOST_ARCH) $@
else

doxygen: Doxyfile
	@$(MKDIR) $(RTD_SRC)
	$(DOXYGEN)

rtd: doxygen $(API_RST_FILES) $(HEADER_MD_FILES)
	rsync -av $(INSTALL_DOC)/ $(RTD_SRC)/
	rsync -av $(HEADER_MD_FILES) $(RTD_SRC)/
	rsync -av $(API_RST_FILES) $(RTD_SRC)/
	rsync -av ../index.rst ../conf.py $(RTD_SRC)/

sphinx: rtd
	cd $(COMMON_DIR); $(PYTHON) -m sphinx rtd-src readthedocs
	rsync -av $(COMMON_DIR)/readthedocs $(INSTALL_HTML)/
endif

.PHONY: doxygen rtd sphinx
