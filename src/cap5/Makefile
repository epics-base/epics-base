#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************

TOP=../..
include $(TOP)/configure/CONFIG

ifneq ($(findstring darwin,$(T_A)),)
  # Perl loadable libraries on Darwin have funny names
  LOADABLE_SHRLIB_PREFIX =
  LOADABLE_SHRLIB_SUFFIX = .bundle
endif

EXPAND_default += cainfo.pl@ caput.pl@ caget.pl@ camonitor.pl@ CA.pm@
EXPAND_WIN32 = -nil-

PERL_SCRIPTS_default += cainfo.pl
PERL_SCRIPTS_default += caput.pl
PERL_SCRIPTS_default += caget.pl
PERL_SCRIPTS_default += camonitor.pl
PERL_SCRIPTS_WIN32 = -nil-

PERL_MODULES_default += CA.pm
PERL_MODULES_WIN32 = -nil-

LOADABLE_LIBRARY_HOST_default = Cap5
LOADABLE_LIBRARY_HOST_WIN32 = -nil-

Cap5_SRCS = Cap5.xs
Cap5_LIBS = ca Com
Cap5_INCLUDES = -I $(shell $(PERL) ../perlConfig.pl archlib)/CORE
Cap5_CFLAGS = $(shell $(PERL) ../perlConfig.pl ccflags)

ifeq ($(findstring Host,$(VALID_BUILDS)),Host)
  # Can only create docs in Host build
  HTMLS_DIR = .
  HTMLS = CA.html
endif

include $(TOP)/configure/RULES

ifdef T_A
  PERL5_DIR = $(dir $(shell $(PERL) ../perlConfig.pl perl5))
  XSUBPP = $(PERL) $(PERL5_DIR)xsubpp
  PODCHECKER = $(PERL) $(PERL5_DIR)podchecker
  TYPEMAP = $(shell $(PERL) ../perlConfig.pl privlib)/ExtUtils/typemap

  %.c: ../%.xs
	$(RM) $@ $@_new
	$(XSUBPP) -typemap $(TYPEMAP) $< > $@_new && $(MV) $@_new $@

  %.html: %.pm
	$(RM) $@
	$(PODCHECKER) $< && pod2html --infile=$< --outfile=$@

  clean::
	$(RM) Cap5.c
endif
