
TOP=../..

include $(TOP)/configure/CONFIG

#YACCOPT = -l
#LEXOPT = -L

#USR_LDLIBS = -ll
YACCOPT = -d -v

SYS_PROD_LIBS_DEFAULT := m
SYS_PROD_LIBS_WIN32   := -nil-

INC += seq.h
INC += seqCom.h

SRCS += snc_main.c
SRCS += parse.c
SRCS += phase2.c
SRCS += gen_ss_code.c
SRCS += gen_tables.c
SRCS += sncVersion.c
SRCS += snc.c

PROD += snc

seq_CFLAGS =

seq_SRCS = \
	seq_main.c seq_ca.c seq_qry.c seq_task.c \
	seq_mac.c seq_prog.c seq_if.c seqVersion.c

IOC_LIBRARY_vxWorks = seq

include $(TOP)/configure/RULES_BUILD

#
#  The generated lex file includes snc.h
#
snc.c: snc_lex.c

sncVersion.c: ../Version ../makeVersion.pl
	$(PERL) ../makeVersion.pl ../Version sncVersion

clean::
	@$(RM) y.output sncVersion.c snc_lex.c snc.c snc.h

seqVersion.c: ../Version ../makeVersion.pl
	$(PERL) ../makeVersion.pl ../Version seqVersion
