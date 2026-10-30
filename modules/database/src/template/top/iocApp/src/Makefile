# SPDX-FileCopyrightText: 2003 Argonne National Laboratory
#
# SPDX-License-Identifier: EPICS

TOP=../..
# This file should do very little - it's purpose is to set the APPNAME and then load build.mak

# this definition is used in build.mak
APPNAME=_APPNAME_

# If we are ###-IOC-01 leave this as is, if we are ###-IOC-02 or higher change to ###-IOC-01 and delete build.mak from this directory
# there should only be a single build.mak for all IOCs of a given family and it is located in the ###-IOC-01 directory
include $(TOP)/_01_APP_NAME_App/src/build.mak
