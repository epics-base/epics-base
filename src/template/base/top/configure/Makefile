# SPDX-FileCopyrightText: 1999 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES
