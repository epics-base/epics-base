TOP = ..
include $(TOP)/configure/CONFIG

EXPAND = Doxyfile@
EXPAND_ME += EPICS_VERSION
EXPAND_ME += EPICS_REVISION
EXPAND_ME += EPICS_MODIFICATION
EXPAND_ME += EPICS_PATCH_LEVEL
EXPAND_ME += OS_CLASS CMPLR_CLASS

DOXYGEN = doxygen
ME = documentation/O.$(T_A)/html
GH_FILES = $(ME)/ $(ME)/.nojekyll $(ME)/*.* $(ME)/*/*.*

c_TITLE = "Generic C/C++ API"

c_HEADERS += adjustment
c_HEADERS += alarm
c_HEADERS += alarmString
c_HEADERS += asTrapWrite
c_HEADERS += bucketLib
c_HEADERS += cantProceed
c_HEADERS += chfPlugin
c_HEADERS += compilerDependencies
c_HEADERS += cvtFast
c_HEADERS += dbChannel
c_HEADERS += dbCommon
c_HEADERS += dbDefs
c_HEADERS += dbExtractArray
c_HEADERS += dbLink
c_HEADERS += dbmf
c_HEADERS += dbServer
c_HEADERS += dbState
c_HEADERS += dbStaticLib
c_HEADERS += dbUnitTest
c_HEADERS += devLib
c_HEADERS += devLibVME
c_HEADERS += devLibVMEImpl
c_HEADERS += devSup
c_HEADERS += drvSup
c_HEADERS += ellLib
c_HEADERS += envDefs
c_HEADERS += epicsAlgorithm
c_HEADERS += epicsAssert
c_HEADERS += epicsAtomic
c_HEADERS += epicsEvent
c_HEADERS += epicsExit
c_HEADERS += epicsExport
c_HEADERS += epicsGeneralTime
c_HEADERS += epicsGuard
c_HEADERS += epicsMessageQueue
c_HEADERS += epicsMutex
c_HEADERS += epicsReadline
c_HEADERS += epicsRingBytes
c_HEADERS += epicsRingPointer
c_HEADERS += epicsSignal
c_HEADERS += epicsSpin
c_HEADERS += epicsStackTrace
c_HEADERS += epicsStdio
c_HEADERS += epicsStdlib
c_HEADERS += epicsString
c_HEADERS += epicsTempFile
c_HEADERS += epicsThread
c_HEADERS += epicsTime
c_HEADERS += epicsTypes
c_HEADERS += epicsUnitTest
c_HEADERS += errlog
c_HEADERS += freeList
c_HEADERS += initHooks
c_HEADERS += iocsh
c_HEADERS += ipAddrToAsciiAsynchronous
c_HEADERS += logClient
c_HEADERS += macLib
c_HEADERS += osiPoolStatus
c_HEADERS += osiProcess
c_HEADERS += osiSock
c_HEADERS += postfix
c_HEADERS += shareLib
c_HEADERS += testMain
c_HEADERS += yajl_alloc
c_HEADERS += yajl_common
c_HEADERS += yajl_gen
c_HEADERS += yajl_parse

record_TITLE = "Record C/C++ API"

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

menu_TITLE = "Menu C/C++ API"

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

HEADER_TYPES = c record menu

include $(TOP)/configure/RULES

install:

doxygen/%.rst: generate-doxygen-header.py
	@python3 generate-doxygen-header.py "$*"

define API_RST_template
$(1)_HEADER_FILES = $$(patsubst %,doxygen/%.rst,$$($(1)_HEADERS))

ALL_HEADER_FILES += $$($(1)_HEADER_FILES)

API_RST_FILES += $(1)-api.rst

$(1)-api.rst: generate-api-index.py $$($(1)_HEADER_FILES)
	@python3 generate-api-index.py $$@ $$($(1)_TITLE) $$($(1)_HEADER_FILES)
endef
$(foreach type, $(HEADER_TYPES),$(eval $(call API_RST_template,$(type))))

doxygen: Doxyfile $(INSTALL_HTMLS) $(ALL_HEADER_FILES) $(API_RST_FILES)
	$(DOXYGEN) -v
	$(DOXYGEN)
	rsync -av $(TOP)/html/ html/

clean:
	$(RM) -rf *-api.rst doxygen generated Doxyfile

.PHONY: doxygen

commit: doxygen
	$(TOUCH) html/.nojekyll
	(cd $(TOP) && $(CURDIR)/../commit-gh.sh $(GH_FILES))

.PHONY: commit
