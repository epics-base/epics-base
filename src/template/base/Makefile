# SPDX-FileCopyrightText: 1997 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../../..

include $(TOP)/configure/CONFIG

TEMPLATES_DIR = makeBaseApp

TEMPLATES += top/Makefile
TEMPLATES += top/.gitignore
TEMPLATES += top/configure/CONFIG
TEMPLATES += top/configure/CONFIG_SITE
TEMPLATES += top/configure/Makefile
TEMPLATES += top/configure/RELEASE
TEMPLATES += top/configure/RULES
TEMPLATES += top/configure/RULES.ioc
TEMPLATES += top/configure/RULES_DIRS
TEMPLATES += top/configure/RULES_TOP

# This is a template for a module that provides local templates.
# It needs to install the TEMPLATES files named above into the
# templateApp/top/... tree.
SUBTEMPL := $(TEMPLATES)
T_APP = top/templateApp
SUBFILES = $(addprefix $(T_APP)/,$(SUBTEMPL))

TEMPLATES += $(T_APP)/Makefile
TEMPLATES += $(T_APP)/makeSiteApp@
TEMPLATES += $(SUBFILES)

SCRIPTS_HOST += makeBaseApp.pl

include $(TOP)/configure/RULES

$(addprefix $(INSTALL_TEMPLATES_SUBDIR)/,$(SUBFILES)): \
    $(INSTALL_TEMPLATES_SUBDIR)/$(T_APP)/%: ../%
	$(ECHO) "Installing $@"
	@$(INSTALL) -d -m $(INSTALL_PERMISSIONS) $< $(@D)
