
TOP=../../..
 
include $(TOP)/configure/CONFIG
 
HOST_WARN=NO
 
YACC = $(EYACC)
YACCOPT = -l -d

SKELETON_FILE=include/flex.skel.static

#	ARCH_CLASS hp700 needs special flags,
#	everything else uses DEFAULT:
#
USR_CFLAGS_DEFAULT := -DUSG
USR_CFLAGS_hp700 := -DSCO_UNIX

#	This applies to all arch.classes:
#
USR_CFLAGS = -DDEFAULT_SKELETON_FILE=\"$(SKELETON_FILE)\"

INC += flex.skel.static

# main.c is included in parse.c
#
SRCS := ccl.c dfa.c ecs.c gen.c misc.c nfa.c sym.c tblcmp.c parse.c

PROD_DEFAULT := e_flex
PROD_vxWorks := -nil-

include $(TOP)/configure/RULES_BUILD

clean::
	$(RM) parse.c parse.h

#	EOF Makefile.Host for base/src/toolsComm/flex
