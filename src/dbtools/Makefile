
TOP=../..

include $(TOP)/configure/CONFIG

YACCOPT := -l
LEXOPT  := -L

#dbSubs_SRCS  = dbLoadTemplate_lex.c
dbSubs_SRCS += dbLoadTemplate.c

IOC_LIBRARY_vxWorks = dbSubs

include $(TOP)/configure/RULES_BUILD
 
#
#	These lex sources are included in some C sources,
#	so they have to be created in time:
#
dbLoadTemplate$(OBJ): dbLoadTemplate_lex.c

clean::
	@$(RM) dbLoadTemplate_lex.c dbLoadTemplate.c 

