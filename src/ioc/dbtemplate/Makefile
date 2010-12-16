#*************************************************************************
# Copyright (c) 2002 The University of Chicago, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution. 
#*************************************************************************
TOP=../..

include $(TOP)/configure/CONFIG

INC += dbLoadTemplate.h
INC += dbtoolsIocRegister.h

LIB_SRCS += dbLoadTemplate.c
LIB_SRCS += dbtoolsIocRegister.c

LIBRARY_IOC = dbtoolsIoc

dbtoolsIoc_LIBS = dbIoc dbStaticIoc Com

dbtoolsIoc_RCS = dbtoolsIoc.rc

HTMLS += dbLoadTemplate.html

include $(TOP)/configure/RULES
 
#
#	These lex sources are included in some C sources,
#	so they have to be created in time:
#
dbLoadTemplate.c: dbLoadTemplate_lex.c ../dbLoadTemplate.h

clean::
	@$(RM) dbLoadTemplate_lex.c dbLoadTemplate.c 

