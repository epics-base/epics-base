#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ../../../..
include $(TOP)/configure/CONFIG

ifdef T_A
  PERL_VERSION = $(shell $(PERL) ../perlConfig.pl version)
  PERL_ARCHNAME = $(shell $(PERL) ../perlConfig.pl archname)
  PERL_ARCHPATH := $(PERL_VERSION)/$(PERL_ARCHNAME)

  PERL_ARCHLIB  := $(shell $(PERL) ../perlConfig.pl archlibexp)
  PERL_h = $(PERL_ARCHLIB)/CORE/perl.h

  EXTUTILS := $(shell $(PERL) ../perlConfig.pl privlibexp)/ExtUtils
  PERLBIN := $(shell $(PERL) ../perlConfig.pl binexp)
  XSUBPP := $(firstword $(wildcard $(PERLBIN)/xsubpp $(EXTUTILS)/xsubpp))

# Special settings for Darwin:
ifeq ($(OS_CLASS),Darwin)
  # Use hdepends command (not GNU compiler flags)
  # to generate header file dependancies for Darwin.
  # Darwin has multiple -arch compiler flags.
  HDEPENDS_METHOD = MKMF

  # Perl loadable libraries on Darwin have funny names
  LOADABLE_SHRLIB_PREFIX =
  LOADABLE_SHRLIB_SUFFIX = .$(shell $(PERL) ../perlConfig.pl dlext)

ifeq ($(wildcard $(PERL_h)),)
  # Perl's headers moved in Mojave
  SDK_PATH := $(shell xcodebuild -version -sdk macosx Path)
  PERL_ARCHLIB := $(SDK_PATH)/$(PERL_ARCHLIB)
endif
endif

ifneq ($(filter-out inc,$(strip $(MAKECMDGOALS))),)
ifeq ($(T_A),$(EPICS_HOST_ARCH))                 # No cross-builds (wrong Perl!)
ifeq ($(strip $(XSUBPP)),)
  $(warning Perl's xsubpp program was not found.)
  $(warning The Perl CA module will not be built.)
else
ifeq ($(wildcard $(PERL_h)),)
  $(warning Perl's C header files were not found.)
  $(warning The Perl CA module will not be built.)
else
ifeq ($(findstring $(OS_CLASS),WIN32 cygwin32),) # Doesn't build on WIN32
  LOADABLE_LIBRARY_HOST = Cap5

  PERL_SCRIPTS += capr.pl

  PERL_MODULES += CA.pm
  PERL_MODULES += $(PERL_ARCHPATH)/$(LOADABLE_SHRLIB_PREFIX)Cap5$(LOADABLE_SHRLIB_SUFFIX)

  HTMLS_DIR = .
  HTMLS = CA.html
endif
endif
endif
endif
endif
endif

Cap5_SRCS = Cap5.xs
Cap5_LIBS = ca Com
Cap5_INCLUDES = -I$(PERL_ARCHLIB)/CORE
Cap5_CFLAGS = $(shell $(PERL) ../perlConfig.pl ccflags)

CLEANS += Cap5.c

include $(TOP)/configure/RULES

ifdef T_A
  %.c: ../%.xs
	$(RM) $@ $@_new
	$(PERL) $(XSUBPP) -typemap $(EXTUTILS)/typemap $< > $@_new && $(MV) $@_new $@

  $(INSTALL_PERL_MODULES)/$(PERL_ARCHPATH)/%: %
	$(ECHO) "Installing loadable shared library $@"
	@$(INSTALL_LIBRARY) -d -m $(LIB_PERMISSIONS) $< $(INSTALL_PERL_MODULES)/$(PERL_ARCHPATH)
endif
