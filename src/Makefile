#
# $Id$
#

TOP = ..
include $(TOP)/configure/CONFIG

#
# for c++ build add the dirs gdd and cas after ca
# (and uncomment the c++ files in src/lbCom/Makefile.Unix)
#
DIRS += include
DIRS += cxxTemplates
DIRS += toolsComm
DIRS += makeBaseApp
DIRS += libCom
DIRS += dbStatic
DIRS += db
DIRS += registry
DIRS += bpt
DIRS += ca
DIRS += util
DIRS += misc
DIRS += sequencer
DIRS += dbtools
DIRS += rsrv
DIRS += rec
DIRS += as
DIRS += dev
#FOR NOW JUST BUILD vxWorks dependent stuff
DIRS += vxWorks

#
# if CPLUSPLUS isnt empty then include C++ src codes
#
DIRS += $(patsubst %,gdd,$(strip $(CPLUSPLUS)))
DIRS += $(patsubst %,cas,$(strip $(CPLUSPLUS)))

include $(TOP)/configure/RULES_DIRS

