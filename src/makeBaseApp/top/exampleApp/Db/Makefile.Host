TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
#  Optimization needs the name of the appropriate
#  dbd file (compare with ../src/Makefile.Vx)
#DBDNAME = exampleApp.dbd

#----------------------------------------------------
# Add databases, templates, substitutions like this
DB += dbExample1.db dbExample2.db

#----------------------------------------------------
# Declare template files which do not show up in DB
#USES_TEMPLATE += dbExample2.template

#----------------------------------------------------
# The following line is for local dbd expansion
#DBDEXPAND = exampleInclude.dbd

include $(TOP)/configure/RULES.Db
#----------------------------------------
#  ADD RULES AFTER THIS LINE
